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
	
	virtual renderobj_handle register_obj() = 0;
	virtual void update_obj(renderobj_handle handle, const Render_Object_Proxy& proxy) = 0;
	virtual void remove_obj(renderobj_handle handle) = 0;
};

extern RendererPublic* idraw;