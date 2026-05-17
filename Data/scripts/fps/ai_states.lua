-- Guard FSM state tables. Each state defines enter/update/exit(guard, dt).
-- States are dispatched by string id stored on the guard.

FPS_GUARD_STATE_PATROL      = "Patrol"
FPS_GUARD_STATE_INVESTIGATE = "Investigate"
FPS_GUARD_STATE_CHASE       = "Chase"
FPS_GUARD_STATE_ATTACK      = "Attack"
FPS_GUARD_STATE_SEARCH      = "Search"
FPS_GUARD_STATE_RETURN      = "Return"

---@type table<string, table>
FpsGuardStates = {}

-- ---- helpers ----------------------------------------------------------

---@param guard FpsGuard
---@return number
local function dist_to_player(guard)
    if gFpsManager == nil or gFpsManager.player == nil then return 1e9 end
    local pp = gFpsManager.player:get_owner():get_ws_position()
    local me = guard:get_owner():get_ws_position()
    return lMath.length(vec_sub(pp, me))
end

---@param guard FpsGuard
---@param target lVec3
local function path_to(guard, target)
    if guard.agent == nil then return end
    guard.agent.move_speed = guard.move_speed
    guard.agent:request_path_to(target)
end

local function noop() end

-- ---- Patrol -----------------------------------------------------------

FpsGuardStates[FPS_GUARD_STATE_PATROL] = {
    enter = function (guard)
        guard.move_speed = guard.patrol_speed
        if #guard.waypoints == 0 then return end
        if guard.current_waypoint_idx < 1 or guard.current_waypoint_idx > #guard.waypoints then
            guard.current_waypoint_idx = 1
        end
        path_to(guard, guard.waypoints[guard.current_waypoint_idx].pos)
        guard.wait_timer = 0.0
    end,
    update = function (guard, dt)
        if guard.awareness >= 0.7 and guard:has_los_to_player() then
            guard:transition(FPS_GUARD_STATE_CHASE)
            return
        end
        if guard.awareness >= 0.3 then
            guard:transition(FPS_GUARD_STATE_INVESTIGATE)
            return
        end
        if #guard.waypoints == 0 then return end
        if guard.agent ~= nil and guard.agent:has_arrived() then
            guard.wait_timer = guard.wait_timer + dt
            local wp = guard.waypoints[guard.current_waypoint_idx]
            if guard.wait_timer >= (wp.wait_time or 0.0) then
                guard.current_waypoint_idx = guard.current_waypoint_idx + 1
                if guard.current_waypoint_idx > #guard.waypoints then
                    guard.current_waypoint_idx = 1
                end
                path_to(guard, guard.waypoints[guard.current_waypoint_idx].pos)
                guard.wait_timer = 0.0
            end
        end
    end,
    exit = noop,
}

-- ---- Investigate ------------------------------------------------------

FpsGuardStates[FPS_GUARD_STATE_INVESTIGATE] = {
    enter = function (guard)
        guard.move_speed = guard.patrol_speed * 1.2
        local target = guard.noise_pos or guard.last_seen_pos
        if target ~= nil then path_to(guard, target) end
        guard.investigate_timer = 0.0
    end,
    update = function (guard, dt)
        guard.investigate_timer = guard.investigate_timer + dt
        if guard.awareness >= 0.7 and guard:has_los_to_player() then
            guard:transition(FPS_GUARD_STATE_CHASE)
            return
        end
        if guard.awareness < 0.15 and guard.investigate_timer > 3.0 then
            guard:transition(FPS_GUARD_STATE_RETURN)
            return
        end
        if guard.agent ~= nil and guard.agent:has_arrived() and guard.investigate_timer > 5.0 then
            guard:transition(FPS_GUARD_STATE_RETURN)
        end
    end,
    exit = noop,
}

-- ---- Chase ------------------------------------------------------------

FpsGuardStates[FPS_GUARD_STATE_CHASE] = {
    enter = function (guard)
        guard.move_speed = guard.patrol_speed * 2.0
        guard.repath_timer = 0.0
    end,
    update = function (guard, dt)
        if gFpsManager == nil or gFpsManager.player == nil then
            guard:transition(FPS_GUARD_STATE_RETURN)
            return
        end
        if not guard:has_los_to_player() then
            guard:transition(FPS_GUARD_STATE_SEARCH)
            return
        end
        local d = dist_to_player(guard)
        if d <= guard.attack_range then
            guard:transition(FPS_GUARD_STATE_ATTACK)
            return
        end
        guard.repath_timer = guard.repath_timer - dt
        if guard.repath_timer <= 0.0 then
            guard.repath_timer = 0.4
            path_to(guard, gFpsManager.player:get_owner():get_ws_position())
        end
    end,
    exit = noop,
}

