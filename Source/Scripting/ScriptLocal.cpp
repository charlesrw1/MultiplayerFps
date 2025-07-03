#include "ScriptComponent.h"
#include "ScriptAsset.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "FunctionReflection.h"
#include "Assets/AssetDatabase.h"
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include "Game/Entity.h"
#include "Game/EntityComponent.h"

#include "Framework/ReflectionMacros.h"
#include <iostream>

#include "Game/LevelAssets.h"

void stack_dump(lua_State* L);


void* get_class_from_stack(lua_State* L, int index);

ClassBase* get_object_from_lua(lua_State* L, int index, const ClassTypeInfo* expected_type)
{
	ClassBase* c = (ClassBase*)get_class_from_stack(L, index);
	if (!c) return nullptr;
	if (c->get_type().is_a(*expected_type))
		return c;
	return nullptr;
}
static void make_table_for_class(lua_State* L, ClassBase* c);
static void make_table_for_gameobject(lua_State* L, BaseUpdater* c);

void push_iasset_to_lua(lua_State* L, IAsset* a) {
	make_table_for_class(L, a);
}
void push_entity_to_lua(lua_State* L, Entity* e) {
	make_table_for_gameobject(L, e);
}
void push_entitycomponent_to_lua(lua_State* L, Component* ec) {
	make_table_for_gameobject(L, ec);
}
#if 0
template<>
void push_to_lua(lua_State* L, float f)
{
	lua_pushnumber(L, f);
}
template<>
void push_to_lua(lua_State* L, int i)
{
	lua_pushinteger(L, i);
}
template<>
void push_to_lua(lua_State* L, const char* str)
{
	lua_pushstring(L, str);
}


template<>
float get_from_lua(lua_State* L, int i)
{
	return luaL_checknumber(L, i);
}
template<>
int get_from_lua(lua_State* L, int i)
{
	return luaL_checkinteger(L, i);
}
template<>
glm::vec3 get_from_lua(lua_State* L, int i)
{
	auto ptr = (glm::vec3*)luaL_checkudata(L,i,"Vector");
	ASSERT(ptr);
	return *ptr;
}


template<>
bool get_from_lua(lua_State* L, int i)
{
	return lua_toboolean(L, i);
}

template<>
const char* get_from_lua(lua_State* L, int i)
{
	return luaL_checkstring(L, i);
}

