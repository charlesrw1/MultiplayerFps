#pragma once

#include "RenderObj.h"

struct View_Setup
{
	View_Setup() {}
	View_Setup(glm::vec3 origin, glm::vec3 front, float fov, float near, float far, int width, int height);

	glm::vec3 origin;
	glm::vec3 front;
	glm::mat4 view, proj, viewproj;
	float fov, near, far;
	int width, height;
};

enum class special_render_mode
{
	none,
	lvl_editor,
	anim_editor
};

class RendererPublic
{
public:
	virtual void scene_draw(View_Setup view, special_render_mode mode = special_render_mode::none) = 0;
	virtual void init() = 0;

	virtual void on_level_start() = 0;
	virtual void on_level_end() = 0;
	
	virtual void reload_shaders() = 0;
	
	virtual handle<Render_Object> register_obj() = 0;
	virtual void update_obj(handle<Render_Object> handle, const Render_Object& proxy) = 0;
	virtual void remove_obj(handle<Render_Object>& handle) = 0;

	// only used by animation editor to draw to an imgui window
	virtual uint32_t get_composite_output_texture_handle() = 0;
};

extern RendererPublic* idraw;