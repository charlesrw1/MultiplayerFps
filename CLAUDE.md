This is a game engine project. Uses C++ VS2019, OpenGL, SDL2, vcpkg. No CMAKE.

Write tests. Testing documentation found at [[testing]].

Use Scripts/build_and_test.ps1 to build and run unit tests.

Use git to save progress. Write summaries in the message of all commits. Use git worktrees.

When possible, use parallel subagents with git worktrees.

DO NOT use new/delete/malloc etc UNLESS given permission. ALWAYS use MODERN C++ 17/20. unique_ptr, shared_ptr, etc.

WHEN USING PYTHON: use powershell.exe to launch "py" alias. DO NOT use python from bash.

Uses code generation for ClassBase system, similar to Unreal UOBJECTS. The python codegen script is located under Scripts/. It generates MEGA.cpp in Source/ by parsing headers. See [[codegen]].

vars.txt and init.txt store configuration used at runtime.

vcpkg is located in ~\source\vcpkg\vcpkg.exe

_NEVER_ read source files in External/ _unless_ given permission. ONLY read AGENTS.md in External/ for summary.

Limit the source files you read to the module you are working on. Be brief, spare tokens.

## Documentation

Project documentation lives under `docs/`. Documentation is _meant_ for AI agents to use, not humans. Writing should help AI use the code. Documentation should be concise, do not use execessive markdown or formatting. Do not use excessive writing, only key points and edge cases. use `// @docs [[file#section]` source comments to link do docs. The `docs.exe` CLI validates wiki-style `[[file#section]]` links and `@docs` source-code refs and is used for searching the documentation. See [[tooling/docs-cli]] for the full reference. Quick index at [[README]]. 

- **Looking for a topic** — use rg.exe in docs/ to find relevant content such as matching for a path or topic.
- **Before every commit** — `docs.exe check` must exit 0. The pre-commit clang-format step + `docs check` are both required.
- **When adding a new doc** — add a one-line entry to [[README]]. If it documents internals, sprinkle `// @docs [[file#section]]` comments in the relevant source-file headers so `docs context` surfaces it.


## BikeDemo sign conventions (DO NOT get these wrong)

See [[bike/sign_conventions]] for the canonical reference. Summary:

- **+Y up, +Z forward, +X world-right.**
- `bike_right = cross(bike_direction, up)` points **LEFT** in world space — it is misnamed.
- **Positive steer = turn LEFT.**
- `lateral_pos > 0` means road-right; corrective steer to return to centre is **`+sign(lateral_pos)`**, never negative.