template<>
const ClassTypeInfo* get_from_lua(lua_State* L, int index)
{
	auto str = luaL_checkstring(L, index);
	return ClassBase::find_class(str);
}
template<>
void push_to_lua(lua_State* L, const ClassTypeInfo* ti)
{
	if (ti)
		lua_pushstring(L, ti->classname);
	else
		lua_pushstring(L, "");
}
template<>
void push_to_lua(lua_State* L, bool b)
{
	lua_pushboolean(L, b);
}
template<>
void push_to_lua(lua_State* L, glm::vec3 v)
{
	auto vptr = (glm::vec3*)lua_newuserdata(L, sizeof(glm::vec3));
	*vptr = v;
	luaL_getmetatable(L, "Vector");
	lua_setmetatable(L, -2);
}
static int vec_add(lua_State* L)
{
	auto v1 = get_from_lua<glm::vec3>(L, 1);
	auto v2 = get_from_lua<glm::vec3>(L, 2);
	push_to_lua(L, v1 + v2);
	return 1;
}
static int vec_sub(lua_State* L)
{
	auto v1 = get_from_lua<glm::vec3>(L, 1);
	auto v2 = get_from_lua<glm::vec3>(L, 2);
	push_to_lua(L, v1 - v2);
	return 1;
}
static int vec_mult(lua_State* L)
{
	auto v1 = get_from_lua<glm::vec3>(L, 1);
	auto f2 = get_from_lua<float>(L, 2);
	push_to_lua(L, v1 * f2);
	return 1;
}
static int vec_normalized(lua_State* L)
{
	auto v1 = get_from_lua<glm::vec3>(L, 1);
	push_to_lua(L, glm::normalize(v1));
	return 1;
}
static int vec_length(lua_State* L)
{
	auto v1 = get_from_lua<glm::vec3>(L, 1);
	push_to_lua(L, glm::length(v1));
	return 1;
}
static int vec_cross(lua_State* L)
{
	auto v1 = get_from_lua<glm::vec3>(L, 1);
	auto v2 = get_from_lua<glm::vec3>(L, 2);
	push_to_lua(L, glm::cross(v1,v2));
	return 1;
}
static int vec_dot(lua_State* L)
{
	auto v1 = get_from_lua<glm::vec3>(L, 1);
	auto v2 = get_from_lua<glm::vec3>(L, 2);
	push_to_lua(L, glm::dot(v1, v2));
	return 1;
}
static int vec_index(lua_State* L)
{
	glm::vec3 v = get_from_lua<glm::vec3>(L, 1);
	const char* str = luaL_checkstring(L, 2);
	if (!strcmp(str, "x")) {
		push_to_lua<float>(L, v.x);
	}
	else if (!strcmp(str, "y")) {
		push_to_lua<float>(L, v.y);
	}
	else if (!strcmp(str, "z")) {
		push_to_lua<float>(L, v.z);
	}
	else if (!strcmp(str, "dot")) {
		lua_pushcfunction(L, vec_dot);
	}
	else if (!strcmp(str, "cross")) {
		lua_pushcfunction(L, vec_cross);
	}
	else if (!strcmp(str, "normalized")) {
		lua_pushcfunction(L, vec_normalized);
	}
	else if (!strcmp(str, "length")) {
		lua_pushcfunction(L, vec_length);
	}
	else {
		luaL_error(L, "vec_index no string: %s", str);
	}
	return 1;
}
static int vec_newindex(lua_State* L)
{
	glm::vec3 v = get_from_lua<glm::vec3>(L, 1);
	const char* str = luaL_checkstring(L, 2);
	float f = get_from_lua<float>(L, 3);
	if (strcmp(str, "x")) {
		v.x = f;
	}
	else if (strcmp(str, "y")) {
		v.y = f;
	}
	else if (strcmp(str, "z")) {
		v.z = f;
	}
	else {
		luaL_error(L, "vec_newindex no string: %s", str);
	}
	return 0;
}
static int vec_create(lua_State* L)
{
	float x = get_from_lua<float>(L, 1);
	float y = get_from_lua<float>(L, 2);
	float z = get_from_lua<float>(L, 3);
	push_to_lua(L, glm::vec3(x, y, z));
	return 1;
}
#endif


int object_table_access(lua_State* L);
int classbase_table_access(lua_State* L);
int objects_are_equal(lua_State* L);

#if 0
class ScriptManagerLocal : public ScriptManagerPublic
{
public:
	void init() final {
		L = luaL_newstate();
		luaL_openlibs(L);
		// init metatables
		luaL_newmetatable(L, "classbase");
		lua_pushstring(L, "__index");
		lua_pushcfunction(L, classbase_table_access);
		lua_settable(L, -3);	// metatable.__index = classbase_table_access
		lua_pushstring(L, "__eq");
		lua_pushcfunction(L, objects_are_equal);
		lua_settable(L, -3);
		lua_pushstring(L, "dbg");
		lua_pushinteger(L, 0);
		lua_settable(L, -3);
		lua_pop(L, 1);


		luaL_newmetatable(L, "object");
		lua_pushstring(L, "__index");
		lua_pushcfunction(L, object_table_access);
		lua_settable(L, -3);	// metatable.__index = classbase_table_access
		lua_pushstring(L, "__eq");
		lua_pushcfunction(L, objects_are_equal);
		lua_settable(L, -3);
		lua_pop(L, 1);

		{
			luaL_newmetatable(L, "Vector");
			{
				lua_pushstring(L, "__index");
				lua_pushcfunction(L, vec_index);
				lua_settable(L, -3);
			}
			{
				lua_pushstring(L, "__newindex");
				lua_pushcfunction(L, vec_newindex);
				lua_settable(L, -3);
			}
			{
				lua_pushstring(L, "__add");
				lua_pushcfunction(L, vec_add);
				lua_settable(L, -3);
			}
			{
				lua_pushstring(L, "__sub");
				lua_pushcfunction(L, vec_sub);
				lua_settable(L, -3);
			}
			{
				lua_pushstring(L, "__mult");
				lua_pushcfunction(L, vec_mult);
				lua_settable(L, -3);
			}
			
			lua_pop(L, 1);
			{
				lua_pushcfunction(L, vec_create);
				lua_setglobal(L, "Vector");
			}
		}

		//push_global(enginewrapper.get(), "eng");


		{
			luaL_newmetatable(L, "globalmeta");
			lua_pushstring(L, "__index");
			lua_pushglobaltable(L);
			lua_settable(L, -3);
		}

		lua_settop(L, 0);
	}
	void push_global(ClassBase* c, const char* name) final {
		if (c->is_a<BaseUpdater>())
			make_table_for_gameobject(L, c->cast_to<BaseUpdater>());
		else
			make_table_for_class(L, c);
		lua_setglobal(L, name);
	}
	void remove_global(const char* name) final {
		lua_pushnil(L);
		lua_setglobal(L, name);
	}

