#pragma once
#include <string>
#include <vector>
#include <unordered_map>
using std::string;
using std::vector;
using std::unordered_map;
#include "Framework/ClassTypeInfo.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ConsoleCmdGroup.h"
#include <stdexcept>

enum class ScriptType {
	Nil,
	Number,
	Bool,
	Vec3,
	Quat,
	Transform,
	Color,
	Ptr,
};
struct ParseProperty {
	string name;
	string type_str;
};
struct ParseType {
	string name;
	vector<string> inherited;
	vector<ParseProperty> props;
};
// parses the script
class ScriptLoadingUtil {
public:
	static vector<ParseType> parse_text(string text);
};

class LuaClassTypeInfo : public ClassTypeInfo {
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
	void set_had_changes() {
		had_changes = true;
	}
private:
	bool had_changes = false;
	int template_lua_table = 0;
	static ClassBase* lua_class_alloc(const ClassTypeInfo* c);
	string lua_classname;
};
class LuaRuntimeError : public std::runtime_error {
public:
	LuaRuntimeError(std::string msg) : std::runtime_error("LuaError: " + msg) {
	}
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
	void init_this_class_type(ClassTypeInfo* classTypeInfo);
	void set_class_type_global(ClassTypeInfo* type);
	void set_enum_global(const std::string& name, const EnumTypeInfo*);
	int create_class_table_for(ClassBase* classTypeInfo);
	void free_class_table(int id);
	lua_State* get_lua_state() {
		return lua;
	}
	ClassBase* allocate_class(string name);
	void reload_all_scripts();
	void reload_one_file(const string& fileName);
private:
	bool had_changes = false;
	void initialize_class_type(const ClassTypeInfo* type);
	lua_State* lua = nullptr;
	std::unordered_map<std::string, uptr<LuaClassTypeInfo>> lua_classes;
};