-- Data-only components. Authored in the editor. No runtime behaviour beyond
-- being readable by FpsGameManager at level start. The `---editor` tag opts
-- each into the add-component picker.

-- ---- Player spawn -----------------------------------------------------

---@class FpsPlayerSpawn : Component
---editor
FpsPlayerSpawn = {}

function FpsPlayerSpawn:start() end
function FpsPlayerSpawn:stop() end

-- ---- Guard spawn ------------------------------------------------------

---@class FpsGuardSpawn : Component
---editor
FpsGuardSpawn = {
    ---@type string
    waypoint_group = "default",
    ---@type number
    patrol_speed = 2.0,
    ---@type string
    weapon_id = "pistol",
}

function FpsGuardSpawn:start() end
function FpsGuardSpawn:stop() end

-- ---- Waypoint ---------------------------------------------------------

---@class FpsWaypoint : Component
---editor
FpsWaypoint = {
    ---@type string
    waypoint_group = "default",
    ---@type integer
    index = 0,
    ---@type number
    wait_time = 1.0,
}

function FpsWaypoint:start() end
function FpsWaypoint:stop() end

-- ---- Weapon pickup ----------------------------------------------------

---@class FpsWeaponPickup : Component
---editor
FpsWeaponPickup = {
    ---@type string
    weapon_id = "pistol",
}

function FpsWeaponPickup:start() end
function FpsWeaponPickup:stop() end

-- ---- Health pickup ----------------------------------------------------

---@class FpsHealthPickup : Component
---editor
FpsHealthPickup = {
    ---@type number
    amount = 25.0,
}

function FpsHealthPickup:start() end
function FpsHealthPickup:stop() end

-- ---- Door data --------------------------------------------------------

---@class FpsDoorData : Component
---editor
FpsDoorData = {
    ---@type boolean
    locked = false,
    ---@type string
    key_id = "",
    ---@type number
    open_time = 0.6,
}

function FpsDoorData:start() end
function FpsDoorData:stop() end

-- ---- Game manager (also a data component: holds level config) --------

---@class FpsGameManager : Component
---editor
FpsGameManager = {
    ---@type number
    player_max_health = 100.0,
    ---@type string
    starting_weapons = "pistol",
}

-- start() implementation lives in manager.lua to keep this file declaration-only.
