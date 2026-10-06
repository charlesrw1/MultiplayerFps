# Engine Overview

This is a C++ game engine. It targets Windows, VS2026, OpenGL, and SDL2. It uses `.vcxproj`/`.sln` files directly, with no CMake. It embeds Lua and PhysX, and includes a hand-rolled 2D UI and editor-time asset compilation.

---

## Project structure

```
CsRemake/
  Source/             — all C++ source
    App/              — App.vcxproj (thin main.cpp, links CsRemake.lib)
    UnitTests/        — GTest unit tests
    IntegrationTests/ — C++ coroutine integration tests
    .generated/       — MEGA.gen.cpp (codegen output, gitignored)
  Data/
    scripts/          — all .lua files (auto-loaded by ScriptManager, recursive)
  Scripts/            — Python codegen, PS1 build scripts
  docs/               — documentation
  TestFiles/          — test inputs/goldens/outputs (partially gitignored)
  EngineVars.ini      — runtime config (loaded at startup)
  Projects/<Name>.ini — per-project config (data dir, app class), loaded on top
  Core.vcxproj    — main library project (all engine source)
```

---

## Build system

- **Solution:** `CsRemake.sln` has these projects: `CsRemake` (a static lib with all engine `.cpp` files), `App` (links the lib), `UnitTests`, `IntegrationTests`, and `External`.
- **Add source:** add `<ClCompile>` and `<ClInclude>` entries to `Core.vcxproj`.
- **Codegen:** run `py Scripts/codegen.py`. It parses the C++ headers and writes the Lua bindings to `Source/.generated/MEGA.gen.cpp`. Rerun it after you add or remove a `REF` or `CLASS_BODY`.
- **Unit tests:** run `powershell Scripts/build_and_test.ps1 -Configuration Debug -Platform x64`.
- **Integration tests:** run `powershell Scripts/integration_test.ps1 -Config Debug`. See [[testing]].
- **Clang-format:** run `powershell Scripts/clang-format-all.ps1`. Run this before every commit.

---

## Configuration (Config_Var / Engine_Cmd)

The engine uses Quake/Source-style cvars and commands. See `Source/Framework/Config.h`. At init, the engine executes `EngineVars.ini`, the per-project `.ini`, and `init.txt`, in that order. Write any var sets or commands into these files.

Key cvars:
- `g_application_class` — the Lua class name to instantiate as the Application.
- `g_entry_level` — the first level to load, by asset name. Startup runs `map <g_entry_level>`.
- `g_project_base` — the data directory. Use a relative path, such as `"Data"`, not an absolute path.
- `r.debug_mode` — debug output such as normals, object id, and material id. Values are 0–9. See `Draw.cpp` for the numbers.
- `g_draw_physx_scene` — bitmask: 1 = collision shapes, 2 = joints, 3 = both.
- `g_slomo` — time speed multiplier.

Launch flags:
- `--editor` — opens the editor UI. The default is game mode. This flag is implicit when you use `--tests editor`.
- `--project <name|path>` — the per-project `.ini` to load. The default is the `startup_project` cvar, set in `EngineVars.ini`. A bare name, such as `DevEngine`, resolves to `Projects/DevEngine.ini`. This flag is ignored for `--tests` runs.

`EngineVars.ini` has three sections: `[app]` runs on a normal launch, `[game_test]` runs on `App.exe --tests game`, and `[editor_test]` runs on `--tests editor`. The sections are independent. The engine selects exactly one. See [[testing]].

`[app]` also loads a per-project `.ini`, such as `Projects/DevEngine.ini`, on top of the base config. Project vars win over the base config. This is where per-project vars live, such as `g_project_base`, `g_application_class`, `g_project_name`, and `g_entry_level`. `game_test` and `editor_test` never load a project file. Instead, they set these vars directly in their own section.

Commands:
- `map <mapname>` — opens a map for play.
- `start_ed <AssetType> <file>` — opens an asset editor. Asset types include `Map`, `AnimGraph`, `DataClass`, and `Model`. See `AssetMetadata` in `AssetRegistry.h` for the full list. An empty filename creates a new document.
- `ot <scale> <alpha> <mip> <texture>` — overlays a texture for debugging. `cot` clears the overlay. Internal targets include `_gbuffer0`, `_gbuffer1`, `_gbuffer2`, and `_csm_shadow`. Example: `ot 1 1 0 _gbuffer0`.

