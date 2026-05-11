# `run_game.ps1` / `run_editor.ps1` — App launchers with VS attach

Build App.exe and launch it with VS 2026 attached. `run_editor.ps1` prepends `--editor`; `run_game.ps1` doesn't.

Launch: `.\Scripts\run_game.ps1 [-Config Debug|Release] [-- ...app-args]`

Defaults: `-Config` = `Debug`. Working directory is forced to repo root so `g_project_base "Data"` resolves. Remaining args after `-Config` are forwarded verbatim to App.exe via `ValueFromRemainingArguments`. Exit code is App.exe's.

## VS attach

Shared with `integration_test.ps1 -Debugger`. Helper lives in `Scripts/_vs_attach.ps1` (`Attach-VSDebugger`, `Invoke-AppWithDebugger`). Probes the ROT for `VisualStudio.DTE.18.0` → `17.0` → `VisualStudio.DTE`; attaches via DTE. If no VS instance is running, falls back to `vsjitdebugger.exe -p <pid>` (interactive prompt). Polls for the target PID up to 15s — RPC_E_CALL_REJECTED while VS is busy is retried silently.

## Gotchas

- `--editor` is implicit in `run_editor.ps1`; don't pass it again in extra args.
- Pass-through args still go through App.exe's existing CLI parser, so `-cvar value` and `--tests <mode>` continue to work (but `--tests editor` from `run_game.ps1` would conflict — use `run_editor.ps1` instead).
- No `-NoDebugger` opt-out — to run without a debugger, invoke `x64\<Config>\App.exe` directly.
