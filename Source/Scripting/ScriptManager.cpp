#include "ScriptManager.h"
#include "Framework/StringUtils.h"
#include "Framework/Files.h"
#include "Framework/MapUtil.h"
#include <cassert>
#include <cstring>
#include "Framework/Config.h"
#include "Game/EntityComponent.h"
#include <new>
#include <string>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
void dump_lua_table(lua_State* L, int index, int depth);
vector<ParseType> ScriptLoadingUtil::parse_text(string text) {
	auto lines = StringUtils::to_lines(text);

	struct PendingClass
	{
		string name;
		vector<string> inherited;
		vector<ParseProperty> properties;
	};
	vector<ParseType> out;

	PendingClass currentClass;
	bool inClass = false;
	string pendingType;
	for (int i = 0; i < lines.size(); i++) {
		auto line = StringUtils::strip(lines.at(i));
		if (line.empty())
			continue;
		// Save the original stripped line before replacements for annotation checking
		auto original_stripped_line = line;
		StringUtils::replace(line, "---", "--- ");
		StringUtils::replace(line, ":", " : ");
		StringUtils::replace(line, "=", " = ");
		StringUtils::replace(line, "{", " { ");
		StringUtils::replace(line, "}", " } ");
		StringUtils::replace(line, ",", " , ");

		auto tokens = StringUtils::split(line);
		if (tokens.empty())
			continue;

		auto append_class = [&]() {
			if (!currentClass.name.empty()) {
				out.push_back({currentClass.name, currentClass.inherited, currentClass.properties});
				currentClass = PendingClass();
			}
			inClass = false;
			// printf("inclass=false %d\n", i + 1);
			pendingType.clear();
		};

		// Parse class definition
		// Check original stripped line starts with "---@class" to avoid matching comments containing "@class"
		if (StringUtils::starts_with(original_stripped_line, "---@class") && tokens.size() > 2 && tokens.at(1) == "@class") {

			// printf("found class: %s\n", tokens.at(2).c_str());

			if (inClass && !currentClass.name.empty()) {
				// Save previous class
				out.push_back({currentClass.name, currentClass.inherited, currentClass.properties});
				currentClass = PendingClass();
			}
			currentClass.name = tokens.at(2);
			currentClass.inherited.clear();
			currentClass.properties.clear();
			inClass = false;
			// printf("inclass=false %d\n", i + 1);
			// Parse inheritance
			for (size_t j = 3; j < tokens.size(); ++j) {
				if (tokens[j] == ":") {
					for (size_t k = j + 1; k < tokens.size(); ++k) {
						if (tokens[k] != ",")
							currentClass.inherited.push_back(tokens[k]);
					}
					break;
				}
			}
		}
		// Detect start of class table
		else if (tokens.size() >= 3 && tokens.at(1) == "=" && tokens.at(2) == "{" &&
				 StringUtils::starts_with(lines.at(i), tokens.at(0))) { // test for no leading whitespace

			// printf("start class: %d\n",i+1);

			if (!currentClass.name.empty()) {
				inClass = true;
				// printf("inclass=true %d\n", i + 1);
			}
			if (tokens.at(tokens.size() - 1) == "}") {
				printf("oneline class\n");
				append_class();
			}

		}
		// Parse property type annotation
		// Check original stripped line starts with "---@type" to avoid matching comments containing "@type"
		else if (StringUtils::starts_with(original_stripped_line, "---@type") && tokens.size() >= 3 && tokens.at(1) == "@type") {

			// printf("found property type %s %d\n",tokens.at(2).c_str(), i + 1);

			pendingType = tokens.at(2);
		}
		// Parse property assignment — only record fields preceded by ---@type.
		// Untyped table entries are intentionally dropped so they never reach reflection
		// synthesis (no warnings, no editor rows). Lua scripts can still read/write them
		// at runtime via the per-instance table; they're just not engine-visible.
		else if (inClass && tokens.size() >= 3 && (tokens.at(1) == "=" || tokens.at(1) == ",")) {
			if (!pendingType.empty()) {
				ParseProperty prop;
				prop.name = tokens.at(0);
				prop.type_str = pendingType;
				currentClass.properties.push_back(prop);
			}
			pendingType.clear();
		}
		// End of class table
		else if (inClass && StringUtils::starts_with(lines.at(i), "}")) {

			// printf("end class %d\n", i + 1);

			append_class();
		}
	}
	// Handle last class if file doesn't end with }
	if (inClass && !currentClass.name.empty()) {
		out.push_back({currentClass.name, currentClass.inherited, currentClass.properties});
	}

	return out;
}

static ConfigVar g_lua_debug("g_lua_debug", "0", CVAR_BOOL,
							 "Start MobDebug on script load (requires ZeroBrane/DAP server on port g_lua_debug_port)");
