# Design: inheritance and engine binding

This document records the design decisions for two work items:

- Part 1: class inheritance in the native backend.
- Part 2: binding the engine ClassBase system to LuaSharp. This part is design only.

## Part 1: inheritance

### Language

- A class has zero or one base class: `class Derived : Base { ... }`. Structs have no base.
- Methods are not virtual by default. Use C# keywords:
  - `virtual` declares a method that a derived class can override.
  - `override` replaces an inherited `virtual`, `abstract` or `override` method. The signature must match.
  - `abstract` declares a method without a body. A class with an abstract method, declared or inherited and not overridden, is abstract. Code cannot `new` an abstract class. Mark the class `abstract` too.
  - `sealed` on an `override` stops further overrides. `sealed` on a class stops derivation.
- A method with the same name and parameters as an inherited method, but without `override`, is an error.
- A constructor calls a base constructor with `Derived(int x) : base(x, 2) { ... }`. Without `: base(...)`, the constructor calls the parameterless base constructor. If the base has none, that is an error.
- Construction order: base field initializers, base constructor body, derived field initializers, derived constructor body.
- A virtual call in a constructor calls the most-derived override.
- `base.m(args)` calls the base implementation without virtual dispatch.
- `x is T` is true when `x` is not null and its class is `T` or derives from `T`.
- `x as T` gives `x` as a `T`, or `null` when `x is T` is false.
- `is` and `as` accept class and `any` operands. For `any`, `is` can also test for a primitive, an enum, a list or a struct. `as` needs a reference type (a class, `string`, `table` or a list).
- `base` is not reserved. A field or local with that name keeps its meaning.

### Runtime

- A derived object starts with the fields of its base, in the same order. A `Derived*` is a valid `Base*`.
- `LS_TypeInfo` gains `base` (the base type info, or null) and `vtable` (function pointers, one per virtual slot).
- A virtual method gets a slot when it is declared. An override uses the slot of the method it overrides. A call to a virtual method loads the function from `obj->type->vtable[slot]`.
- Calls to methods that are not virtual stay direct C calls.
- `ls_unbox_object` and `is`/`as` walk the `base` chain.
- The method table of a type info lists inherited methods, with the most-derived implementation. `any` method calls use this table.
- Generated trace functions trace inherited fields.

### Groundwork for Part 2

- Trace functions visit the address of each reference slot, not the value. The same functions can then mark objects and relocate them (see Hot reload).
- Stable class type infos moved to Part 2. A reload today runs `ls_module_init` again with new module state, and objects from an old generation reach new code only through host pins. Stable type infos are useful only together with the reload changes in Part 2 (global migration and relocation).

## Part 2: engine binding

