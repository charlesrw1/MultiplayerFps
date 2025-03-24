#pragma once

union SDL_Event;

class MaterialInstance;
class GuiSystemPublic
{
public:
	virtual void init() = 0;
	// ui events
	virtual void handle_event(const SDL_Event& event) = 0;
	virtual void post_handle_events() = 0;
	// *(game update)*
	// ui thinks and layout updates
	virtual void think() = 0;
	// paint
	virtual void paint() = 0;
	virtual void sync_to_renderer() = 0;
	virtual void set_viewport_ofs(int x, int y) = 0;
	virtual void set_viewport_size(int x, int y) = 0;

	virtual const MaterialInstance* get_default_ui_mat() const = 0;
};

extern GuiSystemPublic* g_guiSystem;