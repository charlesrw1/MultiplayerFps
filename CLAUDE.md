This is a game engine project. Uses C++ VS2026 , OpenGL, SDL2, vcpkg. No CMAKE.

path to compilier: "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"

Write tests. Testing documentation found at [[docs/testing.md]]. Use Scripts/build_and_test.ps1 to build and run unit tests. Use Scripts/integration_tests.ps1 to build and run integration tests (with cli options).

Use git to commit after you are done. Write summaries in the message of all commits. 

DO NOT use new/delete/malloc etc UNLESS given permission. Always use modern C++ 17/20. unique_ptr, shared_ptr, etc.

WHEN USING PYTHON: use powershell.exe to launch "py" alias. DO NOT use python from bash.

Uses code generation for ClassBase system, similar to Unreal UOBJECTS. The python codegen script is located under Scripts/. It generates MEGA.cpp in Source/ by parsing headers.

vars.txt and init.txt store configuration used at runtime.

vcpkg is located in ~\source\vcpkg\vcpkg.exe

_NEVER_ read source files in External/ _unless_ given permission. ONLY read AGENTS.md in External/ for summary.

Limit the source files you read to the module you are working on. Be brief, spare tokens.

Use ASSERTS() liberaly, documenting its preconditions and invariants. 

Follow DRY, SOLID principles. SELF DOCUMENTING code. Single Responsiblity Principle!

## Documentation

Project documentation lives under `docs/`. Documentation is _meant_ for AI agents to use, not humans. Writing should help AI use the code. Documentation should be concise, do not use execessive markdown or formatting. Do not use excessive writing, only key points and edge cases. use `// @docs [[file#section]` source comments to link do docs. The `docs.exe` CLI validates wiki-style `[[file#section]]` links and `@docs` source-code refs and is used for searching the documentation. See [[tooling/docs-cli]] for the full reference. Quick index at [[README]].

Invoke `docs.exe` via `powershell.exe -NoProfile -Command "docs.exe check"` from the Bash tool, otherwise will fail.

- **Looking for a topic** — use rg.exe in docs/ to find relevant content such as matching for a path or topic.
- **Before every commit** — `docs.exe check` must exit 0. The pre-commit clang-format step + `docs check` are both required.
- **When adding a new doc** — add a one-line entry to [[README]]. If it documents internals, sprinkle `// @docs [[file#section]]` comments in the relevant source-file headers so `docs context` surfaces it.


## BikeDemo sign conventions (DO NOT get these wrong)

See [[bike/sign_conventions]] for the canonical reference. Summary:

- **+Y up, +Z forward, +X world-right.**
- `bike_right = cross(bike_direction, up)` points **LEFT** in world space — it is misnamed.
- **Positive steer = turn LEFT.**
- `lateral_pos > 0` means road-right; corrective steer to return to centre is **`+sign(lateral_pos)`**, never negative.
