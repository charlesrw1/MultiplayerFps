#include "LuaTransform.h"
#include "Framework/MathLib.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
// Declares push_mat4_to_lua/get_mat4_from_lua, the codegen-facing names for the
// same push_transform/check_transform used below -- see codegen_generate.py's
// MAT4_TYPE handling in write_push_type_to_lua_func/write_get_type_from_lua_func.
#include "Scripting/ScriptFunctionCodegen.h"

extern "C"
{
#include <lauxlib.h>
}

static const char* TRANSFORM_MT = "Transform_mt";

static glm::mat4* check_transform(lua_State* L, int index) {
	return (glm::mat4*)luaL_checkudata(L, index, TRANSFORM_MT);
}

static glm::mat4* push_transform(lua_State* L, const glm::mat4& m) {
	glm::mat4* ud = (glm::mat4*)lua_newuserdata(L, sizeof(glm::mat4));
	*ud = m;
	luaL_getmetatable(L, TRANSFORM_MT);
	lua_setmetatable(L, -2);
	return ud;
}

// Reads a {x,y,z} table (the lVec3 shape) at index into a glm::vec3.
static glm::vec3 check_vec3_field(lua_State* L, int index) {
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
static void push_vec3_field(lua_State* L, const glm::vec3& v) {
	lua_createtable(L, 0, 3);
	lua_pushnumber(L, v.x);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, v.y);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, v.z);
	lua_setfield(L, -2, "z");
}

// Reads a {w,x,y,z} table (the lQuat shape) at index into a glm::quat.
static glm::quat check_quat_field(lua_State* L, int index) {
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
static void push_quat_field(lua_State* L, const glm::quat& q) {
	lua_createtable(L, 0, 4);
	lua_pushnumber(L, q.w);
	lua_setfield(L, -2, "w");
	lua_pushnumber(L, q.x);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, q.y);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, q.z);
	lua_setfield(L, -2, "z");
}

// Transform.identity()
static int l_transform_identity(lua_State* L) {
	push_transform(L, glm::mat4(1.0f));
	return 1;
}
// Transform.new() -- alias for identity(), matches userdata-constructor convention
static int l_transform_new(lua_State* L) {
	push_transform(L, glm::mat4(1.0f));
	return 1;
}
// Transform.from_pos_rot_scale(pos, rot, scale)
static int l_transform_from_pos_rot_scale(lua_State* L) {
	glm::vec3 pos = check_vec3_field(L, 1);
	glm::quat rot = check_quat_field(L, 2);
	glm::vec3 scale = check_vec3_field(L, 3);
	push_transform(L, compose_transform(pos, rot, scale));
	return 1;
}
// Transform.from_pos_rot(pos, rot)
static int l_transform_from_pos_rot(lua_State* L) {
	glm::vec3 pos = check_vec3_field(L, 1);
	glm::quat rot = check_quat_field(L, 2);
	push_transform(L, compose_transform(pos, rot, glm::vec3(1.0f)));
	return 1;
}
// Transform.from_pos(pos)
static int l_transform_from_pos(lua_State* L) {
	glm::vec3 pos = check_vec3_field(L, 1);
	push_transform(L, glm::translate(glm::mat4(1.0f), pos));
	return 1;
}
// Transform.look_at(eye, target, up)
static int l_transform_look_at(lua_State* L) {
	glm::vec3 eye = check_vec3_field(L, 1);
	glm::vec3 target = check_vec3_field(L, 2);
	glm::vec3 up = check_vec3_field(L, 3);
	// glm::lookAt builds a view matrix (world->camera); invert to get the
	// camera's world transform, which is what scripts expect from "look_at".
	push_transform(L, glm::inverse(glm::lookAt(eye, target, up)));
	return 1;
}

