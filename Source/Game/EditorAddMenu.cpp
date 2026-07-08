#include "EditorAddMenu.h"

EditorAddMenuRegistry& EditorAddMenuRegistry::get() {
	static EditorAddMenuRegistry inst;
	return inst;
}

void EditorAddMenuRegistry::add(std::string menu_path, std::string classname, bool is_component) {
	entries.push_back(EditorAddMenuEntry{std::move(menu_path), std::move(classname), is_component});
}