static ConfigVar g_lua_debug_host("g_lua_debug_host", "localhost", 0, "MobDebug server host");
static ConfigVar g_lua_debug_port("g_lua_debug_port", "8172", CVAR_INTEGER | CVAR_UNBOUNDED, "MobDebug server port");
// Extra directory to add to Lua package.cpath for loading socket/core.dll and mime/core.dll.
// Set to your vcpkg bin dir, e.g.: C:/Users/you/source/vcpkg/installed/x64-windows/bin
static ConfigVar g_lua_cpath_extra("g_lua_cpath_extra", "", 0,
								   "Extra dir appended to Lua package.cpath for C socket extensions");

ScriptManager::ScriptManager() {
	lua = luaL_newstate();
	luaL_openlibs(lua);

	// Add Data/scripts/lib/ to package.path so require("mobdebug"), require("socket"), etc. work.
	// Files in lib/ are module-style (not auto-executed by reload_all_scripts).
	lua_getglobal(lua, "package");
	lua_getfield(lua, -1, "path");
	std::string new_path = std::string(lua_tostring(lua, -1)) + ";Data/scripts/lib/?.lua";
	lua_pop(lua, 1);
	lua_pushstring(lua, new_path.c_str());
	lua_setfield(lua, -2, "path");
	lua_pop(lua, 1);
}

ScriptManager::~ScriptManager() {
	lua_close(lua);
	lua = nullptr;
}
void ScriptManager::init_this_class_type(ClassTypeInfo* classTypeInfo) {
	int height = lua_gettop(lua);
	assert(height == 0);
	lua_newtable(lua); // the output metatable
	lua_newtable(lua); // new __index table
	auto type = classTypeInfo;
	while (type) {
		for (int i = 0; i < type->lua_function_count; i++) {
			auto& func = type->lua_functions[i];
			if (!func.is_static) {
				lua_pushcfunction(lua, func.lua_c_function);
				lua_setfield(lua, -2, func.name);
			}
		}
		type = const_cast<ClassTypeInfo*>(type->super_typeinfo);
	}
	//	sys_print(Debug, "%s __index", classTypeInfo->classname);
	// dump_lua_table(lua, -1,0);

	height = lua_gettop(lua);
	assert(height == 2);
	lua_setfield(lua, -2, "__index");
	assert(lua_gettop(lua) == 1);
	// sys_print(Debug, "%s metatable", classTypeInfo->classname);
	// dump_lua_table(lua, -1,0);
	// store a reference to the metatable
	if (classTypeInfo->lua_prototype_index_table != 0)
		luaL_unref(lua, LUA_REGISTRYINDEX, classTypeInfo->lua_prototype_index_table);
	classTypeInfo->lua_prototype_index_table = luaL_ref(lua, LUA_REGISTRYINDEX);
	// sys_print(Debug, "proto index %d\n", classTypeInfo->lua_prototype_index_table);
	height = lua_gettop(lua);
	assert(height == 0);
}
void ScriptManager::set_class_type_global(ClassTypeInfo* classTypeInfo) {

	// set global
	int id = classTypeInfo->get_table_registry_id();
	lua_rawgeti(lua, LUA_REGISTRYINDEX, id);
	// set static funcs
	for (int i = 0; i < classTypeInfo->lua_function_count; i++) {
		auto& func = classTypeInfo->lua_functions[i];
		if (func.is_static) {
			lua_pushcfunction(lua, func.lua_c_function);
			lua_setfield(lua, -2, func.name);
		}
	}
	// set the global type
	lua_setglobal(lua, classTypeInfo->classname);

	int height = lua_gettop(lua);
	assert(height == 0);
}
int ScriptManager::create_class_table_for(ClassBase* type) {
	assert(type);
	assert(!type->is_class_referenced_from_lua());
	const int startheight = lua_gettop(lua);
	// assert(height == 0);
	lua_newtable(lua);
	//	lua_pushstring(lua, "__index");
	lua_rawgeti(lua, LUA_REGISTRYINDEX, type->get_type().get_prototype_index_table());
	// lua_settable(lua, -3);
	int height = lua_gettop(lua);
	assert(height == startheight + 2);
	lua_setmetatable(lua, -2);

	lua_pushstring(lua, "__ptr");
	lua_pushlightuserdata(lua, type);
	lua_settable(lua, -3);
	height = lua_gettop(lua);
	assert(height == startheight + 1);
	int out = luaL_ref(lua, LUA_REGISTRYINDEX);
	// sys_print(Debug, "obj index %d\n", out);

	height = lua_gettop(lua);
	assert(height == startheight);
	return out;
}
ClassBase* ScriptManager::allocate_class(string name) {
	auto find = MapUtil::get_opt(lua_classes, name);
	if (find) {
		return (*find)->allocate_this_type();
	}
	return nullptr;
}
void ScriptManager::set_enum_global(const std::string& name, const EnumTypeInfo* type) {
	for (int i = 0; i < type->str_count; i++) {
		auto& pair = type->strs[i];
		std::string fullName = StringUtils::to_upper(std::string(type->name) + "_" + pair.name);
		lua_pushinteger(lua, pair.value);
		lua_setglobal(lua, fullName.c_str());
	}
}
void ScriptManager::free_class_table(int id) {
	assert(id != 0);
	// sys_print(Debug, "free class table %d\n", id);

	lua_rawgeti(lua, LUA_REGISTRYINDEX, id);
	// stack_dump(lua);
	// dump_lua_table(lua, -1,0);
	lua_pushlightuserdata(lua, nullptr);
	lua_setfield(lua, -2, "__ptr");
	lua_pop(lua, 1);
	luaL_unref(lua, LUA_REGISTRYINDEX, id);
}
#include "Game/LevelAssets.h"
// Forward decls — full bodies live near LuaClassTypeInfo::init_lua_type below.
static void construct_lua_field(const PropertyInfo& pi, uint8_t* p);
static void destruct_lua_field(const PropertyInfo& pi, uint8_t* p);
static void apply_template_default(lua_State* L, int template_idx, const PropertyInfo& pi, uint8_t* p);
static void destroy_shadow_for(LuaClassTypeInfo* cti, Component* comp);