	lua_State* L = nullptr;

};
#endif

// global mgr

#include "Framework/PropHashTable.h"
static void add_delegate(lua_State* L, ClassBase* md, PropertyInfo* pi, const char* funcname)
{
	lua_getglobal(L, "_G");	// get table
	ASSERT(lua_istable(L, -1));
	lua_getfield(L, -1, "script");
	ASSERT(lua_isuserdata(L, -1));
	stack_dump(L);
	void* ptr = get_class_from_stack(L, -1);
	ASSERT(ptr);
	ScriptComponent* me = (ScriptComponent*)ptr;

	//pi->multicast->add(pi->get_ptr(md), me, funcname);

	if (md->is_a<BaseUpdater>()) {
		OutstandingScriptDelegate out;
		out.handle = ((BaseUpdater*)(md))->get_instance_id();
		out.pi = pi;
		me->outstandings.push_back(out);
	}
	else {
		OutstandingScriptDelegate out;
		out.ptr = md;
		out.pi = pi;
		me->outstandings.push_back(out);
	}
	lua_pop(L, 2);
}

static int multicast_add_lua(lua_State* L)
{
	if (!lua_istable(L, 1))
		luaL_error(L,"multicast add not table");
	if (!lua_isstring(L, 2))
		luaL_error(L, "multicast add not func");

	lua_pushstring(L, "_prp");
	lua_gettable(L, 1);
	if (!lua_isuserdata(L, -1))
		luaL_error(L, "object didnt have prp to access");
	lua_pushstring(L, "_md");
	lua_gettable(L, 1);
	if (!lua_isuserdata(L, -1))
		luaL_error(L, "object didnt have md to access");
	void* md = lua_touserdata(L, -1);
	void* prp = lua_touserdata(L, -2);

	const char* name = lua_tostring(L, 2);

	PropertyInfo* pi = (PropertyInfo*)prp;
	//if (!pi->multicast)
	//	lua_error(L);

	add_delegate(L, (ClassBase*)md, pi, name);

	lua_pop(L, 1);
	return 0;
}
static int multicast_sub_lua(lua_State* L)
{
	// fixme
	return 0;
}

