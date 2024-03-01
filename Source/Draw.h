#pragma once
#include "Config.h"
#include "glm/glm.hpp"
#include "Util.h"
#include "Types.h"
#include "Level.h"
#include "GlmInclude.h"
#include "MeshBuilder.h"
#include "Shader.h"
#include "EnvProbe.h"

class MeshPart;
class Model;
class Animator;
class Texture;
class Entity;
class Game_Shader;

const int BLOOM_MIPS = 6;

struct Texture3d
{
	glm::ivec3 size;
	uint32_t id = 0;
};
Texture3d generate_perlin_3d(glm::ivec3 size, uint32_t seed, int octaves, int frequency, float persistence, float lacunarity);


class Volumetric_Fog_System
{
public:
	int quality = 2;
	uint32_t voltexture = 0;
	uint32_t voltexture_prev = 0;
	glm::ivec3 voltexturesize;

	Shader reproject;
	Shader lightcalc;
	Shader raymarch;

	float density = 10.0;
	float anisotropy = 0.7;
	float spread = 1.0;
	float frustum_end = 50.f;
	int temporal_sequence = 0;

	uint32_t light_ssbo;
	uint32_t param_ubo;

	void init();
	void shutdown();
	void compute();
};
class Shadow_Map_System
{
public:
	void init();
	void update();
	void make_csm_rendertargets();
	void update_cascade(int idx, const View_Setup& vs, glm::vec3 directional_dir);

	const static int MAXCASCADES = 4;
	uint32_t frame_view_ubos[4];
	glm::vec4 split_distances;
	glm::mat4x4 matricies[MAXCASCADES];
	float nearplanes[MAXCASCADES];
	float farplanes[MAXCASCADES];
	uint32_t csm_ubo;
	int csm_resolution = 0;
	bool enabled = false;
	Level_Light current_sun;

	uint32_t shadow_map_array;
	uint32_t framebuffer;
	// Parameters
	bool cull_front_faces = false;
	bool fit_to_scene = true;
	bool reduce_shimmering = true;
	float log_lin_lerp_factor = 0.5;
	float max_shadow_dist = 80.f;
	float epsilon = 0.008f;
	float poly_units = 4;
	float poly_factor = 1.1;
	float z_dist_scaling = 1.f;
	int quality = 2;
	bool targets_dirty = false;
};

class SSAO_System
{
public:
	void init();
	void make_render_targets();
	void render();

	uint32_t noise_tex2;
	glm::vec3 samples[64];
	uint32_t fbo;
	uint32_t rbo;
	uint32_t halfres_texture;	// raw ao texture
	uint32_t fullres1;	// blurx result
	uint32_t fullres2;	// blury result and final output
	int width, height;

	int res_scale = 1;
	float radius = 0.5;
	float bias = 0.025;
};

// more horribleness
struct Render_Level_Params {
	View_Setup view;
	uint32_t output_framebuffer;
	bool clear_framebuffer = true;
	bool draw_level = true;
	bool draw_ents = true;
	enum Pass_Type { OPAQUE, TRANSLUCENT, DEPTH, SHADOWMAP };
	Pass_Type pass = OPAQUE;
	bool cull_front_face = false;
	bool force_backface=false;
	bool upload_constants = false;
	bool include_lightmapped = true;
	bool is_probe_render = false;
	bool draw_viewmodel = false;
	bool is_water_reflection_pass = false;
	bool has_clip_plane = false;
	vec4 custom_clip_plane = vec4(0.f);
	uint32_t provied_constant_buffer = 0;
};


typedef int Light_Handle;

struct Render_Item
{
	Model* model;
	uint32_t part_index;
	Game_Shader* material;
	glm::mat4 transform;
	Entity* ent = nullptr;
	Animator* animator = nullptr;
};

class Object_Cull_Job
{

};

struct Render_Key
{
	int viewport_layer : 2;
	int shader : 8;
	int texture0 : 12;
	int blending_mode : 2;
	int backface_mode : 1;
	int vao : 13;
	int depth : 18;
	int obj_index;
};

class Render_Lists
{
public:
	std::vector<Render_Key> transparents;
	std::vector<Render_Key> opaques;
	std::vector<Render_Key> shadow_casters;
	std::vector<Render_Item> items;

