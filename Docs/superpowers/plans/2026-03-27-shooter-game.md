# Lua FPS Shooter Game Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a multi-room first-person shooter game in Lua on the existing C++ engine, including a new `Gui` C++ drawing layer, a `LuaTestRunner` C++ helper, and a Lua-driven integration test system.

**Architecture:** New C++ classes (`Gui`, `LuaTestRunner`) are added to `Source/UI/` and `Source/Scripting/`, registered via codegen (`REF`/`CLASS_BODY`), and exposed to Lua automatically. Game logic lives entirely in `Data/scripts/shooter/*.lua`. `g_run_tests` ConfigVar triggers `app->run_integration_tests()` after `app->start()`.

**Tech Stack:** C++17, Lua 5.x, existing codegen (`Scripts/codegen.py`), MSBuild (Visual Studio 2019), glm for math, `UiSystem`/`RenderWindow`/`MeshBuilder` for rendering.

---

## File Map

**Create:**
- `Source/UI/Gui.h` — Love2D-style static drawing API (`Gui` class)
- `Source/UI/Gui.cpp` — implementation
- `Source/Scripting/LuaTestRunner.h` — `LuaTestRunner` with `finish()` static method
- `Source/Scripting/LuaTestRunner.cpp` — writes JUnit XML, calls `Quit()`
- `Data/scripts/shooter/app.lua` — `FpsGameApplication`, Lua test framework, level management
- `Data/scripts/shooter/fp_player.lua` — `FpPlayerController` component
- `Data/scripts/shooter/enemy.lua` — `EnemyController` component
- `Data/scripts/shooter/bullet.lua` — `BulletProjectile` component
- `Data/scripts/shooter/door_trigger.lua` — `DoorTrigger` component
- `Data/scripts/shooter/hud.lua` — `HudDrawer` component

**Modify:**
- `Source/Render/RenderWindow.h` — add `LineShape`, `CircleShape` structs + `draw()` overloads
- `Source/Render/RenderWindow.cpp` — implement `draw(LineShape)` and `draw(CircleShape)`
- `Source/Framework/MeshBuilder.h` — declare `Push2dCircle`
- `Source/Framework/MeshBuilder.cpp` — implement `Push2dCircle`
- `Source/GameEnginePublic.h` — add `REF virtual void run_integration_tests() {}` to `Application`
- `Source/EngineMain.cpp` — add `g_run_tests` ConfigVar; call `app->run_integration_tests()` when set
- `CsRemake.vcxproj` — add `Gui.h/.cpp` and `LuaTestRunner.h/.cpp` to project
- `test_game_vars.txt` — add `g_run_tests 1` and `g_application_class "FpsGameApplication"`
- `vars.txt` — add shooter game entry (swap back to editor when done testing)

---

## Task 1: Add `Push2dCircle` to MeshBuilder

**Files:**
- Modify: `Source/Framework/MeshBuilder.h`
- Modify: `Source/Framework/MeshBuilder.cpp`

- [ ] **Step 1: Add declaration to `MeshBuilder.h`**

In `Source/Framework/MeshBuilder.h`, after the existing `AddLineCapsule` declaration (line 50), add:

```cpp
void Push2dCircle(glm::vec2 center, float radius, int segments, Color32 color);
```

- [ ] **Step 2: Implement in `MeshBuilder.cpp`**

In `Source/Framework/MeshBuilder.cpp`, add at the end of the file:

```cpp
void MeshBuilder::Push2dCircle(glm::vec2 center, float radius, int segments, Color32 color) {
    const float step = (2.0f * glm::pi<float>()) / segments;
    const int base = GetBaseVertex();
    // Center vertex
    AddVertex(MbVertex(glm::vec3(center, 0), color));
    for (int i = 0; i <= segments; i++) {
        float angle = i * step;
        glm::vec2 p = center + glm::vec2(cos(angle), sin(angle)) * radius;
        AddVertex(MbVertex(glm::vec3(p, 0), color));
    }
    // Triangle fan: center + adjacent ring vertices
    for (int i = 0; i < segments; i++) {
        AddTriangle(base, base + 1 + i, base + 2 + i);
    }
}
```

- [ ] **Step 3: Verify it compiles**

Open solution in VS2019 and build `CsRemake` (Debug|x64). Expected: 0 errors.

- [ ] **Step 4: Run unit tests to confirm no regressions**

```powershell
powershell.exe -File Scripts/build_and_test.ps1 -Configuration Debug -Platform x64
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add Source/Framework/MeshBuilder.h Source/Framework/MeshBuilder.cpp
git commit -m "Add Push2dCircle triangle fan to MeshBuilder"
```

---

## Task 2: Add `LineShape` and `CircleShape` to RenderWindow

**Files:**
- Modify: `Source/Render/RenderWindow.h`
- Modify: `Source/Render/RenderWindow.cpp`

- [ ] **Step 1: Add structs and `draw()` declarations to `RenderWindow.h`**

After the `TextShape` struct (line 100), add:

