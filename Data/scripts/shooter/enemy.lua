---@class EnemyController : Component
EnemyController = {
    health = 30,
    _move = nil,
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
