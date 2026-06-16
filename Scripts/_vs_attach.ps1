<#
.SYNOPSIS
    VS 2026 debugger attach helper. Dot-source from launcher scripts.

.DESCRIPTION
    Launches the target process with --wait-for-debugger so it spins until the
    debugger is present, then attaches VS via DTE and the process resumes.
    Probes VS 2026 Insiders first, then older ProgIDs. Falls back to
    vsjitdebugger.exe if no VS is running.
#>

function Attach-VSDebugger {
    param(
        [Parameter(Mandatory)][int]$TargetPid,
        [int]$TimeoutSeconds = 15
    )

    $dte = $null
    foreach ($progId in @("VisualStudio.DTE.18.0", "VisualStudio.DTE.17.0", "VisualStudio.DTE")) {
        try {
            $dte = [Runtime.InteropServices.Marshal]::GetActiveObject($progId)
            Write-Host "Attaching VS debugger ($progId) to PID $TargetPid..." -ForegroundColor Cyan
            break
        } catch {
            continue
        }
    }

    if (-not $dte) {
        Write-Warning "No running VS instance found - launching vsjitdebugger.exe (pick a debugger in the prompt)."
        Start-Process "vsjitdebugger.exe" -ArgumentList "-p", $TargetPid | Out-Null
        return
    }

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            foreach ($p in $dte.Debugger.LocalProcesses) {
                if ($p.ProcessID -eq $TargetPid) {
                    $p.Attach()
                    Write-Host "Attached." -ForegroundColor Green
                    return
                }
            }
        } catch {
            # RPC_E_CALL_REJECTED is common while VS is busy; retry.
            Write-Host "Attach attempt failed: $($_.Exception.Message)" -ForegroundColor DarkYellow
        }
        Start-Sleep -Milliseconds 250
    }
    Write-Warning "Could not attach VS debugger to PID $TargetPid within ${TimeoutSeconds}s."
}

function Invoke-AppWithDebugger {
    param(
        [Parameter(Mandatory)][string]$Config,
        [string[]]$AppArgs = @(),
        [string]$ExeName = "App"
    )
    # $PSScriptRoot inside this function refers to the directory of the
    # dot-sourced helper, which is Scripts/. Repo root is one level up.
    $repoRoot = Split-Path -Parent $PSScriptRoot

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $msbuild = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild `
        -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
    if (-not $msbuild) { Write-Error "MSBuild not found"; exit 1 }
    Write-Host "Using MSBuild: $msbuild" -ForegroundColor DarkGray

    Write-Host "=== Building $ExeName ($Config|x64) ===" -ForegroundColor Cyan
    & $msbuild "$repoRoot\CsRemake.sln" /t:$ExeName /p:Configuration=$Config /p:Platform=x64 /v:minimal /m
    if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

    $exe = "$repoRoot\x64\$Config\$ExeName.exe"
    if (-not (Test-Path $exe)) { Write-Error "Binary not found: $exe"; exit 1 }

    # --wait-for-debugger makes the app spin on IsDebuggerPresent() before doing
    # anything, so the attach always wins the race.
    $launchArgs = @("--wait-for-debugger") + $AppArgs
    Write-Host ("=== Launching $ExeName.exe " + ($launchArgs -join ' ') + " ===") -ForegroundColor Cyan

    $proc = Start-Process -FilePath $exe -ArgumentList $launchArgs -PassThru -NoNewWindow -WorkingDirectory $repoRoot
    Attach-VSDebugger -TargetPid $proc.Id
    $proc.WaitForExit()
    exit $proc.ExitCode
}
