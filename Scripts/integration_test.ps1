<#
.SYNOPSIS
    Builds App.exe and runs the integration test passes (game / editor).

.DESCRIPTION
    Runs:
        App.exe --tests game   [pattern...]
        App.exe --tests editor [pattern...]
    Each pass writes TestFiles/integration_<mode>_results.xml.
    Exits 0 if all requested passes succeed, 1 otherwise.

    Pattern matching uses globs (`*` matches any sequence) and applies to both
    C++ and Lua test names. See docs/testing.md for the pattern reference.

    App.exe also writes test_<mode>_output.log next to the working directory
    with the full uncoloured engine log (every line is prefixed with [Error],
    [Warning], [Debug] or [Info]). Console stdout/stderr carries the
    test-runner sentinel lines (`[TEST] ...`) plus any [CRASH]/[Error]/[Fatal]
    lines and stack frames so unhandled SEH exceptions are visible inline.
    On a non-zero pass exit, the tail of test_<mode>_output.log is also
    dumped to the host so agents see the full crash context without having
    to know to read the log file.

.PARAMETER Config
    Build configuration. Defaults to "Debug". Use "Release" for release builds.

.PARAMETER Mode
    Run a single mode: "game" or "editor". Default (empty) runs both.

.PARAMETER Pattern
    Glob pattern(s) to filter tests. Accepts an array. Empty = run all.

.PARAMETER PatternFile
    File(s) containing one glob per line ('#' starts a comment). Passed to
    App.exe as @<file> entries.

.PARAMETER Promote
    Write current screenshots as new goldens.

.PARAMETER Interactive
    Keep the window visible during the run.

.PARAMETER TimingAssert
    Fail tests on slow GPU timings.

.PARAMETER ShowEngineLog
    Forward App.exe's full engine output (init logs, GL warnings, asset
    chatter) to the host. Default behaviour is to forward only the test-runner
    sentinel lines (`[TEST] ...`); the full uncoloured log is always written
    to test_<mode>_output.log regardless.

.PARAMETER Debugger
    Attach Visual Studio 2026 to App.exe at the start of each pass. Uses DTE
    to attach to an already-running VS instance (whichever one is registered
    in the ROT); falls back to vsjitdebugger.exe if no VS is running. In this
    mode App.exe runs as a child process with stdout/stderr streamed directly
    to the console (filtering is bypassed - read test_<mode>_output.log for
    the structured log).

.EXAMPLE
    Scripts/integration_test.ps1
    Build and run both game + editor passes (all tests, concise output).

.EXAMPLE
    Scripts/integration_test.ps1 -Mode editor -Pattern "editor/prefab*"
    Run prefab editor tests.

.EXAMPLE
    Scripts/integration_test.ps1 -Mode game -Pattern "game/boot" -Debugger
    Attach the running VS 2026 instance to App.exe for the game pass.

.EXAMPLE
    Scripts/integration_test.ps1 -ShowEngineLog
    Run all tests and stream the full engine log to the console too.

.EXAMPLE
    Scripts/integration_test.ps1 -Mode game -Pattern "game/boot"
    Run a single test in the game pass.

.EXAMPLE
    Scripts/integration_test.ps1 -Pattern "renderer/*","racing_line/*"
    Run multiple glob patterns across both passes.

.EXAMPLE
    Scripts/integration_test.ps1 -PatternFile smoke.txt
    Read patterns from a file (one glob per line, '#' for comments).

.EXAMPLE
    Scripts/integration_test.ps1 -Config Release -Promote
    Release build, regenerate screenshot goldens.
#>
param(
    [string]$Config       = "Debug",
    # Run a single mode ("game"|"editor"). Default runs both.
    [string]$Mode         = "",
    # Glob pattern(s) to filter tests (positional, space-separated). Empty = all.
    [string[]]$Pattern    = @(),
    # @-prefixed file(s) of patterns (one glob per line, # for comments).
    [string[]]$PatternFile = @(),
    [switch]$Promote,
    [switch]$Interactive,
    [switch]$TimingAssert,
    [switch]$ShowEngineLog,
    [switch]$Debugger
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

# ---- Build ----------------------------------------------------------------
# -prerelease ensures VS 2026 Insiders (Dev18) is picked over older stable installs.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild `
    -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { Write-Error "MSBuild not found"; exit 1 }
Write-Host "Using MSBuild: $msbuild" -ForegroundColor DarkGray

Write-Host "=== Building App ($Config|x64) ===" -ForegroundColor Cyan
& $msbuild "$root\CsRemake.sln" /t:App /p:Configuration=$Config /p:Platform=x64 /v:minimal /m
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

$exe = "$root\x64\$Config\App.exe"
if (-not (Test-Path $exe)) { Write-Error "Binary not found: $exe"; exit 1 }

# ---- Helpers ---------------------------------------------------------------
function Attach-VSDebugger([int]$targetPid, [int]$timeoutSeconds = 15) {
    # Try to grab a running VS instance from the ROT. VS 2026 Insiders
    # registers as VisualStudio.DTE.18.0; we also probe older ProgIDs so this
    # still works on a Dev17 install if someone hasn't upgraded yet.
    $dte = $null
    foreach ($progId in @("VisualStudio.DTE.18.0", "VisualStudio.DTE.17.0", "VisualStudio.DTE")) {
        try {
            $dte = [Runtime.InteropServices.Marshal]::GetActiveObject($progId)
            Write-Host "Attaching VS debugger ($progId) to PID $targetPid..." -ForegroundColor Cyan
            break
        } catch {
            continue
        }
    }

    if (-not $dte) {
        Write-Warning "No running VS instance found - launching vsjitdebugger.exe (pick a debugger in the prompt)."
        Start-Process "vsjitdebugger.exe" -ArgumentList "-p", $targetPid | Out-Null
        return
    }

    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            foreach ($p in $dte.Debugger.LocalProcesses) {
                if ($p.ProcessID -eq $targetPid) {
                    $p.Attach()
                    Write-Host "Attached." -ForegroundColor Green
                    return
                }
            }
        } catch {
            # RPC_E_CALL_REJECTED is common while VS is busy; retry.
        }
        Start-Sleep -Milliseconds 250
    }
    Write-Warning "Could not attach VS debugger to PID $targetPid within ${timeoutSeconds}s."
}