	void add_item(Render_Item item, bool cast_shadows);
};

struct Render_Light
{
	vec3 position;
	vec3 normal;
	vec3 color;
	float conemin;
	float conemax;
	bool casts_shadow = false;
	int shadow_array_index = 0;
	int type = 0;
};

struct Render_Box_Cubemap
{
	vec3 boxmin;
	vec3 boxmax;
	vec3 probe_pos = vec3(0.f);
	int priority = 0;
	bool found_probe_flag = false;
	int id = -1;
};

class Render_Scene
{
public:
	uint32_t skybox = 0;
	std::vector<Render_Box_Cubemap> cubemaps;
	uint32_t cubemap_ssbo;
	uint32_t levelcubemapirradiance_array = 0;
	uint32_t levelcubemapspecular_array = 0;
	int levelcubemap_num = 0;

	int directional_index = -1;
	std::vector<Render_Light> lights;
	uint32_t light_ssbo;
};

struct Draw_Model_Frontend_Params
{
	Model* model = nullptr;
	glm::mat4 transform;
	bool wireframe_render;
	bool solidcolor_render;
	bool render_additive;
	glm::vec4 colorparam;
};

class Renderer
{
public:
	Renderer();

	void Init();

	// editor mode doesn't draw UI and it calls the eddoc hook to draw custom stuff
	void scene_draw(bool editor_mode);

	void render_level_to_target(Render_Level_Params params);

	void draw_text();
	void draw_rect(int x, int y, int width, int height, Color32 color, Texture* texture=nullptr, 
		float srcw=0, float srch=0, float srcx=0, float srcy=0);	// src* are in pixel coords
	void draw_model_immediate(Draw_Model_Frontend_Params params);

	void reload_shaders();
	void ui_render();

	void on_level_start();
	void render_world_cubemap(vec3 position, uint32_t fbo, uint32_t texture, int size);

	void cubemap_positions_debug();


	void AddPlayerDebugCapsule(Entity& e, MeshBuilder* mb, Color32 color);

	uint32_t white_texture;
	uint32_t black_texture;
	uint32_t default_normal_texture;

	Texture3d perlin3d;
	
	int cubemap_index = 0;

	enum {
		SAMPLER_BASE1, SAMPLER_AUX1, SAMPLER_NORM1, SAMPLER_BASE2, SAMPLER_AUX2, SAMPLER_NORM2,
		SAMPLER_SPECIAL, SAMPLER_LIGHTMAP, NUM_SAMPLERS
	};

	// shaders
	enum Shader_List { 
		// meshbuilder's
		S_SIMPLE, S_TEXTURED, S_TEXTURED3D, S_TEXTUREDARRAY, S_SKYBOXCUBE,
		
		// z-prepass'
		S_DEPTH, S_AT_DEPTH, S_ANIMATED_DEPTH, S_WIND_DEPTH, S_WIND_AT_DEPTH,
		
		S_WATER,
		S_PARTICLE_BASIC,

		// other
		S_BLOOM_DOWNSAMPLE, S_BLOOM_UPSAMPLE, S_COMBINE,
		S_HBAO, S_XBLUR, S_YBLUR,	// hbao shaders
		S_SSAO,


		NUM_NON_MODEL_SHADERS,
	};

	enum Shader_Define_Param_Enum
	{
		SDP_ALPHATESTED,
		SDP_NORMALMAPPED,
		SDP_LIGHTMAPPED,
		SDP_ANIMATED,
		SDP_VERTEXCOLORS,
		NUM_SDP
	};
	enum Model_Shader_Types
	{
		MST_STANDARD,
		MST_MULTIBLEND,
		MST_WINDSWAY,
		NUM_MST
	};

	Shader shader_list[NUM_NON_MODEL_SHADERS + NUM_MST * (1 << NUM_SDP)];

	struct {
		uint32_t scene;
		uint32_t reflected_scene;
		uint32_t bloom;
	}fbo;
	struct {
		uint32_t scene_color;
		uint32_t scene_depthstencil;
		uint32_t reflected_color;
		uint32_t reflected_depth;

		uint32_t bloom_depth;
		uint32_t bloom_chain[BLOOM_MIPS];
		glm::ivec2 bloom_chain_isize[BLOOM_MIPS];
		glm::vec2 bloom_chain_size[BLOOM_MIPS];
	}tex;
	struct {
		uint32_t current_frame;
	}ubo;