-- ---- Attack -----------------------------------------------------------

FpsGuardStates[FPS_GUARD_STATE_ATTACK] = {
    enter = function (guard)
        guard.move_speed = 0.0
        if guard.agent ~= nil then
            guard.agent:request_path_to(guard:get_owner():get_ws_position())
        end
    end,
    update = function (guard, dt)
        if gFpsManager == nil or gFpsManager.player == nil then
            guard:transition(FPS_GUARD_STATE_RETURN)
            return
        end
        if not guard:has_los_to_player() then
            guard:transition(FPS_GUARD_STATE_SEARCH)
            return
        end
        local d = dist_to_player(guard)
        if d > guard.attack_range * 1.2 then
            guard:transition(FPS_GUARD_STATE_CHASE)
            return
        end
        local me = guard:get_owner():get_ws_position()
        local pp = gFpsManager.player:get_owner():get_ws_position()
        local to = vec_sub(pp, me)
        local ylen = math.sqrt(to.x*to.x + to.z*to.z)
        if ylen > 0.001 then
            local yaw = math.atan(to.x, -to.z)
            guard:get_owner():set_ls_euler_rotation({x=0, y=yaw, z=0})
        end
        if guard.weapon ~= nil then
            local fire_pos = vec_add(me, {x=0, y=1.5, z=0})
            local fire_dir = normalize(vec_sub(vec_add(pp, {x=0, y=1.2, z=0}), fire_pos))
            guard.weapon:fire(fire_pos, fire_dir, guard:get_owner())
        end
    end,
    exit = noop,
}

-- ---- Search -----------------------------------------------------------

FpsGuardStates[FPS_GUARD_STATE_SEARCH] = {
    enter = function (guard)
        guard.move_speed = guard.patrol_speed * 1.5
        guard.search_timer = 0.0
        guard.search_step_timer = 0.0
        if guard.last_seen_pos ~= nil then
            path_to(guard, guard.last_seen_pos)
        end
    end,
    update = function (guard, dt)
        guard.search_timer = guard.search_timer + dt
        if guard.awareness >= 0.7 and guard:has_los_to_player() then
            guard:transition(FPS_GUARD_STATE_CHASE)
            return
        end
        if guard.search_timer > 8.0 then
            guard:transition(FPS_GUARD_STATE_RETURN)
            return
        end
        guard.search_step_timer = guard.search_step_timer - dt
        if guard.search_step_timer <= 0.0 and guard.agent ~= nil and guard.agent:has_arrived() then
            guard.search_step_timer = 2.0
            local base = guard.last_seen_pos or guard:get_owner():get_ws_position()
            local r = 4.0
            local off = {x=(math.random()*2-1)*r, y=0, z=(math.random()*2-1)*r}
            path_to(guard, vec_add(base, off))
        end
    end,
    exit = noop,
}

-- ---- Return -----------------------------------------------------------

FpsGuardStates[FPS_GUARD_STATE_RETURN] = {
    enter = function (guard)
        guard.move_speed = guard.patrol_speed
        if #guard.waypoints == 0 then return end
        local me = guard:get_owner():get_ws_position()
        local best_i, best_d = 1, 1e9
        for i, w in ipairs(guard.waypoints) do
            local d = lMath.length(vec_sub(w.pos, me))
            if d < best_d then
                best_d = d
                best_i = i
            end
        end
        guard.current_waypoint_idx = best_i
        path_to(guard, guard.waypoints[best_i].pos)
    end,
    update = function (guard, dt)
        if guard.awareness >= 0.7 and guard:has_los_to_player() then
            guard:transition(FPS_GUARD_STATE_CHASE)
            return
        end
        if guard.awareness >= 0.3 then
            guard:transition(FPS_GUARD_STATE_INVESTIGATE)
            return
        end
        if guard.agent == nil or guard.agent:has_arrived() then
            guard:transition(FPS_GUARD_STATE_PATROL)
        end
    end,
    exit = noop,
}