```cpp
struct LineShape {
    glm::ivec2 start, end;
    int thickness = 1;
    Color32 color = COLOR_WHITE;
};
struct CircleShape {
    glm::ivec2 center;
    int radius = 8;
    int segments = 12;
    bool filled = true;
    Color32 color = COLOR_WHITE;
};
```

After the existing `void draw(TextShape text_shape);` in `RenderWindow` class:

```cpp
void draw(LineShape line_shape);
void draw(CircleShape circle_shape);
```

- [ ] **Step 2: Implement `draw(LineShape)` in `RenderWindow.cpp`**

Add at the end of `Source/Render/RenderWindow.cpp`:

```cpp
void RenderWindow::draw(LineShape line_shape) {
    const int start = meshbuilder.get_i().size();
    // Build a thin quad along the line direction
    glm::vec2 a(line_shape.start);
    glm::vec2 b(line_shape.end);
    glm::vec2 dir = b - a;
    float len = glm::length(dir);
    if (len < 0.001f) return;
    dir /= len;
    glm::vec2 perp(-dir.y, dir.x);
    float half = line_shape.thickness * 0.5f;
    glm::vec2 p0 = a + perp * half;
    glm::vec2 p1 = a - perp * half;
    glm::vec2 p2 = b - perp * half;
    glm::vec2 p3 = b + perp * half;
    int base = meshbuilder.GetBaseVertex();
    meshbuilder.AddVertex(MbVertex(glm::vec3(p0, 0), line_shape.color));
    meshbuilder.AddVertex(MbVertex(glm::vec3(p1, 0), line_shape.color));
    meshbuilder.AddVertex(MbVertex(glm::vec3(p2, 0), line_shape.color));
    meshbuilder.AddVertex(MbVertex(glm::vec3(p3, 0), line_shape.color));
    meshbuilder.AddQuad(base, base + 1, base + 2, base + 3);
    auto mat = (MaterialInstance*)UiSystem::inst->get_default_ui_mat();
    add_draw_call(mat, start, nullptr);
}

void RenderWindow::draw(CircleShape circle_shape) {
    const int start = meshbuilder.get_i().size();
    if (circle_shape.filled) {
        meshbuilder.Push2dCircle(glm::vec2(circle_shape.center), (float)circle_shape.radius,
                                  circle_shape.segments, circle_shape.color);
    } else {
        // outline: draw segments as thin lines
        const float step = (2.0f * glm::pi<float>()) / circle_shape.segments;
        for (int i = 0; i < circle_shape.segments; i++) {
            float a0 = i * step;
            float a1 = (i + 1) * step;
            glm::vec2 pa = glm::vec2(circle_shape.center) + glm::vec2(cos(a0), sin(a0)) * (float)circle_shape.radius;
            glm::vec2 pb = glm::vec2(circle_shape.center) + glm::vec2(cos(a1), sin(a1)) * (float)circle_shape.radius;
            int base = meshbuilder.GetBaseVertex();
            meshbuilder.AddVertex(MbVertex(glm::vec3(pa, 0), circle_shape.color));
            meshbuilder.AddVertex(MbVertex(glm::vec3(pb, 0), circle_shape.color));
            meshbuilder.AddLine(base, base + 1);
        }
    }
    auto mat = (MaterialInstance*)UiSystem::inst->get_default_ui_mat();
    add_draw_call(mat, start, nullptr);
}
```

- [ ] **Step 3: Add `#define GLM_ENABLE_EXPERIMENTAL` / include `<glm/gtc/constants.hpp>` if needed**

Check top of `RenderWindow.cpp` — if `glm::pi<float>()` is missing, add:
```cpp
#include <glm/gtc/constants.hpp>
```
at the top of `RenderWindow.cpp`.

- [ ] **Step 4: Build and verify compilation**

Build `CsRemake` (Debug|x64). Expected: 0 errors.

- [ ] **Step 5: Commit**

```bash
git add Source/Render/RenderWindow.h Source/Render/RenderWindow.cpp
git commit -m "Add LineShape and CircleShape draw support to RenderWindow"
```

---

## Task 3: Create `Gui` C++ Class

**Files:**
- Create: `Source/UI/Gui.h`
- Create: `Source/UI/Gui.cpp`
- Modify: `CsRemake.vcxproj`

- [ ] **Step 1: Create `Source/UI/Gui.h`**

```cpp
#pragma once
#include "Framework/ClassBase.h"
#include "Framework/Util.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include "Framework/LuaColor.h"
#include "Framework/Rect2d.h"
class Texture;

class Gui : public ClassBase {
public:
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
private:
    static Color32 current_color;
};
```

- [ ] **Step 2: Create `Source/UI/Gui.cpp`**

