# LuaSharp

A standalone C++20 compiler for a typed, brace-based language that emits Lua. It does not include, link to, or otherwise depend on CsRemake engine source.

Build with MSVC:

```powershell
cl /std:c++20 /EHsc /W4 /I src src\main.cpp src\Compiler.cpp /Fe:luasharp.exe
.\luasharp.exe --test
.\luasharp.exe example.ls
```

The final command writes `example.lua`. The current compiler supports the syntax in `example.ls`: classes, public/private sections, typed fields and parameters, constructors, methods, `auto`, `if`, `return`, `new`, `null`, and Lua table literals. It also emits EmmyLua/LuaLS annotations for fields, parameters, and returns.
