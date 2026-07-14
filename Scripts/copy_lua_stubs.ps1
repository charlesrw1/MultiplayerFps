$src = Join-Path $PSScriptRoot "..\TestFilesData\scripts"
$dst = "D:/Data/scripts"

New-Item -ItemType Directory -Force -Path $dst | Out-Null

Get-ChildItem -Path $src -Filter "*_stubs.lua" | ForEach-Object {
    Write-Host "copying $($_.FullName) -> $dst\$($_.Name)"
    Copy-Item -Path $_.FullName -Destination $dst -Force
}