```cpp
#include "Gui.h"
#include "GUISystemPublic.h"
#include "Render/RenderWindow.h"
#include "UIBuilder.h"

Color32 Gui::current_color = COLOR_WHITE;

void Gui::set_color(float r, float g, float b, float a) {
    current_color = Color32(
        (uint8_t)(glm::clamp(r, 0.f, 1.f) * 255),
        (uint8_t)(glm::clamp(g, 0.f, 1.f) * 255),
        (uint8_t)(glm::clamp(b, 0.f, 1.f) * 255),
        (uint8_t)(glm::clamp(a, 0.f, 1.f) * 255)
    );
}

void Gui::rectangle(int x, int y, int w, int h) {
    RectangleShape shape;
    shape.rect = Rect2d(x, y, w, h);
    shape.color = current_color;
    UiSystem::inst->window.draw(shape);
}

void Gui::rectangle_outline(int x, int y, int w, int h, int thickness) {
    // Draw 4 lines forming the outline
    LineShape top{glm::ivec2(x, y), glm::ivec2(x + w, y), thickness, current_color};
    LineShape bot{glm::ivec2(x, y + h), glm::ivec2(x + w, y + h), thickness, current_color};
    LineShape lft{glm::ivec2(x, y), glm::ivec2(x, y + h), thickness, current_color};
    LineShape rgt{glm::ivec2(x + w, y), glm::ivec2(x + w, y + h), thickness, current_color};
    UiSystem::inst->window.draw(top);
    UiSystem::inst->window.draw(bot);
    UiSystem::inst->window.draw(lft);
    UiSystem::inst->window.draw(rgt);
}

void Gui::circle(int x, int y, int radius, int segments) {
    CircleShape shape;
    shape.center = glm::ivec2(x, y);
    shape.radius = radius;
    shape.segments = segments;
    shape.filled = true;
    shape.color = current_color;
    UiSystem::inst->window.draw(shape);
}

void Gui::line(int x1, int y1, int x2, int y2, int thickness) {
    LineShape shape{glm::ivec2(x1, y1), glm::ivec2(x2, y2), thickness, current_color};
    UiSystem::inst->window.draw(shape);
}

void Gui::image(Texture* tex, int x, int y, int w, int h) {
    RectangleShape shape;
    shape.rect = Rect2d(x, y, w, h);
    shape.color = current_color;
    shape.texture = tex;
    UiSystem::inst->window.draw(shape);
}

void Gui::print(std::string text, int x, int y) {
    TextShape shape;
    shape.text = text;
    shape.rect.x = x;
    shape.rect.y = y;
    shape.color = current_color;
    UiSystem::inst->window.draw(shape);
}

lRect Gui::measure_text(std::string text) {
    auto r = GuiHelpers::calc_text_size(text, nullptr);
    return lRect(r);
}

lRect Gui::get_screen_size() {
    return lRect(UiSystem::inst->get_vp_rect());
}
```

- [ ] **Step 3: Add to `CsRemake.vcxproj`**

In `CsRemake.vcxproj`, find the line:
```xml
<ClCompile Include="Source\UI\GUISystemPublic.cpp" />
```
Add after it:
```xml
<ClCompile Include="Source\UI\Gui.cpp" />
```

Find the line:
```xml
<ClInclude Include="Source\UI\GUISystemPublic.h" />
```
Add after it:
```xml
<ClInclude Include="Source\UI\Gui.h" />
```

- [ ] **Step 4: Run codegen**

```powershell
powershell.exe -c "py Scripts/codegen.py"
```

Expected: `Source/MEGA.cpp` updated, no errors.

- [ ] **Step 5: Build and verify**

Build `CsRemake` (Debug|x64). Expected: 0 errors, `Gui` class appears in Lua globals.

- [ ] **Step 6: Commit**

```bash
git add Source/UI/Gui.h Source/UI/Gui.cpp CsRemake.vcxproj Source/MEGA.cpp
git commit -m "Add Gui C++ drawing layer - Love2D-style rectangle/circle/line/print API"
```

---

## Task 4: Create `LuaTestRunner` C++ Class

**Files:**
- Create: `Source/Scripting/LuaTestRunner.h`
- Create: `Source/Scripting/LuaTestRunner.cpp`
- Modify: `CsRemake.vcxproj`

- [ ] **Step 1: Create `Source/Scripting/LuaTestRunner.h`**

```cpp
#pragma once
#include "Framework/ClassBase.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include <string>

class LuaTestRunner : public ClassBase {
public:
    CLASS_BODY(LuaTestRunner);
    // Called from Lua when all tests are done.
    // pass/fail counts, newline-delimited failure messages.
    // Writes JUnit XML to TestFiles/integration_lua_results.xml, then quits.
    REF static void finish(int pass, int fail, std::string failures);
};
```

- [ ] **Step 2: Create `Source/Scripting/LuaTestRunner.cpp`**

