-- Exercises the Transform userdata type crossing the Lua/C++ boundary.
-- See Source/Scripting/LuaTransform.cpp and Source/UnitTests/lua_transform_test.cpp
-- for the binding implementation and its own isolated unit tests; this file
-- covers the same surface through the full engine's live Lua state.

add_test("lua/transform/new_is_identity", function()
    local t = Transform.new()
    local p = t:translation()
    assert(p.x == 0 and p.y == 0 and p.z == 0, "Transform.new() should be identity")
end)

add_test("lua/transform/from_pos_sets_translation", function()
    local t = Transform.from_pos({x=1,y=2,z=3})
    local p = t:translation()
    assert(p.x == 1 and p.y == 2 and p.z == 3, "from_pos should set translation")
end)

add_test("lua/transform/mul_composes", function()
    local a = Transform.from_pos({x=1,y=0,z=0})
    local b = Transform.from_pos({x=0,y=2,z=0})
    local p = (a * b):translation()
    assert(math.abs(p.x - 1) < 1e-5 and math.abs(p.y - 2) < 1e-5,
        "a * b should compose translations")
end)

add_test("lua/transform/inverse_round_trips", function()
    local t = Transform.from_pos_rot({x=3,y=-1,z=2}, {w=1,x=0,y=0,z=0})
    local composed = t * t:inverse()
    local p = composed:translation()
    assert(math.abs(p.x) < 1e-4 and math.abs(p.y) < 1e-4 and math.abs(p.z) < 1e-4,
        "t * t:inverse() should be identity")
end)

add_test("lua/transform/interops_with_lmath_vec3", function()
    -- lMath.vec_add returns a plain lVec3 table; Transform methods must accept
    -- and return that same {x,y,z} shape without any special conversion.
    local base = lMath.vec_add({x=1,y=1,z=1}, {x=1,y=1,z=1})
    local t = Transform.from_pos(base)
    local p = t:translation()
    assert(p.x == 2 and p.y == 2 and p.z == 2, "Transform should interop with lMath vec3 tables")
end)

-- A REF function can return glm::mat4 directly (not via the Transform library);
-- codegen must marshal it as a real Transform userdata (MAT4_TYPE in codegen_lib.py).
add_test("lua/transform/ref_function_returns_mat4", function()
    local t = lMath.mat4_from_pos_rot_scale({x=4,y=5,z=6}, {w=1,x=0,y=0,z=0}, {x=1,y=1,z=1})
    assert(getmetatable(t) ~= nil, "value returned from a REF glm::mat4 function should be Transform userdata")
    local p = t:translation()
    assert(p.x == 4 and p.y == 5 and p.z == 6, "decoded mat4 should carry through the composed position")
end)

-- A REF function can also take glm::mat4 as an argument, receiving a real
-- Transform userdata built on the Lua side.
add_test("lua/transform/ref_function_accepts_mat4_arg", function()
    local t = Transform.from_pos({x=7,y=8,z=9})
    local p = lMath.mat4_get_translation(t)
    assert(p.x == 7 and p.y == 8 and p.z == 9, "REF function taking glm::mat4 should read the Transform's data")
end)
