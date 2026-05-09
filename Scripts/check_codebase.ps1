<#
.SYNOPSIS
    Run all repo health checks: docs, file length, coverage, TODO scan.

.DESCRIPTION
    Single-shot quality gate. Sections:
      1. docs.exe check
      2. LOC per source file       (warn >600, error >1000)
      3. OpenCppCoverage           (integration tests, per-mode, optional)
      4. TODO / FIXME / HACK scan  (warning only)

    Excludes: Source/External, Source/.generated, MEGA.cpp.

    Exit 0 = no errors. Exit 1 = at least one error. Warnings never fail.

.PARAMETER Quick
    Skip slow checks (coverage). Equivalent to -SkipCoverage.

.PARAMETER SkipDocs / -SkipLoc / -SkipCoverage / -SkipTodos
    Granular skips.

.PARAMETER Config
    Build configuration for coverage runs. Default Debug.

.PARAMETER CoverageOutDir
    Where to write coverage XML + coverage_summary.md. Default: repo root.

.EXAMPLE
    Scripts/check_codebase.ps1 -Quick
.EXAMPLE
    Scripts/check_codebase.ps1
#>
param(
    [switch]$Quick,
    [switch]$SkipDocs,
    [switch]$SkipLoc,
    [switch]$SkipCoverage,
    [switch]$SkipTodos,
    [string]$Config = "Debug",
    [string]$CoverageOutDir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $CoverageOutDir) { $CoverageOutDir = $root }

$LocSoft = 600
$LocHard = 1000

# Sections track their own status; aggregate at the end.
$results = [ordered]@{}
function Set-Section($name, $status, $detail = "") {
    $script:results[$name] = [pscustomobject]@{ Status=$status; Detail=$detail }
}

# ---------- 1. docs.exe check ---------------------------------------------
function Invoke-DocsCheck {
    Write-Host "`n=== docs.exe check ===" -ForegroundColor Cyan
    $docs = Get-Command docs.exe -ErrorAction SilentlyContinue
    if (-not $docs) {
        Write-Host "  docs.exe not found in PATH; skipping" -ForegroundColor Yellow
        Set-Section "docs"  "skip" "docs.exe missing"
        return
    }
    & docs.exe check
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  docs OK" -ForegroundColor Green
        Set-Section "docs" "ok"
    } else {
        Write-Host "  docs check FAILED (exit $LASTEXITCODE)" -ForegroundColor Red
        Set-Section "docs" "error" "exit $LASTEXITCODE"
    }
}

