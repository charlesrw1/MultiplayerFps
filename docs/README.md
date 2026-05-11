# Documentation

All project documentation lives here. Validated by `docs.exe` (see [[tooling/docs-cli]]).

## Top-level

- [[engine_overview]] — engine architecture, build system, ECS, application lifecycle
- [[testing]] — running unit + integration tests, framework details
- [[scripting_system]] — Lua / C++ scriptable class system

## Rendering & Assets

- [[rendering/materials]] — `.mm` / `.mi` authoring + material system internals
- [[rendering/decals]] — decal materials, write flags, parallax occlusion mapping
- [[rendering/model_importing]] — `.glb` import, `.mis` settings, `.cmdl` paths

## Bike Demo

- [[bike/sign_conventions]] — DO NOT get these wrong (axes, steer sign, lateral_pos)

## Tooling

- [[tooling/docs-cli]] — the `docs` CLI: validate links, locate sections, find inbound refs
- [[tooling/asset-cli]] — `asset_cli.py` REPL: asset-group aware mv/cp/trash + reference rewriting, single-step undo
- [[tooling/lua-check]] — `lua_check.ps1`: run sumneko lua-language-server `--check` from CLI, shares `.luarc.json` with VSCode
- [[tooling/run-scripts]] — `run_game.ps1` / `run_editor.ps1`: build App.exe + launch with VS 2026 debugger attached