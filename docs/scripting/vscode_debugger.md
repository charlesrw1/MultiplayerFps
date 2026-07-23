# VS Code Lua Debugger (actboy168.lua-debug)

Attach VS Code to a running game/editor build and step through `Data/scripts/**/*.lua`.

## Setup (one-time)

1. **VS Code extension** — install `actboy168.lua-debug` (recommended via `.vscode/extensions.json`). No separate dll download needed — the extension bundles prebuilt debugger runtimes for Lua 5.1-5.5/LuaJIT and picks the right one at runtime.
2. **Shim script** — `Data/scripts/lib/debugger.lua` is checked in (copied from lua-debug's `examples/attach/debugger.lua`). It locates the installed extension under `%USERPROFILE%/.vscode/extensions/actboy168.lua-debug-*` and loads its bundled `script/debugger.lua`. Nothing to configure here; it's found via `require("debugger")` since `Data/scripts/lib/?.lua` is already on Lua's `package.path` (see `ScriptManager::ScriptManager`).

## Enabling the debugger

Whether/when to start lua-debug is a launch-time choice, not persistent state, so it's a pair of CLI flags rather than `EngineVars.ini` config vars:

| flag | meaning |
|------|---------|
| `--lua_debug` | Calls `debugger:start(host:port)` after script load. |
| `--lua_debug_wait` | With `--lua_debug`, blocks at startup on `debugger:event("wait")` so breakpoints set before launch hit on first frame. |

`Scripts/run_game.ps1 -LuaDebug [-LuaDebugWait]` and `Scripts/run_editor.ps1 -LuaDebug [-LuaDebugWait]` forward these. Host/port are still config vars since they're real values, not toggles:

| var | default | meaning |
|-----|---------|---------|
| `g_lua_debug_host` | `127.0.0.1` | Listen host. Must be a literal IPv4 — lua-debug's `socket.lua` only parses `%d+.%d+.%d+.%d+:%d+` and errors on hostnames like `localhost`. |
| `g_lua_debug_port` | `9966` | Listen port. Must match `.vscode/launch.json`'s `address`. |
| `g_lua_cpath_extra` | `""` | Dir appended to Lua `package.cpath` (needed for LuaSocket's `socket/core.dll`, `mime/core.dll` — unrelated to the debugger itself now). |

## Attach workflow

1. Launch via `Scripts/run_game.ps1 -LuaDebug` or `Scripts/run_editor.ps1 -LuaDebug` (add `-LuaDebugWait` too if you want early breakpoints to hit before scripts run).
2. Console prints `ScriptManager: starting lua-debug on 127.0.0.1:9966 (wait=...)`. With `-LuaDebugWait`, the main thread blocks here until VS Code attaches.
3. In VS Code → Run → **lua-debug Attach (127.0.0.1:9966)**. Game unblocks (if waiting).
4. Set breakpoints in any `Data/scripts/**/*.lua`.

## Source mapping

`ScriptManager::reload_one_file` (ScriptManager.cpp) passes the script's **full path** (`FileSys::get_full_path_from_game_path`, e.g. `D:/Data/scripts/foo.lua`), not the game-relative one, as the Lua chunkname — prefixed with `@` by `reload_from_content` so Lua (and lua-debug) treat it as a real file rather than an anonymous in-memory chunk. Because the chunkname is already a full path, lua-debug resolves it directly without needing `cwd`/`sourceMaps` tricks in `launch.json`, and it works the same regardless of which folder you have open as the VS Code workspace root (repo root, `D:/Data/scripts`, whatever).

If `g_project_base` changes to a different absolute path (or a relative one like `"Data"`, resolved against the game process's own working directory), no `launch.json` changes are needed — the chunkname always reflects wherever the file actually is on disk.

## Troubleshooting

- **`lua-debug shim not found`** — `actboy168.lua-debug` isn't installed, or `Data/scripts/lib/debugger.lua` (repo) / the shim location under `g_project_base` is missing/moved.
- **"Missing `program` to debug" when starting** — VS Code resolved a `launch`-mode config instead of our `attach` one. Check the Run and Debug dropdown has **lua-debug Attach (127.0.0.1:9966)** selected, and that the *currently open workspace root's* `.vscode/launch.json` (not necessarily the repo's, if a different folder is open) actually contains it.
- **Pausing lands on a blank `<Memory>` view / breakpoints never bind** — the chunkname passed to Lua wasn't `@`-prefixed (or wasn't a real resolvable path), so lua-debug treated the script as an anonymous in-memory chunk rather than a file. Should not happen now that `reload_one_file` uses `@` + full path; if it recurs, check `ScriptManager::reload_from_content`.
- **Port in use** — change both `g_lua_debug_port` and `address` in launch.json (must match).
- **Debug session opens then immediately closes, engine stays frozen in `dbg:event("wait")`** — check the extension's own logs at `%USERPROFILE%/.vscode/extensions/actboy168.lua-debug-*/master.log` and `client.log` (separate from VS Code's Debug Console). `"Invalid address"` there means `address` isn't a literal IPv4 (e.g. `localhost` was used instead of `127.0.0.1`).

## Implementation

Single entry point: `ScriptManager::activate_debugger` in `Source/Scripting/ScriptManager.cpp` runs a small `luaL_dostring` snippet equivalent to `require("debugger"):start(host..":"..port)`, optionally chaining `:event("wait")`. No C++ build-time dependency — the debugger is a runtime Lua module loaded via `package.path`, same pattern as the old EmmyLua wiring it replaced.
