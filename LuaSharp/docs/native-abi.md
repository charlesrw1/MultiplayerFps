# LuaSharp native ABI

This document defines the native backend contract. The ABI version is currently `13`. Generated modules expose `ls_module_abi_version`, reject incompatible runtime API tables in `ls_module_bind`, and are initialized through `ls_module_init`.

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
| class reference | `LS_Type*` | Nullable, stable for the lifetime of its owning runtime heap. The first field is `const LS_TypeInfo*`. A derived class starts with the fields of its base (see Classes and inheritance) |
| enum | signed 64-bit `ls_int` | Values use the declaration's resolved integer value |
| struct | C `LS_Name` by value | Value type. Copied on assignment, argument passing and return. Fields can hold references |

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
- A class object unboxes to its own class or to a base class. Each object starts with a `const LS_TypeInfo*`, and the check walks the `base` chain of that type info.
- A list unboxes when its element kind and element type match. Object elements must have the same class. Struct and list elements must have the same layout (see Structs).
- An `any` condition uses Lua truthiness: only nil and false are false. `bool(x)` on `any` does the same.

`LS_TypeInfo` holds the class name, a table of fields (name, offset and kind) and a table of method thunks. Both tables include inherited members, and each method entry calls the most-derived implementation. With it, `any` code can do these operations on class objects:
- `x.field` reads a field, and `x.field = v` writes it with a checked unbox.
- `x.method(args)` calls a method. Missing arguments are nil.

A method that has a parameter or result of an external type cannot be called through `any`.

`x.name(args)` on a `table` or a table in `any` follows the Lua backend, which emits `x:name(args)`. The table is the first argument.

A dynamic call has a limit of 16 arguments.

## Lists

A list is one GC allocation with this layout: `count`, `capacity`, `element_size`, `element_kind`, `element_type` and `data`. The runtime owns the element buffer and frees it when it collects the list. Generated code reads and writes elements in `data` directly, but only the runtime resizes the buffer. Push doubles the capacity when the list is full.

`element_kind` is the `LS_KIND` of the element type, or `LS_ELEMENT_VALUE` (`0`) when the elements are `LS_Value`. `element_type` is the `LS_TypeInfo` of object, struct and list elements, and null for other kinds. The collector traces reference elements, and the references inside struct elements through `element_type`. `any` code uses both fields to box elements.

Each list type that can be an element has a generated `LS_TypeInfo` (`ls_listtype_<id>`) of kind `LS_KIND_LIST`. It has one field, `element`, that describes its elements. A list field in a class or struct uses the same element description.

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

`NativeRuntime` owns zeroed, non-moving allocations and performs precise stop-the-world mark-and-sweep collection. Generated trace functions cover class reference fields (inherited ones included), struct fields that hold references, closure environments, cells and static fields. They pass `mark` the address of each reference slot, not its value, so that a later runtime can also move objects and update the slots; the runtime traces tagged table keys and values and list elements. Module globals are registered as roots while their module is loaded, and host code can keep returned objects alive with `Pin`/`Unpin`. Collection currently occurs only when the host calls `Collect`, so it must not run concurrently with generated code. There is consequently no write barrier or generated stack-map requirement yet.

Interning is weak: identical live strings share one object, but the intern table does not itself keep strings alive. Sweeping a string removes its intern-table entry. ABI changes increment `abi_version`; generated code must never guess the layout of a different version.

## Module state

Top-level variables and static fields live in a generated `LS_ModuleState`, persist after `ls_module_init`, and are discoverable through generated address getters. A static field is the global `Type.field` (C getter `ls_global_static_Type__field`). Static fields initialize at the start of `ls_module_init`, in declaration order. `NativeModule::FindGlobal` resolves those getters using compiler-generated metadata.

Module state remains inside the TinyCC module and therefore dies when that module is destroyed. Reload validates global types but does not migrate their values yet.

## Native symbols

Host functions are registered explicitly with `NativeCompileOptions::symbols`. Semantic declarations are supplied separately through `NativeCompileOptions::externalTypes`; registering an address never bypasses LuaSharp type checking.

External object types are opaque. Generated code may pass their pointers to declared methods, but it may not inspect their C++ layout. Engine integrations should use generation-checked handles or opaque API objects rather than exposing C++ object memory.

## Classes and inheritance

A class is a C `struct LS_Name` on the GC heap. Its first field is `const LS_TypeInfo* ls_type`. A derived class repeats the fields of its base first, in the same order, so a `LS_Derived*` is a valid `LS_Base*`.

