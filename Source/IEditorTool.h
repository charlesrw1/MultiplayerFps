#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <cassert>
#include "Framework/ClassBase.h"
#include <functional>
union SDL_Event;
struct View_Setup;
#include "EngineEditorState.h"

// Base editor tool class
#include "Game/EntityPtr.h"

class IEntitySnapshot {
public:
	virtual ~IEntitySnapshot() {}
};

class IEditorEntity {
public:
};

class IEditorEvents {
public:
	virtual viewMulticastDelegate<> on_selection_changed() = 0;
};

class Entity;
class IEditorApi {
public:
	virtual Entity* add_entity() = 0;
	virtual void remove_entity(Entity* e) = 0;

	virtual void clear_select() = 0;
	virtual void add_select(std::vector<EntityPtr> entities) = 0;
	virtual std::vector<EntityPtr> get_all_entities() = 0;
	virtual bool is_selected(EntityPtr ptr) = 0;
	virtual std::vector<EntityPtr> get_selected() = 0;
	virtual void save() = 0;
	virtual void undo() = 0;
	virtual std::string get_document_name() = 0;
	virtual void set_document_name(const std::string& name) = 0;
	virtual void clear_hide() = 0;
	virtual void add_hide(EntityPtr ptr) = 0;

	// for grouping multiple comamnds in 1 undoable block
	virtual void start_command_group_scope() = 0;
	virtual void end_command_group_scope() = 0;

	virtual std::unique_ptr<IEntitySnapshot> make_entity_snapshot(Entity* e) = 0;
	virtual void commit_entity_changes(std::unique_ptr<IEntitySnapshot> snap) = 0;

	void hide_selected() {
		auto selected = get_selected();
		for (EntityPtr e : selected)
			add_hide(e);
	}
	void save_as(const std::string& name) {
		set_document_name(name);
		save();
	}
	bool not_selected(EntityPtr ptr) {
		return !is_selected(ptr);
	}
	void select_all() {
		add_select(get_all_entities());
	}
	void inverse_select() {
		auto all = get_all_entities();
		std::vector<EntityPtr> select_these;
		for (EntityPtr e : all) {
			if (!is_selected(e)) {
				select_these.push_back(e);
			}
		}
		clear_select();
		add_select(select_these);
	}
};

class IEditorTool
{
public:
	virtual ~IEditorTool() {}


	// if save is called when !current_document_has_path(), then it will open a popup to pick a save directory
	bool save();
	void draw_menu_bar();

	// called every render frame while open
	virtual void tick(float dt) {}
	// return a view to render the world with, can be perspective or ortho
	// return nullptr to disable scene drawing
	virtual const View_Setup* get_vs() { return nullptr; }
	void draw_imgui_public();

	virtual void hook_pre_scene_viewport_draw() {}
	virtual void hook_scene_viewport_draw() {}
	virtual void hook_imgui_newframe() {}
	virtual bool wants_scene_viewport_menu_bar() const { return false; }

	virtual const char* get_save_file_extension() const = 0;

	// the name of the document
	// the full path of the document is ($ENGINE_ROOT)+"get_save_root_dir() + name"
	virtual std::string get_doc_name() const { return ""; }

	bool get_has_editor_changes() const { return has_editor_changes; }
	void clear_editor_changes() { has_editor_changes = false; }
	void set_has_editor_changes();

protected:
	// various hooks to add imgui calls
	virtual void hook_menu_bar_file_menu() {}
	virtual void hook_menu_bar() {}
	virtual bool has_create_new_menu_item() { return true; }

	// called every frame to make imgui calls
	virtual void imgui_draw() {}

	void set_window_title();

private:
	// this is called by save(), it guaranteed that this document will have a valid path to save to
	// (get_save_root_dir()+get_doc_name()) when called, assert otherwise return wether the document could compile+save
	// properly
	virtual bool save_document_internal() = 0;

	bool has_editor_changes = false;
};
#endif