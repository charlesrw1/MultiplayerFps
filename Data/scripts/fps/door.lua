-- FpsDoor: interactable rotating door. Wired onto the data-component's owner
-- entity by FpsGameManager at level start.
-- Provides `:interact_with(by_entity)` which FpsInteractor will invoke.

FPS_DOOR_CLOSED  = 0
FPS_DOOR_OPENING = 1
FPS_DOOR_OPEN    = 2
FPS_DOOR_CLOSING = 3

---@class FpsDoor : Component
FpsDoor = {
    ---@type boolean
    locked = false,
    ---@type string
    key_id = "",
    ---@type number
    open_time = 0.6,
    ---@type number
    open_angle_rad = 1.5707963,  -- pi/2
}

function FpsDoor:start()
    self.state = FPS_DOOR_CLOSED
    self.alpha = 0.0
    self.base_yaw = self:get_owner():get_ls_rotation()  -- snapshot starting rotation
end

---@param by_entity Entity|nil
function FpsDoor:interact_with(by_entity)
    if self.locked then
        if self.key_id ~= "" and by_entity ~= nil then
            local p = by_entity:get_component(FpsPlayer)
            if p ~= nil and p:has_key(self.key_id) then
                self.locked = false
            else
                return  -- still locked, ignore
            end
        else
            return
        end
    end
    if self.state == FPS_DOOR_CLOSED or self.state == FPS_DOOR_CLOSING then
        self.state = FPS_DOOR_OPENING
    else
        self.state = FPS_DOOR_CLOSING
    end
    self:set_ticking(true)
end

function FpsDoor:update()
    local dt = GameplayStatic.get_dt()
    if self.state == FPS_DOOR_OPENING then
        self.alpha = self.alpha + dt/self.open_time
        if self.alpha >= 1.0 then
            self.alpha = 1.0
            self.state = FPS_DOOR_OPEN
            self:set_ticking(false)
        end
    elseif self.state == FPS_DOOR_CLOSING then
        self.alpha = self.alpha - dt/self.open_time
        if self.alpha <= 0.0 then
            self.alpha = 0.0
            self.state = FPS_DOOR_CLOSED
            self:set_ticking(false)
        end
    end
    local eased = lMath.eval_easing(EASING_CUBICEASEOUT, self.alpha) * self.open_angle_rad
    self:get_owner():set_ls_euler_rotation({x=0, y=eased, z=0})
end
