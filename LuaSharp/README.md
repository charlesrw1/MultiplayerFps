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

The primitive types are `int`, `float`, `string`, `table`, and `bool`. `T[]` is a typed, zero-based list with negative indexing, slicing, and `push_back`, `pop_back`, `size`, `empty`, and `clear`. Strings expose `length`/`size`, `contains`, `starts_with`, `ends_with`, `substring`, `lower`, and `upper`. Primitive conversion uses ordinary call syntax such as `int(value)`, `float(value)`, `bool(value)`, and `string(value)`.

`table` is the dynamic Lua-table type: arbitrary key and value types are accepted and reads have the internal dynamic type `any`. `Vec3`, `Quat`, and `Transform` are native language types whose constructors, methods, fields, and metatable operators mirror `vec_quat_lua_stubs.lua` and `transform_lua_stubs.lua`. These native definitions live in one declarative registry. All other project and engine types must come from source declarations; the analyzer contains no engine-specific type or function exceptions.

Known remaining language gaps include non-empty table/list literals, loops, imports and cross-file symbol indexing, user-defined operator overloads, tuple/multiple-return typing, and deeper control-flow analysis. The LSP does not yet provide references, rename, document/workspace symbols, formatting, or code actions.

`LuaSharpTests` is the unit-test executable. The native LSP integration test is self-contained and can be run after building `LuaSharpLsp`:

```powershell
node LuaSharp/test/lsp.integration.test.mjs
```