Part 2 has these milestones (#35):

1. `extern class` declarations, proxies and the host API (#37). Done.
2. Engine codegen: `engine.ls`, the typed C thunks and `ClassBase::script_proxy`. The engine links LuaSharp (#38). Done.
3. Script classes that derive from engine classes (`ScriptImpl_X` slot tables) (#39). Done.
4. `[Editable]` fields as `PropertyInfo` (#40). Done.
5. Hot reload: stable type infos, migration and relocation.

Milestone 1 found that TinyCC on Windows does not resolve data symbols that `tcc_add_symbol` adds. Each module therefore emits its own type info for each extern class, and these match by name (see [native-abi.md](native-abi.md#extern-classes)). Stable script class type infos (milestone 5) must use module data that the host writes after relocation.

Milestone 2 found these items:

- MSVC returns glm types in a different way than C returns a struct, because glm types have constructors. The thunks therefore use plain mirror structs (`lsbind::Vec3` and similar) at the C boundary.
- The Python header parser does not keep the values of C++ enum members. The codegen therefore converts between the C++ value and the LuaSharp ordinal with a generated table.
- Each module must declare all engine classes, so each compile takes approximately 0.6 s. A precompiled declaration header can make this faster.
- `std::vector`, `std::optional`, structs, handles, delegates and overloads are not bound yet. `engine.ls` shows each skipped member as a comment.

Milestone 3 found these items:

- The Lua backend still uses its own path in `ScriptImpl_X`. It runs when `ls_slots` is null, so Lua scripts did not change. Lua can move to the slots later.
- The engine makes objects only with `LuaSharpEngine::create`. Engine creation by class name (a `ClassTypeInfo` for each script class) is necessary for the editor and serialization (milestones 4 and 5).
- C++ must not throw through script frames, so a runtime error in an override stays in `NativeRuntime::LastError`.
- The host shares slot tables by content, not by type info address. A module that loads at the address of an unloaded module then cannot use its stale table.

Milestone 4 found these items:

- `PROP_LUA_BACKED` stays. Lua components still keep their fields in its shadow buffer. Remove it when Lua components move to LuaSharp.
- A script `float` is a C `double` and a script `string` is a GC string. The engine therefore has two new core types: `Double` and `ScriptString`. The editor has an editor for each, and a new `Vec2` editor.
- The supported `[Editable]` types are `bool`, `int`, `float`, `string`, `Vec2`, `Vec3`, `Quat`, enums, assets and `Entity`. Lists are not supported yet.
- `LuaSharpEngine::load(name, source)` registers a `ClassTypeInfo` only for a class whose nearest engine class is scriptable. An `[Editable]` field in a class with a base that is not scriptable makes `load` fail.
- The type infos are unregistered in `~LuaSharpEngine`. No object of a script class can live longer than the engine.

### Declarations

- The C++ codegen makes the engine declarations and puts them into the binary. They declare each reflected class as `extern class`, with its `REF` fields and functions.
- The editor writes them to `<scripts>/.luasharp/engine.ls` for the tools. The checker and the LSP find that file through `luasharp.json`.
- An `extern` method has no body. The C backend calls a generated C thunk for it.

### Calls from script to C++

- Codegen writes one typed C thunk for each `REF` function, for example `ls_native_Entity_get_health`. The compiler registers the thunks with `tcc_add_symbol`. There is no name lookup and no stack marshalling at run time.
- Type mapping:

| C++ | LuaSharp |
|---|---|
| `int32_t`, `int64_t` | `int` |
| `float`, `double` | `float` |
| `bool` | `bool` |
| `std::string`, `StringName` | `string` (interned at the boundary) |
| `glm::vec2/3`, `glm::ivec2/3`, `glm::quat`, `glm::mat4` | `Vec2`, `Vec3`, `Vec2i`, `Vec3i`, `Quat`, `Mat4x4` (same layout, copied) |
| reflected enums | enums |
| `std::vector<T>` | `T[]` (copied) |
| `ClassBase*` subclasses | extern classes |

- The wrapper structs `lVec3`, `lQuat` and similar are removed.

### Engine objects in script

- A script reference to an engine object points to a GC object with this header:

```c
typedef struct LS_NativeObject { const LS_TypeInfo* type; void* native; } LS_NativeObject;
```

- For a plain engine object this object is a proxy with only the header. A script class that derives from an engine class adds its fields after the header. That object is the proxy.
- `ClassBase::script_proxy` is a registered root slot. It keeps one proxy for each object, so `==` compares pointers. It also keeps the proxy alive while the C++ object lives.
- For a script-owned object, `script_proxy` must be a weak pointer, not a root. A root would keep the proxy alive, so the finalizer that deletes the object would never run.
- `~ClassBase` sets `proxy->native = nullptr` and unregisters the root slot. This costs O(1). Every script reference then sees null.
- A destroyed object is null everywhere: `==`, truthiness, `is`, `as`. A call to a native member of a destroyed object reports a runtime error. Script field access on a destroyed scripted object also reports an error.

### Ownership

- Engine-owned classes (the default): entities, components, assets. Script gets them from factory functions. `new` of an engine-owned class is a compile error.
- Script-owned classes (`REFLECT(script_owned)`): `new T()` in script makes the C++ object. The proxy owns it. When the GC frees the proxy, a finalizer deletes the C++ object.
- C++ gets a script-owned object only for the duration of a call. To keep it, C++ uses `ScriptRef<T>`, a move-only RAII root slot. C++ never stores a raw pointer to a GC object. Closures passed to C++ use `ScriptRef` too.

### Overriding C++ virtual functions

- Codegen makes `ScriptImpl_X` independent of the scripting language. Each override calls through a slot table:

```cpp
void update() final { if (auto fn = script_slots->update) fn(script_proxy); else X::update(); }
```

- LuaSharp fills the slots with compiled functions. The Lua backend fills them with thunks that call by name.
- `base.update()` in script calls a generated thunk that calls `X::update()` without virtual dispatch.
- Engine base classes need a parameterless constructor.

### Editor fields

- An attribute marks a script field as editable and serialized: `[Editable] float speed = 3;`.
- The compiler sets `LS_FIELD_EDITABLE` on each such field. The host makes a `PropertyInfo` for it. Its offset is in the script object. The flag `PROP_SCRIPT_FIELD` tells reflection to add the offset to the script object address.

### Hot reload

- A reload happens at a frame boundary, when no script function is on the stack.
- Class type infos are stable: one per class name, owned by the runtime. Generated code reaches them through a pointer that the host sets after relocation. A reload updates the vtable and method table in place, so existing objects call the new code.
- Code changes: the runtime updates function slots, vtables and method tables. No object moves.
- Globals and static fields keep their values. A changed initializer does not run again.
- Layout changes (a field added, removed, moved or changed in type): the runtime makes a new object for each instance of the changed class. It copies fields that have the same name and type. Then it walks every reference slot in the heap and changes pointers to the old objects. A type that stores a changed struct inline also changes layout.
- Every reference from C++ to a GC object goes through a registered root slot, so the walk can update it.
- A field default changed from one constant to another: instances that still have the old default get the new default.
- A closure keeps the code it was made with. The old code stays loaded until no closure uses it. The reload logs the number of closures that use old code. A method passed as a callback uses the stable slot and gets the new code.

### Rejected options

- A separate field block for each object, so that reload can replace it: one more pointer load for every field access.
- Respawn on layout change: the editor loses play state.
- Handles with a hash lookup for engine objects: slower for each access, and they do not work with `is` and `as`.
- A heap scan for each delete to set references to null: O(heap) for each delete.
