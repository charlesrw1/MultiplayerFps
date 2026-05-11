# lua_check.ps1
# Runs sumneko lua-language-server in --check mode over a file or folder
# and prints diagnostics as `path:line:col: severity: message [code]`.
# @docs [[tooling/lua-check]]
#
# Usage:
#   .\Scripts\lua_check.ps1 [-Path <file-or-folder>] [-Level Error|Warning|Information|Hint]
#
# Notes:
# - lua-language-server's --check requires a folder. If -Path is a file, this
#   script checks the parent folder and filters the report to that single file.
# - Exit code: 0 if no diagnostics at >= Level, 1 if diagnostics, 2 on setup failure.

param(
    [string]$Path = "",
    [ValidateSet("Error", "Warning", "Information", "Hint")]
    [string]$Level = "Warning"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path $PSScriptRoot -Parent

if (-not $Path) { $Path = $RepoRoot }
$resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
if (-not $resolved) {
    Write-Error "Path not found: $Path"
    exit 2
}
$target = $resolved.Path

# If target is a file, run --check on its parent and filter results to this file.
$filterFile = $null
if (Test-Path -LiteralPath $target -PathType Leaf) {
    $filterFile = $target
    $target = Split-Path $target -Parent
}

# Locate lua-language-server.exe from the sumneko VSCode extension.
$ls = Get-ChildItem -Path "$env:USERPROFILE\.vscode\extensions" -Filter "sumneko.lua-*" -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName "server\bin\lua-language-server.exe" } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1

if (-not $ls) {
    Write-Error "lua-language-server.exe not found under %USERPROFILE%\.vscode\extensions\sumneko.lua-*\server\bin\. Install the VSCode 'Lua' extension by sumneko."
    exit 2
}

$logDir = Join-Path ([System.IO.Path]::GetTempPath()) ("lua_check_" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$outJson = Join-Path $logDir "check.json"
$configPath = Join-Path $RepoRoot ".luarc.json"

try {
    Write-Host "lua-language-server: $ls" -ForegroundColor DarkGray
    if ($filterFile) {
        Write-Host "checking: $filterFile (level=$Level)" -ForegroundColor Cyan
    } else {
        Write-Host "checking: $target (level=$Level)" -ForegroundColor Cyan
    }

    & $ls `
        --check="$target" `
        --check_format=json `
        --check_out_path="$outJson" `
        --checklevel=$Level `
        --configpath="$configPath" `
        --logpath="$logDir" | Out-Null

    if (-not (Test-Path $outJson)) {
        # LS only writes the file when problems exist.
        Write-Host "No diagnostics." -ForegroundColor Green
        exit 0
    }

    $raw = Get-Content $outJson -Raw
    if ([string]::IsNullOrWhiteSpace($raw) -or $raw.Trim() -in "{}", "[]") {
        Write-Host "No diagnostics." -ForegroundColor Green
        exit 0
    }

    $report = $raw | ConvertFrom-Json
    $sevName = @{ 1 = "error"; 2 = "warning"; 3 = "info"; 4 = "hint" }
    $sevColor = @{ 1 = "Red"; 2 = "Yellow"; 3 = "Cyan"; 4 = "DarkGray" }

    $total = 0
    foreach ($prop in $report.PSObject.Properties) {
        $uri = $prop.Name
        $filePath = $uri -replace "^file:///", "" -replace "/", "\"
        $filePath = [System.Uri]::UnescapeDataString($filePath)

        if ($filterFile -and ($filePath -ne $filterFile)) { continue }

        foreach ($d in $prop.Value) {
            $line = $d.range.start.line + 1
            $col = $d.range.start.character + 1
            $sev = $sevName[[int]$d.severity]
            $color = $sevColor[[int]$d.severity]
            $code = if ($d.code) { " [$($d.code)]" } else { "" }
            $msg = ($d.message -replace "`r?`n", " ")
            Write-Host ("{0}:{1}:{2}: {3}: {4}{5}" -f $filePath, $line, $col, $sev, $msg, $code) -ForegroundColor $color
            $total++
        }
    }

    Write-Host ""
    if ($total -eq 0) {
        Write-Host "No diagnostics." -ForegroundColor Green
        exit 0
    } else {
        Write-Host "$total diagnostic(s) at level >= $Level" -ForegroundColor Yellow
        exit 1
    }
}
finally {
    Remove-Item -Recurse -Force -LiteralPath $logDir -ErrorAction SilentlyContinue
}
