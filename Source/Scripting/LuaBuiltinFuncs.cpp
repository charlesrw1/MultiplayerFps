#include <glm/glm.hpp>
#include "Framework/ClassBase.h"
#include <vector>
#include <string>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}


// (ab)use lua gc?
// each game object gets a lua table? lazy caching?
// when object is deleted, delete the table
// gc for c++ objects? when a ptr is "deleted" just move it to a seperate set. then after the gc scan, delete it.
// have to do this anyways can keep a seperate set for objects with refereneces? ignores most objects (meshcomponents etc)


// entity
//		scriptcomponent
//			script
// 
// 
// 
// MyScript*	->   lua table scriptcomponent
//
//
// 

class BaseUpdater;

int bif_push_integer(int x);
int bif_push_float(float f);
int bif_push_vec3(const glm::vec3& v);
int bif_push_quat(const glm::quat& q);
int bif_push_transform(const glm::mat4& m);
int bif_push_class_ptr(ClassBase* c);	// pushes a ptr
int bif_push_class_handle(BaseUpdater* c);	// pushes a handle
int bif_push_array_classes();
int bif_push_string(const std::string& str);
int bif_push_boolean(bool b);

bool bif_get_boolean(lua_State* L);
int bif_get_integer(lua_State* L);
float bif_get_float(lua_State* L);
ClassBase* bif_get_class_ptr(lua_State* L);

// push struct, get struct

// to call script code from c++
//		use events. multicast_delegate<>
//		use interfaces.


// codgen
void make_lua_table_Entity(lua_State* L)
{
	lua_pushstring(L, "owner");
	//lua_pushcfunction(L, lf_Entity_get_owner);
}


inline glm::vec3* bif_get_vec(lua_State* L) {
	return (glm::vec3*)luaL_checkudata(L, -1, "vec3");
}
static int bif_vec3_get_x(lua_State* L) {
	glm::vec3* v = bif_get_vec(L);
	lua_pushnumber(L, v->x);
	return 1;
}
static int bif_vec3_get_y(lua_State* L) {
	glm::vec3* v = bif_get_vec(L);
	lua_pushnumber(L, v->y);
	return 1;
}
static int bif_vec3_get_z(lua_State* L) {
	glm::vec3* v = bif_get_vec(L);
	lua_pushnumber(L, v->z);
	return 1;
}
static int bif_vec3_dot(lua_State* L) {
	glm::vec3* v1 = bif_get_vec(L);
	glm::vec3* v2 = bif_get_vec(L);
	lua_pushnumber(L, glm::dot(*v1, *v2));
	return 1;
}
inline glm::vec3* bif_vec3_new(lua_State* L) {
	glm::vec3* newv = (glm::vec3*)lua_newuserdata(L, sizeof(glm::vec3));
	new(newv)glm::vec3(0.f);
	return newv;
}
static int big_vec3_cross(lua_State* L) {
	glm::vec3* v1 = bif_get_vec(L);
	glm::vec3* v2 = bif_get_vec(L);
	auto newv = bif_vec3_new(L);
	*newv = glm::cross(*v1, *v2);
	lua_pushlightuserdata(L, newv);	// fixme
	return 1;
}