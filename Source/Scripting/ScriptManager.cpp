#include "ScriptManager.h"
#include "Framework/StringUtils.h"
#include "Framework/Files.h"

#include <cassert>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
void dump_lua_table(lua_State* L, int index, int depth);
vector<ParseType> ScriptLoadingUtil::parse_text(string text)
{
	auto lines = StringUtils::to_lines(text);

	struct PendingClass {
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
		StringUtils::replace(line, "---", "--- ");
		StringUtils::replace(line, ":", " : ");
		StringUtils::replace(line, "=", " = ");
		StringUtils::replace(line, "{", " { ");
		StringUtils::replace(line, "}", " } ");
		StringUtils::replace(line, ",", " , ");

		auto tokens = StringUtils::split(line);
		if (tokens.empty())
			continue;

		// Parse class definition
		if (tokens.at(0) == "---" && tokens.size() > 2 && tokens.at(1) == "@class") {

			//printf("found class: %s\n", tokens.at(2).c_str());

			if (inClass && !currentClass.name.empty()) {
				// Save previous class
				out.push_back({currentClass.name, currentClass.inherited, currentClass.properties});
				currentClass = PendingClass();
			}
			currentClass.name = tokens.at(2);
			currentClass.inherited.clear();
			currentClass.properties.clear();
			inClass = false;
			//printf("inclass=false %d\n", i + 1);
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
		else if (tokens.size() >= 3 && tokens.at(1) == "=" && tokens.at(2) == "{" 
			&& StringUtils::starts_with(lines.at(i), tokens.at(0))) {	// test for no leading whitespace

			//printf("start class: %d\n",i+1);


			if (!currentClass.name.empty()) {
				inClass = true;
				//printf("inclass=true %d\n", i + 1);

			}
		}
		// Parse property type annotation
		else if (tokens.size() >= 3 && tokens.at(0) == "---" && tokens.at(1) == "@type") {

			//printf("found property type %s %d\n",tokens.at(2).c_str(), i + 1);


			pendingType = tokens.at(2);
		}
		// Parse property assignment
		else if (inClass && tokens.size() >= 3 && (tokens.at(1) == "="||tokens.at(1)==",")) {

			//printf("found property name %s %d\n", tokens.at(0).c_str(), i + 1);


			ParseProperty prop;
			prop.name = tokens.at(0);
			prop.type_str = pendingType;
			currentClass.properties.push_back(prop);
			pendingType.clear();
		}
		// End of class table
		else if (inClass && StringUtils::starts_with(lines.at(i),"}")) {

			//printf("end class %d\n", i + 1);


			if (!currentClass.name.empty()) {
				out.push_back({currentClass.name, currentClass.inherited, currentClass.properties});
				currentClass = PendingClass();
			}
			inClass = false;
			//printf("inclass=false %d\n", i + 1);
			pendingType.clear();
		}
	}
	// Handle last class if file doesn't end with }
	if (inClass && !currentClass.name.empty()) {
		out.push_back({currentClass.name, currentClass.inherited, currentClass.properties});
	}

	return out;
}

unordered_map<string, ScriptTypeInfo> ScriptLoadingUtil::load_types(const std::vector<string>& files)
{
	return {};
}

vector<string> ScriptLoadingUtil::collect_script_files(string root)
{
	vector<string> out;
	auto tree = FileSys::find_files(root.c_str());
	for (auto& file : tree) {
		if (StringUtils::get_extension_no_dot(file) == "lua")
			out.push_back(file);
	}
	return out;
}

ScriptManager::ScriptManager()
{
	lua = luaL_newstate();
	luaL_openlibs(lua);
}

ScriptManager::~ScriptManager()
{
	lua_close(lua);
	lua = nullptr;
}
void ScriptManager::init_this_class_type(ClassTypeInfo* classTypeInfo)
{
	int height = lua_gettop(lua);
	assert(height == 0);
	lua_newtable(lua);	// the output metatable
	lua_newtable(lua);  // new __index table
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
	//sys_print(Debug, "%s __index", classTypeInfo->classname);
	//dump_lua_table(lua, -1,0);

	height = lua_gettop(lua);
	assert(height==2);
	lua_setfield(lua, -2, "__index");
	assert(lua_gettop(lua) == 1);
	//sys_print(Debug, "%s metatable", classTypeInfo->classname);
	//dump_lua_table(lua, -1,0);
	// store a reference to the metatable
	classTypeInfo->lua_prototype_index_table = luaL_ref(lua, LUA_REGISTRYINDEX);
	height = lua_gettop(lua);
	assert(height == 0);

}
void ScriptManager::set_class_type_global(ClassTypeInfo* classTypeInfo)
{

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
int ScriptManager::create_class_table_for(ClassBase* type)
{
	assert(type);
	assert(!type->is_class_referenced_from_lua());
	const int startheight = lua_gettop(lua);
	//assert(height == 0);
	lua_newtable(lua);
//	lua_pushstring(lua, "__index"); 
	lua_rawgeti(lua, LUA_REGISTRYINDEX, type->get_type().get_prototype_index_table());
	//lua_settable(lua, -3);
	int height = lua_gettop(lua);
	assert(height == startheight + 2);
	lua_setmetatable(lua, -2);


	lua_pushstring(lua, "__ptr");
	lua_pushlightuserdata(lua, type);
	lua_settable(lua, -3);
	height = lua_gettop(lua);
	assert(height == startheight +1);
	int out = luaL_ref(lua, LUA_REGISTRYINDEX);
	height = lua_gettop(lua);
	assert(height == startheight);
	return out;
}
ClassBase* ScriptManager::allocate_class(string name)
{
	for (auto& t : lua_classes) {
		if (t && t->get_classname() == name) {
			return t->allocate_this_type();//
		}
	}
	return nullptr;
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

	lua_newtable(L);  // Create destination table (on top of stack)
	lua_pushnil(L);   // First key for lua_next
	while (lua_next(L, index) != 0) {
		// Stack: key at -2, value at -1
		lua_pushvalue(L, -2);  // Copy key
		lua_pushvalue(L, -2);  // Copy value
		lua_settable(L, -5);   // Set in new table (at -5)
		lua_pop(L, 1);         // Remove original value, keep key
	}

	// Resulting copy is now on top of the stack
}
static void print_table_keys(lua_State* L, int index) {
	// Normalize index in case it's negative
	if (index < 0) index = lua_gettop(L) + index + 1;

	lua_pushnil(L);  // First key
	while (lua_next(L, index) != 0) {
		// Key is at -2, value is at -1
		lua_pushvalue(L, -2);  // Copy key to top
		const char* key_str = lua_tostring(L, -1);
		if (key_str)
			printf("Key: %s\n", key_str);
		else if (lua_isnumber(L, -1))
			printf("Key: %g\n", lua_tonumber(L, -1));
		else
			printf("Key: [non-string key]\n");

		lua_pop(L, 2);  // Remove value and copied key
	}
}


void ScriptManager::load_script_files()
{
	ClassBase::init_class_info_for_script();

	sys_print(Debug, "ScriptManager::load_script_files\n");
	std::vector<string> files;
	for (auto& file : FileSys::find_game_files_path("scripts/")) {
		if (StringUtils::get_extension_no_dot(file) == "lua") {
			sys_print(Debug, "ScriptManager::load_script_files: found lua file %s\n",file.c_str());
			files.push_back(FileSys::get_game_path_from_full_path(file));
		}
	}
	std::string fullOutput;
	for (auto& strFilePath : files) {
		auto file = FileSys::open_read_game(strFilePath);
		if (file) {
			string out(file->size(), ' ');
			file->read(out.data(), out.size());
			auto outTypes = ScriptLoadingUtil::parse_text(out);
			fullOutput += out;
			for (auto& t : outTypes) {
				auto info = std::make_unique<LuaClassTypeInfo>();
				info->set_classname(t.name);
				info->set_superclass(t.inherited.at(0));
				lua_classes.push_back(std::move(info));
			}
		
			if (luaL_loadstring(lua, fullOutput.c_str()) != LUA_OK) {
				sys_print(Error, "error loading script %s: %s\n", strFilePath.c_str(), lua_tostring(lua, -1));
				lua_pop(lua, 1);
				ASSERT(lua_gettop(lua) == 0);
				continue;
			}

			// Execute the loaded chunk
			if (lua_pcall(lua, 0, LUA_MULTRET, 0) != LUA_OK) {
				fprintf(stderr, "Error executing chunk for %s: %s\n",strFilePath.c_str(), lua_tostring(lua, -1));
				continue;
			}


		}
	}
	for (auto& c : lua_classes) {
		c->init_lua_type();
	}
}

LuaClassTypeInfo::LuaClassTypeInfo() : ClassTypeInfo("lua_class_empty",nullptr,nullptr,nullptr,false,nullptr,0,nullptr,true)
{
}

LuaClassTypeInfo::~LuaClassTypeInfo()
{
}

extern void stack_dump(lua_State* L);


static void dump_lua_table(lua_State* L, int index, int depth = 0) {
	if (!lua_istable(L, index)) {
		printf("%*s(Not a table)\n", depth * 2, "");
		return;
	}

	lua_pushnil(L);  // first key
	while (lua_next(L, index < 0 ? index - 1 : index)) {
		// key at -2, value at -1

		// Print indentation
		printf("%*s", depth * 2, "");

		// Print key
		if (lua_type(L, -2) == LUA_TSTRING) {
			printf("[\"%s\"] = ", lua_tostring(L, -2));
		}
		else if (lua_type(L, -2) == LUA_TNUMBER) {
			printf("[%g] = ", lua_tonumber(L, -2));
		}
		else {
			printf("[%s] = ", luaL_typename(L, -2));
		}

		// Print value
		int vtype = lua_type(L, -1);
		switch (vtype) {
		case LUA_TNIL:     printf("nil\n"); break;
		case LUA_TBOOLEAN: printf(lua_toboolean(L, -1) ? "true\n" : "false\n"); break;
		case LUA_TNUMBER:  printf("%g\n", lua_tonumber(L, -1)); break;
		case LUA_TSTRING:  printf("\"%s\"\n", lua_tostring(L, -1)); break;
		case LUA_TTABLE:
			printf("{\n");
			dump_lua_table(L, -1, depth + 1);
			printf("%*s}\n", depth * 2, "");
			break;
		default:
			printf("(%s)\n", luaL_typename(L, -1));
			break;
		}

		lua_pop(L, 1);  // pop value, keep key for next lua_next
	}
}
void LuaClassTypeInfo::init_lua_type()
{
	assert(template_lua_table == 0);
	auto L = ScriptManager::inst->get_lua_state();
	lua_getglobal(L, lua_classname.c_str());
	assert(!lua_isnil(L, -1));
	//stack_dump(L);
	template_lua_table = luaL_ref(L, LUA_REGISTRYINDEX);
	int top = lua_gettop(L);
	assert(top == 0);
	lua_pushnil(L);
	lua_setglobal(L, lua_classname.c_str());
	ScriptManager::inst->init_this_class_type(this);
	ScriptManager::inst->set_class_type_global(this);
}

ClassBase* LuaClassTypeInfo::lua_class_alloc(const ClassTypeInfo* c)
{
	assert(c->super_typeinfo && c->super_typeinfo->scriptable_allocate);
	auto out = c->super_typeinfo->scriptable_allocate(c);
	LuaClassTypeInfo* luaInfo = (LuaClassTypeInfo*)(c);
	auto L = ScriptManager::inst->get_lua_state();


	lua_rawgeti(L, LUA_REGISTRYINDEX, out->get_table_registry_id()); // -3
	lua_rawgeti(L, LUA_REGISTRYINDEX, luaInfo->template_lua_table);	// -2
	stack_dump(L);
	int top = lua_gettop(L);
	assert(top == 2);

	// Copy table1 into dst
	lua_pushnil(L);  // first key

	top = lua_gettop(L);
	assert(top == 3);
	stack_dump(L);

	while (lua_next(L, -2)) {
		stack_dump(L);

		int top = lua_gettop(L);
		assert(top == 4);

		lua_pushvalue(L, -2);  // duplicate key
		lua_insert(L, -2);     // move key below value
		stack_dump(L);
		lua_settable(L, -5);   // dst[key] = value
		stack_dump(L);
		// leaves key for next lua_next
	}
	top = lua_gettop(L);
	assert(top == 2);
	stack_dump(L);
	sys_print(Debug, "template table");
	dump_lua_table(L, -1);
	top = lua_gettop(L);
	assert(top == 2);
	sys_print(Debug, "output table");
	dump_lua_table(L, -2);

	lua_pop(L, 2);
	top = lua_gettop(L);
	assert(top == 0);
	return out;
}
