<#
.SYNOPSIS
    Build and run unit tests (and optionally integration tests).

.PARAMETER Config
    Build configuration: Debug (default) or Release.

.PARAMETER Filter
    gtest filter string, e.g. "HashMapTest.*" or "HashMapTest.InsertAndFind".
    Passed directly to --gtest_filter.

.PARAMETER Integration
    When set, run integration tests (boots the full engine via EngineTesterApp).
    Requires GPU / SDL2 window.

.EXAMPLE
    .\Scripts\run_tests.ps1
    .\Scripts\run_tests.ps1 -Config Release
    .\Scripts\run_tests.ps1 -Filter "HashMapTest.*"
    .\Scripts\run_tests.ps1 -Integration
#>
param(
    [string]$Config = "Debug",
    [string]$Filter = "",
    [switch]$Integration
)

$Root       = Split-Path $PSScriptRoot -Parent
$Solution   = Join-Path $Root "CsRemake.sln"
$TestExe    = Join-Path $Root "x64\$Config\intermediate\UnitTests\UnitTests.exe"
$MsBuild    = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
                  -latest -requires Microsoft.Component.MSBuild `
                  -find MSBuild\**\Bin\MSBuild.exe 2>$null | Select-Object -First 1

if (-not $MsBuild) {
    Write-Error "MSBuild not found. Make sure Visual Studio is installed."
    exit 1
}

# ---- Build ----
Write-Host "Building UnitTests ($Config)..." -ForegroundColor Cyan
& $MsBuild $Solution /t:UnitTests /p:Configuration=$Config /p:Platform=x64 /v:minimal /nologo
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed."
    exit $LASTEXITCODE
}

if (-not (Test-Path $TestExe)) {
    Write-Error "Test executable not found: $TestExe"
    exit 1
}

# ---- Run ----
$args = @()

if ($Integration) {
    Write-Host "Running integration tests..." -ForegroundColor Yellow
    $args += "--integration"
}
else {
    Write-Host "Running unit tests..." -ForegroundColor Green
    if ($Filter) { $args += "--gtest_filter=$Filter" }
    $args += "--gtest_color=yes"
}

& $TestExe @args
exit $LASTEXITCODE
