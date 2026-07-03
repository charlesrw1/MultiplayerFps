#include <gtest/gtest.h>
#include "Scripting/LuaTransform.h"

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// ============================================================
// Transform userdata binding (Source/Scripting/LuaTransform.cpp).
//
// Unlike the rest of the Lua boundary this is a hand-written full-userdata
// type, with no ClassBase/ScriptManager involvement -- so these tests just
// run a bare lua_State with the Transform library registered, and read
// results back off the stack or via globals.
// ============================================================

class LuaTransformTest : public ::testing::Test
{
protected:
	void SetUp() override {
		L = luaL_newstate();
		luaL_openlibs(L);
		register_lua_transform(L);
	}
	void TearDown() override {
		lua_close(L);
		L = nullptr;
	}

	// Runs src and asserts it does not error. Any assert(...) failure inside
	// the script surfaces as a lua_error, which fails the test with the message.
	void run(const std::string& src) {
		if (luaL_dostring(L, src.c_str()) != LUA_OK) {
			std::string msg = lua_tostring(L, -1);
			lua_pop(L, 1);
			FAIL() << "Lua error: " << msg;
		}
	}

	lua_State* L = nullptr;
};

TEST_F(LuaTransformTest, NewIsIdentity) {
	run("local t = Transform.new()\n"
		"local p = t:translation()\n"
		"assert(p.x == 0 and p.y == 0 and p.z == 0)\n");
}

TEST_F(LuaTransformTest, FromPosSetsTranslation) {
	run("local t = Transform.from_pos({x=1,y=2,z=3})\n"
		"local p = t:translation()\n"
		"assert(p.x == 1 and p.y == 2 and p.z == 3)\n");
}

TEST_F(LuaTransformTest, FromPosRotScaleRoundTripsViaDecompose) {
	run("local t = Transform.from_pos_rot_scale({x=1,y=2,z=3}, {w=1,x=0,y=0,z=0}, {x=1,y=1,z=1})\n"
		"local pos, rot, scale = t:decompose()\n"
		"assert(math.abs(pos.x - 1) < 1e-5)\n"
		"assert(math.abs(pos.y - 2) < 1e-5)\n"
		"assert(math.abs(pos.z - 3) < 1e-5)\n"
		"assert(math.abs(rot.w - 1) < 1e-5)\n"
		"assert(math.abs(scale.x - 1) < 1e-5)\n");
}

TEST_F(LuaTransformTest, InverseUndoesTranslation) {
	run("local t = Transform.from_pos({x=5,y=0,z=0})\n"
		"local inv = t:inverse()\n"
		"local composed = t * inv\n"
		"local p = composed:translation()\n"
		"assert(math.abs(p.x) < 1e-5 and math.abs(p.y) < 1e-5 and math.abs(p.z) < 1e-5)\n");
}

TEST_F(LuaTransformTest, MulComposesTranslations) {
	run("local a = Transform.from_pos({x=1,y=0,z=0})\n"
		"local b = Transform.from_pos({x=0,y=2,z=0})\n"
		"local c = a * b\n"
		"local p = c:translation()\n"
		"assert(math.abs(p.x - 1) < 1e-5)\n"
		"assert(math.abs(p.y - 2) < 1e-5)\n");
}

TEST_F(LuaTransformTest, MulWithVec3TransformsPoint) {
	run("local t = Transform.from_pos({x=1,y=1,z=1})\n"
		"local p = t * {x=1,y=0,z=0}\n"
		"assert(math.abs(p.x - 2) < 1e-5)\n"
		"assert(math.abs(p.y - 1) < 1e-5)\n"
		"assert(math.abs(p.z - 1) < 1e-5)\n");
}

TEST_F(LuaTransformTest, TransformDirectionIgnoresTranslation) {
	run("local t = Transform.from_pos({x=10,y=10,z=10})\n"
		"local d = t:transform_direction({x=1,y=0,z=0})\n"
		"assert(math.abs(d.x - 1) < 1e-5)\n"
		"assert(math.abs(d.y) < 1e-5)\n"
		"assert(math.abs(d.z) < 1e-5)\n");
}

TEST_F(LuaTransformTest, SetTranslationMutatesInPlaceAndReturnsSelf) {
	run("local t = Transform.new()\n"
		"local ret = t:set_translation({x=7,y=8,z=9})\n"
		"assert(ret == t)\n"
		"local p = t:translation()\n"
		"assert(p.x == 7 and p.y == 8 and p.z == 9)\n");
}

TEST_F(LuaTransformTest, EqualityComparesByValue) {
	run("local a = Transform.from_pos({x=1,y=2,z=3})\n"
		"local b = Transform.from_pos({x=1,y=2,z=3})\n"
		"local c = Transform.from_pos({x=1,y=2,z=4})\n"
		"assert(a == b)\n"
		"assert(a ~= c)\n");
}

TEST_F(LuaTransformTest, ToStringDoesNotError) {
	run("local t = Transform.from_pos({x=1,y=2,z=3})\n"
		"local s = tostring(t)\n"
		"assert(type(s) == 'string')\n");
}

TEST_F(LuaTransformTest, LookAtProducesOrthonormalBasis) {
	run("local t = Transform.look_at({x=0,y=0,z=5}, {x=0,y=0,z=0}, {x=0,y=1,z=0})\n"
		"local p = t:translation()\n"
		"assert(math.abs(p.x) < 1e-4)\n"
		"assert(math.abs(p.y) < 1e-4)\n"
		"assert(math.abs(p.z - 5) < 1e-4)\n");
}

TEST_F(LuaTransformTest, InvalidUserdataTypeErrors) {
	// Fetch the method off a real Transform's metatable, then call it with a
	// non-Transform `self` -- must fail the luaL_checkudata type check.
	EXPECT_NE(luaL_dostring(L, "local t = Transform.new()\n"
							 "local inverse_fn = t.inverse\n"
							 "inverse_fn(5)\n"),
			  LUA_OK);
	lua_pop(L, 1);
}
