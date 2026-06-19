
--- @class fpsLuaBridgeImpl : fpsLuaBridge
fpsLuaBridgeImpl = {}

---@class fpsLuaEventBusFunctionImpl : LuaEventBusFunction
fpsLuaEventBusFunctionImpl = {
    ---@type function
    invokeFunc = nil
}
function fpsLuaEventBusFunctionImpl:invoke(invoker,payload)
    self.invokeFunc(invoker,payload)
end

---@param event_id integer
---@param entity Entity
---@param this ClassBase
---@param callback function
local function listen_for_ent_event(event_id, entity, this, callback)
    local cpp_class = ClassBase.alloc(fpsLuaEventBusFunctionImpl)
    cpp_class.invokeFunc = callback
    entity:listen_for_event_lua(event_id,this,cpp_class)
end

PLAYER_ENTER = 100

local function physics_test_level_init(this)
    local trigger0 = GameplayStatic.find_by_name("trigger0")
    local light  = GameplayStatic.find_by_name("light0")
    assert(light and trigger0)
    listen_for_ent_event(PLAYER_ENTER,trigger0,this,function ()
        light:get_component(PointLightComponent):destroy()
    end)
    trigger0:broadcast_event_lua(PLAYER_ENTER,"")

end

function fpsLuaBridgeImpl:start_level_script()
    local levelname = GameplayStatic.get_current_level_name()
    physics_test_level_init()
end