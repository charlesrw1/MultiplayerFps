# Tooling Scripts

Under Scripts/ are scripts for running the game conviently. 

## run_game.ps1 and run_editor.ps1

Builds App.exe and launch it with VS 2026 attached. run_editor.ps1 prepends --editor; run_game.ps1 doesn't.

Example: `.\Scripts\run_game.ps1 [-Config Debug|Release] [-- ...app-args]`

Command line flags: 
- `-Project MyProject`: opens a specific project (`MyProject` in this example) other than default one, listed under Projects/
- `-LuaDebugWait`: waits until lua debugger is connected
- `-WaitForDebugger`: waits until Visual Studio debugger is connected.
- `-Config`: `Debug` or `Release`, defaults to `Debug`

Defaults: -Config = Debug. Remaining args after -Config are forwarded verbatim to App.exe via ValueFromRemainingArguments. Exit code is App.exe's.
VS attach

Shared with integration_test.ps1 -Debugger. Helper lives in Scripts/_vs_attach.ps1 (Attach-VSDebugger, Invoke-AppWithDebugger). Probes the ROT for VisualStudio.DTE.18.0 → 17.0 → VisualStudio.DTE; attaches via DTE. If no VS instance is running, falls back to vsjitdebugger.exe -p <pid> (interactive prompt). Polls for the target PID up to 15s — RPC_E_CALL_REJECTED while VS is busy is retried silently.

## integration_test.ps1

Builds and run integration tests in the project.