// t:inverse()
static int l_transform_inverse(lua_State* L) {
	glm::mat4* m = check_transform(L, 1);
	push_transform(L, glm::inverse(*m));
	return 1;
}
// t:translation() -> lVec3
static int l_transform_translation(lua_State* L) {
	glm::mat4* m = check_transform(L, 1);
	push_vec3_field(L, glm::vec3((*m)[3]));
	return 1;
}
// t:rotation() -> lQuat
static int l_transform_rotation(lua_State* L) {
	glm::mat4* m = check_transform(L, 1);
	glm::vec3 pos, scale;
	glm::quat rot;
	decompose_transform(*m, pos, rot, scale);
	push_quat_field(L, rot);
	return 1;
}
// t:scale() -> lVec3
static int l_transform_scale(lua_State* L) {
	glm::mat4* m = check_transform(L, 1);
	glm::vec3 pos, scale;
	glm::quat rot;
	decompose_transform(*m, pos, rot, scale);
	push_vec3_field(L, scale);
	return 1;
}
// t:decompose() -> pos, rot, scale
static int l_transform_decompose(lua_State* L) {
	glm::mat4* m = check_transform(L, 1);
	glm::vec3 pos, scale;
	glm::quat rot;
	decompose_transform(*m, pos, rot, scale);
	push_vec3_field(L, pos);
	push_quat_field(L, rot);
	push_vec3_field(L, scale);
	return 3;
}
// t:set_translation(pos) -> mutates in place, returns self
static int l_transform_set_translation(lua_State* L) {
	glm::mat4* m = check_transform(L, 1);
	glm::vec3 pos = check_vec3_field(L, 2);
	(*m)[3] = glm::vec4(pos, (*m)[3].w);
	lua_settop(L, 1);
	return 1;
}
// t:transform_point(p) -> lVec3, applies full transform (translation included)
static int l_transform_point(lua_State* L) {
	glm::mat4* m = check_transform(L, 1);
	glm::vec3 p = check_vec3_field(L, 2);
	push_vec3_field(L, glm::vec3(*m * glm::vec4(p, 1.0f)));
	return 1;
}
// t:transform_direction(v) -> lVec3, ignores translation
static int l_transform_direction(lua_State* L) {
	glm::mat4* m = check_transform(L, 1);
	glm::vec3 v = check_vec3_field(L, 2);
	push_vec3_field(L, glm::vec3(*m * glm::vec4(v, 0.0f)));
	return 1;
}

// __mul: Transform * Transform (compose) or Transform * {x,y,z} (transform_point)
static int l_transform_mul(lua_State* L) {
	glm::mat4* a = check_transform(L, 1);
	if (luaL_testudata(L, 2, TRANSFORM_MT)) {
		glm::mat4* b = check_transform(L, 2);
		push_transform(L, *a * *b);
	} else {
		glm::vec3 p = check_vec3_field(L, 2);
		push_vec3_field(L, glm::vec3(*a * glm::vec4(p, 1.0f)));
	}
	return 1;
}
static int l_transform_eq(lua_State* L) {
	glm::mat4* a = check_transform(L, 1);
	glm::mat4* b = check_transform(L, 2);
	lua_pushboolean(L, *a == *b);
	return 1;
}
static int l_transform_tostring(lua_State* L) {
	glm::mat4* m = check_transform(L, 1);
	glm::vec3 pos, scale;
	glm::quat rot;
	decompose_transform(*m, pos, rot, scale);
	lua_pushfstring(L, "Transform(pos=(%f,%f,%f))", pos.x, pos.y, pos.z);
	return 1;
}

static const luaL_Reg TRANSFORM_METHODS[] = {
	{"inverse", l_transform_inverse},
	{"translation", l_transform_translation},
	{"rotation", l_transform_rotation},
	{"scale", l_transform_scale},
	{"decompose", l_transform_decompose},
	{"set_translation", l_transform_set_translation},
	{"transform_point", l_transform_point},
	{"transform_direction", l_transform_direction},
	{"mul", l_transform_mul},
	{nullptr, nullptr},
};
static const luaL_Reg TRANSFORM_METAMETHODS[] = {
	{"__mul", l_transform_mul}, {"__eq", l_transform_eq}, {"__tostring", l_transform_tostring}, {nullptr, nullptr},
};
static const luaL_Reg TRANSFORM_STATICS[] = {
	{"new", l_transform_new},
	{"identity", l_transform_identity},
	{"from_pos", l_transform_from_pos},
	{"from_pos_rot", l_transform_from_pos_rot},
	{"from_pos_rot_scale", l_transform_from_pos_rot_scale},
	{"look_at", l_transform_look_at},
	{nullptr, nullptr},
};

void register_lua_transform(lua_State* L) {
	luaL_newmetatable(L, TRANSFORM_MT);
	luaL_setfuncs(L, TRANSFORM_METAMETHODS, 0);

	lua_newtable(L); // __index table holding the methods
	luaL_setfuncs(L, TRANSFORM_METHODS, 0);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1); // pop metatable

	luaL_newlib(L, TRANSFORM_STATICS);
	lua_setglobal(L, "Transform");
}

// Codegen-facing boundary crossing for REF functions that take/return glm::mat4
// directly (see MAT4_TYPE in codegen_lib.py/codegen_generate.py). Thin wrappers
// over the same userdata push/check used by the Transform library above, so a
// REF function returning glm::mat4 produces exactly what Transform.new() would.
void push_mat4_to_lua(lua_State* L, const glm::mat4& m) {
	push_transform(L, m);
}
glm::mat4 get_mat4_from_lua(lua_State* L, int index) {
	return *check_transform(L, index);
}