- `LS_TypeInfo.base` points to the type info of the base class, or is null. `ls_is_a` walks this chain. `is`, `as` and unboxing from `any` use it.
- `LS_TypeInfo.vtable` holds one function pointer for each virtual slot. A `virtual` or `abstract` method adds a slot. An `override` uses the slot of the method it overrides. An abstract slot holds null; code cannot create an abstract class, so an object never has a null slot.
- A call to a virtual method loads the function from `obj->ls_type->vtable[slot]`. The function type uses the class that declared the slot as `self`. A call through `base`, a call to a sealed method and a call on a value of a sealed class are direct calls.
- Construction has two functions. `LS_T* ls_T_new(...)` allocates the object, sets `ls_type` to `T` and calls `void ls_T__init(LS_T* self, ...)`. `ls_T__init` calls the init function of the base constructor, then runs the field initializers of `T`, then the constructor body. Because `ls_type` is set first, a virtual call in a base constructor calls the most-derived override. An abstract class has no `ls_T_new`.
- Overloaded constructors use `ls_T_new__N` and `ls_T__init__N`.
- A class can derive from an extern class (see Script classes that derive from extern classes). It cannot derive from other external types. The C backend reports an error.

## Extern classes

An `extern class` is a class that the host implements. Its declaration has no code:

```
extern class Entity {
    int health;
    Entity(int health);
    virtual int damage(int amount);
    static int count();
}
extern class Player : Entity { virtual int level(); }
```

Rules:

- A method has no body. A method can be `static` or `virtual`, but not `abstract`, `override` or `sealed`.
- A field has no initializer. An extern class has no static fields.
- An extern class has one constructor or none. Methods are not overloaded. Without a constructor, `new T()` is an error.
- An extern class can derive only from an extern class. A script class can derive from an extern class (see below).
- Declarations can be in the module source or in `NativeCompileOptions::declarations`. A declaration source can contain only extern classes, enums and declared structs (see Declared structs). `DeclarationTypes()` converts declaration sources to `ExternalType` values.

### Proxies

- A script value of an extern class points to a proxy on the GC heap: `LS_NativeObject { const LS_TypeInfo* type; void* native; }`. All extern classes use this C layout.
- `LS_TypeInfo.flags` has `LS_TYPE_NATIVE` for an extern class.
- Each module emits its own `static const` type info for each extern class. Type infos with `LS_TYPE_NATIVE` match by name in `ls_is_a` and `ls_type_same`. The runtime owns the type info that proxies point to: `NativeRuntime::NativeClass(name, base)`. `NativeModule::Load` registers each extern class that the module uses. A base that is different from an earlier registration causes an error.
- The host makes a proxy with `NativeRuntime::NewNativeProxy(type, native, finalize)`. Make one proxy for each host object, so that `==` compares pointers.
- `NativeRuntime::DestroyNative(proxy)` sets `native` to null. After that, the proxy is null for `==`, `is`, `as`, `any` and conditions on `any`.
- A call or a field access on a null or destroyed proxy reports the runtime error "access to 'T.m' on a destroyed or null object" and gives the default value of the result type.
- A finalizer runs when the GC frees the proxy. Use it to delete a host object that script owns.

TinyCC on Windows does not resolve data symbols that `tcc_add_symbol` adds. Only functions cross the boundary as symbols. This is why each module emits its own type infos.

### Host thunks

The host registers one C function in `NativeCompileOptions::symbols` for each member that the module declares:

| Member | Host thunk |
|---|---|
| Method `R m(A a)` of `T` | `R ls_native_T_m(void* self, A a)` |
| Static method `R m(A a)` of `T` | `R ls_native_T_m(A a)` |
| Field `F f` of `T` | `F ls_native_T__get_f(void* self)` and `void ls_native_T__set_f(void* self, F value)` |
| Constructor `T(A a)` | `LS_T* ls_native_T_new(A a)` |

- `self` is the host pointer (`LS_NativeObject.native`), never null.
- A parameter of an extern class type is the host pointer, or null for a null or destroyed proxy.
- A result of an extern class type is a proxy. The constructor also returns a proxy.
- Other types use their normal C types.
- A derived class inherits the thunks of its base. Generated code calls `ls_native_Entity_damage` for `player.damage(1)`.
- Generated code calls the thunks through wrappers (`ls_T_m`, `ls_T__get_f`, `ls_T__set_f`, `ls_T_new`). The wrappers check the receiver.

### Root slots

