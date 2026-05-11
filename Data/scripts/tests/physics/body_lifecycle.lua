-- Smoke test for the PhysicsBody lifecycle setters from Lua. Mirrors the
-- pattern used in import_scripts.lua (set_is_static / set_is_simulating /
-- set_is_enable) and confirms the funnel introduced in apply_actor_config()
-- did not regress the script-facing API.

add_test("physics/body/lua_smoke_static_dynamic_toggle", function()
    local ent = GameplayStatic.spawn_entity()
    local body = ent:create_component(BoxComponent)

    assert(body:get_is_static(), "default body should be static")
    assert(not body:get_is_simulating(), "default body should not simulate")

    body:set_is_static(false)
    body:set_is_simulating(true)
    assert(not body:get_is_static(), "set_is_static(false) takes effect")
    assert(body:get_is_simulating(), "set_is_simulating(true) takes effect")

    -- Flip back. Setting static must canonicalize simulate_physics to false
    -- (per apply_actor_config's illegal-combo guard).
    body:set_is_static(true)
    assert(body:get_is_static(), "set_is_static(true) takes effect")
    assert(not body:get_is_simulating(),
           "static body must not report simulating (canonicalized)")
end)

add_test("physics/body/lua_smoke_enable_disable", function()
    local ent = GameplayStatic.spawn_entity()
    local body = ent:create_component(BoxComponent)
    body:set_is_static(false)
    body:set_is_simulating(true)

    assert(body:get_is_enabled(), "default enabled")
    body:set_is_enable(false)
    assert(not body:get_is_enabled(), "disable takes effect")
    body:set_is_enable(true)
    assert(body:get_is_enabled(), "re-enable takes effect")
end)
