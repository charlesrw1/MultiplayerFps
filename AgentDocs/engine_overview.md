# Engine Overview — Agent Notes

Last updated: 2026-03-27 (shooter game implementation session)

## What This Is

A custom C++ game engine targeting Windows, Visual Studio 2019, OpenGL. No CMake — uses `.vcxproj` / `.sln` directly. Embeds Lua for game logic. Physics via PhysX. UI via a hand-rolled 2D renderer. Asset pipeline compiles models/materials at editor time.

---

## Project Structure

```
CsRemake/
  Source/           — all C++ source
    App/            — App.vcxproj (thin main.cpp, links CsRemake.lib)
    UnitTests/      — GTest unit tests
    IntegrationTests/ — C++ coroutine integration tests
    .generated/     — MEGA.gen.cpp (codegen output, gitignored)
  Data/             — runtime assets, Lua scripts
    scripts/        — all .lua files (auto-loaded by ScriptManager)
    scripts/shooter/— shooter game scripts (added 2026-03-27)
  Scripts/          — Python codegen, PS1 build scripts
  AgentDocs/        — this directory
  TestFiles/        — test inputs/goldens/outputs (partially gitignored)
  vars.txt          — runtime config (loaded at startup)
  test_game_vars.txt— config for integration test runs
  CsRemake.vcxproj  — main library project (all engine source)
```

---

## Build System

- **Solution**: `CsRemake.sln` containing `CsRemake`, `App`, `UnitTests`, `IntegrationTests`, `External`
- **CsRemake** builds as a static lib; App links it. All engine .cpp files live here.
- **Adding source**: add `<ClCompile>` + `<ClInclude>` entries to `CsRemake.vcxproj`
- **Codegen**: `py Scripts/codegen.py` — reads all C++ headers, generates `Source/.generated/MEGA.gen.cpp` with Lua bindings. Must be rerun after adding/removing `REF`/`CLASS_BODY` annotations.
- **Unit tests**: `powershell Scripts/build_and_test.ps1 -Configuration Debug -Platform x64`
- **Integration tests**: `powershell Scripts/integration_test.ps1 -Config Debug`
- **Clang-format**: `powershell Scripts/clang-format-all.ps1` — run before every commit

---

## Core Systems

### Config / CVar (`Source/Framework/Config.h`)

`ConfigVar name("key", "default", CVAR_TYPE, "description")` declares a global config variable. Loaded from `vars.txt` at startup (via `Cmd_Manager`). `test_game_vars.txt` overrides for test runs.

Key cvars:
- `g_application_class` — Lua class name to instantiate as the Application
- `g_entry_level` — first level to load
- `g_project_base` — base data directory. **Use `"Data"` (relative) not `"D:/Data"` (machine-specific)**
- `g_run_tests` — if true, call `app->run_integration_tests()` after startup
- `is_editor_app` — if true, open editor UI

### ClassBase / Codegen (`Source/Framework/ClassBase.h`)

All reflectable C++ objects inherit `ClassBase`. Each class declares itself with:
```cpp
CLASS_BODY(ClassName);           // non-scriptable
CLASS_BODY(ClassName, scriptable); // Lua can inherit from it
```

`REF` before a method/field marks it for codegen → Lua binding. Running `codegen.py` produces `MEGA.gen.cpp` with Lua bindings for all REF-annotated symbols.

For scriptable classes, codegen additionally produces a `ScriptImpl_ClassName` subclass that overrides every virtual and dispatches to Lua first. See `AgentDocs/scripting_system.md`.

### Entity-Component System (`Source/Game/`)

- **Entity** — a node in the world. Has a transform (world-space + local-space position/rotation/scale). Can have child entities.
- **Component** / **EntityComponent** — attached to an entity. Override `start()` (once) and `update()` (every frame if `set_ticking(true)`). Key classes:
  - `MeshComponent` — renders a `.cmdl` model
  - `CapsuleComponent`, `BoxComponent`, `SphereComponent` — physics bodies (extend `PhysicsBody`)
  - `CharacterMovementComponent` — collide+slide movement wrapper (see Physics section)
  - `CameraComponent` — marks an entity as the active camera
  - `LightComponents` — point/spot/directional lights