```cpp
#include "LuaTestRunner.h"
#include "Framework/SysPrint.h"
#include <fstream>
#include <sstream>

extern void Quit();  // defined in EngineMain.cpp

void LuaTestRunner::finish(int pass, int fail, std::string failures) {
    const int total = pass + fail;
    sys_print(Info, "LuaTestRunner: %d/%d tests passed\n", pass, total);
    if (fail > 0)
        sys_print(Error, "LuaTestRunner failures:\n%s\n", failures.c_str());

    // Write JUnit XML
    std::ofstream f("TestFiles/integration_lua_results.xml");
    f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    f << "<testsuite name=\"lua_integration\" tests=\"" << total
      << "\" failures=\"" << fail << "\">\n";

    // Parse failure list: each line is "testname: message"
    std::istringstream ss(failures);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        auto colon = line.find(": ");
        std::string name = (colon != std::string::npos) ? line.substr(0, colon) : line;
        std::string msg  = (colon != std::string::npos) ? line.substr(colon + 2) : "";
        f << "  <testcase name=\"" << name << "\">\n";
        f << "    <failure message=\"" << msg << "\"/>\n";
        f << "  </testcase>\n";
    }
    f << "</testsuite>\n";
    f.close();

    Quit();
}
```

- [ ] **Step 3: Add to `CsRemake.vcxproj`**

Find in vcxproj:
```xml
<ClCompile Include="Source\UI\GUISystemPublic.cpp" />
```
Add:
```xml
<ClCompile Include="Source\Scripting\LuaTestRunner.cpp" />
```

Find:
```xml
<ClInclude Include="Source\UI\GUISystemPublic.h" />
```
Add:
```xml
<ClInclude Include="Source\Scripting\LuaTestRunner.h" />
```

- [ ] **Step 4: Run codegen**

```powershell
powershell.exe -c "py Scripts/codegen.py"
```

Expected: `Source/MEGA.cpp` updated, `LuaTestRunner` appears.

- [ ] **Step 5: Build and verify**

Build `CsRemake` (Debug|x64). Expected: 0 errors.

- [ ] **Step 6: Commit**

```bash
git add Source/Scripting/LuaTestRunner.h Source/Scripting/LuaTestRunner.cpp CsRemake.vcxproj Source/MEGA.cpp
git commit -m "Add LuaTestRunner C++ class - writes JUnit XML and quits on test completion"
```

---

## Task 5: Add `run_integration_tests` to Application + `g_run_tests` cvar

**Files:**
- Modify: `Source/GameEnginePublic.h` (line 52)
- Modify: `Source/EngineMain.cpp` (after line 1535)

- [ ] **Step 1: Add virtual to `Application` in `GameEnginePublic.h`**

After line 52 (`REF virtual void on_map_changed() {}`), add:

```cpp
REF virtual void run_integration_tests() {}
```

- [ ] **Step 2: Add `g_run_tests` ConfigVar in `EngineMain.cpp`**

After line 105 (`ConfigVar g_dontsimphysics(...)`) add:

```cpp
ConfigVar g_run_tests("g_run_tests", "0", CVAR_BOOL, "run lua integration tests on startup then quit");
```

- [ ] **Step 3: Call `run_integration_tests` after `app->start()`**

At line 1535, after `app->start();`, add:

```cpp
if (g_run_tests.get_bool()) {
    app->run_integration_tests();
}
```

- [ ] **Step 4: Run codegen (Application gained a new REF method)**

```powershell
powershell.exe -c "py Scripts/codegen.py"
```

- [ ] **Step 5: Build and verify**

Build `CsRemake` (Debug|x64). Expected: 0 errors.

- [ ] **Step 6: Commit**

```bash
git add Source/GameEnginePublic.h Source/EngineMain.cpp Source/MEGA.cpp
git commit -m "Add run_integration_tests virtual to Application and g_run_tests cvar"
```

---

## Task 6: Write Shooter Lua — app.lua (FpsGameApplication + test framework)

**Files:**
- Create: `Data/scripts/shooter/app.lua`

- [ ] **Step 1: Create the file**

