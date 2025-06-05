#pragma once
#include <string>
#include <vector>
#include <unordered_map>
using std::string;
using std::vector;
using std::unordered_map;

enum class ScriptType
{
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
	string name;
	string type_str;
	ScriptType type_enum = ScriptType::Nil;
};
class ScriptTypeInfo
{
public:
	bool is_defined = false;
	string classname;
	string lua_table_name;	// might be different
	ScriptTypeInfo* parent = nullptr;
	vector<ScriptProperty> properties;
	vector<ScriptTypeInfo*> interfaces;

	string source_file;	// what file
	int definition_line = 0;	// what line

	uint64_t template_table = 0;	// lua template table
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

// manages lua and the loaded scripts

struct lua_State;
class ScriptManager
{
public:
	ScriptManager* inst = nullptr;
	ScriptManager();
	~ScriptManager();

private:
	void load_script_files();
	lua_State* lua = nullptr;
};