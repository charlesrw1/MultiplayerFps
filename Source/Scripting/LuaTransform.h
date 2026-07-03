#pragma once
// Lua userdata binding for glm::mat4, exposed to scripts as `Transform`.
//
// Unlike the rest of the Lua boundary (lVec3/lQuat/lMath in ScriptFunctionCodegen.h,
// which cross as plain tables via codegen'd REF structs/functions), Transform is a
// hand-written full-userdata type with a real metatable: Transform.new(), t:inverse(),
// t1 * t2, tostring(t), etc. There is no codegen involvement and no other userdata type
// in this codebase to mirror — this is the first one. vec3/quat arguments and return
// values still use the existing lVec3/lQuat table shape ({x,y,z} / {w,x,y,z}) so scripts
// can pass values between lMath and Transform freely.
//
// See Data/scripts/tests/transform_userdata.lua for usage examples and
// Source/UnitTests/lua_transform_test.cpp for the binding's own test coverage.
extern "C"
{
#include <lua.h>
}

// Registers the Transform userdata metatable + global constructor table into L.
// Call once per lua_State after luaL_openlibs(). Safe to call multiple times on the
// same L only if the state was closed/reopened in between (re-registering into a live
// state would just overwrite the existing metatable and global, which is harmless but
// pointless).
void register_lua_transform(lua_State* L);
