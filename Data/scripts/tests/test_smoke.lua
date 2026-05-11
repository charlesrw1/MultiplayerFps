-- Lua integration smoke tests. Confirms the test path is wired end-to-end:
-- ScriptManager.load_test_scripts() -> add_test() registration ->
-- TestRunner.start_lua_phase() -> _lua_run_all_tests coroutine ->
-- LuaTestRunner.report -> integration_<mode>_results.xml.

add_test("lua/smoke/arithmetic", function()
    assert(1 + 1 == 2, "arithmetic broken")
    assert(("ab"):rep(2) == "abab", "string ops broken")
end)

add_test("lua/smoke/yield", function()
    local t0 = GameplayStatic.get_time()
    coroutine.yield(0.1)
    local dt = GameplayStatic.get_time() - t0
    assert(dt >= 0.05, "expected >=0.05s elapsed after yield(0.1), got " .. tostring(dt))
end)

add_test("lua/smoke/gameplay_static", function()
    -- Engine API surface should be callable from the test coroutine.
    local t = GameplayStatic.get_time()
    assert(type(t) == "number", "get_time should return number, got " .. type(t))
    assert(GameplayStatic.get_dt() >= 0.0, "dt must be non-negative")
    assert(type(GameplayStatic.is_editor()) == "boolean", "is_editor should return boolean")
end)
