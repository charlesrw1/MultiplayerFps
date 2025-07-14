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
class IEditorTool
{
public:
	virtual ~IEditorTool() {}

	void try_close(std::function<void()> callback);	// will try closing document, if success, does callback.
	void try_save(std::function<void()> callback);
	
	virtual uptr<CreateEditorAsync> create_command_to_load_back() = 0;

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
	virtual const ClassTypeInfo& get_asset_type_info() const = 0;
	

	bool get_has_editor_changes() const {
		return has_editor_changes;
	}
	void clear_editor_changes() {
		has_editor_changes = false;
	}
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


	// this is called by save(), it guaranteed that this document will have a valid path to save to (get_save_root_dir()+get_doc_name())
	// when called, assert otherwise
	// return wether the document could compile+save properly
	virtual bool save_document_internal() = 0;


	bool has_editor_changes = false;

};
#endif