- `NativeRuntime::AddRootSlot(void** slot)` registers a host variable that holds a GC object. `Collect` marks `*slot`. `RemoveRootSlot` removes it.
- `NativeRef` is a move-only root slot. It keeps one GC object alive while it holds it. A move registers the new slot and removes the old one.
- Host code must keep GC objects only in root slots, so that a later moving collector can update them.

### Script classes that derive from extern classes

```
class Spinner : Component {
    float extra = 5;
    override int tick(int dt) { return base.tick(dt * 2) + extra; }
}
```

Rules:

- An `override` of an extern method needs a `virtual` extern method.
- The host makes the objects. `new` in script is an error. Constructors cannot have parameters or call `: base(...)`.
- An object starts with `LS_NativeObject` (`ls_type`, then `ls_native`), then the script fields. The host pointer is in `ls_native`, so the object is also the proxy of its host object.
- The type info has the flag `LS_TYPE_NATIVE_BASE`. Its `base` is the extern class. A destroyed object is null for `==`, `is`, `as` and `any`, the same as a proxy.
- The type info has a table of host overrides (`host_overrides`, `LS_HostOverride { name, function }`). Each entry is the most-derived script implementation of an extern virtual method. The function is `R ls_T_m(LS_T* self, ...)`, with the C types of the module. An extern class argument or result is a proxy.
- Extern fields and methods on these objects, also through implicit `this`, use the extern wrappers.
- A call to an extern method from script uses the host thunk. The host dispatches it, so it gets to the script override.
- `base.m()` for an extern virtual method `m` calls the host thunk `ls_native_N__base_m(void* self, ...)`. `N` is the nearest extern ancestor. The host calls its implementation of `m` without virtual dispatch.

Creation:

1. `LS_T* ls_T__host_alloc(void)` allocates the object and sets `ls_type`. `ls_native` is null.
2. The host makes its object and calls `NativeRuntime::SetNative(object, native)`. `NativeBaseName` gives the extern class, and `HostOverride` gives each override.
3. `void ls_T__init(LS_T* self)` runs the field initializers and the constructor body.

An abstract class has no `ls_T__host_alloc`.

Type infos:

- Each class of this kind exports `const LS_TypeInfo* ls_T__type(void)`. `CCompilationResult::hostClasses` lists these classes, base classes first. `NativeModule::HostClassType(name)` calls the function.
- `NativeRuntime::TypeName`, `NativeBaseTypeName`, `ScriptBaseType` and `TypeHostOverride` read a type info. `ScriptBaseType` is null when the base is the extern class.

Editable fields:

- `[Editable]` before a field sets `LS_FIELD_EDITABLE` in `LS_FieldInfo::flags`. Only instance fields of a class with an extern ancestor can have it. The types are `bool`, `int`, `float`, `string`, `Vec2`, `Vec3`, `Quat` and extern classes.
- An extern-class field holds the proxy of the host object, or null. The CsRemake engine accepts asset classes and `Entity` (and their subclasses). The editor shows the asset picker or the entity picker. Other extern classes are a load error.
- `[Editor("Name")]` sets `LS_FieldInfo::editor`: the name of a host editor (in CsRemake, `PropertyInfo::custom_type_str`). `[Former("old")]` sets `LS_FieldInfo::former`: the previous name of the field, so that the host can read data saved under it. Both need `[Editable]` on the same field. The argument is a string of letters, digits and `_`. Fields without them have null pointers.
- Attributes can share brackets: `[Editable, Editor("BoneNameString")]`.
- Other attributes, a duplicate attribute, a missing or extra argument, and attributes on methods are errors.
- `NativeRuntime::EditableFields(type)` gives the name, offset, kind, type name (structs and extern classes), editor and former name of each editable field that the class declares. It does not give the fields of its base classes.

### Declared structs

A declared struct is a value type that the host and the script both use. The host passes it to thunks and gets it from thunks by value.

```
struct Hit { Entity what; Vec3 pos; bool hit; }
extern class Physics { Hit last; static Hit cast(Vec3 from, float range); }
```

Rules:

