<#
.SYNOPSIS
    Builds App.exe and launches it in game mode, attaching the VS 2026 debugger.

.PARAMETER Config
    Build configuration. "Debug" (default) or "Release".

.PARAMETER AppArgs
    Remaining arguments are forwarded to App.exe verbatim.

.EXAMPLE
    Scripts/run_game.ps1
    Build (Debug) and launch the game with the VS debugger attached.

.EXAMPLE
    Scripts/run_game.ps1 -Config Release -g_project_base "D:/Data"
    Release build; forwards -g_project_base override to App.exe.
#>
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [switch]$WaitForDebugger,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_vs_attach.ps1"

$launchArgs = @()
if ($AppArgs) { $launchArgs += $AppArgs }
Invoke-AppWithDebugger -Config $Config -AppArgs $launchArgs -WaitForDebugger:$WaitForDebugger
