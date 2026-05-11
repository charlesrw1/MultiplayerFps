

ENEMY_IDLE_STATE = 0
ENEMY_RUN_AWAY_STATE = 1
ENEMY_CHASE_STATE = 2

---@class EnemyComponent : Component
EnemyComponent = {
    health = 0,
    state = ENEMY_IDLE_STATE,

    ---@type Component
    movement_component = nil,

    ---@type Signal
    on_damage_taken = nil,
     ---@type Signal
    on_entered_combat = nil,
}

function EnemyComponent:start()
    self:set_ticking(true)
    self.health = 100

    self.on_damage_taken = Signal.new()
    self.on_entered_combat = Signal.new()
    
end
function EnemyComponent:stop()
    
end
function EnemyComponent:update()  
    if self.state == ENEMY_IDLE_STATE then
        -- check for player in radius

    elseif self.state==ENEMY_CHASE_STATE then
        -- chase player

    elseif self.state == ENEMY_RUN_AWAY_STATE then
        -- run away for player for x seconds, then go back to idle
    end
end
