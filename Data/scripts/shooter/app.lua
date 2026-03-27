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
    local player_entity = GameplayStatic.spawn_entity()
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
        assert(app:get_enemies_alive() == 3, "demo_level_0 should spawn 3 enemies, got: " .. tostring(app:get_enemies_alive()))
    end)

    _runner = coroutine.create(run_all_tests)
    coroutine.resume(_runner)
end

function FpsGameApplication:update()
    if _runner and coroutine.status(_runner) ~= "dead" then
        _wait_remaining = _wait_remaining - GameplayStatic.get_dt()
        if _wait_remaining <= 0 then
            local ok, wait = coroutine.resume(_runner)
            _wait_remaining = wait or 0.0
        end
    end
end
