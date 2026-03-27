# Lua FPS Shooter Game — Design Spec
**Date:** 2026-03-27

---

## Overview

A first-person shooter game implemented entirely in Lua, running on the existing C++ engine. Multi-room structure using existing `demo_level_*.tmap` maps. Doom-like AI enemies. No animations. Includes a new `Gui` C++ primitive layer and a Lua-driven integration test system.

---

## 1. Gui C++ Layer (New UI Primitives)

A new stateful 2D drawing API inspired by Love2D. Implemented as a new C++ class exposed to Lua via the existing codegen/REF system.

### New files
- `Source/UI/Gui.h`
- `Source/UI/Gui.cpp`

### C++ class

```cpp
class Gui : public ClassBase {
    CLASS_BODY(Gui);
    REF static void set_color(float r, float g, float b, float a);
    REF static void rectangle(int x, int y, int w, int h);
    REF static void rectangle_outline(int x, int y, int w, int h, int thickness);
    REF static void circle(int x, int y, int radius, int segments);
    REF static void line(int x1, int y1, int x2, int y2, int thickness);
    REF static void image(Texture* tex, int x, int y, int w, int h);
    REF static void print(std::string text, int x, int y);
    REF static lRect measure_text(std::string text);
    REF static lRect get_screen_size();
};
```

Internally stores `static Color32 current_color`. Each call pushes geometry into `UiSystem::inst->window` via new shape types.

### New shapes in `RenderWindow.h`

```cpp
struct LineShape {
    glm::ivec2 start, end;
    int thickness;
    Color32 color;
};
struct CircleShape {
    glm::ivec2 center;
    int radius;
    int segments;
    bool filled;
    Color32 color;
};
```

New `RenderWindow::draw(LineShape)` and `RenderWindow::draw(CircleShape)` push triangles into the existing `MeshBuilder`.

### New MeshBuilder method

```cpp
void Push2dCircle(glm::vec2 center, float radius, int segments, Color32 color);
```

Triangle fan, screen-space. Added to `MeshBuilder.h/.cpp`.

### Lua usage

```lua
Gui.set_color(1, 0, 0, 1)
Gui.rectangle(10, 10, 200, 40)
Gui.set_color(1, 1, 1, 1)
Gui.print("Health: 100", 14, 16)
Gui.circle(640, 360, 4, 12)  -- crosshair dot
Gui.line(600, 360, 680, 360, 1)
Gui.line(640, 320, 640, 400, 1)
```

---

## 2. Game Lua Files

### Structure

```
Data/scripts/shooter/
  app.lua            -- FpsGameApplication : Application
  fp_player.lua      -- FpPlayerController : Component
  enemy.lua          -- EnemyController : Component
  bullet.lua         -- BulletProjectile : Component
  door_trigger.lua   -- DoorTrigger : Component
  hud.lua            -- HudDrawer : Component
```

### vars.txt changes

```
g_application_class "FpsGameApplication"
g_entry_level "demo_level_0.tmap"
is_editor_app 0
```

### Level progression

Defined as a table in `app.lua`:

```lua
local LEVELS = {
    "demo_level_0.tmap",
    "demo_level_1.tmap",
    "demo_level_2.tmap",
    "demo_level_3.tmap",
}

local LEVEL_CONFIG = {
    ["demo_level_0.tmap"] = { enemy_count = 3, door_pos = vec3(0,0,10) },
    ["demo_level_1.tmap"] = { enemy_count = 5, door_pos = vec3(0,0,20) },
    ["demo_level_2.tmap"] = { enemy_count = 7, door_pos = vec3(0,0,15) },
    ["demo_level_3.tmap"] = { enemy_count = 10, door_pos = vec3(0,0,30) },
}
```

`FpsGameApplication:on_map_changed()` reads the current level name, spawns enemies and the door trigger from the config table.

### FpPlayerController (Component)

- Requires: `PhysicsBody`, `CharacterMovementComponent`, `CameraComponent`, `HudDrawer`
- WASD movement via `CharacterMovementComponent:move(displacement, dt, min_dist)`
- Mouse delta drives camera yaw (entity rotation) and pitch (camera child entity rotation)
- `Canvas.set_window_capture_mouse(true)` on start
- Left-click fires: spawns `BulletProjectile` entity at camera world position, aimed along camera forward
- Tracks `health` (100) and `ammo` (30)
- `take_damage(amount)` method called by enemies/bullets

### EnemyController (Component)

