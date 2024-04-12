#pragma once

union SDL_Event;
struct View_Setup;
class EditorDocPublic
{
public:
	virtual void open_doc(const char* levelname) = 0;
	virtual void save_doc() = 0;
	virtual void close_doc() = 0;
	virtual bool handle_event(const SDL_Event& event) = 0;
	virtual void update() = 0;
	virtual void scene_draw_callback() = 0;
	virtual void overlays_draw() = 0;
	virtual void imgui_draw() = 0;
	virtual const View_Setup& get_vs() = 0;
};

extern EditorDocPublic* g_editor_doc;