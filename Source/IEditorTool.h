#pragma once

union SDL_Event;
struct View_Setup;

enum class editor_focus_state
{
	Closed,		// not open
	Background,	// open but game is active
	Focused,	// open and active
};

class IEditorTool
{
public:
	virtual void init() = 0;

	virtual void tick(float dt) = 0;
	virtual const View_Setup& get_vs() = 0;
	virtual void overlay_draw() = 0;
	virtual bool handle_event(const SDL_Event& event) = 0;
	virtual const char* get_name() = 0;
	
	virtual void draw_frame() = 0;
	virtual void imgui_draw() = 0;

	virtual void open(const char* name) = 0;
	virtual void close() = 0;

	virtual void on_change_focus(editor_focus_state newstate) = 0;

	editor_focus_state get_focus_state() const {
		return focus_state;
	}
	void set_focus_state(editor_focus_state state) {
		if (focus_state != state)
			on_change_focus(state);
		focus_state = state;
	}

private:
	editor_focus_state focus_state = editor_focus_state::Closed;
};