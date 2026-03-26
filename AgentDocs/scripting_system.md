# Scripting System — Agent Notes

## Overview

The engine embeds Lua via a `ScriptManager` singleton. C++ classes marked `scriptable`
in their `CLASS_BODY` macro get a generated `ScriptImpl_ClassName` subclass that
overrides each virtual function to first look for a Lua implementation on the instance
table before falling back to the C++ base.

## Key Files

| File | Role |
|------|------|
| `Source/Scripting/ScriptManager.h/.cpp` | Lua state management, class loading, reload |
| `Source/Scripting/ScriptFunctionCodegen.h/.cpp` | Push/get helpers; `ScriptImpl` template alloc |
| `Source/Framework/ClassBase.h/.cpp` | ClassBase; lazy Lua table (`lua_table_id`) |
| `Source/Framework/ClassTypeInfo.h` | Type metadata; `lua_prototype_index_table` |
| `Source/.generated/MEGA.gen.cpp` | Code-generated bindings and `ScriptImpl_*` classes |
| `Source/Testheader.h` | `InterfaceClass` — the lightweight scriptable test class |

## Scriptable Class Lifecycle

1. **C++ side** — class declared with `CLASS_BODY(Foo, scriptable)`.
   Codegen produces `ScriptImpl_Foo` and a non-null `scriptable_allocate` in `Foo::StaticType`.

2. **Lua side** — user writes:
   ```lua
   ---@class MyFoo : Foo
   MyFoo = {}
   function MyFoo:some_virtual() ... end
   ```

3. **Loading** — `ScriptManager::reload_from_content()`:
   - `ScriptLoadingUtil::parse_text()` extracts `---@class` annotations.
   - For each class with a parent, creates a `LuaClassTypeInfo`.
   - `set_superclass()` finds the C++ parent via `ClassBase::find_class()` and copies
     its `lua_prototype_index_table` (requires the parent prototype to already be
     initialised — see *Test Setup* below).
   - Executes the Lua source (defines globals).

4. **Flush** — `ScriptManager::check_for_reload()` → `LuaClassTypeInfo::init_lua_type()`:
   - Grabs the global Lua table, stores it as `template_lua_table` in the registry.
   - Sets the global to nil.
   - `init_this_class_type(this)` — builds a metatable `{__index = {all C++ methods}}`.
   - `set_class_type_global(this)` — exposes the class as a Lua global again (for
     static methods / type inspection from Lua).

5. **Allocation** — `ScriptManager::allocate_class("MyFoo")` → `LuaClassTypeInfo::lua_class_alloc()`:
   - Calls parent's `scriptable_allocate` → creates `ScriptImpl_Foo`.
   - Lazily creates a Lua table for the instance (`get_table_registry_id()`).
     Table gets the metatable set by `init_this_class_type`, so C++ methods are
     reachable via `self:method()`.
   - Shallow-copies the template table (Lua function overrides + declared properties)
     into the instance table.

6. **Dispatch** — `ScriptImpl_Foo::some_virtual()`:
   - Pushes instance table, raw-gets `"some_virtual"`.
   - If it's a function → pcall with `self` + args; throws `LuaRuntimeError` on failure.
   - If not a function → calls the C++ base.

## `lua_table_id` and `lua_prototype_index_table`

- **`ClassBase::lua_table_id`** — lazily-created Lua registry reference to the table
  wrapping a specific C++ object.  Cleared by `free_table_registry_id()`.
- **`ClassTypeInfo::lua_prototype_index_table`** — Lua registry reference to the
  metatable `{__index = {methods}}` for a *class type* (not an instance).  Set by
  `init_this_class_type()`.

`ClassBase::~ClassBase()` calls `free_table_registry_id()`, so objects must be deleted
before the `ScriptManager` (whose `~ScriptManager()` calls `lua_close()`).

## Test Infrastructure (`LuaScriptableClassTest`)

Located in `Source/UnitTests/scripting_test.cpp` — Part 4.

**Required one-time init** (in `SetUpTestSuite`):
```cpp
ClassBase::init_classes_startup();   // sets registry.initialzed = true
```

**Required per-test init** (in `SetUp`):
```cpp
ClassBase::init_class_info_for_script();
// Calls init_this_class_type() + set_class_type_global() for every registered
// C++ class, setting lua_prototype_index_table and lua_table_id on their
// ClassTypeInfo static objects.  Without this, set_superclass() asserts.
```

**Required per-test teardown** (in `TearDown`), in order:
1. `delete` all allocated `ClassBase*` instances (their `~ClassBase()` frees Lua tables
   while the state is still open).
2. For each registered Lua class: `free_table_registry_id()` + `unregister_class()`.
3. `post_changes_class_init()`.
4. Iterate all C++ classes via `ClassBase::get_subclasses(&ClassBase::StaticType)` and
   call `free_table_registry_id()` on each — clears `lua_table_id` set by
   `init_class_info_for_script()` before `lua_close()` runs.
5. `delete sm; ScriptManager::inst = nullptr;`

**`InterfaceClass`** (`Source/Testheader.h`) is the engine's built-in scriptable test
class:
- `virtual int get_value(std::string str)` — returns int, dispatched to Lua.
- `virtual void buzzer()` — void, dispatched to Lua.
- `int self_func()` / `void set_var(int)` — non-virtual C++ methods reachable via `self:`.
- `int variable` / `std::string myStr` — C++ member state.

## Fixture Files

| File | Used by |
|------|---------|
| `TestFiles/Scripting/test_basic.lua` | `ScriptManagerTest.LoadFromFixtureFile` |
| `TestFiles/Scripting/test_class_no_inherit.lua` | `ParseText.FixtureFileTestClassNoInherit` |
| `TestFiles/Scripting/test_lua_scriptable.lua` | `LuaScriptableClassTest.FixtureFile_OverridesAndSelfAccess` |