// Snapshot of one PROP_LUA_BACKED field's value, used to preserve overlapping fields
// (matched by name + core_type_id) across a Lua class reload.
struct LuaFieldSnapshot
{
	core_type_id type;
	float       f = 0;
	int32_t     i = 0;
	int8_t      b = 0;
	std::string s;
};

struct LuaInstanceSnapshot
{
	Component* inst = nullptr;
	std::unordered_map<std::string, LuaFieldSnapshot> values;
};

static LuaFieldSnapshot snapshot_field(const PropertyInfo& pi, uint8_t* p) {
	LuaFieldSnapshot v;
	v.type = pi.type;
	switch (pi.type) {
	case core_type_id::Float: v.f = *(float*)p; break;
	case core_type_id::Int32:
	case core_type_id::Enum32: v.i = *(int32_t*)p; break;
	case core_type_id::Bool: v.b = *(int8_t*)p; break;
	case core_type_id::StdString: v.s = *(std::string*)p; break;
	default: break;
	}
	return v;
}

static void restore_field(const LuaFieldSnapshot& v, const PropertyInfo& pi, uint8_t* p) {
	if (v.type != pi.type)
		return; // type changed across reload — fall through to template default
	switch (pi.type) {
	case core_type_id::Float: *(float*)p = v.f; break;
	case core_type_id::Int32:
	case core_type_id::Enum32: *(int32_t*)p = v.i; break;
	case core_type_id::Bool: *(int8_t*)p = v.b; break;
	case core_type_id::StdString: *(std::string*)p = v.s; break;
	default: break;
	}
}

void ScriptManager::check_for_reload() {
	if (!had_changes)
		return;
	lua_settop(lua, 0);

	// Find classes whose source changed this cycle (clears their per-class flag).
	std::vector<LuaClassTypeInfo*> changed;
	for (auto& [name, c] : lua_classes) {
		if (c->get_and_clear_had_changes())
			changed.push_back(c.get());
	}

	// Phase 1: snapshot every live instance's current field values against the OLD layout.
	std::unordered_map<LuaClassTypeInfo*, std::vector<LuaInstanceSnapshot>> per_class;
	for (auto* cti : changed) {
		auto& snaps = per_class[cti];
		for (auto* inst : cti->get_live_instances()) {
			LuaInstanceSnapshot snap;
			snap.inst = inst;
			uint8_t* shadow = inst->get_lua_field_shadow();
			if (shadow) {
				for (auto& pi : cti->get_lua_props_storage())
					snap.values.emplace(pi.name, snapshot_field(pi, shadow + pi.offset));
			}
			snaps.push_back(std::move(snap));
		}
	}

	// Phase 2: tear down each live instance's old shadow (runs string dtors) before
	// the class's lua_props_storage is rewritten by init_lua_type().
	for (auto& [cti, snaps] : per_class) {
		for (auto& snap : snaps)
			destroy_shadow_for(cti, snap.inst);
	}

	// Phase 2.5: commit any new parsed_properties staged by reload_from_content.
	// Must happen AFTER snapshot phase 1 (which read pi.name pointers into the old
	// parsed_properties storage) and BEFORE synthesize re-reads from parsed_properties.
	for (auto* cti : changed) {
		if (!cti->pending_parsed_properties.empty() || !cti->parsed_properties.empty())
			cti->parsed_properties = std::move(cti->pending_parsed_properties);
	}

	// Phase 3: rebuild type metadata (metatables + synthesized lua_props_storage).
	for (auto* cti : changed)
		cti->init_lua_type();

	// Phase 4: reallocate each live instance's shadow against the NEW layout,
	// seed with template defaults, then overlay snapshot values where (name,type) match.
	for (auto& [cti, snaps] : per_class) {
		if (cti->get_lua_field_shadow_size() == 0) {
			// New class has no Lua-backed fields; live instances simply keep null shadow.
			continue;
		}
		lua_rawgeti(lua, LUA_REGISTRYINDEX, cti->get_template_lua_table());
		int tmpl_idx = lua_gettop(lua);
		for (auto& snap : snaps) {
			auto buf = std::make_unique<uint8_t[]>(cti->get_lua_field_shadow_size());
			std::memset(buf.get(), 0, cti->get_lua_field_shadow_size());
			for (auto& pi : cti->get_lua_props_storage()) {
				uint8_t* p = buf.get() + pi.offset;
				construct_lua_field(pi, p);
				apply_template_default(lua, tmpl_idx, pi, p);
				auto it = snap.values.find(pi.name);
				if (it != snap.values.end())
					restore_field(it->second, pi, p);
			}
			snap.inst->take_lua_field_shadow(std::move(buf));
		}
		lua_pop(lua, 1);
	}

	ClassBase::post_changes_class_init();
	had_changes = false;

	if (!changed.empty())
		on_class_reloaded.invoke();
}
#include <iostream>
void printStackTrace(lua_State* L) {
	const char* traceback = lua_tostring(L, -1);
	if (traceback)
		std::cout << traceback << std::endl;
	lua_pop(L, 1); // remove traceback string
}
static int traceback(lua_State* L) {
	const char* msg = lua_tostring(L, 1);
	if (!msg)
		msg = "(no error message)";

	// Generate a full traceback using luaL_traceback
	luaL_traceback(L, L, msg, 1); // 1 = skip this function
	return 1;					  // leave traceback string on top of stack
}
int safe_pcall(lua_State* L, int nargs, int nresults) {
	int top = lua_gettop(L);

	// Validate stack layout
	if (top < nargs + 1) {
		// not enough elements: function + args
		return LUA_ERRRUN;
	}

	int base = top - nargs;

	luaL_checkstack(L, 1, "not enough stack space");

	lua_pushcfunction(L, traceback);
	lua_insert(L, base);

	int status = lua_pcall(L, nargs, nresults, base);

	// Ensure base is still valid before removing
	if (lua_gettop(L) >= base) {
		lua_remove(L, base);
	}

	return status;
}
void ScriptManager::reload_one_file(const std::string& strFilePath) {
	auto file = FileSys::open_read_game(strFilePath);
	if (file) {
		string out(file->size(), ' ');
		file->read(out.data(), out.size());
		reload_from_content(out, strFilePath);
	}
}

