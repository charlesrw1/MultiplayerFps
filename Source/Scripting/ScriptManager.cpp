#include "ScriptManager.h"
#include "Framework/StringUtils.h"
#include "Framework/Files.h"

#include <cassert>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

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
	printf("ScriptManager init...");
	lua = luaL_newstate();

	load_script_files();
}

ScriptManager::~ScriptManager()
{
	lua_close(lua);
	lua = nullptr;
}

void ScriptManager::load_script_files()
{

}
