$clangFormat = "clang-format"

if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    $clangFormat = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\Llvm\x64\bin\clang-format.exe"
}

Get-ChildItem -Path Source -Recurse -Include *.cpp, *.h |
    Where-Object { $_.FullName -notlike "*.generated*" -and $_.FullName -notlike "*\External\*" } |
    ForEach-Object { & $clangFormat -i $_.FullName }

Write-Host "Done."