void ScriptManager::reload_from_content(const std::string& source, const std::string& chunkname) {
	std::vector<uptr<LuaClassTypeInfo>> newClasses;
	auto outTypes = ScriptLoadingUtil::parse_text(source);
	for (auto& t : outTypes) {
		if (t.inherited.size() > 0) {
			auto info = std::make_unique<LuaClassTypeInfo>();
			info->set_classname(t.name);
			bool b = info->set_superclass(t.inherited.at(0));
			if (b)
				newClasses.push_back(std::move(info));
		}
	}
	had_changes = true;

	if (luaL_loadbuffer(lua, source.c_str(), source.size(), chunkname.c_str()) != LUA_OK) {
		sys_print(Error, "ScriptManager: error loading script %s: %s\n", chunkname.c_str(), lua_tostring(lua, -1));
		lua_pop(lua, 1);
		lua_settop(lua, 0);
		return;
	}
	if (safe_pcall(lua, 0, LUA_MULTRET) != LUA_OK) {
		printStackTrace(lua);
		return;
	}
	for (auto& c : newClasses) {
		if (!MapUtil::contains(lua_classes, c->get_name())) {
			ClassBase::register_class(c.get());
			c->set_had_changes();
			lua_classes.insert({c->get_name(), std::move(c)});
		} else {
			lua_classes[c->get_name()]->set_had_changes();
		}
	}
	// Capture parsed property metadata for every (re)parsed class so init_lua_type()
	// can synthesize PROP_LUA_BACKED reflection for Component subclasses, picking up
	// any added/removed/renamed fields on reload.
	for (auto& t : outTypes) {
		auto found = MapUtil::get_opt(lua_classes, t.name);
		if (found) {
			(*found)->set_parsed_properties(std::move(t.props));
		}
	}
}

void ScriptManager::reload_all_scripts() {
	sys_print(Info, "ScriptManager::reload_all_scripts\n");
	std::vector<string> files;
	for (auto& file : FileSys::find_game_files_path("scripts")) {
		if (file.find("lua_stubs.lua") != string::npos)
			continue;
		// lib/ holds require-style modules (mobdebug, socket, etc.) — not auto-executed
		if (file.find("/lib/") != string::npos || file.find("\\lib\\") != string::npos)
			continue;
		// tests/ is loaded explicitly via load_test_scripts() in test mode only,
		// so add_test() calls don't fire in normal app runs
		if (file.find("/tests/") != string::npos || file.find("\\tests\\") != string::npos)
			continue;
		if (StringUtils::get_extension_no_dot(file) == "lua") {
			sys_print(Debug, "ScriptManager::load_script_files: found lua file %s\n", file.c_str());
			files.push_back(FileSys::get_game_path_from_full_path(file));
		}
	}
	for (auto& strFilePath : files) {
		reload_one_file(strFilePath);
	}
	had_changes = true;
	check_for_reload();
}

