---@class FpPlayerController : Component
FpPlayerController = {
    health = 100,
    ammo = 30,
    _move = nil,
    _cam_ent = nil,
    _hud = nil,
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
    local mdelta = lInput.get_mouse_delta()
    self._yaw   = self._yaw   - mdelta.x * sens
    self._pitch = self._pitch - mdelta.y * sens
    self._pitch = math.max(-1.4, math.min(1.4, self._pitch))

    self:get_owner():set_ls_euler_rotation({y = self._yaw})
    self._cam_ent:set_ls_euler_rotation({x = self._pitch})

    -- WASD movement
    local fwd  = lInput.is_key_down(SDL_SCANCODE_W) and 1 or 0
    local back = lInput.is_key_down(SDL_SCANCODE_S) and 1 or 0
    local rgt  = lInput.is_key_down(SDL_SCANCODE_D) and 1 or 0
    local lft  = lInput.is_key_down(SDL_SCANCODE_A) and 1 or 0

    local move_z = (fwd - back)
    local move_x = (rgt - lft)
    local speed = 5.0
    local yaw = self._yaw
    local cos_y = math.cos(yaw)
    local sin_y = math.sin(yaw)

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
        GameplayStatic.change_level(GameplayStatic.get_current_level_name())
    end
end
