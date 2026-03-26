# build_and_test.ps1
# Builds the UnitTests project and runs the resulting executable.
# Usage: .\Scripts\build_and_test.ps1 [-Configuration Debug|Release] [-Platform x64|x86]

param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path $PSScriptRoot -Parent

# Locate msbuild
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -requires Microsoft.Component.MSBuild `
    -find MSBuild\**\Bin\MSBuild.exe 2>$null | Select-Object -First 1

if (-not $msbuild) {
    Write-Error "Could not find MSBuild via vswhere. Is Visual Studio installed?"
    exit 1
}

$sln = Join-Path $RepoRoot "CsRemake.sln"
$testExe = Join-Path $RepoRoot "$Platform\$Configuration\UnitTests.exe"

# Build only the UnitTests project
Write-Host "==> Building UnitTests ($Configuration|$Platform)..." -ForegroundColor Cyan
& $msbuild $sln /t:UnitTests /p:Configuration=$Configuration /p:Platform=$Platform /v:minimal
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
}

exit $exitCode
