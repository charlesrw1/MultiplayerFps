-- FpsGuard: stealth AI. NavAgent-driven movement + 6-state FSM + sight/hearing
-- perception. Runtime-only; FpsGameManager constructs guards from FpsGuardSpawn
-- data components.

---@class FpsGuard : Component
FpsGuard = {}

FPS_GUARD_SIGHT_RANGE      = 18.0
FPS_GUARD_SIGHT_FOV_COS    = 0.5    -- cos(60 deg)
FPS_GUARD_HEARING_RANGE    = 12.0
FPS_GUARD_AWARENESS_GAIN   = 1.4    -- per second when seeing player
FPS_GUARD_AWARENESS_DECAY  = 0.35   -- per second when not seeing
FPS_GUARD_ATTACK_RANGE     = 15.0
FPS_GUARD_EYE_HEIGHT       = 1.6

function FpsGuard:start()
    self:set_ticking(true)

    -- These get filled in by FpsGameManager just after create_component:
    self.waypoint_group = ""
    self.waypoints      = {}   -- array of {pos, wait_time}
    self.patrol_speed   = 2.0
    self.weapon         = nil  -- FpsWeapon

    -- Cached navigation agent (Manager creates it on the same entity).
    self.agent = self:get_owner():get_component(NavAgentComponent)

    -- Blackboard:
    self.awareness         = 0.0
    self.last_seen_pos     = nil
    self.noise_pos         = nil
    self.move_speed        = self.patrol_speed
    self.attack_range      = FPS_GUARD_ATTACK_RANGE
    self.current_waypoint_idx = 1
    self.wait_timer        = 0.0
    self.investigate_timer = 0.0
    self.repath_timer      = 0.0
    self.search_timer      = 0.0
    self.search_step_timer = 0.0

    self.state = nil
    self:transition(FPS_GUARD_STATE_PATROL)

    -- Wire death.
    local hp = self:get_owner():get_component(FpsHealth)
    if hp ~= nil then
        hp.on_death:add(function () self:_on_killed() end)
    end
end

function FpsGuard:_on_killed()
    self:set_ticking(false)
    self:get_owner():destroy()
end

---@param new_state string
function FpsGuard:transition(new_state)
    if self.state == new_state then return end
    if self.state ~= nil then
        local s = FpsGuardStates[self.state]
        if s ~= nil and s.exit ~= nil then s.exit(self) end
    end
    self.state = new_state
    local s2 = FpsGuardStates[new_state]
    if s2 ~= nil and s2.enter ~= nil then s2.enter(self) end
end

---@return boolean
function FpsGuard:has_los_to_player()
    if gFpsManager == nil or gFpsManager.player == nil then return false end
    local me = self:get_owner():get_ws_position()
    local eye = vec_add(me, {x=0, y=FPS_GUARD_EYE_HEIGHT, z=0})
    local pp, _ = gFpsManager.player:get_view_pos_and_dir()
    local mask = GameplayStatic.get_collision_mask_for_physics_layer(PL_VISIBLITY)
    local cap = self:get_owner():get_component(CapsuleComponent)
    local res = GameplayStatic.cast_ray(eye, pp, mask, cap)
    if res.hit and res.what ~= nil then
        -- Hit something between eye and player. Check if it's the player.
        return res.what:get_component(FpsPlayer) ~= nil
    end
    -- Ray didn't hit anything before reaching destination: assume clear LOS to player.
    return true
end

function FpsGuard:update()
    local dt = GameplayStatic.get_dt()
    self:_update_perception(dt)
    local s = FpsGuardStates[self.state]
    if s ~= nil and s.update ~= nil then
        s.update(self, dt)
    end
end

---@param dt number
function FpsGuard:_update_perception(dt)
    -- SIGHT
    local saw_player = false
    if gFpsManager ~= nil and gFpsManager.player ~= nil then
        local me = self:get_owner():get_ws_position()
        local eye = vec_add(me, {x=0, y=FPS_GUARD_EYE_HEIGHT, z=0})
        local pp = gFpsManager.player:get_owner():get_ws_position()
        local to = vec_sub(pp, eye)
        local dist = lMath.length(to)
        if dist < FPS_GUARD_SIGHT_RANGE and dist > 0.001 then
            local to_n = vec_multf(to, 1.0/dist)
            local forward = lMath.angles_to_vector(0.0, self:_get_yaw())
            local cosang = lMath.dot(to_n, forward)
            if cosang > FPS_GUARD_SIGHT_FOV_COS then
                if self:has_los_to_player() then
                    saw_player = true
                    self.last_seen_pos = pp
                end
            end
        end
    end
    if saw_player then
        self.awareness = math.min(1.0, self.awareness + FPS_GUARD_AWARENESS_GAIN*dt)
    else
        self.awareness = math.max(0.0, self.awareness - FPS_GUARD_AWARENESS_DECAY*dt)
    end

    -- HEARING (consume from manager)
    if gFpsManager ~= nil then
        local me = self:get_owner():get_ws_position()
        for _, ev in ipairs(gFpsManager.noise_events) do
            local d = lMath.length(vec_sub(ev.pos, me))
            if d < math.min(FPS_GUARD_HEARING_RANGE, ev.loudness*0.6) then
                self.noise_pos = ev.pos
                self.awareness = math.min(1.0, self.awareness + 0.25)
            end
        end
    end
end

---@return number
function FpsGuard:_get_yaw()
    local q = self:get_owner():get_ws_rotation()
    local e = lMath.to_euler(q)
    return e.y
end