void ScriptManager::load_test_scripts() {
	sys_print(Info, "ScriptManager::load_test_scripts\n");
	std::vector<string> files;
	for (auto& file : FileSys::find_game_files_path("scripts/tests")) {
		if (StringUtils::get_extension_no_dot(file) == "lua") {
			sys_print(Debug, "ScriptManager::load_test_scripts: found lua file %s\n", file.c_str());
			files.push_back(FileSys::get_game_path_from_full_path(file));
		}
	}
	for (auto& strFilePath : files) {
		reload_one_file(strFilePath);
	}
	check_for_reload();
}
ScriptManager* ScriptManager::inst = nullptr;

static void copy_table(lua_State* L, int index) {
	// Ensure the original table is at the correct stack index
	if (!lua_istable(L, index)) {
		luaL_error(L, "Expected a table at index %d", index);
		return;
	}
	// Normalize index in case it's negative
	if (index < 0)
		index = lua_gettop(L) + index + 1;

	lua_newtable(L); // Create destination table (on top of stack)
	lua_pushnil(L);	 // First key for lua_next
	while (lua_next(L, index) != 0) {
		// Stack: key at -2, value at -1
		lua_pushvalue(L, -2); // Copy key
		lua_pushvalue(L, -2); // Copy value
		lua_settable(L, -5);  // Set in new table (at -5)
		lua_pop(L, 1);		  // Remove original value, keep key
	}

	// Resulting copy is now on top of the stack
}
static void print_table_keys(lua_State* L, int index) {
	// Normalize index in case it's negative
	if (index < 0)
		index = lua_gettop(L) + index + 1;

	lua_pushnil(L); // First key
	while (lua_next(L, index) != 0) {
		// Key is at -2, value is at -1
		lua_pushvalue(L, -2); // Copy key to top
		const char* key_str = lua_tostring(L, -1);
		if (key_str)
			printf("Key: %s\n", key_str);
		else if (lua_isnumber(L, -1))
			printf("Key: %g\n", lua_tonumber(L, -1));
		else
			printf("Key: [non-string key]\n");

		lua_pop(L, 2); // Remove value and copied key
	}
}
#include "Framework/MapUtil.h"

void ScriptManager::load_script_files() {
	ClassBase::init_class_info_for_script();
	sys_print(Debug, "ScriptManager::load_script_files\n");

	// Extend package.cpath so LuaSocket's C DLLs can be found.
	// Set g_lua_cpath_extra to your vcpkg bin dir, e.g.:
	//   C:/Users/you/source/vcpkg/installed/x64-windows/bin
	const char* extra = g_lua_cpath_extra.get_string();
	if (extra && extra[0]) {
		lua_getglobal(lua, "package");
		lua_getfield(lua, -1, "cpath");
		std::string new_cpath = std::string(extra) + "/?.dll;" + lua_tostring(lua, -1);
		lua_pop(lua, 1);
		lua_pushstring(lua, new_cpath.c_str());
		lua_setfield(lua, -2, "cpath");
		lua_pop(lua, 1);
	}

	reload_all_scripts();

	if (g_lua_debug.get_bool())
		activate_debugger(g_lua_debug_host.get_string(), g_lua_debug_port.get_integer());
}

void ScriptManager::activate_debugger(const char* host, int port) {
	sys_print(Info, "ScriptManager: starting MobDebug, connecting to %s:%d\n", host, port);
	std::string code = std::string("require('mobdebug').start('") + host + "'," + std::to_string(port) + ")";
	if (luaL_dostring(lua, code.c_str()) != LUA_OK) {
		sys_print(Error, "ScriptManager: MobDebug start failed: %s\n", lua_tostring(lua, -1));
		lua_pop(lua, 1);
	}
}

LuaClassTypeInfo::LuaClassTypeInfo()
	: ClassTypeInfo("lua_class_empty", nullptr, nullptr, nullptr, false, nullptr, 0, nullptr, true) {
	this->is_lua_implemented = true;
}

LuaClassTypeInfo::~LuaClassTypeInfo() {}

inline void LuaClassTypeInfo::set_classname(string s) {
	this->lua_classname = s;
	this->classname = this->lua_classname.c_str();
}

inline bool LuaClassTypeInfo::set_superclass(string s) {
	auto find = ClassBase::find_class(s.c_str());
	if (!find) {
		sys_print(Error, "LuaClassTypeInfo: no super type %s\n", s.c_str());
		return false;
	} else if (!find->scriptable_allocate) {
		sys_print(Error, "LuaClassTypeInfo: super type isnt scriptable %s\n", s.c_str());
		return false;
	} else {
		this->super_typeinfo = find;
		this->superclassname = find->classname;
		this->lua_prototype_index_table = find->get_prototype_index_table();
		this->allocate = lua_class_alloc;
		return true;
	}
}

