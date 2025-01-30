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

IAsset* get_iasset_from_lua(lua_State* L, int index) {
	return nullptr;
}
EntityComponent* get_component_from_lua(lua_State* L, int index) {
	return nullptr;
}
Entity* get_entity_from_lua(lua_State* L, int index) {
	return nullptr;
}
void push_iasset_to_lua(lua_State* L, IAsset* a) {

}
void push_entity_to_lua(lua_State* L, Entity* e) {

}
void push_entitycomponent_to_lua(lua_State* L, EntityComponent* ec) {

}


template<>
void push_to_lua(lua_State* s, float f)
{

}
template<>
void push_to_lua(lua_State* s, int i)
{

}
template<>
void push_to_lua(lua_State* s, ClassBase* e)
{

}
template<>
void push_to_lua(lua_State* s, const char* str)
{

}


template<>
float get_from_lua(lua_State* s, int i)
{
	return 0.f;
}
template<>
glm::vec3 get_from_lua(lua_State* s, int i)
{
	return{};
}




template<>
const char* get_from_lua(lua_State* s, int i)
{
	return "";
}

ScriptManager& ScriptManager::get() {
	static ScriptManager inst;
	return inst;
}


CLASS_H(EngineWrapper,ClassBase)
public:
	void change_level(const char* s) {
		eng->open_level(s);
	}
	float get_dt() {
		return eng->get_dt();
	}
	Entity* spawn_entity() {
		return eng->get_level()->spawn_entity_class<Entity>();
	}
	Entity* spawn_prefab(PrefabAsset* prefab) {
		return eng->get_level()->spawn_prefab(prefab);
	}
	float get_time() {
		return eng->get_game_time();
	}
	IAsset* find_asset(const char* asset_type, const char* name) {
		return AssetDatabase::get().find_sync(name, ClassBase::find_class(asset_type), 0).get();
	}
	void log_to_screen(const char* text) {
		eng->log_to_fullscreen_gui(Info, text);
	}

	static const PropertyInfoList* get_props() {
		START_PROPS(EngineWrapper)
			REG_FUNCTION(change_level),
			REG_FUNCTION(get_dt),
			REG_FUNCTION(spawn_entity),
			REG_FUNCTION(spawn_prefab),
			REG_FUNCTION(get_time),
			REG_FUNCTION(find_asset),
			REG_FUNCTION(log_to_screen)
		END_PROPS(EngineWrapper)
	}
};
CLASS_IMPL(EngineWrapper);
CLASS_H(VectorWrapper, ClassBase)
public:
	void add(glm::vec3 other) {
		x += other.x;
		y += other.y;
		z += other.z;
	}
	float dot(glm::vec3 other) {
		return glm::dot(glm::vec3(x, y, z), other);
	}
	void normalize() {
		set_from(glm::normalize(glm::vec3(x, y, z)));
	}
	void cross(glm::vec3 other) {
		set_from(glm::cross(glm::vec3(x, y, z), other));
	}
	void mult(float f) {
		x *= f;
		y *= f;
		z *= z;
	}
	void set_from(glm::vec3 v) {
		x = v.x;
		y = v.y;
		z = v.z;
	}

	float x = 0.f;
	float y = 0.f;
	float z = 0.f;

	static const PropertyInfoList* get_props() {
		START_PROPS(VectorWrapper)
			REG_FLOAT(x, PROP_DEFAULT, ""),
			REG_FLOAT(y, PROP_DEFAULT, ""),
			REG_FLOAT(z, PROP_DEFAULT, ""),
			REG_FUNCTION(add),
			REG_FUNCTION(dot),
			REG_FUNCTION(normalize),
			REG_FUNCTION(cross),
			REG_FUNCTION(mult)
		END_PROPS(VectorWrapper)
	}
};
CLASS_IMPL(VectorWrapper);


ScriptManager::ScriptManager()
{
	enginewrapper.reset(new EngineWrapper);
}

