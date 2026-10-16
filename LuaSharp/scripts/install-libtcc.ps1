$ErrorActionPreference = 'Stop'

$version = '0.9.27'
$expectedSha256 = '34A721949A2583FDFF725312DA092FA0F5F1F284B702E6F811C6954714FAABB2'
$url = "https://download.savannah.gnu.org/releases/tinycc/tcc-$version-win64-bin.zip"
$luaSharpRoot = Split-Path -Parent $PSScriptRoot
$destination = Join-Path $luaSharpRoot 'tools\tcc'
$archive = Join-Path ([System.IO.Path]::GetTempPath()) "luasharp-tcc-$version-$([guid]::NewGuid().ToString('N')).zip"

try {
    Invoke-WebRequest -Uri $url -OutFile $archive -UseBasicParsing
    $actualSha256 = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    if ($actualSha256 -ne $expectedSha256) {
        throw "TinyCC archive checksum mismatch. Expected $expectedSha256, got $actualSha256"
    }
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Expand-Archive -LiteralPath $archive -DestinationPath (Split-Path -Parent $destination) -Force
    $library = Join-Path $destination 'libtcc.dll'
    if (-not (Test-Path -LiteralPath $library)) { throw "Archive did not contain $library" }
    Write-Output "Installed TinyCC $version to $destination"
    Write-Output "libtcc: $library"
} finally {
    if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
}
