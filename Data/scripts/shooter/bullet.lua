---@class BulletProjectile : Component
BulletProjectile = {
    _direction = nil,
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

    -- Raycast along movement
    local result = GameplayStatic.cast_ray(pos, {x=pos.x+dx, y=pos.y+dy, z=pos.z+dz}, 1, nil)
    if result.hit then
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
