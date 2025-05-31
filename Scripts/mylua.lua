---@type integer
someVariable = 42

---@type string
someString = "Hello, World!"

---@class ClassBase
ClassBase_obj = {}
ClassBase = "ClassBase"

---@return string
function ClassBase_obj:get_type()
end


---@class Entity : ClassBase
Entity_obj = {}
Entity = "Entity"


---@class EntityComponent : ClassBase
EntityComponent_obj = {}
EntityComponent = "EntityComponent"

---@return Entity
function EntityComponent_obj:get_owner() end

---@type EntityComponent
something = {}

something:get_owner()


function event_trigger()
    -- This function is called when an event is triggered
    print("Event triggered!")
end

---@type integer
gun_damage = 100

---@type number
gun_spread = 0.5

function event_start()
    -- This function starts the event
    print("Event started!")
    -- Trigger the event after some logic
    event_trigger()
end

