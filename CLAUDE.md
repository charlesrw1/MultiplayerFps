This is a game engine project. Uses C++ VS2026 , OpenGL, SDL2, vcpkg. No CMAKE.

path to compiler: "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" OR use msbuild.cmd which is in PATH.

CRITICAL: ALWAYS use the 64-bit MSBuild under `Bin\amd64\` (NOT the 32-bit `Bin\MSBuild.exe`).
The VS 2026 IDE is 64-bit; building from the CLI with the 32-bit MSBuild corrupts the
FileTracker .tlog state and makes the IDE full-rebuild everything on every build,
persistently, until the tlogs are wiped. See docs/gotchas.md. Prefer the wrapper scripts
(build_and_test.ps1 / integration_test.ps1 / run_editor.ps1 / run_game.ps1 / msbuild.cmd),
which already select the 64-bit engine. Never invoke `...\Bin\MSBuild.exe` (32-bit) directly.

NEVER use VS compiler under "C:\Program Files (x86)\...", that is old compiler.

If you see error code c0000135, then it most likely means NO DLLs were copied to the x64/Debug or x64/Release folder for some reason? Bug with vcpkg.

D:/Data is the path to the data directory for assets (g_project_base) used in editor and game. ./TestFilesData/ is used for the test files used during integration tests.

Write tests. Testing documentation found at [[docs/testing.md]]. Use Scripts/build_and_test.ps1 to build and run unit tests. Use Scripts/integration_tests.ps1 to build and run integration tests (with cli options).

When a test crashes, use `Scripts/dbg.ps1 <dump> '<cdb cmd>'` to inspect locals/pointers/stack frames/etc.. See [[docs/debugging/crash_dumps.md]] for commands.

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

Uses RmlUI for the gameplay UI framework, a C++/lua UI framework that is very similar to Css/Html/Js (using .rml files for layout and .rcss for styling). Includes the lua plugin for it. See '..vcpkg\buildtrees\rmlui\src\6.2-203a9587f9.clean\' for the source files for it. Also see 'D:\Data\scripts\rmlui_lua_stubs.lua' for a list of callable functions from the lua api. Look at '...rmlui\src\6.2-203a9587f9.clean\Samples' for samples of using rmlui and lua. See 'https://mikke89.github.io/RmlUiDoc/index.html' for documentation about RmlUI in general. 
Note that rmlui script operates in the same enviorment as the rest of the engine's lua. So you can call the same ClassBase REF'd functions, or lua global variables, and such. You can attach scripts by using inline <script> or by doing 'onclick=' etc.

Editor architecture only allows placing entities with 1 component that can be edited/serialized. Runtime allows multiple components per entity.

## Documentation

Project documentation lives under `docs/`. Documentation is _meant_ for AI agents to use, not humans. Writing should help AI use the code. Documentation should be concise, do not use execessive markdown or formatting. Do not use excessive writing, only key points and edge cases.  Quick index at [[README]].

## Feature Workflow (GitHub Issues)

New feature or non-trivial bug fix:
1. `/plan` to design approach, then create a GH issue with the plan as the body: `gh issue create --title "..." --label "..." --body "..."`
2. Implement on current branch; reference the issue in every relevant commit (`#N` in message)
3. Comment on the issue when the plan changes or a surprise is found: `gh issue comment N --body "..."`
4. Final commit closes it: `Closes #N` in message

Always append this footer to AI-created issues:
```
---
> ✦ Created by Claude
```

GH repo: https://github.com/charlesrw1/MultiplayerFps

