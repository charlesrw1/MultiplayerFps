# Architectural Debt — Needed Changes

Last updated: 2026-03-27

Issues observed during the shooter game implementation session. Ordered roughly by severity / impact.

---

## C++ Issues

### 1. `CharacterMovementComponent` — Split API for what should be one thing

**File:** `Source/Game/Entities/CharacterController.h`

`CharacterMovementComponent` is not a physics body but requires a separate `PhysicsBody` to be passed via `set_physics_body(body)`. The caller must also manually call `entity:set_ws_position(move:get_position())` after every `move()` to sync the entity transform.

**Why it's bad:** Every consumer has to know the two-step setup and the post-move sync dance. It's easy to forget the sync, causing silent desync between the physics position and the rendered entity.

**Suggested fix:** Either absorb the physics body creation into `CharacterMovementComponent::start()` (it knows its entity, it can create the capsule), or make `move()` automatically sync the entity position.

---

### 2. `TextShape.text` is `std::string_view` — latent dangling reference

**File:** `Source/Render/RenderWindow.h`

`TextShape.text` is `std::string_view`. If the caller passes a temporary string (e.g. result of `std::to_string()` or a Lua string that gets GC'd), the view dangles by the time the draw list is consumed.

**Why it's bad:** Silent UB. Works most of the time because the stack frame survives to the draw call, but breaks under any async or deferred rendering path.

**Suggested fix:** Change `TextShape.text` to `std::string`.

---

### 3. `MaterialManagerLocal::create_dynmaic_material_unsafier()` — typo in public API

**File:** Material system

Public-facing method name has a typo ("dynmaic", "unsafier"). Since it's referenced from codegen/Lua this is now a stable ABI concern — renaming it would require a codegen rerun and all Lua call sites to update.

**Suggested fix:** Rename to `create_dynamic_material` (drop "unsafier" which conveys nothing useful), update all call sites, rerun codegen.

---

### 4. `Gui` has no color push/pop stack

**File:** `Source/UI/Gui.h/.cpp`

`Gui::set_color()` writes to a `static Color32 current_color`. There is no push/pop mechanism, so calling a helper function that sets its own color will clobber the caller's color state.

**Why it's bad:** Any nesting of drawing calls (e.g. HUD draws a widget that draws a sub-component) silently corrupts color state.

**Suggested fix:** Add `push_color()` / `pop_color()` using a `static std::vector<Color32>` stack.

---

### 5. `pre_render_update()` for material GPU uploads is commented out

The material system's GPU upload tick (`pre_render_update()`) is commented out (noted in `material_system.md`). This means dynamic material parameter updates may not flush to the GPU each frame.

**Suggested fix:** Identify why it was commented out, fix the underlying issue, and restore the call.

---

### 6. No `on_before_map_change()` lifecycle hook

**File:** `Source/GameEnginePublic.h`

Application only has `on_map_changed()` (after load). There is no callback before the level unloads, making it impossible to cleanly tear down per-level state (clear enemy lists, unregister callbacks, stop coroutines) before the level is destroyed.

**Why it's bad:** Game code has to use `on_map_changed()` as both setup AND cleanup, leading to reset-all-state anti-patterns at the top of `on_map_changed()`.

**Suggested fix:** Add `virtual void on_before_map_change() {}` to `Application`, called by `LevelManager` before unloading.

---

## Lua / Scripting Issues

### 7. Global Lua namespace — no module isolation

**File:** All `.lua` files under `Data/scripts/`

All scripts share a single global Lua state. Any global variable defined in any script is visible everywhere. Naming collisions between modules are silently resolved by load order.

**Why it's bad:** Adding a second game mode (or even a second enemy type) that uses a global of the same name as an existing script will silently break things. Integration tests running against live game code can be polluted by game state left from prior tests.

**Suggested fix:** Adopt a convention (enforced by comment/doc) that all module-level state is stored in a named table (e.g. `ShooterGame = ShooterGame or {}`). Long-term: migrate to proper Lua modules with `local M = {}; return M` and explicit `require`.

---

### 8. Two parallel Application subclasses with no shared base

`Data/scripts/application.lua` contains `MyApp : Application` (the old engine tester app). `Data/scripts/shooter/app.lua` contains `FpsGameApplication : Application`. They share no Lua base class for common functionality (e.g. level management, test runner wiring).

**Suggested fix:** Extract a `BaseGameApplication` Lua class with shared `update()` logic (test runner tick, etc.) that both apps can inherit from.

---

### 9. `import_scripts.lua` and `myscript.lua` — God Files

Scripts that predate the shooter game contain large amounts of unrelated logic mixed together. `myscript.lua` (or equivalent) contains editor testing, rendering tests, and gameplay logic in one file.

**Suggested fix:** Break into single-responsibility files. `ScriptManager` auto-loads all files under `Data/scripts/` recursively, so there is no cost to splitting.

---

## Configuration Issues

### 10. `vars.txt` has machine-specific absolute path checked in

**File:** `vars.txt` line 27

```
g_project_base "D:/Data"
```

This is a machine-specific absolute path that will fail on any other machine. The correct value (`"Data"`) is commented out on line 26.

**Suggested fix:** Flip the comments — make `g_project_base "Data"` the active line, and put the absolute path variant in a `.gitignore`d `vars.local.txt` that overrides on individual machines. Or enforce in CI that `vars.txt` does not contain absolute paths.

---

## Known Pre-Existing Test Failures

These are not newly introduced but are worth tracking:

- `ScriptManagerTest.SyntaxErrorLeavesCleanStack` — known flaky, likely a test isolation issue
- `ClassBaseRegistryTest` suite setup — known failure, blocks some unit test runs