You can also open an editor by double-clicking an asset in the AssetBrowser imgui window, if that asset type has an `IEditorTool`.

---

## AssetBrowser

This is the main level-creation tool. Double-click an asset to open its editor. Drag an asset onto the viewport to instantiate it in the level editor. Drag an asset onto an **AssetPtr** property field — shown as a solid colored rect in the property grid — to set that property. This auto-serializes and loads the asset.

---

## ClassBase

This is the engine's global class system and registry, similar to Unreal's UObjects. See `ClassBase.h`, `ReflectionProp.h`, and `PropertyEd.h`.

A reflectable C++ type inherits `ClassBase` and declares one of these:

```cpp
CLASS_BODY(ClassName);              // non-scriptable
CLASS_BODY(ClassName, scriptable);  // Lua can inherit
```

A property list looks like this:

```cpp
static const PropertyInfoList* get_props() {
    START_PROPS(ClassName)
        // REG_FLOAT(...), REG_INT(...), REG_BOOL(...), REG_ASSETPTR(...)
    END_PROPS(ClassName)
}
```

`PROP_SERIALIZE`, `PROP_DEFAULT`, and `PROP_EDITABLE` control serialization and the editor UI. See `ReflectionMacros.h` for the full set of `REG_*` macros.

A `REF` annotation on a method or field exposes it to Lua. Codegen produces the bindings in `MEGA.gen.cpp`. For `scriptable` classes, codegen also emits a `ScriptImpl_ClassName` class. This class overrides every virtual method to dispatch to Lua first. See [[scripting_system]].

---

## Entity-Component System

- **Entity** — a node in the world. It has a transform (world-space and local-space position, rotation, and scale), and it can have children.
- **Component / EntityComponent** — attached to an entity. Its lifecycle needs these opt-ins, set in the constructor:
  - `set_call_init_in_editor(true)` — required for the engine to call `start()` at all.
  - `editor_start()` — the engine always calls this in the editor, regardless of the setting above.
  - `set_ticking(true)` — required for the engine to call `update()` each frame.

  Built-in components:
  - `MeshComponent` — renders a `.cmdl` file.
  - `CapsuleComponent` / `BoxComponent` / `SphereComponent` — physics bodies. These extend `PhysicsBody`.
  - `CharacterMovementComponent` — a collide-and-slide wrapper. See Physics, below.
  - `CameraComponent` — marks an entity as the active camera.
  - `LightComponents` — point, spot, and directional lights.
- **BaseUpdater** — the base class of both Entity and Component. Call `get_owner()` to get its Entity.
- **Level** — the current collection of entities. Access it with `eng->get_level()`.

---

## Application

See [[Source/GameEnginePublic.h]].

`Application` inherits `ClassBase` and declares `CLASS_BODY(Application, scriptable)`. The engine instantiates the class named by `g_application_class`. Its lifecycle runs in this order:

```
start() → on_map_changed() → [per frame: pre_update(), update()] → stop()
```

`on_map_changed()` also fires on every later level change.

The engine CLI launches integration tests directly, with `App.exe --tests <mode>`. They no longer go through the Application class. See [[testing]].

A Lua subclass looks like this:

```lua
---@class FpsGameApplication : Application
FpsGameApplication = {}
function FpsGameApplication:start() ... end
function FpsGameApplication:on_map_changed() ... end
```

---

## GameplayStatic

See [[Source/Game/Entities/Player.h]].

This is a static utility class, exposed to Lua:
- `spawn_entity()` — returns an Entity.
- `find_by_name(name)` — returns an Entity, or nil if none matches.
- `change_level(levelname)` — changes the level asynchronously, and triggers `on_map_changed()`.
- `get_current_level_name()` — returns the current level name, as a string.
- `get_dt()` — returns the frame delta time, as a float.
- `cast_ray(start, end, channel_mask, ignore_body)` — returns `{hit, what, pos, normal}`.

---

## Physics

See [[Source/Physics/Physics2.h]].

This is a wrapper around PhysX. Physics bodies are components: `CapsuleComponent`, `BoxComponent`, `SphereComponent`, and `MeshColliderComponent`.

- `set_body_type(BodyType)` — sets the body type. Values are `Static`, `Kinematic`, or `Dynamic` (in Lua: `BODYTYPE_*`).
- Triggers: call `set_is_trigger(true)` and `set_send_overlap(true)`.

