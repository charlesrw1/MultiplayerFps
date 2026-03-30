this is a game engine project.

USE AgentDocs/ for documenation made by previous agents.

ALWAYS write tests. DO NOT commmit the code unless you've _proven_ it to work by running tests.

Run tests with Scripts/build_and_test.ps1 and Scripts/integration_tests.ps1.  _prove_ that your code works before commiting it.

D:/Data is directory used. Find lua scripts under D:/Data/scripts

SEE Lua repl for a live REPL enviorment to test running program.

USE git to save your progress. Write summaries in the message of all commits.

DO NOT use rm. Use my powershell's profile "trash" alias.

DO NOT use new/delete/malloc etc UNLESS given permission. ALWAYS use MODERN C++ 17/20. unique_ptr, shared_ptr, etc.

Write _SAFE_ code. DO NOT write unsafe C++ code that is vulnerable to use after frees.

WHEN USING PYTHON: use powershell.exe to launch "py" alias. DO NOT use python from bash.

Built on windows. Visual studio 2019. No CMAKE. 

Uses code generation for ClassBase system. The python codegen script is located under Scripts/. It generates MEGA.cpp in Source/

vars.txt and init.txt store configuration used at runtime

Uses vcpkg for packagement. vcpkg is located in ~\source\vcpkg\vcpkg.exe

Use AGENTS.md summaries in Animation/ AssetCompile/ External/ Render/ Framework/ IntegrationTests/ Game/ LevelEditor/, that summarize key components and functions. 

_NEVER_ read source files in External/ _unless_ given permission. ONLY read AGENTS.md in External/ for summary.

Limit the source files you read to the module you are working on. DONT use tokens liberally.




