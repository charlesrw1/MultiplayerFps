# LuaSharp native ABI

This document defines the native backend contract. The ABI version is currently `6`. Generated modules expose `ls_module_abi_version`, reject incompatible runtime API tables in `ls_module_bind`, and are initialized through `ls_module_init`.

## Scalar semantics

| LuaSharp type | Native ABI type | Semantics |
|---|---|---|
| `int` | signed 64-bit `ls_int` | Two's-complement wrapping for addition, subtraction, multiplication, and negation |
| `float` | IEEE-754 64-bit `double` | `/` always performs floating-point division |
| `bool` | 8-bit `ls_bool` | `0` is false and `1` is true at API boundaries |
| `string` | `LS_String*` | Interned immutable UTF-8 object containing a signed 64-bit byte length and zero-terminated data |
| `table` | opaque `LS_Table*` | GC-managed dynamic table with tagged keys and values |
| `T[]` | `LS_List*` | GC-managed dynamic array of unboxed `T` elements, zero-based |
| `any` | 16-byte `LS_Value` | 64-bit kind tag (`LS_KIND_*`) plus a 64-bit scalar or managed pointer payload |
| class reference | `LS_Type*` | Nullable, stable for the lifetime of its owning runtime heap. The first field is `const LS_TypeInfo*` |
| enum | signed 64-bit `ls_int` | Values use the declaration's resolved integer value |

`int / int` returns `float`. Integer remainder follows Lua floor-modulo semantics, so `-5 % 3` is `1`. Integer division by zero consequently follows floating-point division behavior. Integer remainder by zero reports a structured runtime error and returns zero so native control can unwind through an ABI-safe caller.

Source expressions are defined to evaluate left-to-right. The C emitter must introduce temporaries wherever C does not guarantee that ordering. This lowering is not complete in ABI v1, so calls containing assignment side effects remain a tracked correctness gap.

## Runtime API

Generated code does not call the C allocator directly. The host binds a versioned `LS_RuntimeApi` table. The table, the shared layouts (`LS_String`, `LS_List`, `LS_Value`, `LS_Closure`) and all C helpers are in [`runtime/luasharp/runtime.h`](../runtime/luasharp/runtime.h). Every generated module includes that header, and it also defines `ls_module_abi_version` and `ls_module_bind`. `NativeRuntime.cpp` implements the table, and the two sides must change together.

The table contains these groups:
- allocation, marking and error reports
- string interning, plus string operations that make new strings: concatenation, substring, case conversion, `tostring` and `tonumber`
- table creation, get, set and iteration (`table_next`)
- list creation, growth (`list_reserve`) and slices
- `print`

Boxing, checked unboxing, dynamic access and calls on `any`, and closure cells are header helpers. They do not use the table.

## Tables

A table is a hash map from tagged keys to tagged values. Entries keep their insertion order, and a table foreach loop visits them in that order.
- A float key with an integer value is the same key as that integer, as in Lua. For example, `t[1.0]` and `t[1]` are the same entry.
- A missing key reads as nil. Writing nil removes the entry.
- A nil or NaN key on a write reports a runtime error.
- An uninitialized `table` variable or field is an empty table, the same as `{}` in the Lua backend.

`t.name` is the same as `t["name"]`. Generated code interns each member name once and keeps it in a module static, `ls_name_<name>`, which the module traces as a root.

## Dynamic values

A table read gives `any`. A lambda parameter is also `any`. The native backend converts between `any` and typed values wherever a value flows into a typed slot: locals, assignments, returns, arguments, fields and list elements.
- Boxing a typed value into `LS_Value` cannot fail. A null reference boxes as nil.
- Unboxing checks the kind at run time. A wrong kind reports an error, for example `expected int, got string`, and gives zero or null. Nil unboxes to null for reference types.
- A float with an integer value unboxes to `int`, as in Lua `math.tointeger`.
- A class object unboxes only to its own class. Each object starts with a `const LS_TypeInfo*`, and the check compares that pointer.
- A list unboxes when its element kind matches. The class of object elements is not checked.
- An `any` condition uses Lua truthiness: only nil and false are false. `bool(x)` on `any` does the same.

`LS_TypeInfo` holds the class name, a table of fields (name, offset and kind) and a table of method thunks. With it, `any` code can do these operations on class objects:
- `x.field` reads a field, and `x.field = v` writes it with a checked unbox.
- `x.method(args)` calls a method. Missing arguments are nil.