```lua
---@class FpsGameApplication : Application
FpsGameApplication = {}

local LEVELS = {
    "demo_level_0.tmap",
    "demo_level_1.tmap",
    "demo_level_2.tmap",
    "demo_level_3.tmap",
}
local LEVEL_CONFIG = {
    ["demo_level_0.tmap"] = { enemy_count = 3, door_pos = {x=0, y=0, z=10} },
    ["demo_level_1.tmap"] = { enemy_count = 5, door_pos = {x=0, y=0, z=20} },
    ["demo_level_2.tmap"] = { enemy_count = 7, door_pos = {x=0, y=0, z=15} },
    ["demo_level_3.tmap"] = { enemy_count = 10, door_pos = {x=0, y=0, z=30} },
}

local current_level_index = 1
local enemies_alive = 0
local player_entity = nil

-- Called by EnemyController when an enemy dies
function FpsGameApplication:on_enemy_died()
    enemies_alive = enemies_alive - 1
end

function FpsGameApplication:get_enemies_alive()
    return enemies_alive
end

function FpsGameApplication:get_current_level_index()
    return current_level_index
end

function FpsGameApplication:start()
    GameplayStatic.change_level(LEVELS[current_level_index])
end

function FpsGameApplication:on_map_changed()
    local level_name = GameplayStatic.get_current_level_name()
    local cfg = LEVEL_CONFIG[level_name]
    if cfg == nil then return end

    -- Spawn player
    player_entity = GameplayStatic.spawn_entity()
    player_entity:set_name("player")
    local phys = player_entity:create_component(CapsuleComponent)
    phys:set_data(1.8, 0.25, 0.9)
    phys:set_is_static(false)
    phys:set_is_simulating(false)
    local move = player_entity:create_component(CharacterMovementComponent)
    move:set_physics_body(phys)
    local cam_ent = player_entity:create_child_entity()
    cam_ent:set_ls_position({y=1.6})
    local cam = cam_ent:create_component(CameraComponent)
    cam:set_is_enabled(true)
    local hud = player_entity:create_component(HudDrawer)
    local fp = player_entity:create_component(FpPlayerController)
    fp:init(move, cam_ent, hud)

    -- Spawn enemies
    enemies_alive = cfg.enemy_count
    for i = 1, cfg.enemy_count do
        local eent = GameplayStatic.spawn_entity()
        eent:set_ws_position({x = i * 3, y = 0, z = 5})
        local ephys = eent:create_component(CapsuleComponent)
        ephys:set_data(1.8, 0.25, 0.9)
        ephys:set_is_static(false)
        ephys:set_is_simulating(false)
        local emove = eent:create_component(CharacterMovementComponent)
        emove:set_physics_body(ephys)
        local enemy = eent:create_component(EnemyController)
        enemy:init(emove)
        local emesh = eent:create_component(MeshComponent)
        emesh:set_model(Model.load("SWAT_Model.cmdl"))
    end

    -- Spawn door trigger
    local door_ent = GameplayStatic.spawn_entity()
    door_ent:set_ws_position(cfg.door_pos)
    local door = door_ent:create_component(DoorTrigger)
    door:init(LEVELS, current_level_index)
end

-- Called by DoorTrigger when player exits a level
function FpsGameApplication:advance_level(next_index)
    current_level_index = next_index
    if current_level_index <= #LEVELS then
        GameplayStatic.change_level(LEVELS[current_level_index])
    end
end

-- ---- Lua integration test framework ----
local _tests = {}
local _runner = nil
local _wait_remaining = 0.0

function add_test(name, fn)
    table.insert(_tests, {name=name, fn=fn})
end

local function run_all_tests()
    local pass, fail, failures = 0, 0, {}
    for _, t in ipairs(_tests) do
        local co = coroutine.create(t.fn)
        local ok, err = true, nil
        while coroutine.status(co) ~= "dead" do
            local success, val = coroutine.resume(co)
            if not success then ok = false; err = val; break end
            if val then coroutine.yield(val) end
        end
        if ok then
            pass = pass + 1
        else
            fail = fail + 1
            table.insert(failures, t.name .. ": " .. tostring(err))
        end
    end
    LuaTestRunner.finish(pass, fail, table.concat(failures, "\n"))
end

function FpsGameApplication:run_integration_tests()
    add_test("shooter/player_spawns", function()
        GameplayStatic.change_level("demo_level_0.tmap")
        coroutine.yield(1.5)
        assert(GameplayStatic.find_by_name("player") ~= nil, "player entity should exist after level load")
    end)

    add_test("shooter/enemies_spawned", function()
        GameplayStatic.change_level("demo_level_0.tmap")
        coroutine.yield(1.5)
        local app = Application.get_app()
        assert(app:get_enemies_alive() == 3, "demo_level_0 should spawn 3 enemies")
    end)

    _runner = coroutine.create(run_all_tests)
    coroutine.resume(_runner)
end

function FpsGameApplication:update()
    -- Tick integration test runner
    if _runner and coroutine.status(_runner) ~= "dead" then
        _wait_remaining = _wait_remaining - GameplayStatic.get_dt()
        if _wait_remaining <= 0 then
            local ok, wait = coroutine.resume(_runner)
            _wait_remaining = wait or 0.0
        end
    end
end
```

- [ ] **Step 2: Verify no Lua parse errors**

Build and launch game in Debug mode with `g_application_class "FpsGameApplication"` in vars.txt (temporarily). Look for Lua errors in console on startup.

- [ ] **Step 3: Commit**

```bash
git add Data/scripts/shooter/app.lua
git commit -m "Add FpsGameApplication Lua class with test framework and level management"
```

---

## Task 7: Write HUD Drawer Lua component

**Files:**
- Create: `Data/scripts/shooter/hud.lua`

- [ ] **Step 1: Create the file**

