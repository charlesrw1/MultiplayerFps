# Framework Module

Core engine infrastructure: reflection, serialization, memory, collections, math, job system, UI utilities, and logging.

## Key Files

### Reflection & Class System
- `ClassBase.h` — Base class; `CLASS_BODY()` macro, `is_a<T>()`, `cast_to<T>()`
- `ClassTypeInfo.h` — Runtime type info per class
- `ClassTypePtr.h/cpp` — Weak pointer to class instances
- `AddClassToFactory.h` — Factory auto-registration
- `Reflection2.h` — `REFLECT()` macro system
- `ReflectionProp.h` — `PropertyInfo`, `core_type_id` enum
- `ReflectionMacros.h` — Macro helpers
- `StructReflection.h` — `STRUCT_BODY()` for plain data types
- `ArrayReflection.h`, `VectorReflect2.h` — Container reflection
- `UniquePtrReflection.h` — `unique_ptr<>` reflection
- `EnumDefReflection.h/cpp` — Enum reflection

### Serialization
- `Serializer.h/cpp` — Abstract serializer interface
- `SerializerJson.h/cpp`, `SerializerJson2.h/cpp` — JSON serialization
- `SerializerBinary.h/cpp` — Binary serialization
- `BinaryReadWrite.h` — Low-level binary I/O
- `DictParser.h/cpp`, `DictWriter.h` — Key/value config format
- `SerializedForDiffing.h` — Diffable serialization

### Memory
- `ArenaAllocator.h`, `ArenaStd.h`, `MemArena.h` — Pool/arena allocators
- `PoolAllocator.h` — Fixed-size object pools
- `FreeList.h` — Intrusive free list
- `Handle.h` — Type-safe integer handles for object references
- `SharedPtr.h` — Reference-counted smart pointer
- `Optional.h` — `opt<T>` optional value type

### Collections
- `Hashmap.h` — Open-addressing hash map
- `Hashset.h/cpp` — Hash set
- `InlineVec.h` — Small-buffer-optimized vector
- `RingBuffer.h` — Circular buffer

### String
- `StringName.h/cpp` — Interned strings (hash-based, zero-copy compare)
- `StringUtil.h/cpp`, `StringUtils.h/cpp` — Utilities (split, trim, format, etc.)

### Math & Geometry
- `MathLib.h` — vec2/vec3/vec4, quat, mat4, common math functions
- `Curve.h` — Float/vector animation curves (sampled by time)
- `BVH.h`, `BVHBuild.cpp` — Bounding volume hierarchy for spatial queries
- `Rect2d.h` — 2D axis-aligned rectangle

### Mesh Building
- `MeshBuilder.h/cpp` — Runtime debug/procedural mesh generation
- `MeshBuilderImpl.h` — Internal vertex/index management

### UI & Tools
- `MyImguiLib.h/cpp` — ImGui helper wrappers
- `PropertyEd.h/cpp` — Generic property grid (reads `PropertyInfoList`)
- `PropertyPtr.h/cpp` — Property binding pointers
- `CurveEditorImgui.h/cpp` — In-editor curve visualization and editing
- `ConsoleCmdGroup.h` — Console command registration
- `NodeMenuItem.h` — Contextual menu item generation
- `BoolButton.h` — Toggle button widget
- `MulticastDelegate.h` — Multicast event/delegate system

### Utilities
- `Files.h/cpp` — File I/O (read, write, exists, enumerate)
- `Util.h/cpp` — General utilities
- `Bytepacker.h` — Bit/byte packing helpers
- `Range.h` — Integer/float range type
- `ScopedBoolean.h` — RAII bool scope guard
- `Profilier.h/cpp` — CPU profiling zones
- `Jobs.h/cpp` — Fiber/task-based job system
- `Factory.h`, `FnFactory.h` — Type-keyed factory patterns
- `Config.h/cpp` — Runtime config variables (`vars.txt` / `init.txt`)
- `Log.h` — Logging macros

## Key Concepts

### Reflection System
Every game class inherits from `ClassBase` and uses `CLASS_BODY()`:
```cpp
class Foo : public ClassBase {
    CLASS_BODY(Foo);
    REFLECT() float speed = 1.f; REF
};
```
- `CLASS_BODY(T)` generates: `StaticType`, `get_type()`, `get_props()`, factory registration
- `REFLECT()` / `REF` marks properties; code-gen (`Scripts/`) produces `MEGA.cpp`
- `PropertyInfo` holds: name, byte offset, `core_type_id`, flags, range hints
- Flags: `PROP_EDITABLE`, `PROP_SERIALIZE`, `PROP_EDITOR_ONLY`, `PROP_INSTANCE_EDITABLE`
- `is_a<T>()` — Safe runtime type check; `cast_to<T>()` — Checked downcast

### Serialization
`Serializer` is abstract; `SerializerJson` / `SerializerBinary` are the concrete implementations.
- `serialize(Serializer&)` virtual method on `ClassBase` drives save/load
- Use `DictParser` / `DictWriter` for human-readable config files (`vars.txt`, `init.txt`)

### Handle System (`Handle.h`)
Type-safe integer handles used instead of raw pointers for render objects, assets, etc.
- Prevents dangling pointer bugs across frame boundaries
- `handle<T>` — Wraps a typed integer ID; invalid = 0

### Job System (`Jobs.h`)
Fiber-based parallel task scheduling.
- Submit tasks with `Jobs::run(fn)` / `Jobs::run_and_wait(fn_list)`
- Used by renderer for parallel culling and by asset compiler for parallel mesh processing

### Config System (`Config.h`)
Runtime configuration variables backed by `vars.txt` / `init.txt`.
- Variables declared with `CONFIG_VAR(type, name, default)`
- Read at startup; can be overridden per-level

### StringName (`StringName.h`)
Interned string type. All `StringName` instances with equal content share a pointer — equality is pointer comparison.
Use for identifiers (bone names, tag names, asset names) where string equality is performance-critical.