A method that has a parameter or result of an external type cannot be called through `any`.

`x.name(args)` on a `table` or a table in `any` follows the Lua backend, which emits `x:name(args)`. The table is the first argument.

A dynamic call has a limit of 16 arguments.

## Lists

A list is one GC allocation with this layout: `count`, `capacity`, `element_size`, `element_kind` and `data`. The runtime owns the element buffer and frees it when it collects the list. Generated code reads and writes elements in `data` directly, but only the runtime resizes the buffer. Push doubles the capacity when the list is full.

`element_kind` is the `LS_KIND` of the element type, or `LS_ELEMENT_VALUE` (`0`) when the elements are `LS_Value`. The collector traces elements of kind `LS_KIND_STRING` and higher, and `any` code uses the kind to box elements.

A negative index counts from the end. An index that is out of range, a `pop_back` on an empty list, or the use of a null list reports a runtime error, and the operation continues on a zeroed scratch value. Lua returns `nil` in these cases. A list boxes into `LS_Value` as kind `8`.

In the language, a `table` value can go where a list is expected and the opposite. Natively, lists and tables are separate objects. A table literal in a list context becomes a list literal, and the native backend rejects all other conversions between the two.

## Strings

Strings are immutable and interned, so `==` compares pointers. Every operation that makes a string (`+`, `+=`, `substring`, `lower`, `upper`, `string(x)`) interns its result. These operations follow Lua 5.4:
- `substring` follows `string.sub`.
- `string(x)` follows `tostring`. For example, `string(3.0)` is `"3.0"`.
- `int(s)` and `float(s)` follow `tonumber`. A string that is not a number reports a runtime error and gives `0`.

## Loops

- A list foreach loop reads `count` again before each pass, the same as `ipairs`.
- A range loop evaluates its inclusive bounds once. A change to the loop variable does not change the iteration.
- A table foreach loop visits non-null entries in insertion order.
- A foreach loop over `any` follows the Lua backend. With two variables it is `pairs`, and with one variable it is `ipairs`, which stops at the first nil. A list in `any` gives 1-based keys, the same as Lua.

`NativeRuntime` owns zeroed, non-moving allocations and performs precise stop-the-world mark-and-sweep collection. Generated trace functions cover class reference fields, closure environments and cells; the runtime traces tagged table keys and values and list elements. Module globals are registered as roots while their module is loaded, and host code can keep returned objects alive with `Pin`/`Unpin`. Collection currently occurs only when the host calls `Collect`, so it must not run concurrently with generated code. There is consequently no write barrier or generated stack-map requirement yet.

Interning is weak: identical live strings share one object, but the intern table does not itself keep strings alive. Sweeping a string removes its intern-table entry. ABI changes increment `abi_version`; generated code must never guess the layout of a different version.

## Module state

Top-level variables live in a generated `LS_ModuleState`, persist after `ls_module_init`, and are discoverable through generated address getters. `NativeModule::FindGlobal` resolves those getters using compiler-generated metadata.

Module state remains inside the TinyCC module and therefore dies when that module is destroyed. Reload validates global types but does not migrate their values yet.

## Native symbols

Host functions are registered explicitly with `NativeCompileOptions::symbols`. Semantic declarations are supplied separately through `NativeCompileOptions::externalTypes`; registering an address never bypasses LuaSharp type checking.

External object types are opaque. Generated code may pass their pointers to declared methods, but it may not inspect their C++ layout. Engine integrations should use generation-checked handles or opaque API objects rather than exposing C++ object memory.

## Closures and current limits

A native closure object contains an invoke pointer, an optional GC-managed environment and its arity. Parameters and results cross the closure boundary as `LS_Value`. A call through a typed closure variable has its arity checked at compile time. A call through `any` pads missing arguments with nil.

Captures follow Lua upvalues:
- Module globals are not captured. Closures use module state directly.
- A captured local that no code assigns is copied into the environment.
- A captured local that some code assigns, in the closure or outside it, lives in a GC cell. The enclosing scope and every closure share that cell. Each pass of a foreach loop declares new loop variables, so each pass has its own cell.
- A nested closure captures through its parent closure.

Block closures can return values. `return` boxes the value into the result.

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
- `emit-c` only writes the generated C.
- The CLI exits with a non-zero code when a diagnostic is an error.
- `printf(format, values...)` does not append a newline. It accepts `%d`/`%i` (int), `%f` (numeric), `%s` (string), `%b` (bool), `%p` (managed reference), and `%%`.