```lua
---@class HudDrawer : Component
HudDrawer = {
    health = 100,
    ammo = 30,
    room_index = 1,
    room_total = 4,
}

function HudDrawer:start()
    self:set_ticking(true)
end

function HudDrawer:update()
    local screen = Gui.get_screen_size()
    local sw = screen.w
    local sh = screen.h

    -- Health bar background (dark red)
    Gui.set_color(0.4, 0, 0, 1)
    Gui.rectangle(10, sh - 30, 200, 20)

    -- Health bar fill (bright red)
    local fill_w = math.floor(200 * (self.health / 100))
    Gui.set_color(0.9, 0.1, 0.1, 1)
    Gui.rectangle(10, sh - 30, fill_w, 20)

    -- Health text
    Gui.set_color(1, 1, 1, 1)
    Gui.print("HP: " .. self.health, 14, sh - 27)

    -- Ammo (bottom-right)
    local ammo_str = "AMMO: " .. self.ammo
    local ammo_sz = Gui.measure_text(ammo_str)
    Gui.set_color(1, 1, 1, 1)
    Gui.print(ammo_str, sw - ammo_sz.w - 10, sh - 27)

    -- Crosshair (center)
    local cx = math.floor(sw / 2)
    local cy = math.floor(sh / 2)
    Gui.set_color(0, 1, 0, 1)
    Gui.line(cx - 20, cy, cx - 5, cy, 1)
    Gui.line(cx + 5,  cy, cx + 20, cy, 1)
    Gui.line(cx, cy - 20, cx, cy - 5, 1)
    Gui.line(cx, cy + 5,  cx, cy + 20, 1)
    Gui.circle(cx, cy, 2, 8)

    -- Room number (top-center)
    local room_str = "Room " .. self.room_index .. " / " .. self.room_total
    local room_sz = Gui.measure_text(room_str)
    Gui.set_color(1, 1, 1, 1)
    Gui.print(room_str, math.floor((sw - room_sz.w) / 2), 10)
end
```

- [ ] **Step 2: Commit**

```bash
git add Data/scripts/shooter/hud.lua
git commit -m "Add HudDrawer Lua component - health bar, ammo, crosshair, room number"
```

---

## Task 8: Write FpPlayerController Lua component

**Files:**
- Create: `Data/scripts/shooter/fp_player.lua`

- [ ] **Step 1: Create the file**

```lua
---@class FpPlayerController : Component
FpPlayerController = {
    health = 100,
    ammo = 30,
    _move = nil,       -- CharacterMovementComponent
    _cam_ent = nil,    -- Entity (camera child)
    _hud = nil,        -- HudDrawer
    _pitch = 0.0,
    _yaw = 0.0,
    _gravity_vel = 0.0,
}

function FpPlayerController:init(move, cam_ent, hud)
    self._move = move
    self._cam_ent = cam_ent
    self._hud = hud
end

function FpPlayerController:start()
    self:set_ticking(true)
    Canvas.set_window_capture_mouse(true)
    self._move:set_capsule_info(1.8, 0.25, 0.05)
end

function FpPlayerController:update()
    local dt = GameplayStatic.get_dt()
    local sens = 0.005

    -- Mouse look
    local mdelta = lInput.get_mouse_delta()  -- returns {x, y}
    self._yaw   = self._yaw   - mdelta.x * sens
    self._pitch = self._pitch - mdelta.y * sens
    self._pitch = math.max(-1.4, math.min(1.4, self._pitch))

    self:get_owner():set_ls_euler_rotation({y = self._yaw})
    self._cam_ent:set_ls_euler_rotation({x = self._pitch})

    -- WASD movement (local space)
    local fwd  = lInput.is_key_down(SDL_SCANCODE_W) and 1 or 0
    local back = lInput.is_key_down(SDL_SCANCODE_S) and 1 or 0
    local rgt  = lInput.is_key_down(SDL_SCANCODE_D) and 1 or 0
    local lft  = lInput.is_key_down(SDL_SCANCODE_A) and 1 or 0

    local move_z = (fwd - back)
    local move_x = (rgt - lft)
    local speed = 5.0
    local yaw = self._yaw
    local cos_y, sin_y = math.cos(yaw), math.sin(yaw)

    local vel_x = (cos_y * move_x - sin_y * move_z) * speed
    local vel_z = (sin_y * move_x + cos_y * move_z) * speed

    -- Gravity
    if not self._move:is_touching_down() then
        self._gravity_vel = self._gravity_vel - 9.8 * dt
    else
        self._gravity_vel = 0
    end

    self._move:move({x = vel_x * dt, y = self._gravity_vel * dt, z = vel_z * dt}, dt, 0.001)

    -- Sync entity position to physics
    local pos = self._move:get_position()
    self:get_owner():set_ws_position(pos)

    -- Shoot on left click
    if lInput.was_mouse_pressed(0) and self.ammo > 0 then
        self.ammo = self.ammo - 1
        self:_fire()
    end

    -- Update HUD
    if self._hud ~= nil then
        self._hud.health = self.health
        self._hud.ammo = self.ammo
    end
end

function FpPlayerController:_fire()
    local bullet_ent = GameplayStatic.spawn_entity()
    local cam_pos = self._cam_ent:get_ws_position()
    bullet_ent:set_ws_position(cam_pos)
    local rot = self._cam_ent:get_ws_rotation()
    bullet_ent:set_ws_rotation(rot)
    -- Forward in local space (0,0,-1) rotated to world
    local fwd_x = -math.sin(self._yaw) * math.cos(self._pitch)
    local fwd_y = math.sin(self._pitch)
    local fwd_z = -math.cos(self._yaw) * math.cos(self._pitch)
    local bullet = bullet_ent:create_component(BulletProjectile)
    bullet:init({x=fwd_x, y=fwd_y, z=fwd_z}, false)
    local mesh = bullet_ent:create_component(MeshComponent)
    mesh:set_model(Model.load("eng/sphere.cmdl"))
end

function FpPlayerController:take_damage(amount)
    self.health = self.health - amount
    if self.health <= 0 then
        self.health = 0
        -- respawn: reload level
        GameplayStatic.change_level(GameplayStatic.get_current_level_name())
    end
end
```

