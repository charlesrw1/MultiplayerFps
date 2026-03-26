param(
    [switch]$All  # format every file instead of just git-changed files
)

$clangFormat = "clang-format"

if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    $clangFormat = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\Llvm\x64\bin\clang-format.exe"
}

function Should-Format($path) {
    return $path -match '\.(cpp|h)$' -and
           $path -notmatch '\.generated' -and
           $path -notmatch '[/\\]External[/\\]'
}

if ($All) {
    $files = Get-ChildItem -Path Source -Recurse -Include *.cpp, *.h |
        Where-Object { $_.FullName -notlike "*.generated*" -and $_.FullName -notlike "*\External\*" } |
        Select-Object -ExpandProperty FullName
} else {
    # Only format files changed since last commit (staged + unstaged + untracked)
    $changed = git status --porcelain | ForEach-Object { $_.Substring(3).Trim() }
    $files = $changed | Where-Object { Should-Format $_ } | ForEach-Object {
        Join-Path (Get-Location) $_
    } | Where-Object { Test-Path $_ }

    if (-not $files) {
        Write-Host "No changed source files to format."
        exit 0
    }
}

Write-Host "Formatting $($files.Count) file(s)..."
$files | ForEach-Object { & $clangFormat -i $_ }

Write-Host "Done."
