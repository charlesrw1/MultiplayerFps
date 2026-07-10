# build_and_test.ps1
# Builds the UnitTests project and runs the resulting executable.
# Usage: .\Scripts\build_and_test.ps1 [-Configuration Debug|Release] [-Platform x64|x86]

param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path $PSScriptRoot -Parent
$_item = Get-Item $RepoRoot; if ($_item.LinkType) { $RepoRoot = $_item.Target.TrimEnd('\') }

# Locate msbuild (include -prerelease so VS 2026 Insiders is picked up).
# MUST be the 64-bit Bin\amd64\MSBuild.exe: the VS IDE is 64-bit, and mixing a
# 32-bit CLI MSBuild with the IDE corrupts FileTracker .tlog state -> full rebuilds.
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -prerelease -requires Microsoft.Component.MSBuild `
    -find MSBuild\**\Bin\amd64\MSBuild.exe 2>$null | Select-Object -First 1

if (-not $msbuild) {
    Write-Error "Could not find MSBuild via vswhere. Is Visual Studio installed?"
    exit 1
}
Write-Host "Using MSBuild: $msbuild" -ForegroundColor DarkGray

$sln = Join-Path $RepoRoot "CsRemake.sln"
$testExe = Join-Path $RepoRoot "$Platform\$Configuration\UnitTests.exe"

# Build only the UnitTests project
Write-Host "==> Building UnitTests ($Configuration|$Platform)..." -ForegroundColor Cyan
& $msbuild $sln /t:UnitTests /p:Configuration=$Configuration /p:Platform=$Platform /p:BuildingInsideVisualStudio=true /p:PreferredToolArchitecture=x64 /v:minimal
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed (exit $LASTEXITCODE)"
    exit $LASTEXITCODE
}

# Run tests
Write-Host ""
Write-Host "==> Running tests..." -ForegroundColor Cyan
& $testExe
$exitCode = $LASTEXITCODE

if ($exitCode -eq 0) {
    Write-Host ""
    Write-Host "All tests passed." -ForegroundColor Green
} else {
    Write-Host ""
    Write-Error "Tests FAILED (exit $exitCode)"

    # The SEH filter writes crash_*.dmp into the working directory on an
    # unhandled exception. Print a first-look summary via dbg.ps1; the AI can
    # run further queries against the dump itself for deeper inspection.
    $dumps = Get-ChildItem -Path $RepoRoot -Filter "*.dmp" -ErrorAction SilentlyContinue |
             Where-Object { $_.LastWriteTime -gt (Get-Date).AddMinutes(-10) }
    foreach ($dump in $dumps) {
        Write-Host "`n=== minidump: $($dump.FullName) ===" -ForegroundColor Yellow
        Write-Host "--- stack (kP 30) ---" -ForegroundColor DarkYellow
        & "$PSScriptRoot\dbg.ps1" $dump.FullName "kP 30"
        Write-Host "--- frame 0 locals ---" -ForegroundColor DarkYellow
        & "$PSScriptRoot\dbg.ps1" $dump.FullName ".frame 0; dx -r2 @`$curframe.LocalVariables"
        Write-Host "[CRASH] for deeper inspection: Scripts\dbg.ps1 $($dump.FullName) '<cdb command>'" -ForegroundColor Yellow
    }
}

exit $exitCode
