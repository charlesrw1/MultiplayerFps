#include "PublicPhysicsEditor.h"

class PhysicsEditorLocal : public IEditorTool
{
	// Inherited via IEditorTool
	virtual void init() override
	{
	}
	virtual void tick(float dt) override
	{
	}
	virtual const View_Setup& get_vs() override
	{
		// TODO: insert return statement here
	}
	virtual void overlay_draw() override
	{
	}
	virtual bool handle_event(const SDL_Event& event) override
	{
		return false;
	}
	virtual const char* get_name() override
	{
		return nullptr;
	}

	virtual void imgui_draw() override
	{
	}
	virtual void open(const char* name) override
	{
	}
	virtual void close() override
	{
	}
};