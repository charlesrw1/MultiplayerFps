#pragma once
#include <string>
#include <vector>

// Registers a class into the level editor's scene right-click "Add" menu (see
// EditorDoc::draw_scene_context_menu, LevelEditor/EditorDocViewport.cpp). Nothing is
// auto-enumerated here -- game code opts each class in explicitly via
// REGISTER_EDITOR_ADD_MENU_ENTRY.
struct EditorAddMenuEntry
{
	// May contain '/' to nest under submenus, e.g. "NPCs/Enemy" or "Items/Health Pack".
	// A bare name with no '/' is a top-level entry. The last segment is the displayed label.
	std::string menu_path;
	// Entity or Component class name, resolved via ClassBase::find_class at spawn time.
	std::string classname;
	// true: classname is a Component, attached to a newly spawned empty Entity.
	// false: classname is an Entity subclass, spawned directly.
	bool is_component = false;
};

class EditorAddMenuRegistry
{
public:
	static EditorAddMenuRegistry& get();
	void add(std::string menu_path, std::string classname, bool is_component);
	const std::vector<EditorAddMenuEntry>& get_entries() const { return entries; }

private:
	std::vector<EditorAddMenuEntry> entries;
};

struct EditorAddMenuAutoRegister
{
	EditorAddMenuAutoRegister(const char* menu_path, const char* classname, bool is_component) {
		EditorAddMenuRegistry::get().add(menu_path, classname, is_component);
	}
};

#define EDITOR_ADD_MENU_CONCAT_(a, b) a##b
#define EDITOR_ADD_MENU_CONCAT(a, b) EDITOR_ADD_MENU_CONCAT_(a, b)

// menu_path: e.g. "NPCs/Enemy" (nested) or "Health Pack" (top-level). classname: Entity or
// Component class name. is_component: true for a Component, false for an Entity subclass.
#define REGISTER_EDITOR_ADD_MENU_ENTRY(menu_path, classname, is_component)                                          \
	static EditorAddMenuAutoRegister EDITOR_ADD_MENU_CONCAT(s_editor_add_menu_reg_, __COUNTER__)(                   \
		menu_path, classname, is_component)