	uint32_t active_constants_ubo = 0;
	View_Setup vs;	// globally accessible view for passes
	View_Setup lastframe_vs;

	// graphics_settings
	
	Auto_Config_Var draw_collision_tris;
	Auto_Config_Var draw_sv_colliders;
	Auto_Config_Var draw_viewmodel;
	Auto_Config_Var enable_vsync;
	Auto_Config_Var shadow_quality_setting;
	Auto_Config_Var enable_bloom;
	Auto_Config_Var enable_volumetric_fog;
	Auto_Config_Var enable_ssao;
	Auto_Config_Var use_halfres_reflections;


	void bind_texture(int bind, int id);
	void set_shader(Shader& s) { 
		if (s.ID != cur_shader) {
			s.use();
			cur_shader = s.ID; 
		}
	}
	void set_shader(int index_into_shader_list) {
		ASSERT(index_into_shader_list >= 0 && index_into_shader_list < (sizeof shader_list) / (sizeof Shader));
		Shader& s = shader_list[index_into_shader_list];
		if (s.ID != cur_shader) {
			s.use();
			cur_shader = s.ID;
		}
	}

	void draw_sprite(glm::vec3 pos, Color32 color, glm::vec2 size, Texture* mat, 
		bool billboard, bool in_world_space, bool additive, glm::vec3 orient_face);

	Shader& shader();
	void set_shader_constants();
	void set_depth_shader_constants();


	// >>> PBR BRANCH
	EnvCubemap skybox;
	float rough = 1.f;
	float metal = 0.f;
	glm::vec3 aosphere;
	glm::vec2 vfog = glm::vec2(10,0.0);
	glm::vec3 ambientvfog;
	bool using_skybox_for_specular = false;

	Texture* lens_dirt;
	Texture* casutics;
	Texture* waternormal;

	SSAO_System ssao;
	Shadow_Map_System shadowmap;
	Volumetric_Fog_System volfog;
	float slice_3d=0.0;
	Level_Light dyn_light;

	Render_Scene scene;
private:
	std::vector<Draw_Model_Frontend_Params> immediate_draw_calls;

	struct Sprite_Drawing_State {
		bool force_set = true;
		bool in_world_space = false;
		bool additive = false;
		uint32_t current_t = 0;
	}sprite_state;
	void draw_sprite_buffer();

	struct Model_Drawing_State {
		bool initial_set = true;
		int current_shader = 0;	// S_ANIMATED, S_STATIC, ...
		bool initial_model = true;
		int current_alpha_state = 0;
		int current_backface_state = 0;
		bool is_water_reflection_pass = false;
		Render_Level_Params::Pass_Type pass = Render_Level_Params::OPAQUE;
	};

	void draw_model_real(const Model* m, glm::mat4 transform, const Entity* e, const Animator* a,
		Model_Drawing_State& state);
	void draw_model_real_depth(const Model* m, glm::mat4 transform, const Entity* e, const Animator* a,
		Model_Drawing_State& state);

	void upload_ubo_view_constants(uint32_t ubo, glm::vec4 custom_clip_plane = glm::vec4(0.0));

	void init_bloom_buffers();
	void render_bloom_chain();

	void planar_reflection_pass();

	void InitGlState();
	void InitFramebuffers();

	void DrawEnts(const Render_Level_Params& params);
	void DrawLevel(const Render_Level_Params& params);
	void DrawLevelDepth(const Render_Level_Params& params);
	void DrawSkybox();

	void DrawPlayerViewmodel(const Render_Level_Params& params);

	void DrawEntBlobShadows();
	void AddBlobShadow(glm::vec3 org, glm::vec3 normal, float width);

	void set_shader_sampler_locations();
	void set_wind_constants();
	void set_water_constants();

	int get_shader_index(const MeshPart& part, const Game_Shader& gs, bool depth_pass);

	View_Setup current_frame_main_view;

	int cur_w = 0;
	int cur_h = 0;
	uint32_t cur_shader = 0;
	uint32_t cur_tex[NUM_SAMPLERS];

	MeshBuilder ui_builder;
	uint32_t building_ui_texture;

	MeshBuilder shadowverts;

};

extern Renderer draw;