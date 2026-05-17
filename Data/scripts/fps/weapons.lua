-- FpsWeapon: base weapon class + hitscan & projectile variants.
-- Weapons are plain Lua tables (NOT Components). FpsInventory owns instances.
-- No ammo system; only fire_cooldown gates firing.

---@class FpsWeapon
---@field id string
---@field display_name string
---@field damage number
---@field fire_cooldown number
---@field spread_rad number
---@field range number
---@field knockback number
---@field model_name string
---@field model_scale number
---@field model_offset lVec3
---@field model_rotation lVec3
---@field last_fire_t number
FpsWeapon = {
    id = "",
    display_name = "",
    damage = 10,
    fire_cooldown = 0.2,
    spread_rad = 0.02,
    range = 50.0,
    knockback = 5.0,
    model_name = "",
    model_scale = 0.5,
    model_offset = {x=0.1,y=-0.12,z=-0.2},
    model_rotation = {x=0,y=math.pi,z=0},
    last_fire_t = 0.0,
}

---@param view_pos lVec3
---@param view_dir lVec3
---@param shooter Entity|nil
function FpsWeapon:fire(view_pos, view_dir, shooter) end

---@return boolean
function FpsWeapon:can_fire()
    return GameplayStatic.get_time() >= self.last_fire_t + self.fire_cooldown
end

---@param dir lVec3
---@return lVec3
function FpsWeapon:_apply_spread(dir)
    if self.spread_rad <= 0.0 then return dir end
    local jx = (math.random()*2-1) * self.spread_rad
    local jy = (math.random()*2-1) * self.spread_rad
    local out = {x=dir.x + jx, y=dir.y + jy, z=dir.z}
    return normalize(out)
end

-- =========================================================================
-- Hitscan weapon
-- =========================================================================

---@class FpsHitscanWeapon : FpsWeapon
---@field pellets integer
FpsHitscanWeapon = CopyInst(FpsWeapon)
FpsHitscanWeapon.pellets = 1

---@param view_pos lVec3
---@param view_dir lVec3
---@param shooter Entity|nil
function FpsHitscanWeapon:fire(view_pos, view_dir, shooter)
    if not self:can_fire() then return end
    self.last_fire_t = GameplayStatic.get_time()

    local vis_mask = GameplayStatic.get_collision_mask_for_physics_layer(PL_VISIBLITY)
    local ignore_body = nil
    if shooter ~= nil then
        local cap = shooter:get_component(CapsuleComponent)
        if cap ~= nil then ignore_body = cap end
    end

    for _ = 1, self.pellets do
        local dir = self:_apply_spread(view_dir)
        local endpt = vec_add(view_pos, vec_multf(dir, self.range))
        local res = GameplayStatic.cast_ray(view_pos, endpt, vis_mask, ignore_body)
        if res.hit and res.what then
            self:_apply_damage_to(res.what, res.pos, dir, shooter)
        end
    end

    if gFpsManager ~= nil then
        gFpsManager:report_noise(view_pos, 25.0)
    end
end

---@param target Entity
---@param hit_pos lVec3
---@param dir lVec3
---@param shooter Entity|nil
function FpsHitscanWeapon:_apply_damage_to(target, hit_pos, dir, shooter)
    local hp = target:get_component(FpsHealth)
    if hp ~= nil then
        hp:take_damage({amount=self.damage, source=shooter, hit_pos=hit_pos})
    end
    local body = target:get_component(PhysicsBody)
    if body ~= nil and body:get_is_simulating() then
        body:apply_impulse(hit_pos, vec_multf(dir, self.knockback))
    end
end

-- =========================================================================
-- Projectile weapon (e.g. grenade launcher)
-- =========================================================================

---@class FpsProjectileWeapon : FpsWeapon
---@field launch_speed number
---@field projectile_radius number
---@field projectile_life number
---@field explosion_radius number
---@field explosion_damage number
---@field projectile_model string
FpsProjectileWeapon = CopyInst(FpsWeapon)
FpsProjectileWeapon.launch_speed = 15.0
FpsProjectileWeapon.projectile_radius = 0.15
FpsProjectileWeapon.projectile_life = 3.0
FpsProjectileWeapon.explosion_radius = 4.0
FpsProjectileWeapon.explosion_damage = 60.0
FpsProjectileWeapon.projectile_model = "sphere.cmdl"

