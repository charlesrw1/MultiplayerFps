#include "ScriptFunctionCodegen.h"

void push_bool_to_lua(lua_State* L, bool b) {
	lua_pushboolean(L, b);
}

void push_float_to_lua(lua_State* L, float f) {
	lua_pushnumber(L, f);
}

void push_int_to_lua(lua_State* L, int64_t i)
{
	lua_pushinteger(L, i);
}

void push_std_string_to_lua(lua_State* L, const std::string& str) {
	lua_pushstring(L, str.c_str());
}

void push_object_to_lua(lua_State* L, const ClassBase* ptrConst)
{
	ClassBase* ptr = const_cast<ClassBase*>(ptrConst);
	// find cached table, return it
	// create cached table
	// return
}


void push_vec3_to_lua(lua_State* L, const glm::vec3& v)
{
	// make table, set x,y,z
}

bool get_bool_from_lua(lua_State* L, int index) {
	return lua_toboolean(L, index);
}

float get_float_from_lua(lua_State* L, int index) {
	return luaL_checknumber(L, index);
}

int64_t get_int_from_lua(lua_State* L, int index) {
	return luaL_checknumber(L, index);
}

std::string get_std_string_from_lua(lua_State* L, int index) {
	auto str = luaL_checkstring(L, index);
	return std::string(str);
}
glm::vec3 get_vec3_from_lua(lua_State* L, int index) {
	assert(0);
	return {};
}

ClassBase* get_object_from_lua(lua_State* L, int index) {
	if (lua_isnil(L, index))
		return nullptr;
	if (!lua_istable(L, index)) {
		luaL_error(L, "expected table in finding object to call function");
	}
	lua_getfield(L, index, "__ptr");
	if (lua_isuserdata(L, -1)) {
		void* ptr = lua_touserdata(L, -1);
		lua_pop(L, 1);  // Clean up the stack
		return (ClassBase*)ptr;
	}
	else {
		lua_pop(L, 1);  // Clean up the stack
		luaL_error(L, "expected __ptr user data in table");
		return nullptr;
	}
}

ClassTypeInfo* get_class_type_from_lua(lua_State* L, int index)
{
	return nullptr;
}
