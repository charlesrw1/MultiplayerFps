<#
.SYNOPSIS
    Builds App.exe and launches it in editor mode (--editor), attaching the
    VS 2026 debugger.

.PARAMETER Config
    Build configuration. "Debug" (default) or "Release".

.PARAMETER AppArgs
    Remaining arguments are forwarded to App.exe verbatim (after --editor).

.EXAMPLE
    Scripts/run_editor.ps1
    Build (Debug) and launch the editor with the VS debugger attached.

.EXAMPLE
    Scripts/run_editor.ps1 -Config Release
    Release editor build.
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

$launchArgs = @("--editor")
if ($AppArgs) { $launchArgs += $AppArgs }
Invoke-AppWithDebugger -Config $Config -AppArgs $launchArgs -WaitForDebugger:$WaitForDebugger
