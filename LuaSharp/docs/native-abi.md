# LuaSharp native ABI

This document defines the native backend contract. The ABI version is currently `4`. Generated modules expose `ls_module_abi_version`, reject incompatible runtime API tables in `ls_module_bind`, and are initialized through `ls_module_init`.

## Scalar semantics

| LuaSharp type | Native ABI type | Semantics |
|---|---|---|
| `int` | signed 64-bit `ls_int` | Two's-complement wrapping for addition, subtraction, multiplication, and negation |
| `float` | IEEE-754 64-bit `double` | `/` always performs floating-point division |
| `bool` | 8-bit `ls_bool` | `0` is false and `1` is true at API boundaries |
| `string` | `LS_String*` | Interned immutable UTF-8 object containing a signed 64-bit byte length and zero-terminated data |
| `table` | opaque `LS_Table*` | GC-managed dynamic table with tagged keys and values |
| `any` | 16-byte `LS_Value` | 64-bit kind tag plus a 64-bit scalar or managed pointer payload |
| class reference | `LS_Type*` | Nullable, stable for the lifetime of its owning runtime heap |
| enum | signed 64-bit `ls_int` | Values use the declaration's resolved integer value |

`int / int` returns `float`. Integer remainder follows Lua floor-modulo semantics, so `-5 % 3` is `1`. Integer division by zero consequently follows floating-point division behavior. Integer remainder by zero reports a structured runtime error and returns zero so native control can unwind through an ABI-safe caller.

Source expressions are defined to evaluate left-to-right. The C emitter must introduce temporaries wherever C does not guarantee that ordering. This lowering is not complete in ABI v1, so calls containing assignment side effects remain a tracked correctness gap.

## Runtime API

Generated code does not call the C allocator directly. The host binds a versioned table:

```c
typedef struct LS_RuntimeApi {
    unsigned long long abi_version;
    void* context;
    void* (*allocate)(void* context, unsigned long long size, LS_TraceFunction trace);
    void (*mark)(void* context, void* object);
    void* (*intern)(void* context, const char* value);
    void* (*table_create)(void* context, unsigned long long count, const LS_Value* pairs);
    void (*table_get)(void* context, void* table, const LS_Value* key, LS_Value* out);
    void (*table_set)(void* context, void* table, const LS_Value* key, const LS_Value* value);
    void (*print)(void* context, void* format, unsigned long long count, const LS_Value* arguments);
    void (*report_error)(void* context, const char* message);
} LS_RuntimeApi;
```

`NativeRuntime` owns zeroed, non-moving allocations and performs precise stop-the-world mark-and-sweep collection. Generated trace functions cover class reference fields and closure environments; the runtime traces tagged table keys and values. Module globals are registered as roots while their module is loaded, and host code can keep returned objects alive with `Pin`/`Unpin`. Collection currently occurs only when the host calls `Collect`, so it must not run concurrently with generated code. There is consequently no write barrier or generated stack-map requirement yet.

Interning is weak: identical live strings share one object, but the intern table does not itself keep strings alive. Sweeping a string removes its intern-table entry. ABI changes increment `abi_version`; generated code must never guess the layout of a different version.

## Module state

Top-level variables live in a generated `LS_ModuleState`, persist after `ls_module_init`, and are discoverable through generated address getters. `NativeModule::FindGlobal` resolves those getters using compiler-generated metadata.

Module state remains inside the TinyCC module and therefore dies when that module is destroyed. Reload validates global types but does not migrate their values yet.

## Native symbols

Host functions are registered explicitly with `NativeCompileOptions::symbols`. Semantic declarations are supplied separately through `NativeCompileOptions::externalTypes`; registering an address never bypasses LuaSharp type checking.

External object types are opaque. Generated code may pass their pointers to declared methods, but it may not inspect their C++ layout. Engine integrations should use generation-checked handles or opaque API objects rather than exposing C++ object memory.

## Closures and current limits

The implemented native closure object contains an invoke pointer and an optional GC-managed environment. Parameters cross the closure boundary as tagged `LS_Value` values; numeric dynamic arithmetic and comparisons are implemented. Referenced locals or `this` are copied into a precisely traced environment. Module globals are accessed through module state instead of being copied. Captures are currently by value; shared mutable capture cells, value-returning block closures, and dynamic member dispatch on untyped parameters are still required for full lambda parity.

Closure invoke pointers currently point into their TinyCC module. `ReloadableNativeProgram` retains old generations, so those pointers remain valid there. A later ABI will replace raw invoke pointers with stable function-slot IDs before retired code can be unloaded.

## Reload direction

Hot reload requires module manifests containing stable function IDs, signatures, type IDs, and layout hashes. Reload will compile and validate a new module before atomically replacing function slots. Old code cannot be unloaded until active calls drain, and incompatible persistent state requires an explicit migration or a rejected reload.

The first reload layer is implemented by `ReloadableNativeProgram`. A `NativeFunctionSlot` has a stable identity and atomically changes its implementation address after validation. Reload currently:

- rejects removed exports and changed function signatures;
- rejects removed classes and changed field layouts;
- rejects removed globals and changed global types;
- permits new functions, classes, and globals;
- retains old TinyCC modules so a call that loaded the previous address can finish safely.

Module-global values are not migrated yet, and retired generations are intentionally retained until the reloadable program is destroyed. Host-owned state and active-call epochs will replace those temporary policies.

## Command line and printf

- `LuaSharp <file.ls> [-o out.c]` checks the file, writes the C output, compiles it with libtcc, and runs the module initializer. `run` and `run-native` do the same.
- `emit-c` and `emit-lua` only write generated code.
- The CLI exits with a non-zero code when a diagnostic is an error.
- `printf(format, values...)` does not append a newline. It accepts `%d`/`%i` (int), `%f` (numeric), `%s` (string), `%b` (bool), `%p` (managed reference), and `%%`.
