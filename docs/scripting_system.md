# Scripting System — Agent Notes

## Overview

`ScriptManager` singleton owns one Lua state. C++ classes marked `scriptable` in `CLASS_BODY` get a generated `ScriptImpl_ClassName` subclass that overrides each virtual to look up a Lua impl on the instance table before falling back to C++.

## Key Files

| File | Role |
|------|------|
| `Source/Scripting/ScriptManager.h/.cpp` | Lua state, class loading, reload |
| `Source/Scripting/ScriptFunctionCodegen.h/.cpp` | Push/get helpers; `ScriptImpl` template alloc |
| `Source/Framework/ClassBase.h/.cpp` | ClassBase; lazy Lua table (`lua_table_id`) |
| `Source/Framework/ClassTypeInfo.h` | Type metadata; `lua_prototype_index_table` |
| `Source/.generated/MEGA.gen.cpp` | Generated bindings + `ScriptImpl_*` classes |
| `Source/Testheader.h` | `InterfaceClass` — built-in scriptable test class |

## Scriptable Class Lifecycle

1. **C++** — `CLASS_BODY(Foo, scriptable)`. Codegen emits `ScriptImpl_Foo` and a non-null `scriptable_allocate` in `Foo::StaticType`.

2. **Lua**:
   ```lua
   ---@class MyFoo : Foo
   MyFoo = {}
   function MyFoo:some_virtual() ... end
   ```

3. **Loading** — `ScriptManager::reload_from_content()`:
   - `ScriptLoadingUtil::parse_text()` extracts `---@class` annotations.
   - Each child class becomes a `LuaClassTypeInfo`.
   - `set_superclass()` finds C++ parent via `ClassBase::find_class()` and copies its `lua_prototype_index_table` (parent prototype must already be initialised — see *Test Setup*).
   - Executes Lua source (defines globals).

4. **Flush** — `ScriptManager::check_for_reload()` → `LuaClassTypeInfo::init_lua_type()`:
   - Stores the global Lua table as `template_lua_table` in the registry, sets the global to nil.
   - `init_this_class_type(this)` builds metatable `{__index = {C++ methods}}`.
   - `set_class_type_global(this)` re-exposes the class as a Lua global (for static methods / type inspection).

5. **Allocation** — `ScriptManager::allocate_class("MyFoo")` → `LuaClassTypeInfo::lua_class_alloc()`:
   - Parent's `scriptable_allocate` creates the `ScriptImpl_Foo`.
   - Lazily creates instance Lua table (`get_table_registry_id()`), assigns the metatable from `init_this_class_type` so C++ methods are reachable via `self:method()`.
   - Shallow-copies template table (Lua overrides + declared properties) into instance table.

6. **Dispatch** — `ScriptImpl_Foo::some_virtual()`:
   - Push instance table, raw-get `"some_virtual"`.
   - Function → pcall with `self`+args; throws `LuaRuntimeError` on failure.
   - Non-function → call C++ base.

## `lua_table_id` and `lua_prototype_index_table`

- **`ClassBase::lua_table_id`** — registry ref to the table wrapping a specific C++ instance. Cleared by `free_table_registry_id()`.
- **`ClassTypeInfo::lua_prototype_index_table`** — registry ref to the metatable `{__index = {methods}}` for a *class type* (not instance). Set by `init_this_class_type()`.

`ClassBase::~ClassBase()` calls `free_table_registry_id()`, so objects must be deleted before `~ScriptManager()` runs `lua_close()`.

## Test Infrastructure (`LuaScriptableClassTest`)

`Source/UnitTests/scripting_test.cpp` — Part 4.

**One-time (`SetUpTestSuite`):**
```cpp
ClassBase::init_classes_startup();   // sets registry.initialzed = true
```

**Per-test (`SetUp`):**
```cpp
ClassBase::init_class_info_for_script();
// init_this_class_type() + set_class_type_global() for every registered C++
// class. Without this, set_superclass() asserts.
```

**Per-test (`TearDown`), in order:**
1. `delete` all allocated `ClassBase*` instances (their dtor frees Lua tables while state is still open).
2. For each Lua class: `free_table_registry_id()` + `unregister_class()`.
3. `post_changes_class_init()`.
4. Iterate `ClassBase::get_subclasses(&ClassBase::StaticType)` and `free_table_registry_id()` on each — clears `lua_table_id` set during `init_class_info_for_script()` before `lua_close()`.
5. `delete sm; ScriptManager::inst = nullptr;`

**`InterfaceClass`** (`Source/Testheader.h`) — built-in scriptable test class:
- `virtual int get_value(std::string str)` — dispatched to Lua.
- `virtual void buzzer()` — dispatched to Lua.
- `int self_func()`, `void set_var(int)` — non-virtual C++ methods reachable via `self:`.
- `int variable`, `std::string myStr` — C++ member state.

## Fixture Files

| File | Used by |
|------|---------|
| `TestFiles/Scripting/test_basic.lua` | `ScriptManagerTest.LoadFromFixtureFile` |
| `TestFiles/Scripting/test_class_no_inherit.lua` | `ParseText.FixtureFileTestClassNoInherit` |
| `TestFiles/Scripting/test_lua_scriptable.lua` | `LuaScriptableClassTest.FixtureFile_OverridesAndSelfAccess` |