# ---------- 2. LOC per file -----------------------------------------------
function Get-SourceFiles {
    Get-ChildItem -Path "$root\Source" -Recurse -Include *.cpp,*.h,*.hpp,*.cc -File |
        Where-Object {
            $p = $_.FullName.Replace('\','/')
            ($p -notmatch '/Source/External/') -and
            ($p -notmatch '/\.generated/')      -and
            ($_.Name -ne 'MEGA.cpp')
        }
}

function Invoke-LocCheck {
    Write-Host "`n=== LOC per file (warn >$LocSoft, error >$LocHard) ===" -ForegroundColor Cyan
    $warn = @(); $err = @()
    foreach ($f in Get-SourceFiles) {
        $n = (Get-Content -LiteralPath $f.FullName).Count
        if     ($n -gt $LocHard) { $err  += [pscustomobject]@{ Loc=$n; Path=$f.FullName } }
        elseif ($n -gt $LocSoft) { $warn += [pscustomobject]@{ Loc=$n; Path=$f.FullName } }
    }
    foreach ($x in ($err | Sort-Object Loc -Descending)) {
        Write-Host ("  ERROR {0,5}  {1}" -f $x.Loc, (Resolve-Path -Relative $x.Path)) -ForegroundColor Red
    }
    foreach ($x in ($warn | Sort-Object Loc -Descending | Select-Object -First 25)) {
        Write-Host ("  warn  {0,5}  {1}" -f $x.Loc, (Resolve-Path -Relative $x.Path)) -ForegroundColor Yellow
    }
    if ($warn.Count -gt 25) {
        Write-Host ("  ... and {0} more warnings" -f ($warn.Count - 25)) -ForegroundColor DarkYellow
    }
    Write-Host ("  totals: {0} files, {1} warn, {2} error" -f (Get-SourceFiles).Count, $warn.Count, $err.Count) -ForegroundColor DarkGray
    if ($err.Count -gt 0) { Set-Section "loc" "error" "$($err.Count) over $LocHard" }
    elseif ($warn.Count -gt 0) { Set-Section "loc" "warn" "$($warn.Count) over $LocSoft" }
    else { Set-Section "loc" "ok" }
}

# ---------- 3. OpenCppCoverage --------------------------------------------
function Find-OpenCppCoverage {
    $cmd = Get-Command OpenCppCoverage.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($p in @(
        "$env:ProgramFiles\OpenCppCoverage\OpenCppCoverage.exe",
        "${env:ProgramFiles(x86)}\OpenCppCoverage\OpenCppCoverage.exe"
    )) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

function Build-App {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $msbuild = & $vswhere -latest -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
    if (-not $msbuild) { throw "MSBuild not found" }
    Write-Host "  building App ($Config|x64)" -ForegroundColor DarkGray
    & $msbuild "$root\CsRemake.sln" /t:App /p:Configuration=$Config /p:Platform=x64 /v:minimal /m | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "App build failed" }
}

function Parse-Cobertura {
    param([string]$XmlPath)
    if (-not (Test-Path $XmlPath)) { return @{} }
    [xml]$doc = Get-Content -Raw -LiteralPath $XmlPath
    $map = @{}
    foreach ($pkg in $doc.coverage.packages.package) {
        foreach ($cls in $pkg.classes.class) {
            $file = $cls.filename
            if (-not $file) { continue }
            $rate = [double]$cls.'line-rate'
            # Some cobertura emitters duplicate classes per file; take max rate seen.
            if (-not $map.ContainsKey($file) -or $map[$file] -lt $rate) {
                $map[$file] = $rate
            }
        }
    }
    return $map
}

function Invoke-CoverageCheck {
    Write-Host "`n=== OpenCppCoverage (integration tests) ===" -ForegroundColor Cyan
    $occ = Find-OpenCppCoverage
    if (-not $occ) {
        Write-Host "  OpenCppCoverage.exe not found." -ForegroundColor Yellow
        Write-Host "  Install: winget install OpenCppCoverage.OpenCppCoverage" -ForegroundColor Yellow
        Set-Section "coverage" "skip" "OpenCppCoverage missing"
        return
    }
    try { Build-App } catch {
        Write-Host "  $_" -ForegroundColor Red
        Set-Section "coverage" "error" "build failed"
        return
    }
    $exe = "$root\x64\$Config\App.exe"
    if (-not (Test-Path $exe)) {
        Write-Host "  App.exe missing: $exe" -ForegroundColor Red
        Set-Section "coverage" "error" "App.exe missing"
        return
    }

    $modes = @("game","editor")
    $maps  = @{}
    foreach ($m in $modes) {
        $xml = Join-Path $CoverageOutDir "coverage_$m.xml"
        Write-Host "  running coverage: $m" -ForegroundColor DarkGray
        & $occ `
            --sources "$root\Source" `
            --excluded_sources "External" `
            --excluded_sources "MEGA.cpp" `
            --excluded_sources ".generated" `
            --export_type "cobertura:$xml" `
            --quiet `
            -- $exe --tests $m | Out-Null
        # OpenCppCoverage exit reflects the child; we keep going regardless so
        # coverage data is still produced even with test failures.
        $maps[$m] = Parse-Cobertura $xml
    }

    # Build merged file set + matrix
    $allFiles = New-Object System.Collections.Generic.HashSet[string]
    foreach ($m in $modes) { foreach ($k in $maps[$m].Keys) { [void]$allFiles.Add($k) } }

    $rows = foreach ($file in $allFiles) {
        $game   = if ($maps['game'].ContainsKey($file))   { $maps['game'][$file] }   else { $null }
        $editor = if ($maps['editor'].ContainsKey($file)) { $maps['editor'][$file] } else { $null }
        $best   = @($game, $editor) | Where-Object { $_ -ne $null } | Measure-Object -Maximum
        [pscustomobject]@{
            File   = $file
            Game   = $game
            Editor = $editor
            Best   = if ($best.Count -gt 0) { $best.Maximum } else { 0.0 }
        }
    }
    $rows = $rows | Sort-Object Best

    $md = New-Object System.Text.StringBuilder
    [void]$md.AppendLine("# Coverage summary (integration tests)")
    [void]$md.AppendLine("")
    [void]$md.AppendLine("Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm')")
    [void]$md.AppendLine("Sorted ascending by best per-file coverage. `-` = file not seen by that mode.")
    [void]$md.AppendLine("")
    [void]$md.AppendLine("| file | game | editor |")
    [void]$md.AppendLine("|---|---|---|")
    foreach ($r in $rows) {
        $g = if ($r.Game   -ne $null) { "{0,4:P0}" -f $r.Game }   else { "  -" }
        $e = if ($r.Editor -ne $null) { "{0,4:P0}" -f $r.Editor } else { "  -" }
        [void]$md.AppendLine("| $($r.File) | $g | $e |")
    }
    $summary = Join-Path $CoverageOutDir "coverage_summary.md"
    Set-Content -Encoding UTF8 -LiteralPath $summary -Value $md.ToString()
    Write-Host "  wrote $summary  ($($rows.Count) files)" -ForegroundColor Green

    $low = @($rows | Where-Object { $_.Best -lt 0.5 })
    Write-Host ("  files <50% best: {0}" -f $low.Count) -ForegroundColor DarkGray
    Set-Section "coverage" "ok" "$($rows.Count) files, $($low.Count) <50%"
}

# ---------- 4. TODO / FIXME scan ------------------------------------------
function Invoke-TodoScan {
    Write-Host "`n=== TODO / FIXME / HACK scan ===" -ForegroundColor Cyan
    $rg = Get-Command rg.exe -ErrorAction SilentlyContinue
    if (-not $rg) {
        Write-Host "  rg.exe not in PATH; skipping" -ForegroundColor Yellow
        Set-Section "todos" "skip" "rg.exe missing"
        return
    }
    $hits = & rg.exe --no-heading --line-number `
        --glob '!Source/External/**' `
        --glob '!Source/.generated/**' `
        --glob '!Source/**/MEGA.cpp' `
        -t cpp `
        '\b(TODO|FIXME|HACK|XXX)\b' "$root\Source" 2>$null
    if (-not $hits) {
        Write-Host "  none found" -ForegroundColor Green
        Set-Section "todos" "ok" "0"
        return
    }
    $byFile = @{}
    foreach ($line in $hits) {
        # rg output: path:lineno:content. Windows paths contain `:` (drive
        # letter), so match path as "anything up to :<digits>:" instead of
        # splitting on the first colon.
        if ($line -match '^(.+?):\d+:') {
            $file = $matches[1]
            if (-not $byFile.ContainsKey($file)) { $byFile[$file] = 0 }
            $byFile[$file]++
        }
    }
    $sorted = $byFile.GetEnumerator() | Sort-Object -Property Value -Descending
    $top = $sorted | Select-Object -First 15
    foreach ($e in $top) {
        Write-Host ("  {0,4}  {1}" -f $e.Value, $e.Key) -ForegroundColor DarkYellow
    }
    if ($sorted.Count -gt 15) {
        Write-Host ("  ... and {0} more files" -f ($sorted.Count - 15)) -ForegroundColor DarkGray
    }
    $total = ($byFile.Values | Measure-Object -Sum).Sum
    Write-Host ("  totals: {0} markers in {1} files" -f $total, $byFile.Count) -ForegroundColor DarkGray
    Set-Section "todos" "warn" "$total markers"
}

# ---------- main ----------------------------------------------------------
Push-Location $root
try {
    if (-not $SkipDocs)                      { Invoke-DocsCheck }
    if (-not $SkipLoc)                       { Invoke-LocCheck }
    if (-not $SkipCoverage -and -not $Quick) { Invoke-CoverageCheck }
    elseif (-not $SkipCoverage -and $Quick)  { Write-Host "`n(coverage skipped: -Quick)" -ForegroundColor DarkGray; Set-Section "coverage" "skip" "-Quick" }
    if (-not $SkipTodos)                     { Invoke-TodoScan }
} finally {
    Pop-Location
}

# ---------- summary -------------------------------------------------------
Write-Host "`n=== Summary ===" -ForegroundColor Cyan
$hasError = $false
foreach ($k in $results.Keys) {
    $r = $results[$k]
    $color = switch ($r.Status) {
        "ok"    { "Green" }
        "warn"  { "Yellow" }
        "error" { "Red" }
        "skip"  { "DarkGray" }
        default { "White" }
    }
    $line = "  {0,-10} {1,-6} {2}" -f $k, $r.Status, $r.Detail
    Write-Host $line -ForegroundColor $color
    if ($r.Status -eq "error") { $hasError = $true }
}

if ($hasError) {
    Write-Host "`nCheck FAILED" -ForegroundColor Red
    exit 1
} else {
    Write-Host "`nCheck OK" -ForegroundColor Green
    exit 0
}
