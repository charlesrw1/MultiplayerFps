#include "ScriptManager.h"
#include "Framework/StringUtils.h"

vector<ParseType> ScriptLoadingUtil::parse_text(string text) {
	auto lines = StringUtils::to_lines(text);

	struct PendingClass
	{
		string name;
		vector<string> inherited;
		vector<ParseProperty> properties;
		bool editor_placeable = false;
		bool init_in_editor = false;
		int class_line = 0;
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
				out.push_back({currentClass.name, currentClass.inherited, currentClass.properties, currentClass.editor_placeable, currentClass.init_in_editor, currentClass.class_line});
				currentClass = PendingClass();
			}
			inClass = false;
			pendingType.clear();
		};

		// Parse class definition
		// Check original stripped line starts with "---@class" to avoid matching comments containing "@class"
		if (StringUtils::starts_with(original_stripped_line, "---@class") && tokens.size() > 2 && tokens.at(1) == "@class") {
			if (inClass && !currentClass.name.empty()) {
				// Save previous class
				out.push_back({currentClass.name, currentClass.inherited, currentClass.properties, currentClass.editor_placeable, currentClass.init_in_editor, currentClass.class_line});
				currentClass = PendingClass();
			}
			currentClass.name = tokens.at(2);
			currentClass.inherited.clear();
			currentClass.properties.clear();
			currentClass.class_line = i + 1;
			inClass = false;
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
			if (!currentClass.name.empty()) {
				inClass = true;
			}
			if (tokens.at(tokens.size() - 1) == "}") {
				printf("oneline class\n");
				append_class();
			}
		}
		// Parse property type annotation
		// Check original stripped line starts with "---@type" to avoid matching comments containing "@type"
		else if (StringUtils::starts_with(original_stripped_line, "---@type") && tokens.size() >= 3 && tokens.at(1) == "@type") {
			pendingType = tokens.at(2);
		}
		// `---editor` annotation — opts the class in to the editor's add-component
		// picker. Must appear after `---@class` and before the class table opener.
		// We accept any trailing whitespace/content after the tag word, but require
		// that the (re-spaced) second token is exactly "editor" so things like
		// "---editorial" don't match. An optional trailing `, init_in_editor` also
		// applies set_call_init_in_editor(true) to every instance (start()/stop()
		// run in the editor), equivalent to calling it in the ctor.
		else if (StringUtils::starts_with(original_stripped_line, "---editor") && !currentClass.name.empty()
				 && tokens.size() >= 2 && tokens.at(1) == "editor") {
			currentClass.editor_placeable = true;
			for (size_t j = 2; j < tokens.size(); ++j) {
				if (tokens[j] == "init_in_editor") {
					currentClass.init_in_editor = true;
					break;
				}
			}
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
			append_class();
		}
	}
	// Handle last class if file doesn't end with }
	if (inClass && !currentClass.name.empty()) {
		out.push_back({currentClass.name, currentClass.inherited, currentClass.properties, currentClass.editor_placeable, currentClass.init_in_editor, currentClass.class_line});
	}

	return out;
}
