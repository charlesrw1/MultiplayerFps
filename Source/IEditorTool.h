#pragma once

#include "UI.h"
#include <string>
#include <cassert>
union SDL_Event;
struct View_Setup;

// Focus state allows an editor tool to be run in the background while the game is running.
// This allows for debugging or editing while playing.
// However the engine only has 1 rendering+physics world, thus when a game starts, all the
// editor renderables/lights/physics/etc. handles need to removed.
// When the game closes and returns focus to the editor, all the editor stuff should be brought back.
// Do this in on_change_focus()
enum class editor_focus_state
{
	Closed,		// not open
	Background,	// open but game is active
	Focused,	// open and active
};

// Base editor tool class, inherits from UIControl for paint() and handle_event() calls
class IEditorTool : public UIControl
{
public:
	// these are called by both engine and internally to close/save
	// if save is called when !current_document_has_path(), then it will open a popup to pick a save directory
	void close();
	bool save();

	editor_focus_state get_focus_state() const {
		return focus_state;
	}
	// call this when the game starts or the game ends
	void set_focus_state(editor_focus_state state) {
		if (focus_state != state) {
			focus_state = state;
			on_change_focus(state);
		}
		assert(has_document_open() != (state == editor_focus_state::Closed));
	}
	// this is how documents are opened from the engine layer
	// you must provide the editor_focus_state to enter the editor in
	bool open_and_set_focus(const char* path, editor_focus_state state) {
		if (!is_initialized) {
			init();
			is_initialized = true;
		}
		
		if (focus_state != state) {
			focus_state = state;
			on_change_focus(state);
		}
		bool could_open = open(path);
		if (!could_open) {
			focus_state = editor_focus_state::Closed;
			on_change_focus(editor_focus_state::Closed);
		}
		assert(!could_open || (has_document_open() != (state == editor_focus_state::Closed)));

		return could_open;
	}

	// called every render frame while open
	virtual void tick(float dt) = 0;
	// return a view to render the world with, can be perspective or ortho
	virtual const View_Setup& get_vs() = 0;
	// callback from the renderer to draw gizmos with a MeshBuilder
	virtual void overlay_draw() = 0;
	// called every frame to make imgui calls
	virtual void imgui_draw();

	// this is called when the focus_state changes, when you get a call to
	// editor_focus_state::closed, you must close
	// this is guaranteed to call before open()
	virtual void on_change_focus(editor_focus_state newstate) = 0;

	// various hooks to add imgui calls
	virtual void hook_imgui_newframe() {}
	virtual void hook_scene_viewport_draw() {}
	virtual void draw_menu_bar() {}	// imgui hook
protected:
	bool open(const char* name);
	bool current_document_has_path() const { return !name.empty(); }
	void open_the_open_popup() {
		open_open_popup = true;
	}
	const char* get_name() const {
		return name.c_str();
	}

	// set the empty filename, use this for new documents
	// when the user saves, it will open a prompt to pick a name
	void set_empty_name() {
		name = "";
	}

	// set the name of the editors document, this should be a filename
	void set_doc_name(const std::string& name) {
		this->name = name;
	}

	// called once on the first time a call to open a document
	virtual void init() = 0;

	// path to prefix to get_doc_name(), working directory is always ($ENGINE_ROOT)
	virtual std::string get_save_root_dir() { return "./Data/"; }

	// return wether the document can be saved right now, if false, save_document_internal() will not be called
	virtual bool can_save_document() = 0;

	// returns the editor name for display ('Animation Editor', 'Level Editor')
	virtual const char* get_editor_name() = 0;

	// return wether a document is currently open
	// this is used to check some assertions, make sure this is true/false after calls to open/close
	virtual bool has_document_open() const = 0;

	// the name of the document
	// the full path of the document is ($ENGINE_ROOT)+"get_save_root_dir() + name"
	const std::string& get_doc_name() const { return name; }
private:
	// this is called by open(), if the document doesnt exist or fails to parse, you MUST open something, so create a new doc, this will be checked
	virtual void open_document_internal(const char* name) = 0;
	// this is called by close(), remove all internal references to the currently open document, this only gets called if 
	// has_document_open() is true, so you dont have to check
	virtual void close_internal() = 0;
	// this is called by save(), it guaranteed that this document will have a valid path to save to (get_save_root_dir()+get_doc_name())
	// when called, assert otherwise
	// return wether the document could compile+save properly
	virtual bool save_document_internal() = 0;

	std::string name = "";
	bool is_initialized = false;
	bool open_open_popup = false;
	bool open_save_popup = false;
	editor_focus_state focus_state = editor_focus_state::Closed;
};