- A declared struct has only instance fields. A field has no initializer. A declared struct has no methods, constructors or static fields.
- Script code makes a value with `T()`. All fields start at zero, false or null.
- An `extern struct` gets `T()` from the host. `ls_S_default` calls the host thunk `LS_S ls_native_S_new(void)`. Every default value of the struct uses it, also the default value of a field of that type. The rules for fields are the same as for a declared struct.
- The struct has the C layout of a script struct (see Structs). Fields are in declaration order. `int` is `ls_int`, `float` is `double`, `bool` is `ls_bool`, `string` is `LS_String*`, an enum is `ls_int`, and Vec3 is three 32-bit floats.
- A field of an extern class type holds the proxy, not the host pointer. A thunk parameter of an extern class type is the host pointer (see Host thunks). Do not mix the two.
- A thunk takes and returns the struct as `LS_S` by value. On Win64, MSVC and TinyCC pass a C aggregate the same way: in a register for 1, 2, 4 or 8 bytes, else through a pointer.
- Each module emits the layout, `ls_S_default`, `ls_S_eq` and the type info of each declared struct.
- Each module exports `const LS_TypeInfo* ls_S__type(void)` for each declared struct. `CCompilationResult::externalStructs` lists them. `NativeModule::ExternalStructType(name)` calls the function, and `NativeRuntime::StructLayout(type)` gives the size and the offset of each field. The host uses these to check its own copy of the layout.
- A native module can use an external struct only when it has only instance fields. `CompileToC` reports an error for other external structs.

### External enums

- An enum in `NativeCompileOptions::declarations` or in other external types gets the C constant `LS_E_M`, the same as an enum in the module.
- `ExternalType::values` holds the value of each member. When it is empty, the value of a member is its ordinal.

### Engine binding

The engine binds its reflected classes with generated code (`Scripts/codegen_luasharp.py`):

