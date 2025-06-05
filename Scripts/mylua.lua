


Prefab = {}

---@class Signal
Signal = {
    funcs = {}
}
---@param func fun(...):nil
function Signal:add(func)
    self.funcs[#self.funcs+1]=func
end
function Signal:call(...)
    
end

---@return Signal
function Signal:new()
    return Signal
end

local function create_asset(type, name)
    return nil
end

local function create_event()
    local table = {
        listeners = {},
        add = function (self, what_function)
            self.listeners[#self.listeners+1] = what_function
        end,
        call = function (self,...)
            for _, value in ipairs(self.listeners) do
                value(...)
            end
        end
    }

    return table
end

---@class Interactable
Interactable = {}

---@class Component
Component = {__index = nil, implements = {}}
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


---@class TriggerRegion : Component
TriggerRegion = {}
function TriggerRegion:new()
    return {
        box = 0
    }
end

local var = 0
local x = 0

function inherit(type)
end

inherit(Interactable)

local function start()

end

function ShallowTableCopy(orig)
    local copy = {}
    for k, v in pairs(orig) do
        copy[k] = v
    end
    copy.__index = orig
    return copy
end

---@class RootObject
RootObject = {
    implements={}, 
    ---@type Component
    owner=nil
}

function RootObject:new()
    return ShallowTableCopy(self)
end
function RootObject:ptr()
    return {ptr_type=self}
end

---@class Vec3
---@field x number
---@field y number
---@field z number
Vec3 =  {
}
---@return Vec3
function Vec3:new(...) end
---@return number
function Vec3:length() end
---@return Vec3
---@param other Vec3
---@return number
function Vec3:dot(other) end
---@return Vec3
---@param other Vec3
---@return Vec3
function Vec3:cross(other) end

---@class Transform
Transform = {
}
---@return Transform
function Transform:new(...) end



---@class Vec3
Quat = {
}

---@class SomeEvent : AnimationEvent
SomeEvent = {
    left_foot = false
}
function SomeEvent:triggered(obj)
    if self.left_foot then
        assert(obj:is_a(Component))
    end
end

Ptr = {
    type = {}
}
function Ptr:new(type)
    return {type=type}
end

---@class SomeScript : Component
SomeScript = {
    ---@type Component
    target = nil
}
function SomeScript:start()
    self.target:stuff()
end

CHARACTER_JUMP = 0
CHARACTER_RUN = 1

---@class CharacterAnimator : Animator
CharacterAnimator = {
    some_float = 0,
    some_vec = Vec3:new(0),
    some_quat = Quat:new(0),
    some_bool = false,

    state = CHARACTER_RUN,
}
function CharacterAnimator:on_update()
    self.some_vec = self.some_vec * self.some_float
    if self.some_bool then
        self.state = CHARACTER_JUMP
    end
end

-- test are classes just in script
---@class KartManager : RootObject
KartManager = {
    players = {},
    following_player = {}
}
function KartManager:tick()
    
end

---@class TimeManager : RootObject
TimeManager = {
    current_lap = 0,
    current_time = 0.0
}

local function level1_script()
    local item = find_by_name("the_item")
    item.PhysicsBody.triggered:add()
end
local function level2_script()
    local box = find_by_name("the_item")
end

---@class PuzzleScript : Component
PuzzleScript = {
    board = {}
}

LEVEL_SCRIPTS = {
    level1_script,
    level2_script
}

-- class decl
---@class GameManager : Component
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
    kart_mgr            = nil,
    time_mgr            = nil
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
    
    Debug.text(self.where,"Hello world",0)
    Debug.box(self.where,Vec3:new(0.5),1.0)

end

Debug = {

}
function Debug.box(where, size, time)
    
end
function Debug.text(where, string, time)
    
end



-- declare an interface for damagable things

---@class IDamagable
IDamagable = {
}
function IDamagable:take_damange(dmg) end

-- declare a Player class. inherit it from Component and the damageble interface

---@class Player : Component, IDamagable
Player = {      -- start your class with "<classname> = {"
    -- declare your variables here
    is_alive = true,

    -- use type annotations for intellisense and refelction to the game engine
    ---@type Component
    another_component_in_the_game = nil,

    -- built-in lua types can be inferred and dont need type annotations to work
    name = ""
}   -- end your class with "}" on a newline, sorry my parser sucks

-- add functions

---@return boolean
function Player:another_func()
    return self.is_alive and self.another_component_in_the_game ~= nil
end

function Player:update()
    print("hello world " + self.name)
    local cond = self:another_func()
    if cond then
        print("blah blah")
        self.another_component_in_the_game:stuff()
    end
end

function Player:take_damange(dmg)
    print("ouch")
end

---@class Object : RootObject
Object = {area = 0, length = 0, breadth = 0, __index = RootObject}
Object.implements = {Interactable}
function Object:do_stuff()
end
function Object:update()
    G_Game.on_round_restart:add()
    G_Game.

    self.owner:extend()
    for i in ipairs(Object) do
        self:stuff()
    end
    ---@type Object
    local newobj = Object:new()
    newobj:update()
end

RequiresElectricity = {}


LightSwitch = {
    is_on = false,
    on_used = create_event()
}
LightSwitch.implements = {Interactable, RequiresElectricity}

function LightSwitch:interact()
    self.on_used.call()
end
function LightSwitch:is_on()

end

Piece = {
    x = 0,
    y = 0,
    game_object = nil
}
function Piece:remove()
    self.game_object:remove()
end





local var = 0

function Object:interact()




end
---@
function Object:start()
    if self.implements[Interactable] then
        self:interact()
    end
end

local o = Object:new()
o.something = 0




--- Object:new()

---@return Object
function get_some() return nil
end


local b = get_some()
b.name = 1




o.things

o.name = 1
o.things = "string"
if o.type.implments[Interactable]~=nil then
    o.do_something()
end

---@class MyClass : Interface2
MyClass = {
    owner = {},
    variable = 0,
    what_asset = create_asset(Prefab, "MyAsset.pfb"),
    object_in_world = 0,
    some_event = create_event(),
}
MyClass.__index = MyClass
setmetatable(MyClass, { __index = Interface2 })


function MyClass:shoot()
    return 0 -- Should return a number to match the interface
end
function MyClass:get_gun_name()
    return "The"
end

local function find_by_name(obj_name)
    
end
local function find_by_class(classname)
    
end
local function find_by_tag(tag)
    
end

HitboxTag = "HitboxTag"

local p = owner.find_by_name("entity_name") -- thing with unique name, searches down the current prefab heirarchy
--- spawned objects arent given a name, only editor placed ones
--- if you have nested prefabs, then you need to find_by_name multiple times. find prefab, then find in prefab etc.
--- this lets you make reusable prefabs that can be placed in the scene
local p2 = owner.find_by_class(MyClass)  -- things with component of classname
local p3 = owner.find_by_tag(HitboxTag)      -- things with tags

function MyClass:start()
    local p = self.owner.PhysicsBody

    ---@type MyClass
    local s = self.owner.MyClass

    self.variable = 0
    self.some_event:call("some string")
    self.some_event:add(function()
        print("hello world" + self.variable)
    end)


    local pfb = game:spawn_prefab(self.what_asset)
end


EventBuilder = {
}


function EventBuilder:add(timestamp, callback_func)

end

EventBuilder:add(0.0, function(owner)
    ---@type MyClass
    local p = owner.MyClass
    sounds:play("XyzSound")
end)

EventBuilder:add(10.0, function(owner)
    sounds:play("XyzSound")
end)

EventBuilder:add({
    start_time=0,
    end_time=10,
    enter = function (self)
        
    end,
    exit = function (self)
        
    end
})

function Construct(classname)
    return nil
end

---@class MyClass
local obj = Construct(MyClass)
obj.x = 0
obj.y = 0
obj.name = ""
return obj

-- script types:
-- "level script": anonomous, global
-- "component script": 

function Requires(what)
    
end

---@param obj MyClass
function Ctor(obj)
    obj.x = 10
    obj.other = find_by_name("some_other").MyClass
end
return Requires("MyClass")

local phys_body = owner.PhysicsBody

---@class DoorScript
DoorScript = {
    is_open = false,
    door_handle = nil
}
function DoorScript:prestart()
    self.door_handle = find_by_name("door_handle")
end

---@class LockedDoorScript
LockedDoorScript = {
    door = nil,
    locked = false,
    key_type = 0
}

global_game = nil

function LockedDoorScript:init()
    self.door = find_by_class(DoorScript)
end

---@class Script : Component, Interface -- can inherit from interfaces which lets c++ call this. this enables casting as well
Script = {
    what_to_target = nil,
    numbers = {}
}
return Script   --- parser detects @class and what ends up getting returned. 

function Script:init_door_area(self)
    local d = find_by_class("a")
    local p = d:cast(Interface)
    find_by_name("lockscript_1")
end

Script = nil

find_by_class(Script)

function Script:prestart(self)
    self.what_to_target:add()

    phys_body.triggered:add(function (other_body)
        other_body.something()
    end)

    local obj = game.create()
    local mc = obj.add(MyClass)
    mc.some()
    mc.var = 0
end
function Script:start()
    
end

function Script:on_end()

end

return Script

local o = Create("OtherScript")
o.value = 0
return o


Level0 = {

}

--- script -> components
--- 


--- components: things you can attach to objects
--- go inside their own file
--- initialization:
--- to init the variables of a script, use other scripts
--- constructor(): called before prestart and start
--- 
--- component typenames
--- 
--- each script returns some object, which is what the component is
--- 
--- execution order:
--- object1
--- prefab1
---     object
---     another_prefab
--- object2
--- prefab2
--- 
--- (object1, object2) then (prefab1, prefab2) then (another_prefab)

--- init objects created in level 0
--- init sub prefabs, etc.
--- stuff created after is initialized first