inline const string& LuaClassTypeInfo::get_name() {
	return lua_classname;
}

extern void stack_dump(lua_State* L);

static void dump_lua_table(lua_State* L, int index, int depth = 0) {
	if (!lua_istable(L, index)) {
		printf("%*s(Not a table)\n", depth * 2, "");
		return;
	}

	lua_pushnil(L); // first key
	while (lua_next(L, index < 0 ? index - 1 : index)) {
		// key at -2, value at -1

		// Print indentation
		printf("%*s", depth * 2, "");

		// Print key
		if (lua_type(L, -2) == LUA_TSTRING) {
			printf("[\"%s\"] = ", lua_tostring(L, -2));
		} else if (lua_type(L, -2) == LUA_TNUMBER) {
			printf("[%g] = ", lua_tonumber(L, -2));
		} else {
			printf("[%s] = ", luaL_typename(L, -2));
		}

		// Print value
		int vtype = lua_type(L, -1);
		switch (vtype) {
		case LUA_TNIL:
			printf("nil\n");
			break;
		case LUA_TBOOLEAN:
			printf(lua_toboolean(L, -1) ? "true\n" : "false\n");
			break;
		case LUA_TNUMBER:
			printf("%g\n", lua_tonumber(L, -1));
			break;
		case LUA_TSTRING:
			printf("\"%s\"\n", lua_tostring(L, -1));
			break;
		case LUA_TTABLE:
			printf("{\n");
			dump_lua_table(L, -1, depth + 1);
			printf("%*s}\n", depth * 2, "");
			break;
		default:
			printf("(%s)\n", luaL_typename(L, -1));
			break;
		}

		lua_pop(L, 1); // pop value, keep key for next lua_next
	}
}
// Maps a Lua ---@type annotation string to a (core_type_id, optional EnumTypeInfo*) pair.
// Returns true on a recognized scalar/enum. Unrecognized types (vec3, tables, asset refs, etc.)
// are deferred to a later feature pass.
static bool lua_type_str_to_core_type(const string& type_str, core_type_id& out_type,
									  const EnumTypeInfo*& out_enum) {
	out_enum = nullptr;
	if (type_str == "number" || type_str == "float") {
		out_type = core_type_id::Float;
		return true;
	}
	if (type_str == "integer" || type_str == "int") {
		out_type = core_type_id::Int32;
		return true;
	}
	if (type_str == "boolean" || type_str == "bool") {
		out_type = core_type_id::Bool;
		return true;
	}
	if (type_str == "string") {
		out_type = core_type_id::StdString;
		return true;
	}
	if (auto e = EnumRegistry::find_enum_type(type_str)) {
		out_type = core_type_id::Enum32;
		out_enum = e;
		return true;
	}
	return false;
}

// Returns the byte size required to store a value of the given core_type_id in the
// per-instance shadow buffer. std::string is constructed/destructed via placement new
// when the buffer is (re)allocated.
static uint32_t lua_backed_size_for_type(core_type_id t) {
	switch (t) {
	case core_type_id::Bool: return 1;
	case core_type_id::Int32:
	case core_type_id::Enum32:
	case core_type_id::Float: return 4;
	case core_type_id::StdString: return sizeof(std::string);
	default: ASSERT(0); return 0;
	}
}

// Returns 4-byte alignment for primitives, alignof(std::string) for strings.
static uint32_t lua_backed_align_for_type(core_type_id t) {
	if (t == core_type_id::StdString) return alignof(std::string);
	if (t == core_type_id::Bool) return 1;
	return 4;
}

