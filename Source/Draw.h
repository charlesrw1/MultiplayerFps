#pragma once
#include "Config.h"
#include "glm/glm.hpp"
#include "Util.h"
#include "Types.h"
#include "Level.h"
class Model;
class Animator;
class Texture;
class Entity;

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

struct Render_Level_Params {
	View_Setup view;
	uint32_t output_framebuffer;
	bool clear_framebuffer = true;
	bool draw_level = true;
	bool draw_ents = true;
	enum Pass_Type { STANDARD, DEPTH, SHADOWMAP };
	Pass_Type pass = STANDARD;
	bool cull_front_face = false;
	bool force_backface=false;
	bool upload_constants = false;
	bool include_lightmapped = true;
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


class Renderer
{
public:
	void Init();
	void FrameDraw();

	void render_level_to_target(Render_Level_Params params);

	void draw_text();
	void draw_rect(int x, int y, int width, int height, Color32 color, Texture* texture=nullptr, 
		float srcw=0, float srch=0, float srcx=0, float srcy=0);	// src* are in pixel coords

	void reload_shaders();
	void ui_render();

	void on_level_start();
	void render_world_cubemap(vec3 position, EnvCubemap* out, uint32_t fbo, int size);

	void cubemap_positions_debug();


	void DrawModel(Render_Level_Params::Pass_Type pass, const Model* m, glm::mat4 transform, const Animator* a = nullptr, float rough=1.0, float metal=0.0);
	void AddPlayerDebugCapsule(Entity& e, MeshBuilder* mb, Color32 color);

	uint32_t white_texture;
	uint32_t black_texture;
	Texture3d perlin3d;
	
	int cubemap_index = 0;

	enum {
		BASE0_SAMPLER, AUX0_SAMPLER, BASE1_SAMPLER,
		AUX1_SAMPLER, SPECIAL_SAMPLER, LIGHTMAP_SAMPLER, NUM_SAMPLERS
	};

	// shaders
	enum { 
		// meshbuilder's
		S_SIMPLE, S_TEXTURED, S_TEXTURED3D, S_TEXTUREDARRAY, S_SKYBOXCUBE,
		
		// model's
		S_ANIMATED, 
		S_STATIC, S_STATIC_AT, S_WIND, S_WIND_AT,
		S_LIGHTMAPPED, S_LIGHTMAPPED_AT, S_LIGHTMAPPED_BLEND2,

		// z-prepass'
		S_DEPTH, S_AT_DEPTH, S_ANIMATED_DEPTH, S_WIND_DEPTH, S_WIND_AT_DEPTH,
		
		S_PARTICLE_BASIC,

		// other
		S_BLOOM_DOWNSAMPLE, S_BLOOM_UPSAMPLE, S_COMBINE,

		S_NUM
	};

	Shader shade[S_NUM];

	struct {
		uint32_t scene;

		uint32_t bloom;
	}fbo;
	struct {
		uint32_t scene_color;
		uint32_t scene_depthstencil;

		uint32_t bloom_depth;
		uint32_t bloom_chain[BLOOM_MIPS];
		glm::ivec2 bloom_chain_isize[BLOOM_MIPS];
		glm::vec2 bloom_chain_size[BLOOM_MIPS];

		uint32_t levelcubemap_array = 0;
		int levelcubemap_num = 0;
	}tex;
	struct {
		uint32_t current_frame;
	}ubo;

	uint32_t active_constants_ubo = 0;
	View_Setup vs;	// globally accessible view for passes
	View_Setup lastframe_vs;

	// graphics_settings
	Config_Var* r_draw_collision_tris;
	Config_Var* r_draw_sv_colliders;
	Config_Var* r_draw_viewmodel;
	Config_Var* vsync;
	Config_Var* r_shadow_quality;
	Config_Var* r_bloom;
	Config_Var* r_volumetric_fog;

	void bind_texture(int bind, int id);
	void set_shader(Shader& s) { 
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

	Texture* lens_dirt;

	Shadow_Map_System shadowmap;
	Volumetric_Fog_System volfog;
	float slice_3d=0.0;
	Level_Light dyn_light;
private:
	struct Sprite_Drawing_State {
		bool force_set = true;
		bool in_world_space = false;
		bool additive = false;
		uint32_t current_t = 0;
	}sprite_state;
	void draw_sprite_buffer();

	struct Model_Drawing_State {
		bool is_transparent_pass = false;
		bool initial_set = false;
		bool is_lightmapped = false;
		bool is_animated = false;
		bool is_alpha_tested = false;
		bool backfaces = false;
		int alpha_type = -1;
		int shader_type = -1;

		bool set_model_params = false;
		// light, cubemap, decal list
	};
	void draw_model_real(Model_Drawing_State* state, const Model* m, int part, glm::mat4 transform, 
		const Entity* e = nullptr, const Animator* a = nullptr, Game_Shader* override_mat = nullptr);

	void upload_ubo_view_constants(uint32_t ubo);

	void init_bloom_buffers();
	void render_bloom_chain();

	void InitGlState();
	void InitFramebuffers();

	void DrawEnts(Render_Level_Params::Pass_Type pass);
	void DrawLevel();
	void DrawLevelDepth(const Render_Level_Params& params);
	void DrawSkybox();

	void DrawPlayerViewmodel();

	void DrawEntBlobShadows();
	void AddBlobShadow(glm::vec3 org, glm::vec3 normal, float width);

	void set_shader_sampler_locations();
	void set_wind_constants();

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