- Tracks `health` (30), `shoot_cooldown`
- Each update: compute direction to player, if within 20m move toward player via `CharacterMovementComponent:move()`
- Fire bullet at player every 2 seconds if within 15m
- On health <= 0: `self:get_owner():destroy()`
- Enemy count tracked in app; when 0, door trigger activates

### BulletProjectile (Component)

- Moves forward at `speed = 40 m/s` each frame
- Raycasts along movement vector via `GameplayStatic.cast_ray()`
- On hit entity: calls `target:get_component(EnemyController):take_damage(25)` or player damage
- Destroys self after hit or after 3 seconds lifetime
- Uses `sphere.cmdl` + existing material

### DoorTrigger (Component)

- Placed at level's door position by app on map changed
- Each update: if all enemies dead AND player within 2m radius → `GameplayStatic.change_level(next_level)`
- On final level completion: show win screen, no level change

### HudDrawer (Component)

Drawn in `update()` using `Gui.*`:

- **Health bar**: red rectangle, bottom-left, scales with player health
- **Ammo count**: white text, bottom-right
- **Crosshair**: four lines + center dot at screen center
- **Room number**: top-center text ("Room 1 / 4")

---

## 3. Lua Integration Test System

### C++ changes

**`Source/GameEnginePublic.h`** — add to `Application`:
```cpp
REF virtual void run_integration_tests() {}
```

**`Source/Game/EngineMain.cpp`** (or equivalent startup) — after `app->start()`, if `g_run_tests` cvar is set:
```cpp
app->run_integration_tests();
```

**New C++ class `LuaTestRunner`** (`Source/Scripting/LuaTestRunner.h/.cpp`):
```cpp
class LuaTestRunner : public ClassBase {
    CLASS_BODY(LuaTestRunner);
    REF static void finish(int pass, int fail, std::string failures);
};
```
`finish()` writes JUnit XML to `TestFiles/integration_lua_results.xml` and calls `eng->request_quit(exit_code)`.

**`test_game_vars.txt`** — add:
```
g_run_tests 1
g_application_class "FpsGameApplication"
```

### Lua test framework (inside `app.lua`)

```lua
local _tests = {}
local _runner = nil
local _wait_remaining = 0

function add_test(name, fn)
    table.insert(_tests, {name=name, fn=fn})
end

local function run_all()
    local pass, fail, failures = 0, 0, {}
    for _, t in ipairs(_tests) do
        local co = coroutine.create(t.fn)
        local ok, err = true, nil
        while coroutine.status(co) ~= "dead" do
            local success, val = coroutine.resume(co)
            if not success then ok=false; err=val; break end
            if val then coroutine.yield(val) end
        end
        if ok then pass = pass + 1
        else fail = fail + 1; table.insert(failures, t.name .. ": " .. tostring(err)) end
    end
    LuaTestRunner.finish(pass, fail, table.concat(failures, "\n"))
end

function FpsGameApplication:run_integration_tests()
    -- Register tests inline
    add_test("shooter/player_spawns", function()
        GameplayStatic.change_level("demo_level_0.tmap")
        coroutine.yield(1.5)
        assert(GameplayStatic.find_by_name("player") ~= nil, "player should exist")
    end)

    add_test("shooter/player_takes_damage", function()
        GameplayStatic.change_level("demo_level_0.tmap")
        coroutine.yield(3.0)
        assert(player_health < 100, "player should have taken damage")
    end)

    add_test("shooter/bullet_kills_enemy", function()
        GameplayStatic.change_level("demo_level_0.tmap")
        coroutine.yield(0.5)
        -- fire bullet directly at nearest enemy
        fire_test_bullet()
        coroutine.yield(1.0)
        assert(enemies_remaining < initial_enemy_count, "enemy should be dead")
    end)

    add_test("shooter/door_advances_level", function()
        GameplayStatic.change_level("demo_level_0.tmap")
        coroutine.yield(0.5)
        kill_all_enemies()
        move_player_to_door()
        coroutine.yield(1.0)
        assert(current_level_index == 2, "should have advanced to level 2")
    end)

    _runner = coroutine.create(run_all)
    coroutine.resume(_runner)
end

function FpsGameApplication:update()
    if _runner and coroutine.status(_runner) ~= "dead" then
        _wait_remaining = _wait_remaining - GameplayStatic.get_dt()
        if _wait_remaining <= 0 then
            local ok, wait = coroutine.resume(_runner)
            _wait_remaining = wait or 0
        end
    end
    -- normal game update below ...
end
```

---

## 4. Out of Scope

- Animations (explicitly excluded)
- Pathfinding (enemies use direct seek)
- Multiplayer
- Sound design (spatial audio calls may be stubbed)
- New art assets (use existing Data/ assets only)
