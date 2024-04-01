#pragma once

#include "RenderObj.h"

class RendererPublic
{
public:
	virtual void scene_draw(bool editor_mode) = 0;
	virtual void init() = 0;

	virtual void on_level_start() = 0;
	virtual void on_level_end() = 0;
	
	virtual void reload_shaders() = 0;
	
	virtual handle<Render_Object> register_obj() = 0;
	virtual void update_obj(handle<Render_Object> handle, const Render_Object& proxy) = 0;
	virtual void remove_obj(handle<Render_Object>& handle) = 0;
};

extern RendererPublic* idraw;