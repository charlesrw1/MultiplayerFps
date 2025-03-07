#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <cassert>
#include "Framework/ClassBase.h"

union SDL_Event;
struct View_Setup;


// Base editor tool class
class IEditorTool
{
public:
	// if save is called when !current_document_has_path(), then it will open a popup to pick a save directory
	void close();
	bool save();
	bool open(const char* name, const char* arg = "");
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
	const std::string& get_doc_name() const { return name; }
	virtual const ClassTypeInfo& get_asset_type_info() const = 0;
	
	bool get_is_open() const {
		return is_open;
	}
protected:

	// various hooks to add imgui calls
	virtual void hook_menu_bar_file_menu() {}
	virtual void hook_menu_bar() {}
	virtual bool has_create_new_menu_item() { return true; }

	// called every frame to make imgui calls
	virtual void imgui_draw() {}

	bool current_document_has_path() const { return !name.empty(); }

	void set_empty_doc() { // sets the name to empty, will open a popup to save later
		name = "";
	}

	// called once on the first time a call to open a document
	virtual void init() {}

	// return wether the document can be saved right now, if false, save_document_internal() will not be called
	virtual bool can_save_document() { return true; };

private:
	// this is called by open(), if the document doesnt exist or fails to parse, you MUST open something, so create a new doc, this will be checked
	virtual bool open_document_internal(const char* name, const char* arg) = 0;
	// this is called by close(), remove all internal references to the currently open document, this only gets called if 
	// has_document_open() is true, so you dont have to check
	virtual void close_internal() = 0;
	// this is called by save(), it guaranteed that this document will have a valid path to save to (get_save_root_dir()+get_doc_name())
	// when called, assert otherwise
	// return wether the document could compile+save properly
	virtual bool save_document_internal() = 0;
	void set_window_title();
	void open_the_open_popup() {
		open_open_popup = true;
	}

	std::string name = "";
	bool is_initialized = false;
	bool open_open_popup = false;
	bool open_save_popup = false;
	bool is_open = false;
};
#endif