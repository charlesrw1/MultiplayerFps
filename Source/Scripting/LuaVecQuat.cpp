#include "LuaVecQuat.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

extern "C"
{
#include <lauxlib.h>
}

static const char* VEC3_MT = "lVec3_mt";
static const char* QUAT_MT = "lQuat_mt";

static glm::vec3 check_vec3(lua_State* L, int index) {
	luaL_checktype(L, index, LUA_TTABLE);
	glm::vec3 v{};
	lua_getfield(L, index, "x");
	v.x = (float)luaL_checknumber(L, -1);
	lua_getfield(L, index, "y");
	v.y = (float)luaL_checknumber(L, -1);
	lua_getfield(L, index, "z");
	v.z = (float)luaL_checknumber(L, -1);
	lua_pop(L, 3);
	return v;
}
static void push_vec3(lua_State* L, const glm::vec3& v) {
	lua_createtable(L, 0, 3);
	lua_pushnumber(L, v.x);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, v.y);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, v.z);
	lua_setfield(L, -2, "z");
	luaL_getmetatable(L, VEC3_MT);
	lua_setmetatable(L, -2);
}

static glm::quat check_quat(lua_State* L, int index) {
	luaL_checktype(L, index, LUA_TTABLE);
	float w, x, y, z;
	lua_getfield(L, index, "w");
	w = (float)luaL_checknumber(L, -1);
	lua_getfield(L, index, "x");
	x = (float)luaL_checknumber(L, -1);
	lua_getfield(L, index, "y");
	y = (float)luaL_checknumber(L, -1);
	lua_getfield(L, index, "z");
	z = (float)luaL_checknumber(L, -1);
	lua_pop(L, 4);
	return glm::quat(w, x, y, z);
}
static void push_quat(lua_State* L, const glm::quat& q) {
	lua_createtable(L, 0, 4);
	lua_pushnumber(L, q.w);
	lua_setfield(L, -2, "w");
	lua_pushnumber(L, q.x);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, q.y);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, q.z);
	lua_setfield(L, -2, "z");
	luaL_getmetatable(L, QUAT_MT);
	lua_setmetatable(L, -2);
}

// Vec3.new(x,y,z), all default 0
static int l_vec3_new(lua_State* L) {
	float x = (float)luaL_optnumber(L, 1, 0.0);
	float y = (float)luaL_optnumber(L, 2, 0.0);
	float z = (float)luaL_optnumber(L, 3, 0.0);
	push_vec3(L, glm::vec3(x, y, z));
	return 1;
}
// v:length()
static int l_vec3_length(lua_State* L) {
	lua_pushnumber(L, glm::length(check_vec3(L, 1)));
	return 1;
}
// v:normalize()
static int l_vec3_normalize(lua_State* L) {
	push_vec3(L, glm::normalize(check_vec3(L, 1)));
	return 1;
}
// v:dot(other)
static int l_vec3_dot(lua_State* L) {
	lua_pushnumber(L, glm::dot(check_vec3(L, 1), check_vec3(L, 2)));
	return 1;
}
// v:cross(other)
static int l_vec3_cross(lua_State* L) {
	push_vec3(L, glm::cross(check_vec3(L, 1), check_vec3(L, 2)));
	return 1;
}
// v:clamp(min, max) -- component-wise clamp into [min, max]
static int l_vec3_clamp(lua_State* L) {
	glm::vec3 v = check_vec3(L, 1);
	glm::vec3 lo = check_vec3(L, 2);
	glm::vec3 hi = check_vec3(L, 3);
	push_vec3(L, glm::clamp(v, lo, hi));
	return 1;
}
// v:min(other) -- component-wise minimum
static int l_vec3_min(lua_State* L) {
	push_vec3(L, glm::min(check_vec3(L, 1), check_vec3(L, 2)));
	return 1;
}
// v:max(other) -- component-wise maximum
static int l_vec3_max(lua_State* L) {
	push_vec3(L, glm::max(check_vec3(L, 1), check_vec3(L, 2)));
	return 1;
}
// v:mix(other, t) -- linear interpolation toward other, t in [0,1]
static int l_vec3_mix(lua_State* L) {
	glm::vec3 a = check_vec3(L, 1);
	glm::vec3 b = check_vec3(L, 2);
	float t = (float)luaL_checknumber(L, 3);
	push_vec3(L, glm::mix(a, b, t));
	return 1;
}
// Vec3.splat(f) -- broadcast a scalar into all three components
static int l_vec3_splat(lua_State* L) {
	float f = (float)luaL_checknumber(L, 1);
	push_vec3(L, glm::vec3(f));
	return 1;
}

static int l_vec3_add(lua_State* L) {
	push_vec3(L, check_vec3(L, 1) + check_vec3(L, 2));
	return 1;
}
static int l_vec3_sub(lua_State* L) {
	push_vec3(L, check_vec3(L, 1) - check_vec3(L, 2));
	return 1;
}
// __mul: Vec3 * Vec3 (component-wise) or Vec3 * number (scalar)
static int l_vec3_mul(lua_State* L) {
	glm::vec3 a = check_vec3(L, 1);
	if (lua_isnumber(L, 2)) {
		push_vec3(L, a * (float)lua_tonumber(L, 2));
	} else {
		push_vec3(L, a * check_vec3(L, 2));
	}
	return 1;
}
static int l_vec3_unm(lua_State* L) {
	push_vec3(L, -check_vec3(L, 1));
	return 1;
}
static int l_vec3_eq(lua_State* L) {
	lua_pushboolean(L, check_vec3(L, 1) == check_vec3(L, 2));
	return 1;
}
static int l_vec3_tostring(lua_State* L) {
	glm::vec3 v = check_vec3(L, 1);
	lua_pushfstring(L, "Vec3(%f,%f,%f)", v.x, v.y, v.z);
	return 1;
}

