#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
using std::string;
using std::unordered_map;
using std::vector;
#include "Framework/ClassTypeInfo.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ConsoleCmdGroup.h"
#include "Framework/MulticastDelegate.h"
#include <stdexcept>

class Component;

struct ParseProperty
{
	string name;
	string type_str;
};
struct ParseType
{
	string name;
	vector<string> inherited;
	vector<ParseProperty> props;
	// True when the class's `---@class` annotation block contained a bare
	// `---editor` line. Gates whether Component subclasses show up in the
	// editor add-component picker. See LuaClassTypeInfo::is_editor_placeable.
	bool editor_placeable = false;
};
// parses the script
class ScriptLoadingUtil
{
public:
	static vector<ParseType> parse_text(string text);
};

class LuaClassTypeInfo : public ClassTypeInfo
{
public:
	LuaClassTypeInfo();
	~LuaClassTypeInfo();
	void set_classname(string s);
	bool set_superclass(string s);
	const string& get_name();
	void init_lua_type();
	bool get_and_clear_had_changes() {
		bool b = had_changes;
		had_changes = false;
		return b;
	}
	void set_had_changes() { had_changes = true; }

	// Stage the next ParseProperty list. Held in `pending_parsed_properties` and only
	// committed into `parsed_properties` from inside ScriptManager::check_for_reload, AFTER
	// the live-instance snapshot has been taken — that snapshot reads pi.name pointers
	// into the old parsed_properties' string storage, so we must not destroy it earlier.
	void set_parsed_properties(vector<ParseProperty> props) { pending_parsed_properties = std::move(props); }

	// Whether this Lua-defined Component subclass should appear in the editor's
	// add-component picker. Set from the `---editor` annotation tag on each
	// reload. Defaults to false so script-only helpers stay out of the picker.
	void set_editor_placeable(bool b) { editor_placeable = b; }
	bool is_editor_placeable() const { return editor_placeable; }

	// Reflection accessors used by the reload-merge pass + tests.
	uint32_t get_lua_field_shadow_size() const { return lua_field_shadow_size; }
	const PropertyInfoList* get_lua_props_list() const { return lua_props_list.list ? &lua_props_list : nullptr; }
	const vector<PropertyInfo>& get_lua_props_storage() const { return lua_props_storage; }
	int get_template_lua_table() const { return template_lua_table; }

	// Live-instance bookkeeping for hot-reload field migration. Inserted in lua_class_alloc,
	// erased in the Component dtor hook (Component::~Component calls unregister_lua_instance).
	void register_lua_instance(Component* c) { live_instances.insert(c); }
	void unregister_lua_instance(Component* c) { live_instances.erase(c); }
	const std::unordered_set<Component*>& get_live_instances() const { return live_instances; }

	// Test-only: runs the layout/typing logic against parsed_properties without
	// requiring ClassBase::find_class("Component") to succeed (which needs the full
	// engine registry init). Production code uses init_lua_type() instead.
	void synthesize_lua_props_unchecked_for_test();

private:
	friend class ScriptManager;

	bool had_changes = false;
	bool editor_placeable = false;
	int template_lua_table = 0;
	static ClassBase* lua_class_alloc(const ClassTypeInfo* c);
	string lua_classname;

	// Cached parse + synthesized reflection metadata for Component subclasses.
	// parsed_properties owns the const char* strings referenced by lua_props_storage entries,
	// so its lifetime must outlive snapshot reads in check_for_reload.
	vector<ParseProperty> parsed_properties;
	vector<ParseProperty> pending_parsed_properties; // committed by ScriptManager::check_for_reload
	vector<PropertyInfo>  lua_props_storage;
	PropertyInfoList      lua_props_list = {};
	uint32_t              lua_field_shadow_size = 0;

	std::unordered_set<Component*> live_instances;
	std::vector<std::string> pending_interfaces;

	void synthesize_lua_props_for_component_subclass();
};
class LuaRuntimeError : public std::runtime_error
{
public:
	LuaRuntimeError(std::string msg) : std::runtime_error("LuaError: " + msg) {}
};
struct lua_State;
class ScriptManager
{
public:
	static ScriptManager* inst;
	ScriptManager();
	~ScriptManager();
	void update();
	void check_for_reload();
	void load_script_files();
	// Loads Data/scripts/tests/**/*.lua. Called only in --tests mode so
	// add_test(...) calls at file scope don't fire in normal app runs.
	void load_test_scripts();
	void init_this_class_type(ClassTypeInfo* classTypeInfo);
	void set_class_type_global(ClassTypeInfo* type);
	void set_enum_global(const std::string& name, const EnumTypeInfo*);
	int create_class_table_for(ClassBase* classTypeInfo);
	void free_class_table(int id);
	lua_State* get_lua_state() { return lua; }
	ClassBase* allocate_class(string name);
	void reload_all_scripts();
	void reload_one_file(const string& fileName);
	// Load and execute Lua source text directly, without touching the filesystem.
	// Used by reload_one_file and directly by tests.
	void reload_from_content(const std::string& source, const std::string& chunkname);
	// Start EmmyLuaDebugger TCP listener so VS Code (tangzx.emmylua) can attach.
	// Call after load_script_files(). See docs/scripting/vscode_debugger.md.
	void activate_debugger(const char* host, int port);

	// Called from Component::~Component to drop the instance from its LuaClassTypeInfo
	// live set and run destructors over any non-POD shadow fields (e.g. std::string).
	// No-op when c's type is not Lua-defined or no ScriptManager is alive.
	static void on_component_destructed(Component* c);

	// Fires at the end of check_for_reload() when at least one Lua class was rebuilt.
	// Listeners must drop any cached `const PropertyInfo*` / `PropertyInfoList*` they
	// hold into LuaClassTypeInfo storage — phase 3 reallocates lua_props_storage and
	// rewrites ti->props, freeing the old PropertyInfo entries.
	MulticastDelegate<> on_class_reloaded;

private:
	bool had_changes = false;
	void initialize_class_type(const ClassTypeInfo* type);
	lua_State* lua = nullptr;
	std::unordered_map<std::string, uptr<LuaClassTypeInfo>> lua_classes;
};