# VS Code Lua Debugger (EmmyLuaDebugger)

Attach VS Code to a running game/editor build and step through `Data/scripts/**/*.lua`.

## Setup (one-time)

1. **VS Code extension** — install `tangzx.emmylua` (recommended via `.vscode/extensions.json`).
2. **Native module** — download the prebuilt `emmy_core.dll` matching Lua 5.4 / x64-windows from the [EmmyLuaDebugger releases](https://github.com/EmmyLua/EmmyLuaDebugger/releases) (`emmy_core@emmy.x64-Lua5.4.zip` → `emmy_core.dll`).
3. **Drop the dll** into the dir referenced by `g_lua_cpath_extra` (typically the vcpkg bin dir where `socket/core.dll` already lives, e.g. `C:/Users/<you>/source/vcpkg/installed/x64-windows/bin`).

## Config vars (`vars.txt`)

| var | default | meaning |
|-----|---------|---------|
| `g_lua_debug` | `0` | When `1`, calls `emmy_core.tcpListen` after script load. |
| `g_lua_debug_host` | `localhost` | Listen host. |
| `g_lua_debug_port` | `9966` | Listen port. Must match `.vscode/launch.json`. |
| `g_lua_debug_wait` | `0` | When `1`, blocks at startup on `emmy_core.waitIDE()` so breakpoints set before launch hit on first frame. |
| `g_lua_cpath_extra` | `""` | Dir appended to Lua `package.cpath` (must contain `emmy_core.dll`). |

## Attach workflow

1. Set `g_lua_debug 1` (and optionally `g_lua_debug_wait 1`) in `vars.txt`.
2. Launch the game. Console prints `ScriptManager: starting EmmyLuaDebugger on localhost:9966 (wait=...)`. With `wait=1`, the main thread blocks here.
3. In VS Code → Run → **EmmyLua Attach (localhost:9966)**. Game unblocks.
4. Set breakpoints in any `Data/scripts/**/*.lua`. Source mapping is identity — `ScriptManager::reload_from_content` (ScriptManager.cpp:209) passes game-relative paths as chunknames.

## Troubleshooting

- **`emmy_core not found on package.cpath`** — dll missing or wrong dir; verify `g_lua_cpath_extra` and that the dll is built for Lua 5.4 / x64.
- **Wrong Lua version** — vcpkg ships Lua 5.4.4; only the `Lua5.4` build of `emmy_core.dll` works.
- **Breakpoints unbound** — confirm `sourcePaths` in `.vscode/launch.json` points at `Data/scripts`.
- **Port in use** — change both `g_lua_debug_port` and `port` in launch.json (must match).

## Implementation

Single entry point: `ScriptManager::activate_debugger` in `Source/Scripting/ScriptManager.cpp` runs a small `luaL_dostring` snippet that requires `emmy_core`, calls `tcpListen`, and conditionally `waitIDE`. No C++ build-time dependency — emmy_core is a runtime Lua C module loaded via `package.cpath`.