- **BaseUpdater** — base for both Entity and Component. Has `get_owner()` → Entity.
- **Level** — the currently loaded collection of entities. `eng->get_level()`.

### Application (`Source/GameEnginePublic.h`)

`Application : public ClassBase` with `CLASS_BODY(Application, scriptable)`. The engine creates one instance of the class named by `g_application_class`. Lifecycle:

```
start() → on_map_changed() → [per frame: pre_update(), update()] → stop()
on_map_changed() is also called on subsequent level changes
```

After `start()`, if `g_run_tests` is true, `run_integration_tests()` is called.

Game code inherits Application in Lua:
```lua
---@class FpsGameApplication : Application
FpsGameApplication = {}
function FpsGameApplication:start() ... end
function FpsGameApplication:on_map_changed() ... end
```

### GameplayStatic (`Source/Game/Entities/Player.h`)

Static utility class exposed to Lua. Key methods:
- `spawn_entity()` → Entity
- `find_by_name(name)` → Entity or nil
- `change_level(levelname)` — async level change, triggers `on_map_changed()`
- `get_current_level_name()` → string
- `get_dt()` → float (frame delta time)
- `cast_ray(start, end, channel_mask, ignore_body)` → HitResult `{hit, what, pos, normal}`

### Physics (`Source/Physics/Physics2.h`)

Wraps PhysX. Physics bodies are Components:
- `CapsuleComponent`, `BoxComponent`, `SphereComponent`, `MeshColliderComponent`
- `set_is_static(bool)` — static vs dynamic
- `set_is_simulating(bool)` — kinematic vs simulated
- Triggers: `set_is_trigger(true)` + `add_triggered_callback(IPhysicsEventCallback*)`

**CharacterMovementComponent** (`Source/Game/Entities/CharacterController.h`) — not a physics body, but wraps a `CharacterController`:
- Requires a separate physics body passed via `set_physics_body(body)`
- `move({x,y,z}, dt, min_dist)` — collide+slide for one frame
- After `move()`, caller must manually call `entity:set_ws_position(move:get_position())` to sync the entity transform. **This is intentional but awkward.**
- `is_touching_down()` — ground check
- Gravity must be simulated manually in Lua/C++ (accumulate `gravity_vel -= 9.8 * dt`)

### Rendering

**RenderWindow** (`Source/Render/RenderWindow.h`) — a 2D draw list. Holds a `MeshBuilder` and a list of `UIDrawCmdUnion` commands. Used by UiSystem for all 2D drawing.

Shape types:
- `RectangleShape` — filled rect, optional texture
- `TextShape` — text using a `GuiFont`; **`text` field is `std::string_view`** (keep source string alive during draw call)
- `LineShape` — thin quad aligned to line direction
- `CircleShape` — filled (triangle fan) or outline (line segments)

**MeshBuilder** (`Source/Framework/MeshBuilder.h`) — push vertices/indices manually. Key methods:
- `Push2dQuad(pos, size, uv, uv_size, color)`
- `PushLine(start, end, color)`
- `Push2dCircle(center, radius, segments, color)` — triangle fan, segments guard

**UiSystem** (`Source/UI/GUISystemPublic.h`) — singleton (`UiSystem::inst`). Owns the main `RenderWindow window`. Called by all UI drawing code.

**Canvas** — Lua-exposed static class for basic drawing:
- `draw_rect(lRect, Texture*, lColor)`
- `draw_text(string, x, y)`
- `get_window_rect()` → lRect
- `set_window_capture_mouse(bool)`

