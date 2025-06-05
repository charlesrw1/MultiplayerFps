

---@class obj.Component
Component = {__index = nil
}
function Component.extend()
    return { __index = Component }
end
function Component:is_a(other)
    local table = self.__index
    while table ~= nil do
        if table == other then
            return true
        else
            table = table.__index
        end
    end
    return false
end

function Component:stuff()
    
end


function ShallowTableCopy(orig)
    local copy = {}
    for k, v in pairs(orig) do
        copy[k] = v
    end
    copy.__index = orig
    return copy
end


CHARACTER_JUMP = 0
CHARACTER_RUN = 1

    ---     @class CharacterAnimator : Component
CharacterAnimator = {
    some_float = 0,
    some_vec = Vec3:new(0),
    some_quat = Quat:new(0),
    some_bool = false,

    a_table = {

    },

    state = CHARACTER_RUN,
}
function CharacterAnimator:on_update()
    self.some_vec = self.some_vec * self.some_float
    if self.some_bool then
        self.state = CHARACTER_JUMP
    end
end


-- class decl
---@class GameManager : obj.Component
GameManager_T = {

    ---@type Object         -- type annotations, for editor
    ending_object = nil,

    ---@type CharacterAnimator
    player = nil,

    -- test types can be inferred
    a_number = 0,
    on_round_restart    = Signal:new(),
    where               = Vec3:new(),

    ---@type KartManager
    kart_mgr            = KartManager:new(),

    time_mgr            = TimeManager:new()
}

-- global variable
---@type GameManager
GameManager = nil

function GameManager_T:start()
    GameManager = self

    self.ending_object.area = 0
    self.where.x = 0
    self.where.y = 0
    local vec = self.where:cross(self.where)
    local l = vec:length()
    local num = self.where:dot(Vec3:new(0))
    self.where.z = num:length() + l

    local t = Transform:new(vec)
    t = t * Transform:new(Quat:new())

    self.on_round_restart:add(function (param)
        print(param)
    end)
    self.on_round_restart:call(1)
    
    self.kart_mgr:tick()

    self.time_mgr.current_lap = 0
    
    --Debug.text(self.where,"Hello world",0)
    --Debug.box(self.where,Vec3:new(0.5),1.0)

end

    ---    @class Object : RootObject
Object = {area = 0, length = 0, breadth = 0, __index = RootObject
}

function Object:do_stuff()
end
function Object:update()
    GameManager.on_round_restart:add(function ()
        print("HELLO !!")
    end)
    self.owner:extend()
    for i in ipairs(Object) do
        self:stuff()
    end
    ---@type Object
    local newobj = Object:new()
    newobj:update()
end

RequiresElectricity = {}


function Object:interact()


end

function Object:start()
    if self.implements[Interactable] then
        self:interact()
    end
end