---@param view_pos lVec3
---@param view_dir lVec3
---@param shooter Entity|nil
function FpsProjectileWeapon:fire(view_pos, view_dir, shooter)
    if not self:can_fire() then return end
    self.last_fire_t = GameplayStatic.get_time()

    local ent = GameplayStatic.spawn_entity()
    ent:set_ws_position(view_pos)

    local mesh = ent:create_component(MeshComponent)
    local m = Model.load(self.projectile_model)
    if m ~= nil then mesh:set_model(m) end
    ent:set_ls_scale({x=self.projectile_radius*2, y=self.projectile_radius*2, z=self.projectile_radius*2})

    local sphere = ent:create_component(SphereComponent)
    sphere:set_radius(self.projectile_radius)
    sphere:set_physics_layer(PL_PHYSICSOBJECT)
    sphere:set_is_static(false)
    sphere:set_is_simulating(true)
    sphere:apply_impulse(view_pos, vec_multf(view_dir, self.launch_speed))

    local proj = ent:create_component(FpsProjectile)
    proj.shooter = shooter
    proj.life = self.projectile_life
    proj.explosion_radius = self.explosion_radius
    proj.explosion_damage = self.explosion_damage
    proj.knockback = self.knockback

    if gFpsManager ~= nil then
        gFpsManager:report_noise(view_pos, 25.0)
    end
end

-- =========================================================================
-- FpsProjectile: ticked component on launched projectile entity.
-- Detonates on overlap or after `life` seconds. Radial damage + impulse.
-- =========================================================================

---@class FpsProjectile : Component
FpsProjectile = {
    ---@type number
    life = 3.0,
    ---@type number
    explosion_radius = 4.0,
    ---@type number
    explosion_damage = 60.0,
    ---@type number
    knockback = 15.0,
}

function FpsProjectile:start()
    self:set_ticking(true)
    self.spawn_time = GameplayStatic.get_time()
    self.shooter = nil  -- set by spawner
    self.exploded = false
end

function FpsProjectile:update()
    if self.exploded then return end
    local pos = self:get_owner():get_ws_position()

    -- Hit something solid?
    local mask = GameplayStatic.get_collision_mask_for_physics_layer(PL_CHARACTER)
    local hits = GameplayStatic.sphere_overlap(pos, 0.25, mask)
    if #hits > 0 then
        self:_explode(pos)
        return
    end

    if GameplayStatic.get_time() - self.spawn_time > self.life then
        self:_explode(pos)
    end
end

---@param pos lVec3
function FpsProjectile:_explode(pos)
    self.exploded = true
    -- Damage characters in radius
    local char_mask = GameplayStatic.get_collision_mask_for_physics_layer(PL_CHARACTER)
    local victims = GameplayStatic.sphere_overlap(pos, self.explosion_radius, char_mask)
    for _, v in ipairs(victims) do
        local hp = v:get_component(FpsHealth)
        if hp ~= nil then
            local d = vec_sub(v:get_ws_position(), pos)
            local dist = lMath.length(d)
            local falloff = math.max(0.0, 1.0 - dist/self.explosion_radius)
            hp:take_damage({amount=self.explosion_damage*falloff, source=self.shooter, hit_pos=pos})
        end
    end
    -- Impulse physics props
    local phys_mask = GameplayStatic.get_collision_mask_for_physics_layer(PL_PHYSICSOBJECT)
    local props = GameplayStatic.sphere_overlap(pos, self.explosion_radius, phys_mask)
    for _, v in ipairs(props) do
        local body = v:get_component(PhysicsBody)
        if body ~= nil and body:get_is_simulating() then
            local to = vec_sub(v:get_ws_position(), pos)
            local len = lMath.length(to)
            if len > 0.001 then
                local dir = vec_multf(to, 1.0/len)
                body:apply_impulse(v:get_ws_position(), vec_multf(dir, self.knockback))
            end
        end
    end
    if gFpsManager ~= nil then
        gFpsManager:report_noise(pos, 40.0)
    end
    self:get_owner():destroy()
end
