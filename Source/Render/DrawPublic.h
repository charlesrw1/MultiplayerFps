#pragma once
#include "Framework/Handle.h"
#include <glm/glm.hpp>

struct View_Setup
{
	View_Setup() {}
	View_Setup(glm::vec3 origin, glm::vec3 front, float fov, float near, float far, int width, int height);
	View_Setup(glm::mat4 viewMat, float fov, float near, float far, int width, int height);

	// dont use this, just for some things that dont play nice with infinite Z
	glm::mat4 make_opengl_perspective_with_near_far() const;

	glm::vec3 origin;
	glm::vec3 front;
	glm::mat4 view, proj, viewproj;
	float fov, near, far;
	int width, height;
};

struct SceneDrawParamsEx {
	SceneDrawParamsEx(float time, float dt) 
		: time(time), dt(dt) {}
	bool draw_ui = true;
	bool output_to_screen = true;	// else output to a framebuffer texture, later sampled by ie ImGui ui
	bool draw_world = true;
	float time;
	float dt;
	bool is_editor = false;
	bool is_cubemap_view = false;
	bool skybox_only = false;
};

struct Particle_Object;
struct Render_Object;
struct MeshBuilder_Object;
struct Render_Decal;
struct Render_Light;
struct Render_Sun;
struct Render_Irradiance_Volume;
struct Render_Reflection_Volume;
struct Render_Skylight;
class TerrainInterfacePublic;
struct RenderFog;

class MeshBuilder;
class IEditorTool;
class UIControl;

class RenderScenePublic
{
public:
	// Mesh API

	virtual handle<Render_Object> register_obj() = 0;
	virtual void update_obj(handle<Render_Object> handle, const Render_Object& proxy) = 0;
	virtual void remove_obj(handle<Render_Object>& handle) = 0;
	// returns null on a bad handle
	// DONT cache this! just use for quick reads of an existing object
	virtual const Render_Object* get_read_only_object(handle<Render_Object> handle) = 0;

	// Decal API
	virtual handle<Render_Decal> register_decal() = 0;
	virtual void update_decal(handle<Render_Decal> handle, const Render_Decal& d) = 0;
	virtual void remove_decal(handle<Render_Decal>& handle) = 0;

	// Light API

	virtual handle<Render_Light> register_light() = 0;
	virtual void update_light(handle<Render_Light> handle, const Render_Light& l) = 0;
	virtual void remove_light(handle<Render_Light>& handle) = 0;

	virtual handle<Render_Sun> register_sun() = 0;
	virtual void update_sun(handle<Render_Sun> handle, const Render_Sun& s) = 0;
	virtual void remove_sun(handle<Render_Sun>& handle) = 0;

	virtual handle<Render_Reflection_Volume> register_reflection_volume() = 0;
	virtual void update_reflection_volume(handle<Render_Reflection_Volume> handle, const Render_Reflection_Volume& v) = 0;
	virtual void remove_reflection_volume(handle<Render_Reflection_Volume>& handle) = 0;
	virtual handle<Render_Skylight> register_skylight() = 0;
	virtual void update_skylight(handle<Render_Skylight> handle, const Render_Skylight& v) = 0;
	virtual void remove_skylight(handle<Render_Skylight>& handle) = 0;

	// only one fog allowed in a scene, others will just not work
	virtual handle<RenderFog> register_fog() = 0;
	virtual void update_fog(handle<RenderFog> handle, const RenderFog& fog) = 0;
	virtual void remove_fog(handle<RenderFog>& handle) = 0;

	virtual TerrainInterfacePublic* get_terrain_interface() = 0;


	// Debug line renderer handles
	virtual handle<MeshBuilder_Object> register_meshbuilder() = 0;
	virtual void update_meshbuilder(handle<MeshBuilder_Object> handle, const MeshBuilder_Object& mbobj) = 0;
	virtual void remove_meshbuilder(handle<MeshBuilder_Object>& handle) = 0;

	virtual handle<Particle_Object> register_particle_obj() = 0;
	virtual void update_particle_obj(handle<Particle_Object> handle, const Particle_Object& mbobj) = 0;
	virtual void remove_particle_obj(handle<Particle_Object>& handle) = 0;
};

class GuiSystemPublic;
class RendererPublic
{
public:
	virtual void init() = 0;

	// Game call api

	virtual RenderScenePublic* get_scene() = 0;

	virtual void scene_draw(
		SceneDrawParamsEx params,
		View_Setup view	/* camera */
	) = 0;
	virtual void sync_update() = 0;

	virtual void on_level_start() = 0;
	virtual void on_level_end() = 0;
	
	virtual void bake_cubemaps() = 0;

	virtual void reload_shaders() = 0;

	// only used by animation editor to draw to an imgui window
	virtual uint32_t get_composite_output_texture_handle() = 0;

	// tests the output buffer from last frame and returns whatever object was drawn
	// ONLY for level editor (requires SceneDrawParamEx::is_editor to have been true LAST frame)
	// returns -1 on none
	virtual handle<Render_Object> mouse_pick_scene_for_editor(int x, int y) = 0;
	// test the depth buffer, returns LINEAR depth, ONLY for editor!
	virtual float get_scene_depth_for_editor(int x, int y) = 0;
};

extern RendererPublic* idraw;