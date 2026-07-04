<#
.SYNOPSIS
    Builds App.exe and launches it in game mode, attaching the VS 2026 debugger.

.PARAMETER Config
    Build configuration. "Debug" (default) or "Release".

.PARAMETER LuaDebug
    Start lua-debug (actboy168.lua-debug) so VS Code can attach. Equivalent to
    "--lua_debug". See docs/scripting/vscode_debugger.md.

.PARAMETER LuaDebugWait
    Block at startup until VS Code attaches (so breakpoints in early-running
    scripts still hit). Equivalent to "--lua_debug_wait"; implies -LuaDebug.

.PARAMETER AppArgs
    Remaining arguments are forwarded to App.exe verbatim.

.EXAMPLE
    Scripts/run_game.ps1
    Build (Debug) and launch the game with the VS debugger attached.

.EXAMPLE
    Scripts/run_game.ps1 -Config Release -g_project_base "D:/Data"
    Release build; forwards -g_project_base override to App.exe.

.EXAMPLE
    Scripts/run_game.ps1 -LuaDebug -LuaDebugWait
    Launch and wait for VS Code's lua-debug to attach before scripts run.
#>
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [switch]$WaitForDebugger,

    [switch]$LuaDebug,

    [switch]$LuaDebugWait,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_vs_attach.ps1"

$launchArgs = @()
if ($LuaDebug -or $LuaDebugWait) { $launchArgs += "--lua_debug" }
if ($LuaDebugWait) { $launchArgs += "--lua_debug_wait" }
if ($AppArgs) { $launchArgs += $AppArgs }
Invoke-AppWithDebugger -Config $Config -AppArgs $launchArgs -WaitForDebugger:$WaitForDebugger
