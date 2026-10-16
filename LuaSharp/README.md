# LuaSharp

A standalone C++20 compiler for a typed, brace-based language that emits Lua. It does not include, link to, or otherwise depend on CsRemake engine source.

Build the `LuaSharp` project in Visual Studio, or from the repository root:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' LuaSharp\LuaSharp.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
.\x64\Debug\LuaSharp.exe --test
.\x64\Debug\LuaSharp.exe .\LuaSharp\example.ls
.\x64\Debug\LuaSharpTests.exe
```

The final command writes `LuaSharp\example.lua`. The compiler supports the syntax in `example.ls`: classes and inheritance, public/private sections, typed fields, parameters and locals, constructors, methods, `auto`, `if`/`else`, `return`, `new`, `null`, arrays, indexing/slices, and empty Lua table literals. It also emits EmmyLua/LuaLS annotations for fields, parameters, and returns.

The lexer produces a source-located token stream and recoverable diagnostics. One recursive-descent syntax tree is shared by the semantic analyzer, transpiler, and native language server. Semantic analysis binds scoped locals and parameters, resolves inherited members and expression types, and records declaration references used by completion, hover, definition, signatures, and semantic tokens.

Only `int`, `float`, `string`, `table`, and `bool` are built-in types. All project and engine types must come from source declarations; the analyzer contains no engine-specific type names.

`LuaSharpTests` is the unit-test executable. The native LSP integration test is self-contained and can be run after building `LuaSharpLsp`:

```powershell
node LuaSharp/test/lsp.integration.test.mjs
```
