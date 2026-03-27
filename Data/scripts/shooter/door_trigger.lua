---@class DoorTrigger : Component
DoorTrigger = {
    _levels = nil,
    _current_index = 1,
}

function DoorTrigger:init(levels, current_index)
    self._levels = levels
    self._current_index = current_index
end

function DoorTrigger:start()
    self:set_ticking(true)
end

function DoorTrigger:update()
    local app = Application.get_app()
    if app:get_enemies_alive() > 0 then return end

    local player_ent = GameplayStatic.find_by_name("player")
    if player_ent == nil then return end

    local my_pos = self:get_owner():get_ws_position()
    local pl_pos = player_ent:get_ws_position()
    local dx = pl_pos.x - my_pos.x
    local dy = pl_pos.y - my_pos.y
    local dz = pl_pos.z - my_pos.z
    local dist = math.sqrt(dx*dx + dy*dy + dz*dz)

    if dist < 2.0 then
        local next_index = self._current_index + 1
        if next_index <= #self._levels then
            app:advance_level(next_index)
        else
            local hud_comp = player_ent:get_component(HudDrawer)
            if hud_comp ~= nil then
                hud_comp.room_index = #self._levels + 1
            end
        end
        self:get_owner():destroy()
    end
end
