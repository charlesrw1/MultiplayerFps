# App Launchers

Build `App.exe` and launch it with Visual Studio 2026 attached. `run_editor.ps1` prepends `--editor`. `run_game.ps1` does not.

```
.\Scripts\run_game.ps1 [-Config Debug|Release] [-Project Name] [-LuaDebugWait] [-WaitForDebugger] [-- ...app-args]
```

Flags:

- `-Config`: `Debug` or `Release`. Default is `Debug`.
- `-Project Name`: load `Projects/Name.ini` instead of the default project. See [[tooling/managing_projects]].
- `-LuaDebugWait`: wait until the Lua debugger connects. See [[scripting/vscode_debugger]].
- `-WaitForDebugger`: wait until the Visual Studio debugger connects.

The working directory is the repo root, so `g_project_base "Data"` resolves. Remaining args after the flags go to `App.exe`. The exit code is the `App.exe` exit code.

## VS Attach

Shared with `integration_test.ps1 -Debugger`. The helper is `Scripts/_vs_attach.ps1` (`Attach-VSDebugger`, `Invoke-AppWithDebugger`). It probes the ROT for `VisualStudio.DTE.18.0`, then `17.0`, then `VisualStudio.DTE`. It attaches through DTE. If no Visual Studio instance is running, it falls back to `vsjitdebugger.exe -p <pid>`. It polls for the target PID for up to 15 seconds. It retries `RPC_E_CALL_REJECTED` while Visual Studio is busy.

## Integration Tests

`Scripts/integration_test.ps1` builds and runs the integration tests. See [[testing]].

## Gotchas

- `--editor` is implicit in `run_editor.ps1`. Do not pass it again in extra args.
- Pass-through args still go through the `App.exe` CLI parser. `-cvar value` and `--tests <mode>` still work. Do not run `--tests editor` from `run_game.ps1`. Use `run_editor.ps1` instead.
- There is no `-NoDebugger` flag. To run without a debugger, invoke `x64\<Config>\App.exe` directly.
