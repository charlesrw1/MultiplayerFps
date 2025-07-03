#pragma once
#include "Framework/ClassBase.h"
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include <cstdint>
#include <string>
#include "glm/glm.hpp"
class ClassBase;
void push_bool_to_lua(lua_State* L, bool b);
void push_float_to_lua(lua_State* L, float f);
void push_int_to_lua(lua_State* L, int64_t i);
void push_std_string_to_lua(lua_State* L, const std::string& str);
void push_object_to_lua(lua_State* L, const ClassBase* ptr);
void push_vec3_to_lua(lua_State* L, const glm::vec3& v);
bool get_bool_from_lua(lua_State* L, int index);
float get_float_from_lua(lua_State* L, int index);
int64_t get_int_from_lua(lua_State* L, int index);
std::string get_std_string_from_lua(lua_State* L, int index);
glm::vec3 get_vec3_from_lua(lua_State* L, int index);
ClassBase* get_object_from_lua(lua_State* L, int index);

template<typename T>
ClassBase* allocate_script_impl_internal(const ClassTypeInfo* info) {
	T* ptr = new T();
	ptr->type = info;
	return (ClassBase*)ptr;
}
template<typename T>
ClassTypeInfo::CreateObjectFunc get_allocate_script_impl_internal() {
	return allocate_script_impl_internal<T>;
}