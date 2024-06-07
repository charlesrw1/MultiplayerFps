#pragma once
#include "IEditorTool.h"
#if 0
class UIEdNode
{

};

class UIEditorLocal : public IEditorTool
{
	// Inherited via IEditorTool
	virtual void init() override;
	virtual void tick(float dt) override;
	virtual const View_Setup& get_vs() override;
	virtual void overlay_draw() override;
	virtual bool handle_event(const SDL_Event& event) override;
	virtual const char* get_name() override;
	virtual void draw_frame() override;
	virtual void imgui_draw() override;
	virtual void open(const char* name) override;
	virtual void close() override;
	virtual void on_change_focus(editor_focus_state newstate) override;
};
#endif