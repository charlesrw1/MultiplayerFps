-- Weapon registry: maps a string weapon_id to a factory that produces a
-- fresh FpsWeapon instance. New weapon types are added here.

---@type table<string, fun(): FpsWeapon>
FpsWeaponRegistry = {}

---@param id string
---@param factory fun(): FpsWeapon
function FpsWeaponRegistry_register(id, factory)
    FpsWeaponRegistry[id] = factory
end

---@param id string
---@return FpsWeapon|nil
function FpsWeaponRegistry_make(id)
    local f = FpsWeaponRegistry[id]
    if f == nil then
        print("FpsWeaponRegistry: unknown weapon id '"..id.."'")
        return nil
    end
    return f()
end

-- ---- Built-in weapons ---------------------------------------------------

FpsWeaponRegistry_register("pistol", function ()
    local w = CopyInst(FpsHitscanWeapon)
    w.id = "pistol"
    w.display_name = "Pistol"
    w.damage = 18.0
    w.fire_cooldown = 0.20
    w.spread_rad = 0.01
    w.range = 60.0
    w.knockback = 5.0
    w.pellets = 1
    w.model_name = "top_down/rifle.cmdl"
    return w
end)

FpsWeaponRegistry_register("rifle", function ()
    local w = CopyInst(FpsHitscanWeapon)
    w.id = "rifle"
    w.display_name = "Rifle"
    w.damage = 28.0
    w.fire_cooldown = 0.10
    w.spread_rad = 0.02
    w.range = 80.0
    w.knockback = 8.0
    w.pellets = 1
    w.model_name = "top_down/rifle.cmdl"
    return w
end)

FpsWeaponRegistry_register("shotgun", function ()
    local w = CopyInst(FpsHitscanWeapon)
    w.id = "shotgun"
    w.display_name = "Shotgun"
    w.damage = 14.0
    w.fire_cooldown = 0.75
    w.spread_rad = 0.10
    w.range = 25.0
    w.knockback = 6.0
    w.pellets = 8
    w.model_name = "supershotgun.cmdl"
    return w
end)

FpsWeaponRegistry_register("grenade_launcher", function ()
    local w = CopyInst(FpsProjectileWeapon)
    w.id = "grenade_launcher"
    w.display_name = "Grenade Launcher"
    w.damage = 0.0
    w.fire_cooldown = 0.8
    w.spread_rad = 0.0
    w.range = 0.0
    w.knockback = 25.0
    w.launch_speed = 18.0
    w.projectile_radius = 0.15
    w.projectile_life = 2.5
    w.explosion_radius = 4.5
    w.explosion_damage = 70.0
    w.projectile_model = "grenade.cmdl"
    w.model_name = "grenade_launcher.cmdl"
    return w
end)