function Run-Pass([string]$mode) {
    Write-Host "`n=== $mode pass ===" -ForegroundColor Cyan
    $args = @("--tests", $mode)
    foreach ($p in $Pattern)     { $args += $p }
    foreach ($f in $PatternFile) { $args += "@$f" }
    if ($Promote)      { $args += "--promote" }
    if ($Interactive)  { $args += "--interactive" }
    if ($TimingAssert) { $args += "--timing-assert" }

    if ($Debugger) {
        # Launch as a separate process so we can grab its PID and attach VS
        # before user-code-of-interest runs. Output filtering is dropped here -
        # the full engine log is still on disk via test_<mode>_output.log.
        $proc = Start-Process -FilePath $exe -ArgumentList $args -PassThru -NoNewWindow
        Attach-VSDebugger $proc.Id
        $proc.WaitForExit()
        return $proc.ExitCode
    }

    # Stream App.exe output to the host directly (Out-Host) so it does NOT get
    # captured into this function's pipeline output. Without this, every stdout
    # line ends up in the function's return value alongside the exit code,
    # which breaks the `-ne 0` exit-code check downstream.
    # PS 5.1 + $ErrorActionPreference="Stop" + native stderr is hostile: any
    # write to stderr by App.exe surfaces as a NativeCommandError that
    # terminates the pipeline. Drop EAP to Continue around the exe run so
    # [CRASH] stack dumps print in full (would otherwise stop after one line).
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        if ($ShowEngineLog) {
            & $exe @args 2>&1 | Out-Host
        } else {
            # Stdout: filter to test-runner sentinels. Stderr: untouched, flows to host.
            & $exe @args | Where-Object { $_ -match '^\[TEST\]' } | Out-Host
        }
    } finally {
        $ErrorActionPreference = $prevEAP
    }
    return $LASTEXITCODE
}

function Dump-LogTail([string]$mode) {
    $log = "$root\test_${mode}_output.log"
    if (-not (Test-Path $log)) {
        Write-Host "  (no log file at $log)" -ForegroundColor DarkYellow
        return
    }
    # Stream the tail and surface only crash/error/assert lines — the full log is
    # ~thousands of lines of init chatter, and the agent only cares about the
    # failure context. Belt-and-braces in case stderr was redirected or filtered.
    Write-Host "`n=== $mode crash/error lines from $log ===" -ForegroundColor Yellow
    Get-Content -Path $log -Tail 400 |
        Where-Object { $_ -match '\[CRASH\]|\[Error\]|\[Fatal\]|Assertion failed:' } |
        Out-Host
    Write-Host "=== end $mode log tail ===" -ForegroundColor Yellow
}

function Print-XmlSummary([string]$mode, [string]$xml) {
    if (-not (Test-Path $xml)) {
        Write-Host ("  {0,-6} : (no results XML)" -f $mode) -ForegroundColor Red
        return
    }
    [xml]$doc = Get-Content -Raw $xml
    $suite   = $doc.testsuite
    $total   = [int]$suite.tests
    $failed  = [int]$suite.failures
    $passed  = $total - $failed
    $color   = if ($failed -eq 0) { "Green" } else { "Red" }
    Write-Host ("  {0,-6} : {1}/{2} passed" -f $mode, $passed, $total) -ForegroundColor $color
    foreach ($tc in @($suite.testcase)) {
        if ($tc.failure) {
            Write-Host ("      FAIL  {0}" -f $tc.name) -ForegroundColor Red
            foreach ($f in @($tc.failure)) {
                Write-Host ("            {0}" -f $f.message) -ForegroundColor Red
            }
        } else {
            Write-Host ("      pass  {0}" -f $tc.name) -ForegroundColor DarkGreen
        }
    }
}

# ---- Run passes -----------------------------------------------------------
$exits = [ordered]@{}
if ($Mode -eq "" -or $Mode -eq "game")   { $exits["game"]   = Run-Pass "game" }
if ($Mode -eq "" -or $Mode -eq "editor") { $exits["editor"] = Run-Pass "editor" }
if ($exits.Count -eq 0) {
    Write-Error "Mode must be 'game', 'editor', or empty (both). Got '$Mode'."
    exit 1
}

# ---- Summary --------------------------------------------------------------
Write-Host "`n=== Summary ===" -ForegroundColor Cyan
$failed = $false
foreach ($kv in $exits.GetEnumerator()) {
    $xml = "$root\TestFiles\integration_$($kv.Key)_results.xml"
    Print-XmlSummary $kv.Key $xml
    if ($kv.Value -ne 0) {
        $failed = $true
        # Belt-and-braces: even if a crash line slipped past the regex above, the
        # full uncoloured engine log on disk will contain it. Dump the tail so the
        # agent gets a stack trace without having to know about the log file.
        Dump-LogTail $kv.Key
    }
}
Write-Host "  log files: test_<mode>_output.log (uncoloured, [Error]/[Warning]/... tagged)" -ForegroundColor DarkGray

if ($failed) {
    $codes = ($exits.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join " "
    Write-Host "`nINTEGRATION TESTS FAILED ($codes)" -ForegroundColor Red
    exit 1
} else {
    Write-Host "`nAll integration tests passed." -ForegroundColor Green
    exit 0
}