static int special_script_connect(lua_State* L)
{
	ClassBase* c = (ClassBase*)get_class_from_stack(L, 1);
	if (!lua_isstring(L, 2) || !lua_isstring(L, 3)) {
		luaL_error(L, "connect(<string>,<string>)");
	}
	if (!c->is_a<Entity>())
		luaL_error(L, "connect only on entities");
	ClassBase* what = c;
	auto ent = (Entity*)c;
	StringView name(lua_tostring(L, 2));
	ClassBase* p = nullptr;
	if (!p) {
		for (auto c : ent->get_components()) {
			//p = find_delegate(c, name);
			//if (p) {
			//	what = c;
			//	break;
			//}
		}
	}
	if (!p) {
		sys_print(Warning, "couldnt connect delegate %s %s\n", lua_tostring(L, 2), lua_tostring(L, 3));
	}
	else {
		//add_delegate(L, what, p, lua_tostring(L, 3));
	}
	return 0;
}
static void stack_dump(lua_State* L) {
	int top = lua_gettop(L);  // Get the index of the top element

	printf("Stack Dump:\n");
	for (int i = 1; i <= top; i++) {
		int type = lua_type(L, i);
		switch (type) {
		case LUA_TSTRING:
			printf("%d: '%s'\n", i, lua_tostring(L, i));
			break;
		case LUA_TBOOLEAN:
			printf("%d: %s\n", i, lua_toboolean(L, i) ? "true" : "false");
			break;
		case LUA_TNUMBER:
			printf("%d: %g\n", i, lua_tonumber(L, i));
			break;
		case LUA_TTABLE:
			printf("%d: Table\n", i);
			break;
		case LUA_TFUNCTION:
			printf("%d: Function\n", i);
			break;
		case LUA_TLIGHTUSERDATA:
			printf("%d: Light Userdata\n", i);
			break;
		case LUA_TUSERDATA:
			printf("%d: Userdata\n", i);
			break;
		case LUA_TNIL:
			printf("%d: nil\n", i);
			break;
		default:
			printf("%d: Unknown\n", i);
			break;
		}
	}
	printf("\n");
}
static int table_access_shared(lua_State* L, ClassBase* c)
{
	const char* str = lua_tostring(L, 2);
	auto find = c->get_type().prop_hash_table->prop_table.find(StringView(str));
	if (find == c->get_type().prop_hash_table->prop_table.end()) {

		// special connect function
		if (strcmp(str, "connect")==0) {
			lua_pushcfunction(L, special_script_connect);
			return 1;
		}
		else {
			luaL_error(L, "object didnt have property: %s", str);
			return 0;
		}
	}
#if 0
	auto prop = find->second;
	if (prop->type == core_type_id::Function) {
		lua_pushcfunction(L, prop->call_function);
	}
	else if (prop->type == core_type_id::GetterFunc) {
		ASSERT(lua_isuserdata(L, 1));
		lua_pushcfunction(L, prop->call_function);
		lua_pushvalue(L, 1);	// push "this"
		if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
			sys_print(Error,"Error calling inner function: %s\n", lua_tostring(L, -1));
			lua_error(L);
		}
		return 1;
	}
	else if (prop->type == core_type_id::Bool) {
		lua_pushboolean(L, prop->get_int(c));
	}
	else if (prop->type == core_type_id::Float) {
		lua_pushnumber(L, prop->get_float(c));
	}
	else if (prop->is_integral_type()) {
		lua_pushinteger(L, prop->get_int(c));
	}
	else if (prop->type == core_type_id::MulticastDelegate) {
		lua_newtable(L);
		lua_pushstring(L, "_md");
		lua_pushlightuserdata(L, c);
		lua_settable(L, -3);
		lua_pushstring(L, "_prp");
		lua_pushlightuserdata(L, prop);
		lua_settable(L, -3);
		lua_pushstring(L, "add");
		lua_pushcfunction(L, multicast_add_lua);
		lua_settable(L, -3);
		lua_pushstring(L, "remove");
		lua_pushcfunction(L, multicast_sub_lua);
		lua_settable(L, -3);
		return 1;
	}
	else {
		return 0;
	}
#endif
		luaL_error(L, "not valid property to access: %s", str);
	return 1;
}

