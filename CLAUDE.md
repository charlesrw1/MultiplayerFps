this is a game engine project.

USE AgentDocs/ for documenation made by previous agents.

ALWAYS write tests. DO NOT commmit the code unless you've _proven_ it to work by running tests.

USE git to save your progress. Write summaries in the message of all commits.

DO NOT use rm. Use my powershell's profile "trash" alias.

WHEN USING PYTHON: use powershell.exe to launch "py" alias. DO NOT use python from bash.

### Project information

Built on windows. Visual studio 2019. No CMAKE. 

Uses code generation for ClassBase system. The python codegen script is located under Scripts/. It generates MEGA.cpp in Source/

vars.txt and init.txt store configuration used at runtime

uses vcpkg for packagement. vcpkg is located in ~\source\vcpkg\vcpkg.exe

use Scripts/build_and_test.ps1 to run application unit tests. _prove_ that your code works before commiting it.

RUN clang-format-all.ps1 before commits

write summaries for each commit message