#include "Framework/PropHashTable.h"
// assumed 
static int table_access_shared(lua_State* L, ClassBase* c)
{
	const char* str = lua_tostring(L, 2);
	auto find = c->get_type().prop_hash_table->prop_table.find(StringView(str));
	if (find == c->get_type().prop_hash_table->prop_table.end()) {
		luaL_error(L, "object didnt have property: %s",str);
		return 0;
	}
	auto prop = find->second;
	if (prop->type == core_type_id::Function) {
		lua_pushcfunction(L, prop->call_function);
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
	else {
		luaL_error(L, "not valid property to access: %s", str);
		return 0;
	}
	return 1;
}

static int classbase_table_access(lua_State* L)
{
	lua_pushstring(L, "_ptr");
	lua_gettable(L, -3);
	if (!lua_isuserdata(L, -1))
		luaL_error(L, "object didnt have ptr to access");
	void* user = lua_touserdata(L, -1);
	//lua_pop(L, -1);
	return table_access_shared(L, (ClassBase*)user);
}
static int object_table_access(lua_State* L)
{
	// stack is table, string
	lua_pushstring(L, "_id");
	lua_gettable(L, -3);
	if (!lua_isinteger(L, -1))
		luaL_error(L,"object didnt have id to access");
	lua_Integer integer = lua_tointeger(L, -1);
	lua_pop(L, 1);
	auto obj = eng->get_object(integer);
	if (!obj) {
		lua_pushnil(L);
		return 1;
	}
	return table_access_shared(L, obj);
}
void* get_class_from_stack(lua_State* L)
{
	if (!lua_istable(L, 1)) {
		luaL_error(L,"expected object to call function");
	}
	lua_pushstring(L, "_id");
	lua_gettable(L, 1);
	if (lua_isinteger(L, -1)) {
		lua_Integer integer = lua_tointeger(L, -1);
		lua_pop(L, 1);
		auto obj = eng->get_object(integer);
		if (!obj) {
			luaL_error(L, "expected object to call function");
		}
		return obj;
	}
	lua_pushstring(L, "_ptr");
	lua_gettable(L, 1);
	if (lua_isuserdata(L, -1)) {
		void* user = lua_touserdata(L, -1);
		lua_pop(L, 1);
		return user;
	}
	luaL_error(L, "not valid table to find object");
	return nullptr;
}

static void make_table_for_gameobject(lua_State* L, BaseUpdater* u)
{
	lua_newtable(L);
	lua_pushstring(L, "_id");
	lua_pushinteger(L, u->get_instance_id());
	lua_settable(L, -3);

	luaL_newmetatable(L, u->get_type().classname);
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, object_table_access);
	lua_settable(L, -3);	// metatable.__index = object_table_access


	lua_setmetatable(L, -2); // table.metatable = metatable
}

static void make_table_for_class(lua_State* L, ClassBase* c)
{
	lua_newtable(L);
	lua_pushstring(L, "_ptr");
	lua_pushlightuserdata(L, c);
	lua_settable(L, -3);	// table._ptr = c

	luaL_newmetatable(L,c->get_type().classname);
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, classbase_table_access);
	lua_settable(L, -3);	// metatable.__index = classbase_table_access


	lua_setmetatable(L, -2); // table.metatable = metatable
}


static int global_metatable_access(lua_State* s)
{
	// global functions (creation types)
	return 0;
}

void ScriptManager::init_new_script(ScriptComponent* script, lua_State* L)
{
	//lua_pushglobaltable(L);
	//luaL_newmetatable(L, "global");
	//lua_pushstring(L, "__index");
	//lua_pushcfunction(L, global_metatable_access);
	//lua_settable(L, -3);
	//lua_setmetatable(L, -2);

	make_table_for_class(L, enginewrapper.get());
	lua_setglobal(L, "eng");
	//make_table_for_gameobject(L, script->get_owner());
	//lua_setglobal(L, "this");
}


CLASS_IMPL(ScriptComponent);

ScriptComponent::ScriptComponent()
{

}
ScriptComponent::~ScriptComponent()
{
	ASSERT(!state);
}

void ScriptComponent::pre_start()
{
	if (script) {
		state = luaL_newstate();
		luaL_openlibs(state);
		if (luaL_loadstring(state, script->script_str.c_str()) != LUA_OK) {
			sys_print(Error, "error loading script: %s\n",lua_tostring(state,-1));
			lua_close(state);
			state = nullptr;
			return;
		}

		ScriptManager::get().init_new_script(this, state);

		if (lua_pcall(state, 0, 0, 0) != LUA_OK) {
			sys_print(Error, "script failed to initialize: %s\n",lua_tostring(state,-1));
			lua_close(state);
			state = nullptr;
			return;
		}
	}
}
void ScriptComponent::start()
{
	if (state) {
		auto L = state;
		lua_getglobal(L, "event_start");
		if (lua_isfunction(L, -1)) {
			if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
				sys_print(Error, "script start failed: %s\n", lua_tostring(state, -1));
			}
		}

	}
}
void ScriptComponent::end()
{
	if (state) {
		auto L = state;
		lua_getglobal(L, "event_end");
		if (lua_isfunction(L, -1)) {
			if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
				sys_print(Error, "script start end: %s\n", lua_tostring(state, -1));
			}
		}

		lua_close(state);
		state = nullptr;
	}
}
void ScriptComponent::update()
{
	if (!state)
		return;
	auto L = state;
	lua_getglobal(L, "event_update");
	if (lua_isfunction(L, -1)) {
		if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
			sys_print(Error, "script start end: %s\n", lua_tostring(state, -1));
		}
	}
	else {
		set_ticking(false);
	}
}