static int classbase_table_access(lua_State* L)
{
	return table_access_shared(L, (ClassBase*)get_class_from_stack(L, -2));
}
static int object_table_access(lua_State* L)
{
	return table_access_shared(L, (ClassBase*)get_class_from_stack(L,-2));
}
void* get_class_from_stack(lua_State* L, int index)
{
	if (lua_isnil(L, index))
		return nullptr;

	if (!lua_isuserdata(L, index)) {
		luaL_error(L,"expected object to call function");
	}
	void* userdata = lua_touserdata(L, index);

	lua_getmetatable(L, index);
	luaL_getmetatable(L, "object");
	if (lua_rawequal(L, -1, -2)) {
		uint64_t handle = *((uint64_t*)userdata);
		auto obj = eng->get_object(handle);
		if (!obj) {
			stack_dump(L);
			luaL_error(L, "expected object to call function");
		}
		lua_pop(L, 2);
		return obj;
	}
	lua_pop(L, 2);
	lua_getmetatable(L, index);
	luaL_getmetatable(L, "classbase");
	if (lua_rawequal(L, -1, -2)) {
		lua_pop(L, 2);
		return *(ClassBase**)userdata;
	}
	luaL_error(L, "not valid table to find object");
	return nullptr;
}
static int objects_are_equal(lua_State* L)
{
	void* a = get_class_from_stack(L, 1);
	void* b = get_class_from_stack(L, 1);
	lua_pushboolean(L, bool(a == b));
	return 1;
}

static void make_table_for_gameobject(lua_State* L, BaseUpdater* u)
{
	if (!u) {
		lua_pushnil(L);
		return;
	}
	auto handle = (uint64_t*)lua_newuserdata(L, sizeof(uint64_t));
	*handle = u->get_instance_id();
	luaL_getmetatable(L, "object");
	lua_setmetatable(L, -2);
}

static void make_table_for_class(lua_State* L, ClassBase* c)
{
	if (!c) {
		lua_pushnil(L);
		return;
	}

	auto ptr = (ClassBase**)lua_newuserdata(L, sizeof(ClassBase*));
	*ptr = c;
	luaL_getmetatable(L, "classbase");
	lua_setmetatable(L, -2);
}
void call_lua_func_internal_part1(ScriptComponent* s, const char* func_name)
{
	s->call_function_part1(func_name);
}
bool ScriptComponent::call_function_part1(const char* func_name)
{
	lua_State* L = nullptr;
	const int start_top = lua_gettop(L);
	if (*func_name) {
		push_table_to_stack();
		ASSERT(lua_istable(L, -1));

		lua_getfield(L, -1, func_name);

		if (!lua_isfunction(L, -1)) {
			sys_print(Error, "script doesnt have function %s\n", func_name);
			lua_pop(L, 2);
			return false;
		}
		// table, function
		lua_insert(L, -2);
		lua_pop(L, 1);
	}
	ASSERT(lua_isfunction(L, -1));

	lua_getglobal(L, "_G");
	ASSERT(lua_istable(L, -1));
	push_table_to_stack();
	lua_setglobal(L, "_G");
	ASSERT(lua_istable(L, -1));
	lua_insert(L, -2);	// insert it below function
	ASSERT(lua_isfunction(L, -1) && lua_istable(L, -2));

	return true;
}
bool ScriptComponent::call_function_part2(const char* func_name, int num_args)
{
	lua_State* L = nullptr; 
	
	ASSERT(lua_isfunction(L, -1 - num_args));
	ASSERT(lua_istable(L, -2 - num_args));

	if (lua_pcall(L, num_args, 0, 0) != LUA_OK) {
		sys_print(Error, "error calling function %s script: %s\n", func_name, lua_tostring(L, -1));
		lua_pop(L, 2/* one error code, one global*/);
		return false;
	}
	ASSERT(lua_gettop(L) == 1);
	ASSERT(lua_istable(L, -1));
	lua_setglobal(L, "_G");
	ASSERT(lua_gettop(L) == 0);

	return true;
}
bool ScriptComponent::call_function(const char* func_name)
{
	if (!call_function_part1(func_name))
		return false;
	if (!call_function_part2(func_name,0))
		return false;

	return true;
}
void call_lua_func_internal_part2(ScriptComponent* s, const char* func_name, int num_args)
{
	s->call_function_part2(func_name, num_args);
}

lua_State* get_lua_state_for_call_func()
{
	return nullptr;
}



ScriptComponent::ScriptComponent()
{
	set_call_init_in_editor(true);
}
ScriptComponent::~ScriptComponent()
{
	
}

void ScriptComponent::editor_on_change_property()
{

}