**Gui** — new Love2D-style static drawing class (added 2026-03-27):
- `set_color(r,g,b,a)` — sets static `current_color` (float 0-1)
- `rectangle(x,y,w,h)`, `rectangle_outline(x,y,w,h,thickness)`
- `circle(x,y,radius,segments)`, `line(x1,y1,x2,y2,thickness)`
- `image(tex,x,y,w,h)`, `print(text,x,y)`
- `measure_text(text)` → lRect, `get_screen_size()` → lRect

### Lua Scripting

All `.lua` files under `Data/scripts/` (recursively) are auto-loaded at startup by `ScriptManager::load_script_files()`. No explicit require/import needed.

All scripts share one global Lua namespace. Convention: define classes with `---@class MyClass : ParentClass` annotations.

**Type wrappers for Lua:**
- `lRect` — wrapper for `Rect2d`, has `.x .y .w .h`
- `lColor` — `Color32` wrapper, has `.r .g .b .a`
- `vec3` tables `{x=,y=,z=}` map to `glm::vec3` via codegen
- `lInput` — Lua wrapper for `Input` static class

**LuaTestRunner** — C++ static class exposed to Lua (added 2026-03-27):
- `LuaTestRunner.finish(pass, fail, failures_string)` — writes JUnit XML to `TestFiles/integration_lua_results.xml` and calls `Quit()`

### Input (`Source/Input/InputSystem.h`)

`lInput` in Lua:
- `is_key_down(SDL_SCANCODE_*)` — held
- `was_key_pressed(SDL_SCANCODE_*)` — pressed this frame
- `get_mouse_delta()` → `{x, y}` (ivec2 table, NOT separate x/y methods)
- `was_mouse_pressed(button)` — 0=left, 1=right, 2=middle
- Mouse delta only meaningful when `Canvas.set_window_capture_mouse(true)` is active

### Asset System

- `Model.load("filename.cmdl")` — loads a compiled model asset
- `MaterialInstance.load("filename.mi")` — loads a material
- `Texture` — loaded as part of material; not typically loaded directly in Lua
- Assets are reference-counted via `IAsset`. Hot-reloading supported in editor builds.

---

## Lua Game Structure (Shooter, added 2026-03-27)

```
Data/scripts/shooter/
  app.lua          — FpsGameApplication : Application
  hud.lua          — HudDrawer : Component
  fp_player.lua    — FpPlayerController : Component
  bullet.lua       — BulletProjectile : Component
  enemy.lua        — EnemyController : Component
  door_trigger.lua — DoorTrigger : Component
```

### Test Framework (in app.lua)

Lua-native coroutine test runner. Tests registered with `add_test(name, fn)`. Runner lives in `update()` loop, yielding seconds between frames:

```lua
add_test("my/test", function()
    GameplayStatic.change_level("demo_level_0.tmap")
    coroutine.yield(1.5)  -- wait 1.5 seconds
    assert(condition, "message")
end)
```

Driven by `FpsGameApplication:run_integration_tests()` (called by engine when `g_run_tests=1`). Results written by `LuaTestRunner.finish()`.

---

## Key Workflows

### Adding a new scriptable C++ class

1. Create `.h`/`.cpp` in appropriate `Source/` subdirectory
2. Add `CLASS_BODY(MyClass)` + `REF` methods
3. Add `<ClCompile>`/`<ClInclude>` to `CsRemake.vcxproj`
4. Run `py Scripts/codegen.py`
5. Build and verify

### Running unit tests

```powershell
powershell Scripts/build_and_test.ps1 -Configuration Debug -Platform x64
```

Pre-existing failures (unrelated to most work):
- `ScriptManagerTest.SyntaxErrorLeavesCleanStack` — known flaky
- `ClassBaseRegistryTest` suite setup — known issue

### Running Lua integration tests

Set `test_game_vars.txt` with `g_run_tests 1`, `g_application_class "FpsGameApplication"`, `g_project_base "Data"`. Then:
```powershell
powershell Scripts/integration_test.ps1 -Config Debug
```

Or build App and launch directly:
```
x64\Debug\App.exe --vars test_game_vars.txt
```