static uint32_t align_up(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

static void synthesize_layout_from_parsed(LuaClassTypeInfo* self,
										  vector<PropertyInfo>& storage,
										  PropertyInfoList& list_out,
										  uint32_t& shadow_size_out,
										  const string& classname,
										  const vector<ParseProperty>& parsed_properties);

void LuaClassTypeInfo::synthesize_lua_props_unchecked_for_test() {
	// Mirror what ScriptManager::check_for_reload does: commit any pending parsed
	// properties staged by set_parsed_properties() before synthesis reads them.
	if (!pending_parsed_properties.empty() || !parsed_properties.empty())
		parsed_properties = std::move(pending_parsed_properties);
	synthesize_layout_from_parsed(this, lua_props_storage, lua_props_list,
								  lua_field_shadow_size, lua_classname, parsed_properties);
}

void LuaClassTypeInfo::synthesize_lua_props_for_component_subclass() {
	lua_props_storage.clear();
	lua_props_list = {};
	lua_field_shadow_size = 0;

	// Only synthesize for classes whose super chain reaches Component. We walk the
	// chain by pointer rather than calling ClassTypeInfo::is_a() because the latter
	// compares id ranges set by ClassBase::post_changes_class_init() — which on the
	// reload path runs AFTER init_lua_type(), so ids are still zeroed here.
	auto component_ti = ClassBase::find_class("Component");
	if (!component_ti)
		return;
	const ClassTypeInfo* p = this->super_typeinfo;
	while (p && p != component_ti)
		p = p->super_typeinfo;
	if (!p)
		return;
	synthesize_layout_from_parsed(this, lua_props_storage, lua_props_list,
								  lua_field_shadow_size, lua_classname, parsed_properties);
}

static void synthesize_layout_from_parsed(LuaClassTypeInfo* /*self*/,
										  vector<PropertyInfo>& lua_props_storage,
										  PropertyInfoList& lua_props_list,
										  uint32_t& lua_field_shadow_size,
										  const string& lua_classname,
										  const vector<ParseProperty>& parsed_properties) {
	lua_props_storage.clear();
	lua_props_list = {};
	lua_field_shadow_size = 0;
	uint32_t cursor = 0;
	for (auto& parsed : parsed_properties) {
		core_type_id ctype;
		const EnumTypeInfo* einfo = nullptr;
		if (!lua_type_str_to_core_type(parsed.type_str, ctype, einfo)) {
			sys_print(Warning, "LuaClassTypeInfo[%s]: skipping field '%s' with unsupported ---@type '%s'\n",
					  lua_classname.c_str(), parsed.name.c_str(), parsed.type_str.c_str());
			continue;
		}
		const uint32_t align = lua_backed_align_for_type(ctype);
		const uint32_t size  = lua_backed_size_for_type(ctype);
		cursor = align_up(cursor, align);

		PropertyInfo pi;
		// PropertyInfo holds a raw `const char*` to the name; the parsed_properties vector
		// owns the storage for the string for as long as this class exists.
		pi.name = parsed.name.c_str();
		pi.offset = (uint16_t)cursor;
		pi.flags = PROP_DEFAULT | PROP_LUA_BACKED;
		pi.type = ctype;
		pi.enum_type = einfo;
		lua_props_storage.push_back(pi);
		cursor += size;
	}
	if (!lua_props_storage.empty()) {
		lua_props_list.list = lua_props_storage.data();
		lua_props_list.count = (int)lua_props_storage.size();
		lua_props_list.type_name = lua_classname.c_str();
		lua_field_shadow_size = cursor;
	}
}

// Placement-construct one PROP_LUA_BACKED field within a freshly-allocated shadow buffer.
// Primitives are left zero-initialized by the caller's memset; only non-POD types need work.
static void construct_lua_field(const PropertyInfo& pi, uint8_t* p) {
	if (pi.type == core_type_id::StdString)
		new (p) std::string();
}

// Inverse of construct_lua_field. Called before freeing the shadow buffer or
// reallocating it for a new layout.
static void destruct_lua_field(const PropertyInfo& pi, uint8_t* p) {
	if (pi.type == core_type_id::StdString)
		((std::string*)p)->~basic_string();
}

// Reads a default value for `pi` from the class's template Lua table (top of stack at
// `template_idx`) and writes it into `p` (already constructed). Missing/wrong-type
// template entries leave `p` at its constructed default.
static void apply_template_default(lua_State* L, int template_idx, const PropertyInfo& pi, uint8_t* p) {
	lua_getfield(L, template_idx, pi.name);
	if (!lua_isnil(L, -1)) {
		switch (pi.type) {
		case core_type_id::Float:
			*(float*)p = (float)lua_tonumber(L, -1);
			break;
		case core_type_id::Int32:
		case core_type_id::Enum32:
			*(int32_t*)p = (int32_t)lua_tointeger(L, -1);
			break;
		case core_type_id::Bool:
			*(int8_t*)p = lua_toboolean(L, -1) ? 1 : 0;
			break;
		case core_type_id::StdString:
			if (lua_isstring(L, -1))
				*(std::string*)p = lua_tostring(L, -1);
			break;
		default:
			break;
		}
	}
	lua_pop(L, 1);
}

// Walks every field in the class's lua_props_storage, runs destructors, returns the buffer to nullptr.
static void destroy_shadow_for(LuaClassTypeInfo* cti, Component* comp) {
	uint8_t* shadow = comp->get_lua_field_shadow();
	if (!shadow)
		return;
	for (auto& pi : cti->get_lua_props_storage())
		destruct_lua_field(pi, shadow + pi.offset);
	comp->take_lua_field_shadow(nullptr);
}

// Allocates + initializes a shadow buffer for `cti`, pulling defaults from the
// already-loaded template_lua_table. Caller must hand the result to the Component.
static std::unique_ptr<uint8_t[]> allocate_and_init_shadow(lua_State* L, LuaClassTypeInfo* cti) {
	if (cti->get_lua_field_shadow_size() == 0)
		return nullptr;
	auto buf = std::make_unique<uint8_t[]>(cti->get_lua_field_shadow_size());
	std::memset(buf.get(), 0, cti->get_lua_field_shadow_size());
	lua_rawgeti(L, LUA_REGISTRYINDEX, cti->get_template_lua_table());
	int tmpl_idx = lua_gettop(L);
	for (auto& pi : cti->get_lua_props_storage()) {
		uint8_t* p = buf.get() + pi.offset;
		construct_lua_field(pi, p);
		apply_template_default(L, tmpl_idx, pi, p);
	}
	lua_pop(L, 1);
	return buf;
}

void LuaClassTypeInfo::init_lua_type() {
	// assert(template_lua_table == 0);
	auto L = ScriptManager::inst->get_lua_state();
	assert(lua_gettop(L) == 0);
	lua_getglobal(L, lua_classname.c_str());
	assert(lua_gettop(L) == 1);
	if (lua_isnil(L, -1)) {
		// class not found
		sys_print(Warning, "LuaClassTypeInfo::init_lua_type: class not found %s\n", lua_classname.c_str());
	} else {
		assert(lua_gettop(L) == 1);
		// sys_print(Debug, "lua actual table %s\n", lua_classname.c_str());
		// dump_lua_table(L, -1);

		if (template_lua_table != 0)
			luaL_unref(L, LUA_REGISTRYINDEX, template_lua_table);
		assert(lua_gettop(L) == 1);
		template_lua_table = luaL_ref(L, LUA_REGISTRYINDEX);
		// sys_print(Debug, "template index %d\n", template_lua_table);

		assert(lua_gettop(L) == 0);
		lua_pushnil(L);
		lua_setglobal(L, lua_classname.c_str());
		assert(lua_gettop(L) == 0);
		free_table_registry_id(); // free it if it exists
		assert(lua_gettop(L) == 0);
		ScriptManager::inst->init_this_class_type(this);
		assert(lua_gettop(L) == 0);
		ScriptManager::inst->set_class_type_global(this);
	}
	// Synthesize PROP_LUA_BACKED reflection from the most recent parse. Must happen AFTER
	// the metatable is rebuilt, BEFORE live-instance shadow buffers are reallocated by
	// the reload-merge path in check_for_reload().
	synthesize_lua_props_for_component_subclass();
	this->props = get_lua_props_list();
}

ClassBase* LuaClassTypeInfo::lua_class_alloc(const ClassTypeInfo* c) {
	assert(c->super_typeinfo && c->super_typeinfo->scriptable_allocate);
	auto out = c->super_typeinfo->scriptable_allocate(c);
	LuaClassTypeInfo* luaInfo = (LuaClassTypeInfo*)(c);
	auto L = ScriptManager::inst->get_lua_state();

	const int startTop = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, out->get_table_registry_id()); // -3
	lua_rawgeti(L, LUA_REGISTRYINDEX, luaInfo->template_lua_table);	 // -2
	// stack_dump(L);

	auto check_top = [&](int v) {
		int topNow = lua_gettop(L);
		assert(topNow == startTop + v);
	};
	check_top(2);

	// Copy table1 into dst
	lua_pushnil(L); // first key
	check_top(3);

	while (lua_next(L, -2)) {

		check_top(4);

		lua_pushvalue(L, -2); // duplicate key
		lua_insert(L, -2);	  // move key below value
		// stack_dump(L);
		lua_settable(L, -5); // dst[key] = value
							 // stack_dump(L);
							 // leaves key for next lua_next
	}
	check_top(2);
	// stack_dump(L);
	// sys_print(Debug, "template table");
	// dump_lua_table(L, -1);
	check_top(2);
	// sys_print(Debug, "output table");
	// dump_lua_table(L, -2);

	lua_pop(L, 2);
	check_top(0);

	// For Component subclasses with PROP_LUA_BACKED fields, allocate the shadow
	// buffer and pull initial values from the template Lua table. The instance Lua
	// table still holds the same defaults (copied above) for script-side reads;
	// editor edits and serialization read/write only the shadow buffer.
	if (luaInfo->lua_field_shadow_size > 0) {
		if (Component* comp = out->cast_to<Component>()) {
			comp->take_lua_field_shadow(allocate_and_init_shadow(L, luaInfo));
			comp->set_lua_owner_type(luaInfo);
			luaInfo->register_lua_instance(comp);
		}
	}
	return out;
}

void ScriptManager::on_component_destructed(Component* c) {
	if (!ScriptManager::inst || !c)
		return;
	// Don't rely on c->get_type() here: by the time ~Component runs, the most-derived
	// (scriptable) destructor has already executed and the vtable has been downgraded
	// to Component's, so get_type() returns Component::StaticType. Use the cached
	// lua_owner_type pointer captured by lua_class_alloc instead.
	auto* lti = const_cast<LuaClassTypeInfo*>(c->get_lua_owner_type());
	if (!lti)
		return;
	destroy_shadow_for(lti, c);
	lti->unregister_lua_instance(c);
}
static void stack_dump(lua_State* L) {
	int top = lua_gettop(L); // Get the index of the top element

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

void ScriptManager::update() {
#ifdef EDITOR_BUILD
	check_for_reload();
#endif
	lua_settop(lua, 0); // avoid stack overflow for weird stuff
}