- [ ] **Step 2: Commit**

```bash
git add Data/scripts/shooter/fp_player.lua
git commit -m "Add FpPlayerController Lua component - WASD movement, mouse look, shooting"
```

---

## Task 9: Write BulletProjectile Lua component

**Files:**
- Create: `Data/scripts/shooter/bullet.lua`

- [ ] **Step 1: Create the file**

```lua
---@class BulletProjectile : Component
BulletProjectile = {
    _direction = nil,  -- {x,y,z} unit vector
    _speed = 40.0,
    _lifetime = 3.0,
    _is_enemy_bullet = false,
}

function BulletProjectile:init(direction, is_enemy_bullet)
    self._direction = direction
    self._is_enemy_bullet = is_enemy_bullet
end

function BulletProjectile:start()
    self:set_ticking(true)
end

function BulletProjectile:update()
    local dt = GameplayStatic.get_dt()
    self._lifetime = self._lifetime - dt

    if self._lifetime <= 0 then
        self:get_owner():destroy()
        return
    end

    local pos = self:get_owner():get_ws_position()
    local d = self._direction
    local dx = d.x * self._speed * dt
    local dy = d.y * self._speed * dt
    local dz = d.z * self._speed * dt

    -- Raycast along movement (channel_mask=1=world static, ignore_this=nil)
    local result = GameplayStatic.cast_ray(pos, {x=pos.x+dx, y=pos.y+dy, z=pos.z+dz}, 1, nil)
    if result.hit then
        -- result.what is the Entity that was hit
        if self._is_enemy_bullet then
            local player = result.what ~= nil and result.what:get_component(FpPlayerController) or nil
            if player ~= nil then
                player:take_damage(10)
            end
        else
            local enemy = result.what ~= nil and result.what:get_component(EnemyController) or nil
            if enemy ~= nil then
                enemy:take_damage(25)
            end
        end
        self:get_owner():destroy()
        return
    end

    self:get_owner():set_ws_position({x=pos.x+dx, y=pos.y+dy, z=pos.z+dz})
end
```

- [ ] **Step 2: Commit**

```bash
git add Data/scripts/shooter/bullet.lua
git commit -m "Add BulletProjectile Lua component - raycast movement, damage on hit"
```

---

## Task 10: Write EnemyController Lua component

**Files:**
- Create: `Data/scripts/shooter/enemy.lua`

- [ ] **Step 1: Create the file**

```lua
---@class EnemyController : Component
EnemyController = {
    health = 30,
    _move = nil,         -- CharacterMovementComponent
    _shoot_cooldown = 0.0,
    _gravity_vel = 0.0,
}

function EnemyController:init(move)
    self._move = move
end

function EnemyController:start()
    self:set_ticking(true)
    self._move:set_capsule_info(1.8, 0.25, 0.05)
end

function EnemyController:update()
    local dt = GameplayStatic.get_dt()
    local player_ent = GameplayStatic.find_by_name("player")
    if player_ent == nil then return end

    local my_pos = self:get_owner():get_ws_position()
    local pl_pos = player_ent:get_ws_position()

    local dx = pl_pos.x - my_pos.x
    local dy = pl_pos.y - my_pos.y
    local dz = pl_pos.z - my_pos.z
    local dist = math.sqrt(dx*dx + dy*dy + dz*dz)

    -- Move toward player if within 20m
    if dist < 20.0 and dist > 1.0 then
        local speed = 3.0
        local nx = dx / dist
        local nz = dz / dist

        if not self._move:is_touching_down() then
            self._gravity_vel = self._gravity_vel - 9.8 * dt
        else
            self._gravity_vel = 0
        end

        self._move:move({x=nx*speed*dt, y=self._gravity_vel*dt, z=nz*speed*dt}, dt, 0.001)
        local new_pos = self._move:get_position()
        self:get_owner():set_ws_position(new_pos)
    end

    -- Shoot at player if within 15m
    self._shoot_cooldown = self._shoot_cooldown - dt
    if dist < 15.0 and self._shoot_cooldown <= 0 then
        self._shoot_cooldown = 2.0
        self:_fire_at(pl_pos, my_pos)
    end
end

function EnemyController:_fire_at(target, origin)
    local dx = target.x - origin.x
    local dy = target.y - origin.y
    local dz = target.z - origin.z
    local len = math.sqrt(dx*dx + dy*dy + dz*dz)
    if len < 0.001 then return end
    local bullet_ent = GameplayStatic.spawn_entity()
    bullet_ent:set_ws_position(origin)
    local bullet = bullet_ent:create_component(BulletProjectile)
    bullet:init({x=dx/len, y=dy/len, z=dz/len}, true)
end

function EnemyController:take_damage(amount)
    self.health = self.health - amount
    if self.health <= 0 then
        self:get_owner():destroy()
        local app = Application.get_app()
        app:on_enemy_died()
    end
end
```

