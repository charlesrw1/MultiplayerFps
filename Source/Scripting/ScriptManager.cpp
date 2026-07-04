#include "ScriptManager.h"
#include "Framework/StringUtils.h"
#include "Framework/Files.h"
#include "Framework/MapUtil.h"
#include "Framework/Config.h"
#include "LuaTransform.h"
#include "LuaVecQuat.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// @docs [[scripting/vscode_debugger]]
// Whether/when to start lua-debug is a launch-time choice, not persistent state,
// so it's the --lua_debug / --lua_debug_wait CLI flags (see EngineMain_Init.cpp),
// not a ConfigVar.
// Must be a literal IPv4 (lua-debug's socket.lua only parses "%d+.%d+.%d+.%d+:%d+"; "localhost" fails to parse).
static ConfigVar g_lua_debug_host("g_lua_debug_host", "127.0.0.1", 0, "lua-debug listen host");
static ConfigVar g_lua_debug_port("g_lua_debug_port", "9966", CVAR_INTEGER | CVAR_UNBOUNDED,
								  "lua-debug listen port (VS Code attach)");
// Extra directory to add to Lua package.cpath for loading native Lua C modules
// (socket/core.dll, mime/core.dll).
// Set to a dir containing those dlls, e.g.: C:/Users/you/source/vcpkg/installed/x64-windows/bin
static ConfigVar g_lua_cpath_extra("g_lua_cpath_extra", "", 0,
								   "Extra dir appended to Lua package.cpath for native Lua C modules");

ScriptManager::ScriptManager() {
	lua = luaL_newstate();
	luaL_openlibs(lua);

	// Add Data/scripts/lib/ to package.path so require("socket"), etc. work.
	// Files in lib/ are module-style (not auto-executed by reload_all_scripts).
	lua_getglobal(lua, "package");
	lua_getfield(lua, -1, "path");
	std::string new_path = std::string(lua_tostring(lua, -1)) + ";Data/scripts/lib/?.lua";
	lua_pop(lua, 1);
	lua_pushstring(lua, new_path.c_str());
	lua_setfield(lua, -2, "path");
	lua_pop(lua, 1);

	register_lua_transform(lua);
	register_lua_vec_quat(lua);
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

	height = lua_gettop(lua);
	assert(height == 2);
	lua_setfield(lua, -2, "__index");
	assert(lua_gettop(lua) == 1);
	// store a reference to the metatable
	if (classTypeInfo->lua_prototype_index_table != 0)
		luaL_unref(lua, LUA_REGISTRYINDEX, classTypeInfo->lua_prototype_index_table);
	classTypeInfo->lua_prototype_index_table = luaL_ref(lua, LUA_REGISTRYINDEX);
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
	lua_newtable(lua);
	lua_rawgeti(lua, LUA_REGISTRYINDEX, type->get_type().get_prototype_index_table());
	int height = lua_gettop(lua);
	assert(height == startheight + 2);
	lua_setmetatable(lua, -2);

	lua_pushstring(lua, "__ptr");
	lua_pushlightuserdata(lua, type);
	lua_settable(lua, -3);
	height = lua_gettop(lua);
	assert(height == startheight + 1);
	int out = luaL_ref(lua, LUA_REGISTRYINDEX);

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
	lua_rawgeti(lua, LUA_REGISTRYINDEX, id);
	lua_pushlightuserdata(lua, nullptr);
	lua_setfield(lua, -2, "__ptr");
	lua_pop(lua, 1);
	luaL_unref(lua, LUA_REGISTRYINDEX, id);
}

static void printStackTrace(lua_State* L) {
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
		// Chunkname is the full path (via g_project_base) rather than the
		// game-relative path, so lua-debug can resolve breakpoints to the real
		// file on disk without depending on VS Code's workspace root/cwd.
		reload_from_content(out, FileSys::get_full_path_from_game_path(strFilePath));
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
			if (b) {
				// inherited[1..] may be interface names
				for (size_t i = 1; i < t.inherited.size(); i++) {
					info->pending_interfaces.push_back(t.inherited[i]);
				}
				newClasses.push_back(std::move(info));
			}
		}
	}
	had_changes = true;

	// "@"-prefixed chunkname marks this as a real file to Lua (and to lua-debug's
	// source.lua, which requires the '@' prefix to treat a source as a debuggable
	// file instead of an anonymous in-memory chunk).
	const std::string lua_chunkname = "@" + chunkname;
	if (luaL_loadbuffer(lua, source.c_str(), source.size(), lua_chunkname.c_str()) != LUA_OK) {
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
			(*found)->set_editor_placeable(t.editor_placeable);
		}
	}
}

void ScriptManager::reload_all_scripts() {
	sys_print(Info, "ScriptManager::reload_all_scripts\n");
	std::vector<string> files;
	for (auto& file : FileSys::find_game_files_path("scripts")) {
		if (file.find("lua_stubs.lua") != string::npos)
			continue;
		// lib/ holds require-style modules (socket, etc.) — not auto-executed
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

void ScriptManager::load_script_files(bool start_debugger, bool debugger_wait) {
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

	if (start_debugger)
		activate_debugger(g_lua_debug_host.get_string(), g_lua_debug_port.get_integer(), debugger_wait);
}

void ScriptManager::activate_debugger(const char* host, int port, bool wait) {
	sys_print(Info, "ScriptManager: starting lua-debug on %s:%d (wait=%d)\n", host, port, wait ? 1 : 0);
	std::string code = "local ok, dbg = pcall(require, 'debugger')\n";
	code += "if not ok then error('lua-debug shim not found; ensure actboy168.lua-debug extension is installed and Data/scripts/lib/debugger.lua is present: ' .. tostring(dbg)) end\n";
	code += "dbg:start('" + std::string(host) + ":" + std::to_string(port) + "')\n";
	if (wait)
		code += "dbg:event('wait')\n";
	if (luaL_dostring(lua, code.c_str()) != LUA_OK) {
		sys_print(Error, "ScriptManager: lua-debug start failed: %s\n", lua_tostring(lua, -1));
		lua_pop(lua, 1);
	}
}

void ScriptManager::update() {
#ifdef EDITOR_BUILD
	check_for_reload();
#endif
	lua_settop(lua, 0); // avoid stack overflow for weird stuff
}
