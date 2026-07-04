#pragma once
// Lua metatables + constructors for lVec3/lQuat.
//
// lVec3/lQuat still cross the Lua boundary as plain {x,y,z}/{w,x,y,z} tables via
// codegen (push_lVec3_to_lua/get_lVec3_from_lua, auto-generated per-struct in
// MEGA.gen.cpp) -- unlike Transform (see LuaTransform.h) they are NOT userdata,
// so scripts can still pass literal tables ({x=1,y=2,z=3}) anywhere a vec3 is
// expected. This file only attaches a metatable ("lVec3_mt"/"lQuat_mt", set by
// codegen_generate.py's write_struct_getter_setter special case) giving those
// tables operators (+, -, *, ==, unary -) and methods (:length(), :normalize(),
// ...). A hand-built literal table has no metatable until it round-trips through
// a push function (or Vec3.new()/Quat.new()), so it won't have operators/methods
// -- an accepted, minor inconsistency vs. rewriting the whole vec3/quat boundary
// to userdata.
//
// See Data/scripts/vec_quat_lua_stubs.lua for EmmyLua stubs and
// Source/UnitTests/lua_vec_quat_test.cpp for this binding's test coverage.
extern "C"
{
#include <lua.h>
}

// Registers the lVec3_mt/lQuat_mt metatables plus the Vec3/Quat global
// constructor tables into L. Call once per lua_State after luaL_openlibs(),
// alongside register_lua_transform.
void register_lua_vec_quat(lua_State* L);
