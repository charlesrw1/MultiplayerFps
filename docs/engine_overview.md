# Engine Overview

C++ game engine, Windows / VS2019 / OpenGL / SDL2, no CMake (`.vcxproj` / `.sln` direct). Embedded Lua, PhysX, hand-rolled 2D UI, editor-time asset compilation.

---

## Project Structure

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
  vars.txt            — runtime config (loaded at startup)
  CsRemake.vcxproj    — main library project (all engine source)
```

---

## Build System

- **Solution:** `CsRemake.sln` — projects: `CsRemake` (static lib, all engine .cpp), `App` (links lib), `UnitTests`, `IntegrationTests`, `External`.
- **Add source:** add `<ClCompile>` + `<ClInclude>` to `CsRemake.vcxproj`.
- **Codegen:** `py Scripts/codegen.py` — parses C++ headers, writes `Source/.generated/MEGA.gen.cpp` (Lua bindings). Rerun after adding/removing `REF` / `CLASS_BODY`.
- **Unit tests:** `powershell Scripts/build_and_test.ps1 -Configuration Debug -Platform x64`.
- **Integration tests:** `powershell Scripts/integration_test.ps1 -Config Debug` — see [[testing.md]].
- **Clang-format:** `powershell Scripts/clang-format-all.ps1` — required pre-commit.

---

## Configuration (Config_Var / Engine_Cmd)

Quake/Source-style cvars + commands. See `Source/Framework/Config.h`. `vars.txt` and `init.txt` execute in order at engine init — write any var sets or commands.

Key cvars:
- `g_application_class` — Lua class name to instantiate as Application.
- `g_entry_level` — first level (asset name). Startup runs `map <g_entry_level>`.
- `g_project_base` — data dir. **Use `"Data"` (relative), not absolute paths.**

Launch flags:
- `--editor` — open editor UI (default: game mode). Implicit when `--tests editor` is used.

`vars.txt` is sectioned: `[app]` runs on a normal launch, `[game_test]` on `App.exe --tests game`, `[editor_test]` on `--tests editor`. Sections are independent; the engine selects exactly one. See [[testing]].

Commands:
- `map <mapname>` — open for play.
- `start_ed <AssetType> <file>` — open asset editor. Types: `Map`, `AnimGraph`, `DataClass`, `Model`, etc. (see `AssetMetadata` in `AssetRegistry.h`). Empty filename = new doc.
- `ot <scale> <alpha> <mip> <texture>` — overlay a texture for debug. `cot` clears. Internal targets: `_gbuffer0/1/2`, `_csm_shadow`. Example: `ot 1 1 0 _gbuffer0`.

Editors also open by double-clicking an asset in the AssetBrowser imgui window (if the asset type has an `IEditorTool`).

---

## AssetBrowser

Main level-creation tool. Double-click an asset to open its editor. Drag onto viewport to instantiate in level editor. Drag onto **AssetPtr** property fields (solid colored rects in the property grid) to set them — auto-serializes and loads.

---

## ClassBase

Global class system / registry, similar to Unreal UObjects. See `ClassBase.h`, `ReflectionProp.h`, `PropertyEd.h`.

Reflectable C++ types inherit `ClassBase` and declare:

```cpp
CLASS_BODY(ClassName);              // non-scriptable
CLASS_BODY(ClassName, scriptable);  // Lua can inherit
```

Property list:

```cpp
static const PropertyInfoList* get_props() {
    START_PROPS(ClassName)
        // REG_FLOAT(...), REG_INT(...), REG_BOOL(...), REG_ASSETPTR(...)
    END_PROPS(ClassName)
}
```

`PROP_SERIALIZE`, `PROP_DEFAULT`, `PROP_EDITABLE` control serialization/UI. Full `REG_*` set in `ReflectionMacros.h`.

`REF` annotation on a method/field exposes it to Lua (codegen produces bindings in `MEGA.gen.cpp`). For `scriptable` classes codegen also emits `ScriptImpl_ClassName` overriding every virtual to dispatch to Lua first. See [[scripting_system]].

---

## Entity-Component System

- **Entity** — world node. Transform (ws+ls pos/rot/scale). Can have children.
- **Component / EntityComponent** — attached to entity. Lifecycle opt-ins (must be set in constructor):
  - `set_call_init_in_editor(true)` — required for `start()` to be called at all.
  - `editor_start()` — always called in editor regardless of the above.
  - `set_ticking(true)` — required for `update()` to be called each frame.
  Built-ins:
  - `MeshComponent` — renders a `.cmdl`.
  - `CapsuleComponent` / `BoxComponent` / `SphereComponent` — physics bodies (extend `PhysicsBody`).
  - `CharacterMovementComponent` — collide+slide wrapper (see Physics).
  - `CameraComponent` — marks entity as active camera.
  - `LightComponents` — point/spot/directional.
- **BaseUpdater** — base of Entity and Component; `get_owner()` → Entity.
- **Level** — current entity collection. `eng->get_level()`.

---

## Application 

[[Source/GameEnginePublic.h]]

`Application : public ClassBase` with `CLASS_BODY(Application, scriptable)`. Engine instantiates the class named by `g_application_class`. Lifecycle:

```
start() → on_map_changed() → [per frame: pre_update(), update()] → stop()
```

`on_map_changed()` also fires on subsequent level changes.

Integration tests are launched via the engine CLI (`App.exe --tests <mode>`); they no longer go through the Application class. See [[testing]].

Lua subclass:

```lua
---@class FpsGameApplication : Application
FpsGameApplication = {}
function FpsGameApplication:start() ... end
function FpsGameApplication:on_map_changed() ... end
```

---

## GameplayStatic 

[[Source/Game/Entities/Player.h]]

Static utility, exposed to Lua:
- `spawn_entity()` → Entity
- `find_by_name(name)` → Entity or nil
- `change_level(levelname)` — async, triggers `on_map_changed()`
- `get_current_level_name()` → string
- `get_dt()` → float
- `cast_ray(start, end, channel_mask, ignore_body)` → `{hit, what, pos, normal}`

---

## Physics 

[[Source/Physics/Physics2.h]]

PhysX wrapper. Bodies are components: `CapsuleComponent`, `BoxComponent`, `SphereComponent`, `MeshColliderComponent`.

- `set_is_static(bool)` — static vs dynamic.
- `set_is_simulating(bool)` — kinematic vs simulated.
- Triggers: `set_is_trigger(true)` + `add_triggered_callback(IPhysicsEventCallback*)`.

**CharacterMovementComponent** (`Source/Game/Entities/CharacterController.h`) — wraps a `CharacterController`, NOT a physics body:
- Requires separate body via `set_physics_body(body)`.
- `move({x,y,z}, dt, min_dist)` — collide+slide, one frame.
- After `move()`, caller MUST `entity:set_ws_position(move:get_position())` to sync the entity transform. Awkward but intentional.
- `is_touching_down()` — ground check.
- Gravity is manual (`gravity_vel -= 9.8 * dt`).

---

## Rendering

**RenderWindow** [[Source/Render/RenderWindow.h]] — 2D draw list (`MeshBuilder` + `UIDrawCmdUnion`s). Used by UiSystem.

Shape types:
- `RectangleShape` — filled, optional texture.
- `TextShape` — `GuiFont` text. **`text` field is `std::string_view`** — caller string must outlive the draw call.
- `LineShape` — thin quad along direction.
- `CircleShape` — filled (triangle fan) or outline (line segments).

**MeshBuilder** [[Source/Framework/MeshBuilder.h]] — manual vertex/index push:

**UiSystem** [[Source/UI/GUISystemPublic.h]] — singleton (`UiSystem::inst`), owns the main `RenderWindow window`.

**Canvas** — Lua static, basic drawing:
- `draw_rect(lRect, Texture*, lColor)`
- `draw_text(string, x, y)`
- `get_window_rect()` → lRect
- `set_window_capture_mouse(bool)`

**Gui** — Love2D-style static drawing:
- `set_color(r,g,b,a)` — sets static `current_color` (floats 0–1).
- `rectangle(x,y,w,h)`, `rectangle_outline(x,y,w,h,thickness)`
- `circle(x,y,radius,segments)`, `line(x1,y1,x2,y2,thickness)`
- `image(tex,x,y,w,h)`, `print(text,x,y)`
- `measure_text(text)` → lRect, `get_screen_size()` → lRect

Materials/master shaders: [[rendering/materials]]. Decals/POM: [[rendering/decals]].

---

## Lua Scripting

All `Data/scripts/**/*.lua` auto-loaded at startup by `ScriptManager::load_script_files()`. No require/import.

Single shared global namespace. Convention: declare classes via `---@class MyClass : ParentClass`.

Lua type wrappers:
- `lRect` — `Rect2d`. `.x .y .w .h`.
- `lColor` — `Color32`. `.r .g .b .a`.
- `vec3` — table `{x=,y=,z=}`, mapped to `glm::vec3` via codegen.
- `lInput` — wrapper for `Input` static.

Full lifecycle/dispatch: [[scripting_system]].

---

## Input (`Source/Input/InputSystem.h`)

`lInput` (Lua):
- `is_key_down(SDL_SCANCODE_*)` — held.
- `was_key_pressed(SDL_SCANCODE_*)` — pressed this frame.
- `get_mouse_delta()` → `{x, y}` ivec2 table (NOT separate methods).
- `was_mouse_pressed(button)` — 0=left, 1=right, 2=middle.
- Mouse delta only meaningful while `Canvas.set_window_capture_mouse(true)`.

---

## Asset System

- `Model.load("filename.cmdl")` — see [[rendering/model_importing]].
- `MaterialInstance.load("filename.mi")` — see [[rendering/materials]].
- `Texture` — loaded via material; rarely loaded directly in Lua.
- Refcounted (`IAsset`). Hot-reload supported in editor builds.

---

## Key Workflows

### Adding a new scriptable C++ class

1. Create `.h`/`.cpp` under appropriate `Source/` subdir.
2. Add `CLASS_BODY(MyClass)` + `REF` methods.
3. Add `<ClCompile>`/`<ClInclude>` to `CsRemake.vcxproj`.
4. `py Scripts/codegen.py`.
5. Build & verify.

### Running tests

See [[testing]].
