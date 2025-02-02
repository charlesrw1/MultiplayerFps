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
#include "Game/AssetPtrMacro.h"
#include "Game/EntityPtrArrayMacro.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"

#include "Framework/ReflectionMacros.h"


IAsset* get_iasset_from_lua(lua_State* L, int index) {
	if (lua_isnil(L, index))
		return nullptr;
	if (!lua_istable(L, index)) {
		luaL_error(L, "expected table object function");
	}
	lua_pushstring(L, "_ptr");
	lua_rawget(L, index);
	if (!lua_isuserdata(L, -1)) {
		luaL_error(L, "expected iasset to call function");
	}
	void* user = lua_touserdata(L, -1);
	lua_pop(L, 1);
	ClassBase* c = (ClassBase*)user;
	if (!c)
		return nullptr;
	if (c->is_a<IAsset>()) {
		return (IAsset*)c;
	}
	luaL_error(L, "expected iasset object");
	return nullptr;
}
EntityComponent* get_component_from_lua(lua_State* L, int index) {
	if (lua_isnil(L, index))
		return nullptr;
	if (!lua_istable(L, index)) {
		luaL_error(L, "expected table object function");
	}
	lua_pushstring(L, "_id");
	lua_rawget(L, index);
	if (!lua_isinteger(L, -1)) {
		luaL_error(L, "expected object to call function");
	}
	lua_Integer integer = lua_tointeger(L, -1);
	lua_pop(L, 1);
	auto obj = eng->get_object(integer);
	if (!obj) return nullptr;
	if (obj->is_a<EntityComponent>()) return (EntityComponent*)obj;
	luaL_error(L, "object not an entity component type");
	return nullptr;
}
Entity* get_entity_from_lua(lua_State* L, int index) {
	if (lua_isnil(L, index))
		return nullptr;

	if (!lua_istable(L, index)) {
		luaL_error(L, "expected table object function");
	}
	lua_pushstring(L, "_id");
	lua_rawget(L, index);
	if (!lua_isinteger(L, -1)) {
		luaL_error(L, "expected object to call function");
	}
	lua_Integer integer = lua_tointeger(L, -1);
	lua_pop(L, 1);
	auto obj = eng->get_object(integer);
	if (!obj) return nullptr;
	if (obj->is_a<Entity>()) return (Entity*)obj;
	luaL_error(L, "object not an entity type");
	return nullptr;
}
static void make_table_for_class(lua_State* L, ClassBase* c);
static void make_table_for_gameobject(lua_State* L, BaseUpdater* c);

