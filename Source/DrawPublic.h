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

struct SceneDrawParamsEx {
	bool draw_ui = true;
	bool output_to_screen = true;	// else output to a framebuffer texture, later sampled by ie ImGui ui
	bool draw_world = true;
};

class MeshBuilder;
class IEditorTool;
class UIControl;
class RendererPublic
{
public:
	virtual void init() = 0;

	// Game call api

	virtual void scene_draw(
		SceneDrawParamsEx params,
		View_Setup view,	/* camera */
		UIControl* ui_root = nullptr /* ui_paint callback */, 
		IEditorTool* tool = nullptr /* overlay_draw callback (might remove this)*/) = 0;
	virtual void on_level_start() = 0;
	virtual void on_level_end() = 0;
	
	// Mesh API

	virtual handle<Render_Object> register_obj() = 0;
	virtual void update_obj(handle<Render_Object> handle, const Render_Object& proxy) = 0;
	virtual void remove_obj(handle<Render_Object>& handle) = 0;

	// Light API

	virtual handle<Render_Light> register_light(const Render_Light& l) = 0;
	virtual void update_light(handle<Render_Light> handle, const Render_Light& l) = 0;
	virtual void remove_light(handle<Render_Light>& handle) = 0;

	virtual void reload_shaders() = 0;

	// only used by animation editor to draw to an imgui window
	virtual uint32_t get_composite_output_texture_handle() = 0;
};

extern RendererPublic* idraw;