#pragma once

union SDL_Event;
struct View_Setup;


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

	virtual void signal_going_to_game() { }
};