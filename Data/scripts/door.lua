---@class Interactable : Component
Interactable = {
    ---@type function
    interact_with = nil
}

FPDOOR_CLOSED = 0
FPDOOR_OPEN = 1
FPDOOR_CLOSING = 2
FPDOOR_OPENING = 3
DOOR_OPEN_TIME = 0.6

---@class FpDoor : Component
---editor
FpDoor = {
    ---@type number
    myfield = 0
}


function FpDoor:start()
    self.state = FPDOOR_CLOSED
    self.alpha = 0
    self.is_locked = false
    self.on_state = Signal.new()
end
---@param locked boolean
---@param is_open boolean
function FpDoor:set_state(locked,is_open)
    self.is_locked = locked
    if is_open then
       self.alpha = 1
       self.state = FPDOOR_OPEN
       self:get_owner():set_ls_euler_rotation({y=math.pi*0.5})
    end
end

function FpDoor:update()
    local dt = GameplayStatic.get_dt()
    if self.state==FPDOOR_CLOSING then
        self.alpha = self.alpha - dt/DOOR_OPEN_TIME
        if self.alpha <= 0.0 then
            self.alpha = 0
            self.state = FPDOOR_CLOSED
            self:set_ticking(false)
            self.on_state:invoke({is_open=false})
        end
    elseif self.state==FPDOOR_OPENING then
        self.alpha = self.alpha + dt/DOOR_OPEN_TIME
        if self.alpha >= 1.0 then
            self.alpha = 1
            self.state = FPDOOR_OPEN
            self:set_ticking(false)
            self.on_state:invoke({is_open=true})
        end
    else
        self.alpha = 0
        self.state = FPDOOR_CLOSED
        self:set_ticking(false)
    end

    local a = lMath.eval_easing(EASING_CUBICEASEOUT,self.alpha)*math.pi*0.5
    self:get_owner():set_ls_euler_rotation({y=a})
end
function FpDoor:stop()
    
end

function FpDoor:interact()
    if self.state==FPDOOR_OPEN then
        self.state = FPDOOR_CLOSING
    elseif self.state==FPDOOR_CLOSED and not self.is_locked then
        self.state = FPDOOR_OPENING
    elseif self.state==FPDOOR_CLOSING then
        self.state = FPDOOR_OPENING
    elseif self.state==FPDOOR_OPENING then
        self.state = FPDOOR_CLOSING
    end
    self:set_ticking(true)
end