
function delete_thing()
    ---@type MeshComponent[]
    local meshcomponents = GameplayStatic.find_components(MeshComponent)
    for index, value in ipairs(meshcomponents) do
        local pos = value:get_owner():get_ws_position()
        local mod = value:get_model()
        if mod and mod:get_name_l()=="work_prop/gas_cylinder.cmdl" then
            print("deleting")
            value:destroy_deferred()
        end
    end
end

if GameplayStatic.is_editor() then
   --delete_thing()
end

---@class MyClass : InterfaceClass
MyClass = {
}
function MyClass:buzzer()
    print("buzzing")
end
function MyClass:get_value(str)
    print(self:my_type():get_classname())

    local d = ClassBase.alloc(MyClass)
    d:buzzer()
    ClassBase.free(d)

    self:set_var(5)
    self:self_func()
    StaticClass.do_something()
    return StaticClass.get_int() + string.len(str)
end


---@return FpPlayer
function find_player()
    return GameplayStatic.find_components(FpPlayer)[1]
end

if not GameplayStatic.is_editor() then

  --  ---@type FpPlayer
  --  local player = find_player()
  --  player.weapon_data:switch_to(WEAPON_GRENADE_LAUNCHER)
  --  local flashlight = player.cam:get_owner():create_component(SpotLightComponent)
  --  flashlight:set_shadows(true)
end