#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <cassert>
#include "Framework/ClassBase.h"
#include <functional>
union SDL_Event;
struct View_Setup;

#include "Framework/MulticastDelegate.h"

// Base editor tool class
#include "Game/EntityPtr.h"

class IEditorApi2;
class IEditorTool
{
public:
	virtual ~IEditorTool() {}

	static IEditorTool* create(string mapname);

	// if save is called when !current_document_has_path(), then it will open a popup to pick a save directory
	bool save();
	void draw_menu_bar();
	virtual IEditorApi2& get_editor_api() = 0;

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

	// Selects every entity in this tool's open document that references asset_gamepath (any
	// AssetPtr-typed component property). No-op for tools without a selectable entity list.
	// Used by the asset browser's "Select Entities Using This Asset" context-menu action.
	virtual void select_entities_using_asset(const std::string& asset_gamepath) {}

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