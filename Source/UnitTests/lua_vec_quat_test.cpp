#include <gtest/gtest.h>
#include "Scripting/LuaVecQuat.h"

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// ============================================================
// Vec3/Quat metatable binding (Source/Scripting/LuaVecQuat.cpp).
//
// Unlike Transform, lVec3/lQuat still cross as plain tables, so these tests
// just run a bare lua_State with the Vec3/Quat metatables registered, and
// read results back off the stack or via globals.
// ============================================================

class LuaVecQuatTest : public ::testing::Test
{
protected:
	void SetUp() override {
		L = luaL_newstate();
		luaL_openlibs(L);
		register_lua_vec_quat(L);
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

TEST_F(LuaVecQuatTest, NewDefaultsToZero) {
	run("local v = Vec3.new()\n"
		"assert(v.x == 0 and v.y == 0 and v.z == 0)\n");
}

TEST_F(LuaVecQuatTest, NewSetsComponents) {
	run("local v = Vec3.new(1,2,3)\n"
		"assert(v.x == 1 and v.y == 2 and v.z == 3)\n");
}

TEST_F(LuaVecQuatTest, AddOperator) {
	run("local a = Vec3.new(1,2,3)\n"
		"local b = Vec3.new(4,5,6)\n"
		"local c = a + b\n"
		"assert(c.x == 5 and c.y == 7 and c.z == 9)\n");
}

TEST_F(LuaVecQuatTest, SubOperator) {
	run("local a = Vec3.new(4,5,6)\n"
		"local b = Vec3.new(1,2,3)\n"
		"local c = a - b\n"
		"assert(c.x == 3 and c.y == 3 and c.z == 3)\n");
}

TEST_F(LuaVecQuatTest, MulByScalar) {
	run("local a = Vec3.new(1,2,3)\n"
		"local c = a * 2\n"
		"assert(c.x == 2 and c.y == 4 and c.z == 6)\n");
}

TEST_F(LuaVecQuatTest, MulComponentWise) {
	run("local a = Vec3.new(1,2,3)\n"
		"local b = Vec3.new(2,2,2)\n"
		"local c = a * b\n"
		"assert(c.x == 2 and c.y == 4 and c.z == 6)\n");
}

TEST_F(LuaVecQuatTest, UnaryMinus) {
	run("local a = Vec3.new(1,-2,3)\n"
		"local c = -a\n"
		"assert(c.x == -1 and c.y == 2 and c.z == -3)\n");
}

TEST_F(LuaVecQuatTest, EqualityComparesByValue) {
	run("local a = Vec3.new(1,2,3)\n"
		"local b = Vec3.new(1,2,3)\n"
		"local c = Vec3.new(1,2,4)\n"
		"assert(a == b)\n"
		"assert(a ~= c)\n");
}

TEST_F(LuaVecQuatTest, LengthAndNormalize) {
	run("local a = Vec3.new(3,0,4)\n"
		"assert(math.abs(a:length() - 5) < 1e-5)\n"
		"local n = a:normalize()\n"
		"assert(math.abs(n:length() - 1) < 1e-5)\n");
}

TEST_F(LuaVecQuatTest, DotAndCross) {
	run("local a = Vec3.new(1,0,0)\n"
		"local b = Vec3.new(0,1,0)\n"
		"assert(a:dot(b) == 0)\n"
		"local c = a:cross(b)\n"
		"assert(math.abs(c.z - 1) < 1e-5)\n");
}

TEST_F(LuaVecQuatTest, Splat) {
	run("local v = Vec3.splat(2)\n"
		"assert(v.x == 2 and v.y == 2 and v.z == 2)\n");
}

TEST_F(LuaVecQuatTest, ClampMinMax) {
	run("local v = Vec3.new(-1, 5, 0.5)\n"
		"local lo = Vec3.new(0,0,0)\n"
		"local hi = Vec3.new(1,1,1)\n"
		"local c = v:clamp(lo, hi)\n"
		"assert(c.x == 0 and c.y == 1 and math.abs(c.z - 0.5) < 1e-6)\n"
		"local mn = v:min(hi)\n"
		"assert(mn.x == -1 and mn.y == 1 and mn.z == 0.5)\n"
		"local mx = v:max(lo)\n"
		"assert(mx.x == 0 and mx.y == 5 and mx.z == 0.5)\n");
}

TEST_F(LuaVecQuatTest, Mix) {
	run("local a = Vec3.new(0,0,0)\n"
		"local b = Vec3.new(10,20,30)\n"
		"local m = a:mix(b, 0.5)\n"
		"assert(m.x == 5 and m.y == 10 and m.z == 15)\n");
}

TEST_F(LuaVecQuatTest, QuatFromEulerRoundTrips) {
	run("local q = Quat.from_euler(Vec3.new(0, 0, 0))\n"
		"assert(q == Quat.identity())\n");
}

TEST_F(LuaVecQuatTest, QuatSlerpEndpoints) {
	// Compare by rotating a test point -- avoids quaternion double-cover pitfalls.
	run("local a = Quat.identity()\n"
		"local b = Quat.new(0,1,0,0)\n"     // 180 deg about x
		"local p = Vec3.new(1,2,3)\n"
		"assert(((a:slerp(b,0) * p) - (a * p)):length() < 1e-4)\n"
		"assert(((a:slerp(b,1) * p) - (b * p)):length() < 1e-4)\n");
}

TEST_F(LuaVecQuatTest, QuatDeltaTo) {
	// from:delta_to(to) == to * inverse(from), so (delta * from) rotates like to.
	run("local a = Quat.new(0,0,1,0)\n"   // 180 about y
		"local b = Quat.new(0,1,0,0)\n"   // 180 about x
		"local d = a:delta_to(b)\n"
		"local p = Vec3.new(1,2,3)\n"
		"assert((((d * a) * p) - (b * p)):length() < 1e-4)\n");
}

TEST_F(LuaVecQuatTest, ToStringDoesNotError) {
	run("local v = Vec3.new(1,2,3)\n"
		"local s = tostring(v)\n"
		"assert(type(s) == 'string')\n");
}

TEST_F(LuaVecQuatTest, QuatNewDefaultsToIdentity) {
	run("local q = Quat.new()\n"
		"assert(q.w == 1 and q.x == 0 and q.y == 0 and q.z == 0)\n"
		"local i = Quat.identity()\n"
		"assert(q == i)\n");
}

TEST_F(LuaVecQuatTest, QuatMulComposesAndInverseUndoes) {
	// Quat.new(w,x,y,z) -- 180 deg around x axis
	run("local q = Quat.new(0,1,0,0)\n"
		"local inv = q:inverse()\n"
		"local composed = q * inv\n"
		"assert(composed == Quat.identity())\n");
}

TEST_F(LuaVecQuatTest, QuatMulWithVec3RotatesPoint) {
	// Quat.new(w,x,y,z) -- 180 deg around y axis
	run("local q = Quat.new(0,0,1,0)\n"
		"local p = q * Vec3.new(1,0,0)\n"
		"assert(math.abs(p.x - -1) < 1e-4)\n"
		"assert(math.abs(p.y) < 1e-4)\n"
		"assert(math.abs(p.z) < 1e-4)\n");
}

TEST_F(LuaVecQuatTest, QuatToEuler) {
	run("local q = Quat.identity()\n"
		"local e = q:to_euler()\n"
		"assert(math.abs(e.x) < 1e-5)\n"
		"assert(math.abs(e.y) < 1e-5)\n"
		"assert(math.abs(e.z) < 1e-5)\n");
}

TEST_F(LuaVecQuatTest, QuatToStringDoesNotError) {
	run("local q = Quat.identity()\n"
		"local s = tostring(q)\n"
		"assert(type(s) == 'string')\n");
}