void stack_dump(lua_State* L);
void push_iasset_to_lua(lua_State* L, IAsset* a) {
	make_table_for_class(L, a);
}
void push_entity_to_lua(lua_State* L, Entity* e) {
	make_table_for_gameobject(L, e);
}
void push_entitycomponent_to_lua(lua_State* L, EntityComponent* ec) {
	make_table_for_gameobject(L, ec);
}

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
glm::vec3 get_from_lua(lua_State* s, int i)
{
	return{};
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

static void add_delegate(lua_State* L, ClassBase* md, PropertyInfo* pi, const char* funcname)
{
	pi->multicast->add(pi->get_ptr(md), L, funcname);

	lua_getfield(L, LUA_REGISTRYINDEX, "out");
	auto outstandings = (std::vector<OutstandingScriptDelegate>*)lua_touserdata(L, -1);
	if (!outstandings)
		luaL_error(L, "object didnt have outstanding reg");
	ClassBase* c = (ClassBase*)md;
	if (c->is_a<BaseUpdater>()) {
		OutstandingScriptDelegate out;
		out.handle = ((BaseUpdater*)(c))->get_instance_id();
		out.pi = pi;
		outstandings->push_back(out);
	}
	else {
		OutstandingScriptDelegate out;
		out.ptr = c;
		out.pi = pi;
		outstandings->push_back(out);
	}
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
	if (!pi->multicast)
		lua_error(L);

	add_delegate(L, (ClassBase*)md, pi, name);

	lua_pop(L, 1);
	return 0;
}
static int multicast_sub_lua(lua_State* L)
{
	// fixme
	return 0;
}

static PropertyInfo* find_delegate(BaseUpdater* c, StringView str)
{
	auto find = c->get_type().prop_hash_table->prop_table.find(str);
	if (find == c->get_type().prop_hash_table->prop_table.end()) {
		return nullptr;
	}
	if (find->second->type == core_type_id::MulticastDelegate)
		return find->second;
	return nullptr;
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
	auto p = find_delegate(ent,name);
	if (!p) {
		for (auto c : ent->get_components()) {
			p = find_delegate(c, name);
			if (p) {
				what = c;
				break;
			}
		}
	}
	if (!p) {
		sys_print(Warning, "couldnt connect delegate %s %s\n", lua_tostring(L, 2), lua_tostring(L, 3));
	}
	else {
		add_delegate(L, what, p, lua_tostring(L, 3));
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
	auto prop = find->second;
	if (prop->type == core_type_id::Function) {
		lua_pushcfunction(L, prop->call_function);
	}
	else if (prop->type == core_type_id::GetterFunc) {
		ASSERT(lua_istable(L, 1));
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
void* get_class_from_stack(lua_State* L, int index)
{
	if (lua_isnil(L, index))
		return nullptr;

	if (!lua_istable(L, index)) {
		luaL_error(L,"expected object to call function");
	}
	lua_pushstring(L, "_id");
	lua_rawget(L, 1);
	if (lua_isinteger(L, -1)) {
		lua_Integer integer = lua_tointeger(L, -1);
		lua_pop(L, 1);
		auto obj = eng->get_object(integer);
		if (!obj) {
			luaL_error(L, "expected object to call function");
		}
		return obj;
	}
	lua_pop(L, 1);
	lua_pushstring(L, "_ptr");
	lua_rawget(L, index);
	if (lua_isuserdata(L, -1)) {
		void* user = lua_touserdata(L, -1);
		lua_pop(L, 1);
		return user;
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

	lua_newtable(L);
	lua_pushstring(L, "_id");
	if (u)
		lua_pushinteger(L, u->get_instance_id());
	else
		lua_pushinteger(L, 0);
	lua_settable(L, -3);

	luaL_newmetatable(L, "object");
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, object_table_access);
	lua_settable(L, -3);	// metatable.__index = object_table_access
	lua_pushstring(L, "__eq");
	lua_pushcfunction(L, objects_are_equal);
	lua_settable(L, -3);


	lua_setmetatable(L, -2); // table.metatable = metatable
}

static void make_table_for_class(lua_State* L, ClassBase* c)
{
	if (!c) {
		lua_pushnil(L);
		return;
	}

	lua_newtable(L);
	lua_pushstring(L, "_ptr");
	lua_pushlightuserdata(L, c);
	lua_settable(L, -3);	// table._ptr = c

	luaL_newmetatable(L,c->get_type().classname);
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, classbase_table_access);
	lua_settable(L, -3);	// metatable.__index = classbase_table_access
	lua_pushstring(L, "__eq");
	lua_pushcfunction(L, objects_are_equal);
	lua_settable(L, -3);


	lua_setmetatable(L, -2); // table.metatable = metatable
}
void call_lua_func_internal_part1(lua_State* L, const char* func_name)
{
	lua_getglobal(L, func_name);
}

int call_lua_func_internal_part2(lua_State* L, const char* func_name, int num_args)
{
	if (lua_pcall(L, num_args, 0, 0) != LUA_OK) {
		sys_print(Error, "Error in lua delegate callback (%s): %s\n", func_name, lua_tostring(L, -1));
		lua_pop(L, 1);
		return -1;
	}
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
	make_table_for_gameobject(L, script->get_owner());
	lua_setglobal(L, "this");
	make_table_for_gameobject(L, script);
	lua_setglobal(L, "script");
}


CLASS_IMPL(ScriptComponent);

ScriptComponent::ScriptComponent()
{
	set_call_init_in_editor(true);
}
ScriptComponent::~ScriptComponent()
{
	ASSERT(!state);
}

void ScriptComponent::editor_on_change_property()
{

}

void ScriptComponent::pre_start()
{
	if (eng->is_editor_level())
	{

	}
	else {
		if (script) {
			state = luaL_newstate();
			
			lua_pushlightuserdata(state, &outstandings);
			lua_setfield(state, LUA_REGISTRYINDEX, "out");

			luaL_openlibs(state);
			if (luaL_loadstring(state, script->script_str.c_str()) != LUA_OK) {
				sys_print(Error, "error loading script: %s\n", lua_tostring(state, -1));
				lua_close(state);
				state = nullptr;
				return;
			}

			ScriptManager::get().init_new_script(this, state);

			if (lua_pcall(state, 0, 0, 0) != LUA_OK) {
				sys_print(Error, "script failed to initialize: %s\n", lua_tostring(state, -1));
				lua_close(state);
				state = nullptr;
				return;
			}
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

		lua_getglobal(L, "event_update");
		if (lua_isfunction(L, -1))
			set_ticking(true);
		lua_pop(L, 1);

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

		for (auto& out : outstandings) {
			if (out.ptr) {
				out.pi->multicast->remove(out.ptr, L);
			}
			else {
				auto obj = eng->get_object(out.handle);
				if (obj) {
					out.pi->multicast->remove(out.pi->get_ptr(obj), L);
				}
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
const PropertyInfoList* ScriptComponent::get_props() {
	MAKE_VECTORCALLBACK_ATOM(EntityPtr, refs);
	START_PROPS(ScriptComponent)
		REG_ASSET_PTR(script, PROP_DEFAULT),
		REG_STDVECTOR(refs, PROP_DEFAULT),
		REG_FUNCTION(get_ref),
		REG_FUNCTION(get_num_refs)
	END_PROPS(ScriptComponent)
}