static const luaL_Reg VEC3_METHODS[] = {
	{"length", l_vec3_length},
	{"normalize", l_vec3_normalize},
	{"dot", l_vec3_dot},
	{"cross", l_vec3_cross},
	{"clamp", l_vec3_clamp},
	{"min", l_vec3_min},
	{"max", l_vec3_max},
	{"mix", l_vec3_mix},
	{nullptr, nullptr},
};
static const luaL_Reg VEC3_METAMETHODS[] = {
	{"__add", l_vec3_add}, {"__sub", l_vec3_sub}, {"__mul", l_vec3_mul},
	{"__unm", l_vec3_unm}, {"__eq", l_vec3_eq}, {"__tostring", l_vec3_tostring},
	{nullptr, nullptr},
};
static const luaL_Reg VEC3_STATICS[] = {
	{"new", l_vec3_new},
	{"splat", l_vec3_splat},
	{nullptr, nullptr},
};

// Quat.new(w,x,y,z), defaults to identity (1,0,0,0)
static int l_quat_new(lua_State* L) {
	float w = (float)luaL_optnumber(L, 1, 1.0);
	float x = (float)luaL_optnumber(L, 2, 0.0);
	float y = (float)luaL_optnumber(L, 3, 0.0);
	float z = (float)luaL_optnumber(L, 4, 0.0);
	push_quat(L, glm::quat(w, x, y, z));
	return 1;
}
static int l_quat_identity(lua_State* L) {
	push_quat(L, glm::quat(1, 0, 0, 0));
	return 1;
}
// q:inverse()
static int l_quat_inverse(lua_State* L) {
	push_quat(L, glm::inverse(check_quat(L, 1)));
	return 1;
}
// q:to_euler() -> Vec3
static int l_quat_to_euler(lua_State* L) {
	push_vec3(L, glm::eulerAngles(check_quat(L, 1)));
	return 1;
}
// q:slerp(other, alpha) -- spherical interpolation toward other, alpha in [0,1]
static int l_quat_slerp(lua_State* L) {
	glm::quat a = check_quat(L, 1);
	glm::quat b = check_quat(L, 2);
	float alpha = (float)luaL_checknumber(L, 3);
	push_quat(L, glm::slerp(a, b, alpha));
	return 1;
}
// q:delta_to(other) -- the rotation that takes q to other (other * inverse(q))
static int l_quat_delta_to(lua_State* L) {
	glm::quat from = check_quat(L, 1);
	glm::quat to = check_quat(L, 2);
	push_quat(L, to * glm::inverse(from));
	return 1;
}
// Quat.from_euler(euler) -- build from XYZ Euler angles (radians) held in a {x,y,z} vec
static int l_quat_from_euler(lua_State* L) {
	push_quat(L, glm::quat(check_vec3(L, 1)));
	return 1;
}
// __mul: Quat * Quat (compose) or Quat * {x,y,z} (rotate vector). Tables aren't
// userdata, so distinguish by checking for a "w" field rather than luaL_testudata.
static int l_quat_mul(lua_State* L) {
	glm::quat a = check_quat(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);
	lua_getfield(L, 2, "w");
	bool is_quat = !lua_isnil(L, -1);
	lua_pop(L, 1);
	if (is_quat) {
		push_quat(L, a * check_quat(L, 2));
	} else {
		push_vec3(L, a * check_vec3(L, 2));
	}
	return 1;
}
static int l_quat_eq(lua_State* L) {
	glm::quat a = check_quat(L, 1);
	glm::quat b = check_quat(L, 2);
	lua_pushboolean(L, a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z);
	return 1;
}
static int l_quat_tostring(lua_State* L) {
	glm::quat q = check_quat(L, 1);
	lua_pushfstring(L, "Quat(w=%f,x=%f,y=%f,z=%f)", q.w, q.x, q.y, q.z);
	return 1;
}

static const luaL_Reg QUAT_METHODS[] = {
	{"inverse", l_quat_inverse},
	{"to_euler", l_quat_to_euler},
	{"slerp", l_quat_slerp},
	{"delta_to", l_quat_delta_to},
	{nullptr, nullptr},
};
static const luaL_Reg QUAT_METAMETHODS[] = {
	{"__mul", l_quat_mul}, {"__eq", l_quat_eq}, {"__tostring", l_quat_tostring}, {nullptr, nullptr},
};
static const luaL_Reg QUAT_STATICS[] = {
	{"new", l_quat_new},
	{"identity", l_quat_identity},
	{"from_euler", l_quat_from_euler},
	{nullptr, nullptr},
};

void register_lua_vec_quat(lua_State* L) {
	luaL_newmetatable(L, VEC3_MT);
	luaL_setfuncs(L, VEC3_METAMETHODS, 0);
	lua_newtable(L); // __index table holding the methods
	luaL_setfuncs(L, VEC3_METHODS, 0);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1); // pop metatable

	luaL_newlib(L, VEC3_STATICS);
	lua_setglobal(L, "Vec3");

	luaL_newmetatable(L, QUAT_MT);
	luaL_setfuncs(L, QUAT_METAMETHODS, 0);
	lua_newtable(L);
	luaL_setfuncs(L, QUAT_METHODS, 0);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	luaL_newlib(L, QUAT_STATICS);
	lua_setglobal(L, "Quat");
}
