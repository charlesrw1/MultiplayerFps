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

struct ScriptProperty
{
	ScriptType type_enum = ScriptType::Nil;
	std::string_view name;
};
class ScriptTypeInfo
{
public:
	bool is_defined = false;
	std::string_view classname;
	ScriptTypeInfo* parent = nullptr;
	vector<ScriptProperty> properties;
	vector<ScriptTypeInfo*> interfaces;
	string source_file;	// what file
	int definition_line = 0;	// what line
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

struct ParseOutput {
	string all_code;
	unordered_map<string, ScriptTypeInfo> types;
};

// parses the script
class ScriptLoadingUtil
{
public:
	static vector<ParseType> parse_text(string text);
	static unordered_map<string, ScriptTypeInfo> load_types(const std::vector<string>& files);
	static vector<string> collect_script_files(string root);
};

class ScriptTypeManager
{
public:
	const ScriptTypeInfo* find_type(string name) const;

	unordered_map<string, ScriptTypeInfo> typeinfo;
};

class LuaClassTypeInfo : public ClassTypeInfo {
public:
	LuaClassTypeInfo();
	~LuaClassTypeInfo();

	void set_classname(string s) {
		this->lua_classname = s;
		this->classname = this->lua_classname.c_str();
	}
	void set_superclass(string s) {
		auto find = ClassBase::find_class(s.c_str());
		if (!find) {
			sys_print(Error, "LuaClassTypeInfo: no super type %s\n", s.c_str());
		}
		else if (!find->scriptable_allocate) {
			sys_print(Error, "LuaClassTypeInfo: super type isnt scriptable %s\n", s.c_str());
		}
		else {
			this->super_typeinfo = find;
			this->superclassname = find->classname;
			this->lua_prototype_index_table = find->get_prototype_index_table();
			this->id = find->id;
			this->last_child = this->id;
			this->allocate = lua_class_alloc;
		}
	}
	void init_lua_type();
private:
	int template_lua_table = 0;
	static ClassBase* lua_class_alloc(const ClassTypeInfo* c);
	string lua_classname;
};
class LuaScriptDef {
public:
	std::vector<LuaClassTypeInfo*> lua_classes;
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
	void load_script_files();
	void init_this_class_type(ClassTypeInfo* classTypeInfo);
	void set_class_type_global(ClassTypeInfo* type);
	int create_class_table_for(ClassBase* classTypeInfo);
	lua_State* get_lua_state() {
		return lua;
	}
	ClassBase* allocate_class(string name);
private:
	void initialize_class_type(const ClassTypeInfo* type);
	lua_State* lua = nullptr;
	std::vector<uptr<LuaClassTypeInfo>> lua_classes;
};