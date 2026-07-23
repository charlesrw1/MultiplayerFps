# Framework Module

Core engine infrastructure: reflection, serialization, memory, collections, math, job system, UI utilities, and logging. Glob the directory for files; this doc covers concepts, not file lists.

## Reflection

Every game class inherits `ClassBase` with `CLASS_BODY()`:
```cpp
class Foo : public ClassBase {
    CLASS_BODY(Foo);
    REFLECT() float speed = 1.f; REF
};
```
- `CLASS_BODY(T)` generates `StaticType`, `get_type()`, `get_props()`, and factory registration.
- `REFLECT()` / `REF` mark properties; the python codegen in `Scripts/` parses headers and emits `Source/MEGA.cpp`. If reflection is missing at runtime, MEGA is stale — re-run codegen.
- `PropertyInfo` holds name, byte offset, `core_type_id`, flags, range hints.
- Flags: `PROP_EDITABLE`, `PROP_SERIALIZE`, `PROP_EDITOR_ONLY`, `PROP_INSTANCE_EDITABLE`.
- `is_a<T>()` for safe runtime checks; `cast_to<T>()` for checked downcasts.
- Container/struct reflection: `STRUCT_BODY()` for plain data, plus dedicated headers for arrays, vectors, `unique_ptr`, and enums.

## Serialization

`Serializer` is abstract; JSON and binary backends exist. `ClassBase::serialize(Serializer&)` virtual drives save/load. `DictParser` / `DictWriter` handle the human-readable key/value format used by `EngineVars.ini` and `init.txt`. `SerializedForDiffing` produces stable output for diff tooling.

## Memory

Arena/pool allocators and intrusive free lists are preferred over `new`/`delete` for hot paths and per-frame data. `handle<T>` is a type-safe integer ID (invalid = 0) used instead of raw pointers for render objects and assets — survives across frames where pointers may dangle. `SharedPtr` is the in-house refcounted smart pointer; `opt<T>` is the optional type.

## Collections & Strings

Open-addressing `Hashmap`/`Hashset`, small-buffer-optimized `InlineVec`, and `RingBuffer`. `StringName` is the interned string type: equal content means equal pointer, so equality is a pointer compare — use for bone/tag/asset identifiers in hot paths.

## Math & Geometry

`MathLib.h` provides vec2/3/4, quat, mat4. `Curve` samples float/vector animation curves by time. `BVH` is the spatial-query acceleration structure. `Rect2d` is 2D AABB. See [[bike/sign_conventions]] for project-wide axis conventions (+Y up, +Z forward, +X world-right).

## Job System

Fiber-based task scheduling in `Jobs.h`. Submit with `Jobs::run` / `Jobs::run_and_wait`. Used by renderer for parallel culling and by the asset compiler for mesh processing. Tasks may suspend on fiber boundaries — do not hold non-fiber-safe locks across submits.

## Config

`CONFIG_VAR(type, name, default)` declares a runtime variable backed by `EngineVars.ini` / `init.txt`. Read at startup; can be overridden per-level. `g_project_base` must be `"Data"` (relative), not an absolute path, when running game/tests.

## UI / Tools

ImGui wrappers, a generic property grid driven by `PropertyInfoList`, an in-editor curve editor, console-command registration, and a multicast delegate system. `PropertyEd` reads reflection metadata directly — adding a `REFLECT()` field is enough to surface it in the editor.

## Misc Utilities

File I/O, bit/byte packing, integer/float ranges, RAII bool scope guard, CPU profiling zones, type-keyed factories, and logging macros.
