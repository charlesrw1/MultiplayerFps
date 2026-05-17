-- FpsGameManager runtime: bootstrap. Scans data components in the scene and
-- wires up runtime entities/components. One instance per level. Caches itself
-- as the global `gFpsManager`.

---@type FpsGameManager|nil
gFpsManager = nil

-- A simple noise event ring. Guards consume entries each frame and we age them.
FPS_NOISE_EVENT_LIFE = 0.4

function FpsGameManager:start()
    if gFpsManager ~= nil then
        print("FpsGameManager: multiple instances in scene; ignoring duplicate")
        return
    end
    gFpsManager = self
    self:set_ticking(true)

    self.player = nil          -- FpsPlayer
    self.guards = {}           -- FpsGuard[]
    self.waypoint_groups = {}  -- name -> array of {pos, wait_time}
    self.noise_events = {}     -- array of {pos=lVec3, loudness=number, t=number}

    self:_collect_waypoints()
    self:_spawn_player()
    self:_spawn_guards()
    self:_wire_pickups()
    self:_wire_doors()
end

function FpsGameManager:update()
    -- Age out noise events
    local now = GameplayStatic.get_time()
    local kept = {}
    for _, ev in ipairs(self.noise_events) do
        if now - ev.t < FPS_NOISE_EVENT_LIFE then
            kept[#kept+1] = ev
        end
    end
    self.noise_events = kept
end

---@param pos lVec3
---@param loudness number
function FpsGameManager:report_noise(pos, loudness)
    self.noise_events[#self.noise_events+1] = {pos=pos, loudness=loudness, t=GameplayStatic.get_time()}
end

-- ---- Bootstrap steps -------------------------------------------------

function FpsGameManager:_collect_waypoints()
    ---@type FpsWaypoint[]
    local all = GameplayStatic.find_components(FpsWaypoint)
    local groups = {}
    for _, wp in ipairs(all) do
        if wp ~= nil then
            local g = wp.waypoint_group
            if groups[g] == nil then groups[g] = {} end
            local pos = wp:get_owner():get_ws_position()
            groups[g][#groups[g]+1] = {index=wp.index, pos=pos, wait_time=wp.wait_time}
        end
    end
    for name, arr in pairs(groups) do
        table.sort(arr, function (a, b) return a.index < b.index end)
        self.waypoint_groups[name] = arr
    end
end

function FpsGameManager:_spawn_player()
    ---@type FpsPlayerSpawn[]
    local spawns = GameplayStatic.find_components(FpsPlayerSpawn)
    if #spawns == 0 then
        print("FpsGameManager: no FpsPlayerSpawn in scene; player not spawned")
        return
    end
    local spawn = spawns[1]
    local pos = spawn:get_owner():get_ws_position()

    local ent = GameplayStatic.spawn_entity()
    ent:set_ws_position(pos)

    local hp = ent:create_component(FpsHealth)
    hp.max_health = self.player_max_health
    hp.current    = self.player_max_health

    local inv = ent:create_component(FpsInventory)
    local player = ent:create_component(FpsPlayer)
    -- Hook view-model to player camera. The camera entity is created in
    -- FpsPlayer:start(), but FpsInventory:start() ran in create_component order
    -- before that, so we wire the view AFTER both starts have completed by
    -- doing it here (player.cam_ent now exists).
    inv:init_view(player.cam_ent)

    -- Starting weapons
    for id in string.gmatch(self.starting_weapons, "([^,%s]+)") do
        inv:add_weapon_by_id(id)
    end

    self.player = player
    hp.on_death:add(function () self:_on_player_died() end)
end

function FpsGameManager:_on_player_died()
    print("FpsGameManager: player died")
    GameplayStatic.change_level(GameplayStatic.get_current_level_name())
end

function FpsGameManager:_spawn_guards()
    ---@type FpsGuardSpawn[]
    local spawns = GameplayStatic.find_components(FpsGuardSpawn)
    for _, gs in ipairs(spawns) do
        if gs ~= nil then
            self:_spawn_one_guard(gs)
        end
    end
end

---@param gs FpsGuardSpawn
function FpsGameManager:_spawn_one_guard(gs)
    local pos = gs:get_owner():get_ws_position()
    local ent = GameplayStatic.spawn_entity()
    ent:set_ws_position(pos)

    local cap = ent:create_component(CapsuleComponent)
    cap:set_data(1.8, 0.30, 0.9)
    cap:set_physics_layer(PL_CHARACTER)
    cap:set_is_static(false)
    cap:set_is_simulating(false)

    local mesh = ent:create_component(MeshComponent)
    local m = Model.load("cylinder_nose.cmdl")
    if m ~= nil then mesh:set_model(m) end

    local agent = ent:create_component(NavAgentComponent)
    agent.move_speed = gs.patrol_speed
    agent.arrive_radius = 0.6

    local hp = ent:create_component(FpsHealth)
    hp.max_health = 60.0
    hp.current    = 60.0

    local guard = ent:create_component(FpsGuard)
    guard.waypoint_group = gs.waypoint_group
    guard.waypoints      = self.waypoint_groups[gs.waypoint_group] or {}
    guard.patrol_speed   = gs.patrol_speed
    guard.weapon         = FpsWeaponRegistry_make(gs.weapon_id)
    guard.agent          = agent

    self.guards[#self.guards+1] = guard
end

function FpsGameManager:_wire_pickups()
    ---@type FpsHealthPickup[]
    local hps = GameplayStatic.find_components(FpsHealthPickup)
    for _, hp in ipairs(hps) do
        if hp ~= nil then
            local owner = hp:get_owner()
            local rt = owner:create_component(FpsHealthPickupRuntime)
            rt.amount = hp.amount
        end
    end
    ---@type FpsWeaponPickup[]
    local wps = GameplayStatic.find_components(FpsWeaponPickup)
    for _, wp in ipairs(wps) do
        if wp ~= nil then
            local owner = wp:get_owner()
            local rt = owner:create_component(FpsWeaponPickupRuntime)
            rt.weapon_id = wp.weapon_id
        end
    end
end

function FpsGameManager:_wire_doors()
    ---@type FpsDoorData[]
    local dds = GameplayStatic.find_components(FpsDoorData)
    for _, dd in ipairs(dds) do
        if dd ~= nil then
            local owner = dd:get_owner()
            local door = owner:create_component(FpsDoor)
            door.locked    = dd.locked
            door.key_id    = dd.key_id
            door.open_time = dd.open_time
        end
    end
end

function FpsGameManager:stop()
    if gFpsManager == self then
        gFpsManager = nil
    end
end
