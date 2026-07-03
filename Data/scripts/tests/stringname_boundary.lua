-- Exercises StringName crossing the Lua/C++ boundary via LuaSystem.name /
-- LuaSystem.name_to_string. See Source/Framework/StringName.cpp and
-- get_stringname_from_lua/push_stringname_to_lua in Source/Scripting/ScriptFunctionCodegen.cpp.

add_test("lua/stringname/interning_is_stable", function()
    local a = LuaSystem.name("idle")
    local b = LuaSystem.name("idle")
    assert(type(a) == "number", "StringName should cross as a number (hash), got " .. type(a))
    assert(a == b, "interning the same string twice must produce the same hash")
end)

add_test("lua/stringname/different_strings_differ", function()
    local a = LuaSystem.name("idle")
    local b = LuaSystem.name("run")
    assert(a ~= b, "different strings must not hash equal")
end)

add_test("lua/stringname/round_trips_through_debug_name", function()
    local h = LuaSystem.name("my_test_name")
    assert(LuaSystem.name_to_string(h) == "my_test_name",
        "name_to_string should resolve the hash back to its original string")
end)

add_test("lua/stringname/empty_string_is_null", function()
    local h = LuaSystem.name("")
    assert(h == 0, "empty string should intern to the null StringName (hash 0)")
end)
