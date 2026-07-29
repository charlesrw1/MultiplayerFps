# Lua Cookbook

- **IMPORTANT** See all Lua functions available in `lua_stubs.lua` `transform_lua_stubs.lua` `vec_quat_lua_stubs.lua` and other `*_lua_stubs.lua`
- `REF`/`CLASS_BODY` C++ methods are exposed to Lua directly: `Type.static_method()` for statics,
`obj:method()` for instance methods. Reflected static classes act as namespaces (`GameplayStatic`,
`lInput`).

- See example scripts in: `D:/Data/scripts/**` or `TestFilesData/scripts/***`.
- Lua snippets below work identically from `cscli eval` against a running engine instance.

## Entities

```lua
local e = GameplayStatic.spawn_entity()
e:set_ws_position(Vec3.new(1, 0, 2))
e:set_ws_rotation(Quat.identity())
e:destroy()

local door = GameplayStatic.find_by_name("Door_01")
```
`Source/Game/GameplayStatic.h:35,58-59`, `Source/Game/Entity.h:40,68,71,74`

```lua
child:parent_to(parent_entity)
child:set_ls_position(Vec3.new(0, 1, 0))
child:set_ls_euler_rotation(Vec3.new(0, 1.57, 0))
```
`Source/Game/Entity.h:55-60,78`

## Components

```lua
local mesh = e:create_component(MeshComponent)
mesh:set_model(Model.load("models/swatman/swatman.cmdl"))
local existing = e:get_component(MeshComponent)
```
`Source/Game/Entity.h:41-46`, `Source/Game/Components/MeshComponent.h:37`

- Common types: `MeshComponent`, `PointLightComponent`, `SpotLightComponent`,
  `SunLightComponent`, `SkylightComponent`, `SoundComponent`, `CapsuleComponent`,
  `BoxComponent`, `SphereComponent`, `CameraComponent`, `ParticleSystemComponent`.
- No `remove_component` — destroy the owning entity instead.

## Component lifecycle

A gameplay "class" is a Lua table set as a component type, attached via `create_component`
(`Source/Game/EntityComponent.h:29-45`):

```lua
---@class MyComponent : Component
MyComponent = { health = 100 }

function MyComponent:start()   -- once, after attach (not in plain edit mode, see below)
    self:set_ticking(true)     -- enables update()
end

function MyComponent:update()  -- every frame while ticking
    local dt = GameplayStatic.get_dt()
    local pos = self:get_owner():get_ws_position()
end

function MyComponent:stop() end   -- on destroy, only if start() ran
```

Collision callbacks (fire on every sibling component of the hit entity):
```lua
function MyComponent:on_collider_trigger(other, entered) end
function MyComponent:on_collider_hit(other, position, normal, impulse) end
```
- `on_collider_hit` needs `body:set_send_hit(true)` on the entity's `PhysicsBody`.
- `Source/Game/Components/PhysicsComponents.h:81`

Full example: `TestFilesData/scripts/shooter/fp_player.lua` (mouse look, WASD via capsule body, firing a bullet entity).

- `editor_start()` always runs in the editor
- `start()`/`stop()` do **not** run in plain edit mode by default (only play mode by default)
- A comment tag right after `---@class` controls both editor visibility and this behavior:

```lua
---@class FpDoor : Component
---editor, init_in_editor
FpDoor = { hp = 100 }
```
- `---editor` alone — opts into the editor's "add component" picker only.
- `---editor, init_in_editor` — also makes `start()`/`stop()` run in plain edit mode (equivalent
  to a C++ component calling `set_call_init_in_editor(true)` in its constructor).
- Tag must literally start with `---editor` (`---editorial note` does not match); re-parsed on
  every script reload.
-  Gating logic:`start()`/`stop()` run when
  `!is_editor_level() || get_call_init_in_editor()`.

## Physics

```lua
local body = e:create_component(CapsuleComponent)
body:set_data(1.8, 0.25, 0.0)   -- height, radius, height_offset
body:set_physics_layer(PL.SomeLayer)
body:set_send_hit(true)
body:apply_impulse(Vec3.new(0, 0, 0), Vec3.new(0, 5, 0))
```
`Source/Game/Components/PhysicsComponents.h:225,85,81,102`

```lua
local mask = GameplayStatic.get_collision_mask_for_physics_layer(PL.SomeLayer)
local hit = GameplayStatic.cast_ray(Vec3.new(0, 5, 0), Vec3.new(0, -5, 0), mask, nil)
if hit.hit then print(hit.what, hit.pos, hit.normal) end

local nearby = GameplayStatic.sphere_overlap(Vec3.new(0, 0, 0), 5.0, mask)
```
`Source/Game/GameplayStatic.h:51,53,72`

## Input

```lua
if lInput.is_key_down(SDL_SCANCODE_W) then ... end
if lInput.was_mouse_pressed(0) then ... end
local delta = lInput.get_mouse_delta()   -- {x=, y=}
lInput.set_capture_mouse(true)
```
`Source/Game/Entities/Player.h:132-151`

## Asset loading

```lua
local model  = Model.load("models/swatman/swatman.cmdl")
local tex    = Texture.load("brick.dds")
local sound  = SoundFile.load("explosion.wav")
local mat    = MaterialInstance.load("explosion_sphere.mm")
local prefab = PrefabAsset.load("ragdoll.pfb")
```
`Source/Render/Model.h:153`, `Source/Render/Texture.h:20`, `Source/Sound/SoundPublic.h:19`,
`Source/Render/MaterialPublic.h:28`, `Source/Game/Prefab.h:62`

- No `spawn_prefab` gameplay API — instantiating a `.pfb` into a live level is editor-only
  (`Source/LevelEditor/CommandsPrefab.cpp`). Gameplay scripts only spawn entities + components.

## Sound

```lua
GameplayStatic.play_simple_sound(SoundFile.load("beep.wav"))
GameplayStatic.play_spatial_sound(pos, SoundFile.load("explosion.wav"), 1.0, 20.0, SndAtn.Linear)
```
`Source/Game/GameplayStatic.h:39-47`

## Debug drawing

```lua
GameplayStatic.debug_sphere(pos, 0.5, 2.0, {r=1,g=0,b=0,a=1})   -- life=2s
GameplayStatic.debug_text_world(pos, "hi", 1.0, {r=1,g=1,b=1,a=1})
GameplayStatic.debug_line_normal(pos, normal, 1.0, 1.0, color)
```
`Source/Game/GameplayStatic.h:80-86`

## Misc GameplayStatic helpers

```lua
GameplayStatic.get_dt()        -- frame delta time
GameplayStatic.get_time()      -- game time
GameplayStatic.is_editor()     -- true if in the editor's play/edit level
GameplayStatic.change_level("maps/next.tmap")
GameplayStatic.get_current_level_name()
-- array of every instance of a type in the loaded level, e.g. for i, c in ipairs(...) do ... end
GameplayStatic.find_components(MeshComponent)
```
`Source/Game/GameplayStatic.h:62-70,100`

## Gotchas

- Nil-check a possibly-destroyed engine object with `GameplayStatic.is_null(e)`, not `e == nil` —
  a destroyed object's Lua table stays non-nil. `Source/Game/GameplayStatic.h:91-95`
- Build vectors/quats with `Vec3.new(x,y,z)` / `Quat.new(w,x,y,z)` / `Quat.identity()` /
  `Quat.from_euler(Vec3.new(x,y,z))` — gives metatable methods (`v:length()`, `v:normalize()`)
  that a bare `{x=,y=,z=}` table doesn't. `Source/Scripting/LuaVecQuat.cpp:66,181,189,218`
