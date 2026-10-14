<#
.SYNOPSIS
    Starts the CsRemake build/test/debug launcher.

.EXAMPLE
    .\EngineLauncher.ps1
#>

$ErrorActionPreference = "Stop"
$repoRoot = $PSScriptRoot
& dotnet run --project (Join-Path $repoRoot "Launcher\CsRemakeLauncher.csproj") --configuration Debug
exit $LASTEXITCODE