- [ ] **Step 2: Commit**

```bash
git add Data/scripts/shooter/enemy.lua
git commit -m "Add EnemyController Lua component - seek player, shoot, die on 0 HP"
```

---

## Task 11: Write DoorTrigger Lua component

**Files:**
- Create: `Data/scripts/shooter/door_trigger.lua`

- [ ] **Step 1: Create the file**

```lua
---@class DoorTrigger : Component
DoorTrigger = {
    _levels = nil,
    _current_index = 1,
}

function DoorTrigger:init(levels, current_index)
    self._levels = levels
    self._current_index = current_index
end

function DoorTrigger:start()
    self:set_ticking(true)
end

function DoorTrigger:update()
    local app = Application.get_app()
    if app:get_enemies_alive() > 0 then return end

    local player_ent = GameplayStatic.find_by_name("player")
    if player_ent == nil then return end

    local my_pos = self:get_owner():get_ws_position()
    local pl_pos = player_ent:get_ws_position()
    local dx = pl_pos.x - my_pos.x
    local dy = pl_pos.y - my_pos.y
    local dz = pl_pos.z - my_pos.z
    local dist = math.sqrt(dx*dx + dy*dy + dz*dz)

    if dist < 2.0 then
        local next_index = self._current_index + 1
        if next_index <= #self._levels then
            app:advance_level(next_index)
        else
            -- All levels beaten — show win on HUD, don't advance
            local hud_comp = player_ent:get_component(HudDrawer)
            if hud_comp ~= nil then
                -- Overwrite room text indirectly: room_index > room_total signals win
                hud_comp.room_index = #self._levels + 1
            end
        end
        -- Destroy self so it doesn't trigger again
        self:get_owner():destroy()
    end
end
```

- [ ] **Step 2: Commit**

```bash
git add Data/scripts/shooter/door_trigger.lua
git commit -m "Add DoorTrigger Lua component - advance level when all enemies dead and player approaches"
```

---

## Task 12: Update `test_game_vars.txt` and wire game entry

**Files:**
- Modify: `test_game_vars.txt`

- [ ] **Step 1: Update `test_game_vars.txt`**

Replace contents with:
```
g_application_class "FpsGameApplication"
is_editor_app 0
g_run_tests 1
g_entry_level "demo_level_0.tmap"
g_project_base "Data"
r.taa 0
ui.disable_screen_log 1
disable_render_time_tick 1
```

Note: `g_project_base "Data"` uses the repo-local Data/ folder. The main vars.txt has `D:/Data` which is a machine-specific absolute path — never use that for tests or the shooter game.

- [ ] **Step 2: Commit**

```bash
git add test_game_vars.txt
git commit -m "Update test_game_vars.txt to run FpsGameApplication with Lua integration tests"
```

---

## Task 13: End-to-end build and integration test run

**Files:** none new

- [ ] **Step 1: Run codegen one final time**

```powershell
powershell.exe -c "py Scripts/codegen.py"
```

Expected: `Source/MEGA.cpp` up to date, no errors.

- [ ] **Step 2: Build App project (Debug|x64)**

In Visual Studio, build `App` project. Expected: 0 errors.

- [ ] **Step 3: Run integration tests**

```powershell
powershell.exe -File Scripts/integration_test.ps1 -Config Debug
```

Watch for:
- App launches, loads `demo_level_0.tmap`
- Lua test output appears in console: `LuaTestRunner: 2/2 tests passed`
- `TestFiles/integration_lua_results.xml` written
- App exits cleanly (exit code 0)

- [ ] **Step 4: Run clang-format**

```powershell
powershell.exe -File Scripts/clang-format-all.ps1
```

- [ ] **Step 5: Final commit**

```bash
git add Source/MEGA.cpp
git commit -m "Shooter game: final codegen refresh and formatting pass"
```

---

## Task 14: Smoke-test the game manually

- [ ] **Step 1: Switch `vars.txt` to game mode (temporarily)**

In `vars.txt`, change:
```
g_application_class "FpsGameApplication"
is_editor_app 0
g_entry_level "demo_level_0.tmap"
```

- [ ] **Step 2: Launch `App` (Debug|x64)**

Expected:
- First-person view of the level
- Crosshair visible at screen center
- Health bar at bottom-left, ammo at bottom-right, "Room 1 / 4" at top
- WASD + mouse look works
- Left-click fires bullets
- Enemies move toward player and shoot back
- When all enemies dead, walking to door position advances to next level

- [ ] **Step 3: Restore `vars.txt`**

Revert `vars.txt` to editor mode after confirming game works.

```bash
git checkout vars.txt
```
