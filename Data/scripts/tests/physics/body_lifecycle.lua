-- Smoke test for the PhysicsBody body-type API from Lua (get/set_body_type +
-- set_is_enable). Mirrors the pattern used in import_scripts.lua and confirms
-- the funnel in apply_actor_config() did not regress the script-facing API.

add_test("physics/body/lua_smoke_static_dynamic_toggle", function()
    local ent = GameplayStatic.spawn_entity()
    local body = ent:create_component(BoxComponent)

    assert(body:get_body_type() == BODYTYPE_STATIC, "default body should be static")

    body:set_body_type(BODYTYPE_DYNAMIC)
    assert(body:get_body_type() == BODYTYPE_DYNAMIC, "set Dynamic takes effect")

    body:set_body_type(BODYTYPE_KINEMATIC)
    assert(body:get_body_type() == BODYTYPE_KINEMATIC, "set Kinematic takes effect")

    body:set_body_type(BODYTYPE_STATIC)
    assert(body:get_body_type() == BODYTYPE_STATIC, "set Static takes effect")
end)

add_test("physics/body/lua_smoke_enable_disable", function()
    local ent = GameplayStatic.spawn_entity()
    local body = ent:create_component(BoxComponent)
    body:set_body_type(BODYTYPE_DYNAMIC)

    assert(body:get_is_enabled(), "default enabled")
    body:set_is_enable(false)
    assert(not body:get_is_enabled(), "disable takes effect")
    body:set_is_enable(true)
    assert(body:get_is_enabled(), "re-enable takes effect")
end)
