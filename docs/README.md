# Documentation

All project documentation lives here. Validated by `docs.exe` (see [[tooling/docs-cli]]).

## Top-level

- [[engine_overview]] — engine architecture, build system, ECS, application lifecycle
- [[testing]] — running unit + integration tests, framework details
- [[scripting_system]] — Lua / C++ scriptable class system
- [[reflection_macros]] — every `REFLECT(...)` flag the codegen accepts (min/max/category/readonly/no_nil/...)

## Rendering & Assets

- [[asset_tools]] — AssetTools static lib: compiler, diagnostics, file ops (mv/rm/cp), asset templates
- [[rendering/texture_pipeline]] — .tis/.dds/.png flow: compile, nearest_filtering, UI textures, inspector
- [[rendering/materials]] — `.mm` / `.mi` authoring + material system internals
- [[rendering/decals]] — decal materials, write flags, parallax occlusion mapping
- [[rendering/model_importing]] — `.glb` import, `.mis` settings, `.cmdl` paths
- [[rendering/skeleton_armature_bake]] — how armature root transforms are baked into skeleton/animations at compile time
- [[rendering/ddgi_baking]] — DDGI probe-volume + global bias parameters for baking
- [[rendering/gfx_abstraction]] — IGraphicsDevice migration plan (wrap → redesign → DX11/DX12 backends)
- [[rendering/gfx_abstraction_changelog]] — per-sub-phase status entries for Phase 1 + Phase 2 (as-landed notes split from the design doc)
- [[rendering/gfx_abstraction_nextsteps]] — DX11 backend (M1) status / punch list

## Bike Demo

- [[bike/sign_conventions]] — DO NOT get these wrong (axes, steer sign, lateral_pos)

## AI

- [[navigation]] — Recast/Detour navmesh system: components, baker, sidecar format, debug cvars

## Debugging

- [[debugging/crash_dumps]] — minidump capture on SEH crashes + `analyze_dump.ps1` for cdb-driven inspection
- [[scripting/vscode_debugger]] — attach VS Code (EmmyLuaDebugger) to a running build to step Lua scripts

## Tooling

- [[tooling/docs-cli]] — the `docs` CLI: validate links, locate sections, find inbound refs
- [[tooling/asset-cli]] — `asset_cli.py` REPL: asset-group aware mv/cp/trash + reference rewriting, single-step undo
- [[tooling/lua-check]] — `lua_check.ps1`: run sumneko lua-language-server `--check` from CLI, shares `.luarc.json` with VSCode
- [[tooling/run-scripts]] — `run_game.ps1` / `run_editor.ps1`: build App.exe + launch with VS 2026 debugger attached