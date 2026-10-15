# LuaSharp

A standalone C++20 compiler for a typed, brace-based language that emits Lua. It does not include, link to, or otherwise depend on CsRemake engine source.

Build the `LuaSharp` project in Visual Studio, or from the repository root:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' LuaSharp\LuaSharp.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
.\x64\Debug\LuaSharp.exe --test
.\x64\Debug\LuaSharp.exe .\LuaSharp\example.ls
```

The final command writes `LuaSharp\example.lua`. The current compiler supports the syntax in `example.ls`: classes, public/private sections, typed fields and parameters, constructors, methods, `auto`, `if`, `return`, `new`, `null`, and Lua table literals. It also emits EmmyLua/LuaLS annotations for fields, parameters, and returns.
