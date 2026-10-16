# LuaSharp

A standalone C++20 compiler for a typed, brace-based language. It emits Lua and has an experimental native C/libtcc backend. It does not include, link to, or otherwise depend on CsRemake engine source.

Build the `LuaSharp` project in Visual Studio, or from the repository root:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' LuaSharp\LuaSharp.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
.\x64\Debug\LuaSharp.exe --test
.\x64\Debug\LuaSharp.exe .\LuaSharp\native-example.ls
.\x64\Debug\LuaSharp.exe .\LuaSharp\native-example.ls -o .\x64\Debug\native-example.generated.c
.\x64\Debug\LuaSharp.exe emit-c .\LuaSharp\example.ls
.\x64\Debug\LuaSharp.exe emit-lua .\LuaSharp\example.ls
.\x64\Debug\LuaSharpTests.exe
```

Passing a `.ls` file directly checks it, prints all errors and warnings with line/column locations, writes generated C beside the script (or to `-o`), compiles it in memory with libtcc, and runs its module initializer. `run` and `run-native` are explicit aliases for that workflow. `emit-c` and `emit-lua` only generate their respective output.

The compiler supports the syntax in `example.ls`: classes and inheritance, enums with inferred or explicit integer values, expression-bodied and multiline brace-bodied lambdas, enforced public/private sections, typed fields, parameters and locals, constructors, methods, `auto`, `if`/`else if`/`else`, `while`, C-style `for`, value or zero-based index/value iteration over lists, key/value iteration over tables, inclusive integer ranges such as `1..10`, `break`, `continue`, `return`, `new`, `null`, arrays, indexing/slices, populated list literals, and Lua-style positional/keyed table values. It also emits EmmyLua/LuaLS annotations for fields, parameters, and returns.

## Native backend

`emit-c` checks the source normally and writes C11 generated directly from the bound semantic tree. `run-native` loads `libtcc.dll`, compiles that C in memory, relocates it, and runs the generated module initializer without creating object files or launching another process. Install the pinned official Win64 TinyCC package with the checksum-verifying setup script:

```powershell
& .\LuaSharp\scripts\install-libtcc.ps1
.\x64\Debug\LuaSharp.exe run-native .\LuaSharp\native-example.ls
```

The build copies `tools\tcc` (`libtcc.dll`, `lib\`, `include\`) to `tcc\` beside the executable (`TinyCC.targets`). After an explicit API path, the loader checks `LUASHARP_LIBTCC` (a full DLL path), then `<exe dir>\tcc\libtcc.dll`. It does not use the working directory. libtcc finds its runtime relative to its own DLL, so a lone `libtcc.dll` without `lib\libtcc1-64.a` is rejected.

Host code can use `NativeModule::FindSymbol` with the export metadata returned by `CompileToC`. Generated names are stable (`Class.method` becomes `ls_Class_method`, and constructors become `ls_Class_new`), which provides the function-slot boundary needed for hot reload. A module owns its TinyCC state, so its function pointers remain valid until that module is destroyed.

The versioned runtime contract, scalar semantics, module state, external-symbol rules, and GC/reload direction are specified in [`docs/native-abi.md`](docs/native-abi.md).

LuaSharp `int` is a signed 64-bit native value. Generated arithmetic uses private unsigned bit operations only to guarantee two's-complement wrapping without invoking C signed-overflow undefined behavior; unsigned integers are not exposed as a LuaSharp type. `ReloadableNativeProgram` provides stable atomic function slots, validates signatures/class layouts/global types, and retains old code generations so already-loaded calls can finish safely.

The native slice supports primitives, enums, classes without inheritance, constructors, methods, top-level initialization, `if`, `while`, C-style `for`, `break`/`continue`, arithmetic, interned strings and their non-allocating queries, mixed-key/mixed-value tables, and closures with tagged parameters and by-value local captures. The shared runtime uses a precise, non-moving mark-and-sweep heap and traces module globals, class fields, table contents, closure environments, and explicit host pins. Lists, dynamic member calls on untyped closure parameters, value-returning block closures, foreach/range loops, inheritance, allocating string operations, and native `Vec3`/`Quat`/`Transform` ABIs currently fail with an explicit backend error. Lua emission continues to support all of them.

The lexer produces a source-located token stream and recoverable diagnostics. The parser builds an error-tolerant syntax tree, and semantic analysis produces a separate bound tree containing resolved expression types, symbols, children, and lowering operations. The transpiler consumes that bound tree directly. Semantic analysis binds scoped locals and parameters, resolves inherited members, validates calls/operators/assignment targets/control-flow conditions/access/overrides, warns about unreachable code, and blocks Lua emission when errors remain.

The primitive types are `int`, `float`, `string`, `table`, and `bool`. `T[]` is a typed, zero-based list with negative indexing, slicing, and `push_back`, `pop_back`, `size`, `empty`, and `clear`. Strings expose `length`/`size`, `contains`, `starts_with`, `ends_with`, `substring`, `lower`, and `upper`. Primitive conversion uses ordinary call syntax such as `int(value)`, `float(value)`, `bool(value)`, and `string(value)`.

`table` is the dynamic Lua-table type: arbitrary key and value types are accepted and reads have the internal dynamic type `any`. `Vec3`, `Quat`, and `Transform` are native language types whose constructors, methods, fields, and metatable operators mirror `vec_quat_lua_stubs.lua` and `transform_lua_stubs.lua`. These native definitions and typed default values live in one declarative registry. All other project and engine types must come from source declarations; the analyzer contains no engine-specific type or function exceptions.

`printf(format, values...)` writes without automatically appending a newline. Native formatting supports `%d`/`%i` for `int`, `%f` for numeric values, `%s` for strings, `%b` for booleans, `%p` for managed references, and `%%` for a literal percent sign. Use `\n` in the format when a newline is wanted.

The VS Code extension indexes every `.ls` file in the workspace and refreshes that index as files change. Cross-file types and members participate in diagnostics, completion, hover, definition, references, rename, and document/workspace symbols. Semantic tokens distinguish user-declared class types from grammar-colored built-in types, and protocol positions are UTF-16 correct.

Remaining features that need an explicit language design are explicit imports/modules, base-constructor arguments, user-defined operator declarations, tuple/multiple-return types, and explicit nullable-type syntax. Formatting and diagnostic code actions are also not implemented yet.

`LuaSharpTests` is the unit-test executable. The native LSP integration test is self-contained and can be run after building `LuaSharpLsp`:

```powershell
node LuaSharp/test/lsp.integration.test.mjs
node LuaSharp/test/cli.integration.test.mjs
```
