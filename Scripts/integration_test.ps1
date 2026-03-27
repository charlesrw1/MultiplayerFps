# integration_test.ps1
# Builds IntegrationTests project and runs both game and editor passes.
# Writes combined JUnit XML to TestFiles/integration_results.xml.
# Exit code: 0 if both passes succeed, 1 if either fails.

param(
    [string]$Config   = "Debug",
    [string]$TestFilter = "",
    [switch]$Promote,
    [switch]$TimingAssert
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

# ---- Build ----------------------------------------------------------------
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { Write-Error "MSBuild not found"; exit 1 }

Write-Host "=== Building IntegrationTests ($Config|x64) ===" -ForegroundColor Cyan
& $msbuild "$root\CsRemake.sln" /t:IntegrationTests /p:Configuration=$Config /p:Platform=x64 /v:minimal /m
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

$exe = "$root\x64\$Config\IntegrationTests.exe"
if (-not (Test-Path $exe)) { Write-Error "Binary not found: $exe"; exit 1 }

# ---- Helpers ---------------------------------------------------------------
function Build-Args([string]$mode) {
    $a = @("--mode=$mode") + "--no_console_print"
    if ($TestFilter)   { $a += "--test=$TestFilter" }
    if ($Promote)      { $a += "--promote" }
    if ($TimingAssert) { $a += "--timing-assert" }
    return $a
}

# ---- Game pass -------------------------------------------------------------
Write-Host "`n=== Game pass ===" -ForegroundColor Cyan
& $exe (Build-Args "game") -exec "test_game_vars.txt"
$gameExit = $LASTEXITCODE

# ---- Editor pass -----------------------------------------------------------
Write-Host "`n=== Editor pass ===" -ForegroundColor Cyan
& $exe (Build-Args "editor") -exec "test_editor_vars.txt"
$editorExit = $LASTEXITCODE

# ---- Merge JUnit XML -------------------------------------------------------
$gameXml   = "$root\TestFiles\integration_game_results.xml"
$editorXml = "$root\TestFiles\integration_editor_results.xml"
$outXml    = "$root\TestFiles\integration_results.xml"

$combined = '<?xml version="1.0" encoding="UTF-8"?>' + "`n" + '<testsuites>' + "`n"
foreach ($src in @($gameXml, $editorXml)) {
    if (Test-Path $src) {
        $content = Get-Content $src -Raw
        # Strip XML declaration and wrap
        $content = $content -replace '^\s*<\?xml[^>]+\?>\s*', ''
        $combined += $content
    }
}
$combined += '</testsuites>' + "`n"
$combined | Set-Content -Encoding UTF8 $outXml
Write-Host "Results written to $outXml"

# ---- Summary ---------------------------------------------------------------
$anyFail = ($gameExit -ne 0) -or ($editorExit -ne 0)
if ($anyFail) {
    Write-Host "`nINTEGRATION TESTS FAILED (game=$gameExit editor=$editorExit)" -ForegroundColor Red
    exit 1
} else {
    Write-Host "`nAll integration tests passed." -ForegroundColor Green
    exit 0
}
