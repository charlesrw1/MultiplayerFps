# integration_test.ps1
# Builds App.exe and runs the integration test passes:
#   App.exe --tests game [pattern...]
#   App.exe --tests editor [pattern...]
# Each pass writes TestFiles/integration_<mode>_results.xml.
# Exit 0 if all requested passes succeed, 1 otherwise.

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
    [switch]$TimingAssert
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

# ---- Build ----------------------------------------------------------------
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { Write-Error "MSBuild not found"; exit 1 }

Write-Host "=== Building App ($Config|x64) ===" -ForegroundColor Cyan
& $msbuild "$root\CsRemake.sln" /t:App /p:Configuration=$Config /p:Platform=x64 /v:minimal /m
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

$exe = "$root\x64\$Config\App.exe"
if (-not (Test-Path $exe)) { Write-Error "Binary not found: $exe"; exit 1 }

# ---- Helpers ---------------------------------------------------------------
function Run-Pass([string]$mode) {
    Write-Host "`n=== $mode pass ===" -ForegroundColor Cyan
    $args = @("--tests", $mode)
    foreach ($p in $Pattern)     { $args += $p }
    foreach ($f in $PatternFile) { $args += "@$f" }
    if ($Promote)      { $args += "--promote" }
    if ($Interactive)  { $args += "--interactive" }
    if ($TimingAssert) { $args += "--timing-assert" }
    & $exe @args
    return $LASTEXITCODE
}

# ---- Run passes -----------------------------------------------------------
$exits = @{}
if ($Mode -eq "" -or $Mode -eq "game")   { $exits["game"]   = Run-Pass "game" }
if ($Mode -eq "" -or $Mode -eq "editor") { $exits["editor"] = Run-Pass "editor" }
if ($exits.Count -eq 0) {
    Write-Error "Mode must be 'game', 'editor', or empty (both). Got '$Mode'."
    exit 1
}

# ---- Summary --------------------------------------------------------------
$failed = $false
foreach ($kv in $exits.GetEnumerator()) {
    $xml = "$root\TestFiles\integration_$($kv.Key)_results.xml"
    if (Test-Path $xml) { Write-Host "Wrote $xml" }
    if ($kv.Value -ne 0) { $failed = $true }
}

if ($failed) {
    $summary = ($exits.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join " "
    Write-Host "`nINTEGRATION TESTS FAILED ($summary)" -ForegroundColor Red
    exit 1
} else {
    Write-Host "`nAll integration tests passed." -ForegroundColor Green
    exit 0
}
