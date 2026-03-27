this is a game engine project.

USE AgentDocs/ for documenation made by previous agents.

ALWAYS write tests. DO NOT commmit the code unless you've _proven_ it to work by running tests.

USE git to save your progress. Write summaries in the message of all commits.

DO NOT use rm. Use my powershell's profile "trash" alias.

DO NOT use new/delete/malloc etc UNLESS given permission. ALWAYS use MODERN C++ 17/20. unique_ptr, shared_ptr, etc.

Write _SAFE_ code. DO NOT write unsafe C++ code that is vulnerable to use after frees.

WHEN USING PYTHON: use powershell.exe to launch "py" alias. DO NOT use python from bash.

### Project information

Built on windows. Visual studio 2019. No CMAKE. 

Uses code generation for ClassBase system. The python codegen script is located under Scripts/. It generates MEGA.cpp in Source/

vars.txt and init.txt store configuration used at runtime

Uses vcpkg for packagement. vcpkg is located in ~\source\vcpkg\vcpkg.exe

Use Scripts/build_and_test.ps1 to run application unit tests. _prove_ that your code works before commiting it.

RUN clang-format-all.ps1 before commits

Write summaries for each commit message

Create and update AGENTS.md summaries in Animation/ AssetCompile/ External/ Render/ Framework/ IntegrationTests/ Game/ LevelEditor/, that summarize key components and functions. When you modify files in these "modules" always update the AGENTS.md so its fresh.

_NEVER_ read source files in External/ _unless_ given permission. ONLY read AGENTS.md in External/ for summary.




