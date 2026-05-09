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
- [[tooling/agent_repl]] — TCP REPL for issuing console / Lua commands to a running engine