- Each codegen pass puts its declarations into the generated C++ file. The engine compiles against these declarations.
- For the tools, the editor writes all declarations to `<scripts>/.luasharp/engine.ls` at startup (`write_luasharp_declarations`, `Source/Scripting/LuaSharpDeclarations.h`). `App.exe --dump-luasharp-decls <scripts folder>` does the same without the editor. The tools find the file through `luasharp.json` (see the [README](../README.md#projects-luasharpjson)).
- The generated C++ file has one thunk for each bound member, and it registers the declarations and the thunks with `lsbind::Registrar` (`Source/Scripting/LuaSharpBinding.h`).
- `LuaSharpEngine` collects all registrations. It owns the `NativeRuntime`, registers each class base first, and compiles modules with all declarations and thunks.
- `LuaSharpEngine::proxy_of` makes one proxy for each `ClassBase` and keeps it in `ClassBase::script_proxy`, which is a root slot. The type of the proxy is the nearest bound class of the object.
- `~ClassBase` calls `DestroyNative` on the proxy and removes the root slot.
- A `REFLECT(script_read)` field has a setter that reports a runtime error.
- A `STRUCT_BODY()` struct is a declared struct with its bindable `REF` fields. The declaration lists each other field as a `// skipped` comment. The pass that owns the header declares the struct.
- Each pass that uses a struct writes a C++ mirror with the C layout, and functions that convert each field: `ls_to_ls_S` (C++ to script), `ls_assign_S` and `ls_to_cpp_S` (script to C++). These are before the classes, because the `ScriptImpl_X` slot calls use them.
- Each engine struct is an `extern struct`. The owning pass registers `ls_native_S_new`, which returns `ls_to_ls_S(S{})`, so `T()` in script has the C++ default values.
- A method argument and a slot result start from `T{}`. Fields that script cannot see get their default values. A field setter of struct type assigns in place, so those fields keep their values.
- `obj<T>` is the proxy of `T`, the same as `T*`.
- The owning pass registers the layout of the mirror (`lsbind::StructLayout`). `LuaSharpEngine::compile` compares the layout of each declared struct in the module with it. A different size, field name or field offset is a compile error.
- Struct methods do not cross.
- `std::vector<T>` is `T[]` when `T` binds: a scalar, a string, an enum, a class pointer, `obj<T>`, a math type or an engine struct. A nested list and a list of a type that does not bind are skipped.
- A list crosses as a copy. C++ to script makes a new list (`lsbind::to_list`), and script to C++ makes a new vector (`lsbind::from_list`). A null list is an empty vector. A change to the list in script does not change the C++ vector. To change a list field, assign the list back to the field.
- A list element has the C layout of a struct field, so an object element is the proxy.
- A list of objects or structs needs an element type that stays valid after its module unloads. `NativeRuntime::AdoptStructType` keeps a runtime copy of a struct type, and `FindStructType` finds it by name. `LuaSharpEngine::compile` adopts the math types and the external structs of each module before it runs the module initializer. Each module exports `ls_<Math>__type` for the math types, and `NativeModule::StructType(name)` returns a math or external struct type. Object elements use `FindNativeClass`.
- `NativeRuntime::NewList` makes a list of zeroed elements for host code.

A script class can derive from a bound class `X`:

- For `CLASS_BODY(X, scriptable)`, codegen makes `ScriptImpl_X`. It has `ls_slots`, one slot for each bound virtual method of `X` and its ancestors. Each override calls the slot when it is set, else `X::m`. When `ls_slots` is null, the object is a Lua object, and the Lua path runs.
- Codegen also makes the base thunks `ls_native_X__base_m` and registers the slot names with `lsbind::Registrar`.
- `LuaSharpEngine::create(module, "Spinner")` allocates the script object, makes `ScriptImpl_X` with `scriptable_allocate`, and fills the slots from the host overrides. Then it sets `script_proxy` and runs the script constructor. It returns `std::unique_ptr<ClassBase>`.
- A class that is not scriptable can be a base only when the script class overrides none of its methods. `create` then makes a plain `X`.
- A method that returns a reference has no slot. `create` reports an error when a script class overrides it.
- The module must stay loaded while its objects are alive.
- `LuaSharpEngine::load(name, source)` keeps the module and registers a `ClassTypeInfo` for each class with a scriptable engine base. `ClassBase::create_class`, the editor and serialization then find the class by name. Each editable field is a `PropertyInfo` with `PROP_SCRIPT_FIELD`: its offset is in the script object. `int` is `Int64`, `float` is `Double` and `string` is `ScriptString`.
- A runtime error in an override stays in `NativeRuntime::LastError`. The override continues with default values, the same as other script code. No C++ exception goes through script frames.

## Structs

A struct is a C `struct LS_Name` that is passed, returned and stored by value. A class field, a list element or a closure cell stores it inline. Its fields can hold references. The collector traces those through the struct's `LS_TypeInfo` wherever the struct is stored: class fields, list elements, boxes, cells, globals and static fields.

- Constructors are `LS_S ls_S_new(...)`. `LS_S ls_S_default(void)` applies the field initializers. `ls_bool ls_S_eq(LS_S, LS_S)` compares member by member: numbers by value, references by identity.
- Instance methods take `LS_S* self`. A call on a variable passes its address. A call on a temporary first copies it to a local.
- A list of structs has `element_kind` `LS_KIND_STRUCT`, any `element_size`, and the struct's `element_type`.
- Converting a struct to `any` allocates a box: a `const LS_TypeInfo*` followed by the value at `LS_STRUCT_PAYLOAD` (16) bytes. `ls_unbox_struct` checks the type and copies the value out. The box kind is `LS_KIND_STRUCT`. Code cannot assign a field of a box, because other values and table keys can share the box.
- Each module emits its own type info. Two struct type infos match when they have the same name, size and fields (name, offset, kind and type). Class type infos match only when they are the same pointer.
- `ls_value_equal` and table keys compare boxed structs field by field, the same as `ls_S_eq`. A struct key with a NaN field reports a runtime error.
- The type info lists the fields with offsets that include the box header. A field has a slot kind: the `LS_KIND_*` of its type, or `LS_SLOT_FLOAT32` and `LS_SLOT_INT32` for packed native components. A nested struct field uses `LS_KIND_STRUCT` and points to its own type info. `LS_TypeInfo.size` is the size of the value. `LS_TypeInfo.kind` is `LS_KIND_OBJECT`, `LS_KIND_STRUCT` or `LS_KIND_LIST`.
- `string(value)` formats a boxed struct as `Name(field, ...)`.

The native types in `vecmath.h` follow the same rules. `Vec2`/`Vec3` use `float` components, `Vec2i`/`Vec3i` use `int`, `Quat` is `{x, y, z, w}` and `Mat4x4` is `float m[16]`, column-major.

## Overloading, static methods and static fields

The first declaration of a name keeps the symbol `ls_Type_name`. Each later overload gets `ls_Type_name__N`, where `N` is its declaration order. Export names of later overloads include the parameter types, for example `Type.name(float)`. A static method has no `self` parameter. Reload treats each overload as a separate function. A static field is the module-state member `ls_static_Type__field`, and it is not part of the instance layout.

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

Type infos and vtables live in each module. An object keeps the type info of the generation that created it, so its virtual calls use the code of that generation. [`design.md`](design.md) describes the planned change: one type info for each class, owned by the runtime and updated in place.

## Command line and printf

- `LuaSharp <file.ls> [-o out.c]` checks the file, writes the C output, compiles it with libtcc, and runs the module initializer. `run` and `run-native` do the same.
- `emit-c` only writes the generated C.
- The CLI exits with a non-zero code when a diagnostic is an error.
- `printf(format, values...)` does not append a newline. It accepts `%d`/`%i` (int), `%f` (numeric), `%s` (string), `%b` (bool), `%p` (managed reference), and `%%`.
