# `lua_check.ps1` — Lua type checking CLI

Wraps sumneko's `lua-language-server` in `--check` mode so agents can validate Lua diagnostics from the command line. Shares config with the VSCode Lua LSP via `.luarc.json` at repo root.

Launch: `.\Scripts\lua_check.ps1 [-Path <file-or-folder>] [-Level Error|Warning|Information|Hint]`

Defaults: `-Path` = repo root, `-Level` = Warning. Exit codes: `0` clean, `1` diagnostics found, `2` setup failure (LS not installed, path not found, etc).

## How it works

Locates `lua-language-server.exe` by globbing `%USERPROFILE%\.vscode\extensions\sumneko.lua-*\server\bin\` (newest version wins). The sumneko VSCode extension is the only supported install path.

Runs `--check_format=json` with `--check_out_path` pointing at a fresh temp dir per invocation, then parses the resulting `check.json` into `path:line:col: severity: message [code]` lines. Output file is only written by the LS when problems exist — absence = clean.

## File vs folder

`lua-language-server --check` **requires a folder**. When `-Path` is a single file, the script checks the file's parent directory and filters the report to that file. This means single-file mode still pays the cost of scanning the whole parent — pass narrower folders when iterating.

## Config

`.luarc.json` at repo root is the single source of truth (both VSCode LSP and CLI read it via `--configpath`). Key choices:

- `runtime.version` = `"LuaJIT"` (matches the embedded host; gives `bit`, `jit` globals).
- `workspace.library` = `["Data/scripts"]` so `lua_stubs.lua` annotations resolve from anywhere (e.g. files under `TestFiles/`).
- `workspace.ignoreDir` excludes `.claude`, `External`, build dirs, and `Data/scripts/lib` (LuaSocket and other third-party modules — noisy).
- `diagnostics.disable` = `["lowercase-global", "missing-fields"]`. Lowercase globals are intentional in this codebase (vec_add alias style); missing-fields fires on every `{}` table literal.
- `diagnostics.globals` whitelists the `vec_*`/`normalize`/`cross` aliases from `bike_player.lua`.

## Ignoring a whole file

Use the sumneko file-level annotation, placed on a line near the top:

```lua
---@diagnostic disable                                       -- all diagnostics off
---@diagnostic disable: undefined-global, lowercase-global   -- specific codes
---@diagnostic disable-next-line                             -- next line only
```

`Data/scripts/lua_stubs.lua` carries `---@diagnostic disable` because it's an auto-generated stub file from the C++ codegen. The directive is emitted by [[Scripts/codegen_generate.py]] — do not edit `lua_stubs.lua` manually.

## Gotchas

- The LS does not emit `check.json` when no diagnostics fire — script treats absence as clean.
- The progress bar / spinner from the LS is suppressed via `Out-Null`; if the run hangs, drop `Out-Null` temporarily to see the LS's own output.
- `--configpath` is passed explicitly so the same config applies whether or not the LS's auto-discovery would find `.luarc.json` from the target path.
- Filter on file-mode is a string-equality match on the resolved absolute path. Comparison uses PowerShell's default case-insensitive `-ne`, which is fine on Windows but would break on a case-sensitive filesystem.