bool ScriptComponent::has_function(const char* func_name)
{
	lua_State* L = nullptr;
	push_table_to_stack();
	lua_getfield(L,-1, func_name);
	bool res = lua_isfunction(L, -1);
	lua_pop(L, 2);
	return res;
}
void asset_s(){

}
void ScriptComponent::print_my_table()
{
	
}
void ScriptComponent::push_table_to_stack()
{
	lua_State* L = nullptr;
	lua_rawgetp(L, LUA_REGISTRYINDEX, this);
	if (!lua_istable(L, -1))
		luaL_error(L, "not table for script_component");
}


void ScriptComponent::pre_start()
{
	loaded_successfully = false;
	if (eng->is_editor_level())
	{

	}
	else {
		if (0) {
			lua_State* L = nullptr;

			{
				ASSERT(lua_gettop(L) == 0);
				lua_newtable(L);		// make a new table for us

				lua_pushvalue(L, -1);
				luaL_getmetatable(L, "globalmeta");
				ASSERT(lua_gettop(L) == 3&&lua_istable(L,-1)&&lua_istable(L,-2));
				lua_setmetatable(L, -2);	// inherit from global table

				// set registry
				ASSERT(lua_gettop(L) == 2 && lua_istable(L, -1));
				lua_rawsetp(L, LUA_REGISTRYINDEX, this);

				ASSERT(lua_gettop(L) == 1);
				ASSERT(lua_istable(L, -1));
				lua_pop(L, 1);	// pop table

				ASSERT(lua_gettop(L) == 0);
			}
			return;
			//if (luaL_loadstring(L, script->script_str.c_str()) != LUA_OK) {
			//	sys_print(Error, "error loading script: %s\n", lua_tostring(L, -1));
			//	lua_pop(L, 1);
			//	ASSERT(lua_gettop(L) == 0);
			//	return;
			//}

			push_table_to_stack();
			{
				lua_getfield(L, -1, "eng");
				ASSERT(lua_isuserdata(L, -1));
				lua_pop(L, 1);

				lua_getmetatable(L, -1);
				luaL_getmetatable(L, "globalmeta");
				ASSERT(lua_rawequal(L, -1, -2));
				lua_pop(L, 2);
			}
			ASSERT(lua_istable(L, -1));
			ASSERT(lua_isfunction(L, -2));
			lua_setupvalue(L, -2, 1);	// set global table

			// set our globals
			{
				ASSERT(lua_gettop(L) == 1);	// only function
				push_table_to_stack();
				make_table_for_gameobject(L, this);
				ASSERT(lua_istable(L, -2));
				ASSERT(lua_isuserdata(L, -1));
				lua_setfield(L, -2, "script");

				make_table_for_gameobject(L, get_owner());
				ASSERT(lua_istable(L, -2));
				ASSERT(lua_isuserdata(L, -1));
				lua_setfield(L, -2, "this");

				lua_pop(L, 1);	// pop table
			}
			ASSERT(lua_gettop(L) == 1 && lua_isfunction(L, -1));

			// run initial
			if (!call_function("")) {
				sys_print(Error, "script failed to initialize: %s\n", lua_tostring(L, -1));
				lua_settop(L, 0);
				return; 
			}
			ASSERT(lua_gettop(L) == 0);

			loaded_successfully = true;
		}
	}
}
void ScriptComponent::start()
{
	if (loaded_successfully) {
		call_function("event_start");
		set_ticking(has_function("event_update"));
	}
}
void ScriptComponent::end()
{
#if 0
	if (loaded_successfully) {
		call_function("event_end");
		
		for (auto& out : outstandings) {
			if (out.ptr) {
				out.pi->multicast->remove(out.pi->get_ptr(out.ptr),this);
			}
			else {
				auto obj = eng->get_object(out.handle);
				if (obj) {
					out.pi->multicast->remove(out.pi->get_ptr(obj),this);
				}
			}
		}
		auto L = script_local.L;
		lua_pushnil(L);
		lua_rawsetp(L, LUA_REGISTRYINDEX, this);

	}
#endif
}
void ScriptComponent::update()
{
	return;
	if (!loaded_successfully || !call_function("event_update")) {
		set_ticking(false);
	}
}
