this is a game engine project.

ALWAYS write tests. do not commmit the code unless you've _proven_ it to work.

USE git to save your progress. Write summaries in the message of all commits.

DO NOT use rm. Use my powershell's profile "trash" alias.

### Project information

* Built on windows. Visual studio 2019. No CMAKE. 

* Uses code generation for ClassBase system. The python codegen script is located under Scripts/. It generates MEGA.cpp in Source/

* vars.txt and init.txt store configuration used at runtime

* uses vcpkg for packagement