**CharacterMovementComponent** (`Source/Game/Entities/CharacterController.h`) wraps a `CharacterController`. It is not itself a physics body:
- It requires a separate body, set with `set_physics_body(body)`.
- `move({x,y,z}, dt, min_dist)` — runs one frame of collide-and-slide movement.
- After you call `move()`, you must call `entity:set_ws_position(move:get_position())` to sync the entity's transform. This step is awkward, but intentional.
- `is_touching_down()` — checks whether the character is on the ground.
- Gravity is manual: apply it yourself, for example with `gravity_vel -= 9.8 * dt`.

---

## Rendering

**RenderWindow** ([[Source/Render/RenderWindow.h]]) holds a 2D draw list, built from `MeshBuilder` and `UIDrawCmdUnion`s. `UiSystem` uses it.

Shape types:
- `RectangleShape` — a filled rectangle, with an optional texture.
- `TextShape` — text drawn with a `GuiFont`. Its `text` field is a `std::string_view`, so the caller's string must outlive the draw call.
- `LineShape` — a thin quad, drawn along a direction.
- `CircleShape` — filled, as a triangle fan, or drawn as an outline, as line segments.

**MeshBuilder** ([[Source/Framework/MeshBuilder.h]]) lets you push vertices and indices manually.

**UiSystem** ([[Source/UI/GUISystemPublic.h]]) is a singleton, `UiSystem::inst`. It owns the main `RenderWindow window`.

**Canvas** is a Lua static class for basic drawing:
- `draw_rect(lRect, Texture*, lColor)`
- `draw_text(string, x, y)`
- `get_window_rect()` — returns an lRect.
- `set_window_capture_mouse(bool)`

**Gui** is a Love2D-style static class for drawing:
- `set_color(r,g,b,a)` — sets the static `current_color`. Values are floats from 0 to 1.
- `rectangle(x,y,w,h)`, `rectangle_outline(x,y,w,h,thickness)`
- `circle(x,y,radius,segments)`, `line(x1,y1,x2,y2,thickness)`
- `image(tex,x,y,w,h)`, `print(text,x,y)`
- `measure_text(text)` — returns an lRect. `get_screen_size()` — returns an lRect.

For materials and master shaders, see [[rendering/materials]]. For decals and POM, see [[rendering/decals]].

---

## Lua scripting

The engine auto-loads every file under `Data/scripts/**/*.lua` at startup, through `ScriptManager::load_script_files()`. There is no require or import step.

All Lua code shares one global namespace. By convention, declare classes with `---@class MyClass : ParentClass`.

Lua type wrappers:
- `lRect` — wraps `Rect2d`. Fields: `.x .y .w .h`.
- `lColor` — wraps `Color32`. Fields: `.r .g .b .a`.
- `vec3` — a table `{x=,y=,z=}`, mapped to `glm::vec3` by codegen.
- `lInput` — a wrapper around the `Input` static class.

For the full lifecycle and dispatch model, see [[scripting_system]].

---

## Input (`Source/Input/InputSystem.h`)

`lInput`, in Lua:
- `is_key_down(SDL_SCANCODE_*)` — true while the key is held.
- `was_key_pressed(SDL_SCANCODE_*)` — true if the key was pressed this frame.
- `get_mouse_delta()` — returns an `{x, y}` ivec2 table. There are no separate x/y methods.
- `was_mouse_pressed(button)` — button values are 0 for left, 1 for right, 2 for middle.
- The mouse delta is meaningful only while `Canvas.set_window_capture_mouse(true)` is active.

---

## Asset System

- `Model.load("filename.cmdl")` — see [[rendering/model_importing]].
- `MaterialInstance.load("filename.mi")` — see [[rendering/materials]].
- `Texture` — usually loaded through a material. Lua code rarely loads a texture directly.
- Assets are refcounted through `IAsset`. Editor builds support hot-reload.

---

## Key workflows

### Adding a new scriptable C++ class

1. Create a `.h`/`.cpp` pair under the appropriate `Source/` subdirectory.
2. Add `CLASS_BODY(MyClass)` and any `REF` methods.
3. Add `<ClCompile>`/`<ClInclude>` entries to `Core.vcxproj`.
4. Run `py Scripts/codegen.py`.
5. Build the project and verify the result.

### Running tests

See [[testing]].

## See Also

- [[tooling/level_editor]]
- [[tooling/cscli]]
- [[tooling/managing_projects]]
- [[ui/rmlui_agent_guide]]
- [[navigation]]
- [[debugging/crash_dumps]]
