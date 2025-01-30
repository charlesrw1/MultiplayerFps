#pragma once

class GUI;
union SDL_Event;

struct World_GUI
{
	glm::vec3 worldspace_pos{};
	GUI* gui_ptr = nullptr;
};

class GuiSystemPublic
{
public:
	static GuiSystemPublic* create_gui_system();

	// ui events
	virtual void handle_event(const SDL_Event& event) = 0;
	virtual void post_handle_events() = 0;
	// *(game update)*
	// ui thinks and layout updates
	virtual void think() = 0;
	// paint
	virtual void paint() = 0;

	virtual void set_focus_to_this(GUI* panel) = 0;

	virtual void add_gui_panel_to_root(GUI* panel) = 0;
	virtual void remove_reference(GUI* panel) = 0;
	virtual void add_to_think_list(GUI* panel) = 0;
	virtual void remove_from_think_list(GUI* panel) = 0;


	virtual void set_viewport_ofs(int x, int y) = 0;
	virtual void set_viewport_size(int x, int y) = 0;

};