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
#include "Texture.h"
#include "../Shaders/SharedGpuTypes.txt";
#include "DrawTypedefs.h"

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

	glm::ivec3 voltexturesize;
	float density = 10.0;
	float anisotropy = 0.7;
	float spread = 1.0;
	float frustum_end = 50.f;
	int temporal_sequence = 0;

	struct buffers {
		bufferhandle light;
		bufferhandle param;
	}buffer;

	struct programs {
		Shader reproject;
		Shader lightcalc;
		Shader raymarch;
	}prog;

	struct textures {
		texhandle volume;
		texhandle last_volume;
	}texture;

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

	struct uniform_buffers {
		bufferhandle frame_view[4];
		bufferhandle info;
	}ubo;

	struct framebuffers {
		fbohandle shadow;
	}fbo;

	struct textures {
		texhandle shadow_array;
	}texture;

	struct params {
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
	}tweak;

	glm::vec4 split_distances;
	glm::mat4x4 matricies[MAXCASCADES];
	float nearplanes[MAXCASCADES];
	float farplanes[MAXCASCADES];
	int csm_resolution = 0;
	bool enabled = false;
	Level_Light current_sun;
	bool targets_dirty = false;
};

class SSAO_System
{
public:
	void init();
	void reload_shaders();
	void make_render_targets(bool initial);
	void render();
	void update_ubo();

	int width=0, height=0;
	const static int RANDOM_ELEMENTS = 16;

	struct framebuffers {
		fbohandle depthlinear        = 0;
		fbohandle viewnormal         = 0;
		fbohandle hbao2_deinterleave = 0;
		fbohandle hbao2_calc         = 0;
		fbohandle finalresolve = 0;
	}fbo;

	struct programs {
		Shader hbao_calc;
		Shader linearize_depth;
		Shader make_viewspace_normals;
		Shader hbao_blur;
		Shader hbao_deinterleave;
		Shader hbao_reinterleave;
	}prog;

	struct textures {
		texhandle random	= 0;
		texhandle result	= 0;
		texhandle blur		= 0;
		texhandle viewnormal = 0;
		texhandle depthlinear = 0;
		texhandle deptharray = 0;
		texhandle resultarray = 0;
		texhandle depthview[RANDOM_ELEMENTS];
	}texture;

	struct uniform_buffers {
		bufferhandle data = 0;
	}ubo;

	struct params {
		float radius = 0.4;
		float intensity = 2.5;
		float bias = 0.1;
		float blur_sharpness = 40.0;
	}tweak;

	gpu::HBAOData data = {};
	glm::vec4 random_elements[RANDOM_ELEMENTS];
};

// more horribleness
struct Render_Level_Params {
	View_Setup view;
	uint32_t output_framebuffer;
	bool clear_framebuffer = true;
	enum Pass_Type { 
		OPAQUE, 
		TRANSLUCENT, 
		DEPTH, 
		SHADOWMAP 
	};
	Pass_Type pass = OPAQUE;
	bool draw_viewmodel = false;
	bool is_probe_render = false;
	bool is_water_reflection_pass = false;

	bool has_clip_plane = false;
	vec4 custom_clip_plane = vec4(0.f);
	bool upload_constants = false;
	uint32_t provied_constant_buffer = 0;
};

template<typename T>
class Free_List
{
public:
	using handle_type = int;

	T& get(handle_type handle) {
		assert(handle_to_obj[handle] != -1);
		return objects[handle_to_obj[handle]].type_;
	}
	handle_type make_new() {
		handle_type h = 0;
		if (free_handles.empty()) {
			h =  first_free++;
			handle_to_obj.resize(handle_to_obj.size() + 1);
		}
		else {
			h = free_handles.back();
			free_handles.pop_back();
		}
		handle_to_obj[h] = objects.size();
		objects.resize(objects.size() + 1);

		return h;
	}
	void free(handle_type handle) {
		int obj_index = handle_to_obj[handle];
		handle_to_obj[obj_index] = -1;
		objects[obj_index] = objects.back();
		handle_to_obj[objects[obj_index].handle] = obj_index;
		objects.resize(objects.size() - 1);
		free_handles.push_back(handle);
	}

	handle_type first_free = 0;
	vector<handle_type> free_handles;
	vector<int> handle_to_obj;
	struct pair {
		handle_type handle;
		T type_;
	};
	vector<pair> objects;
};


typedef int Light_Handle;

class Object_Cull_Job
{

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
	renderobj_handle register_renderable() {
		return proxy_list.make_new();
	}
	void update(renderobj_handle handle, const Render_Object_Proxy& proxy) {
		proxy_list.get(handle) = proxy;
	}
	void remove(renderobj_handle handle) {
		if (handle != -1) {
			proxy_list.free(handle);
		}
	}

	Free_List<Render_Object_Proxy> proxy_list;


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

struct Draw_Call_Object
{
	int transform_idx = 0;
	int mesh_idx = 0;
	int owner_handle_idx = 0;
};

struct Multidraw_Indirect_Command
{
	unsigned int  count;
	unsigned int  primCount;
	unsigned int  firstIndex;
	int   baseVertex;
	unsigned int  baseInstance;
};

struct Gpu_Material
{
	glm::vec4 diffuse_tint;
	float rough_mult;
	float metal_mult;
	float rough_remap_x;
	float rough_remap_y;
};

struct Gpu_Object
{
	glm::mat4x4 model;
	glm::mat4x4 invmodel;
	glm::vec4 color_val;
	int anim_matrix_offset = 0;
	float obj_param1;
	float obj_param2;
	float padding;
};


struct Gpu_Batch
{

};


typedef int Renering_Pass_Handle;

// defines the resources needed for each pass (forward, shadow, reflection)
struct Gpu_Mesh_Pass
{
	// these is written to by the gpu for culling and drawn
	uint32_t indirect_draw_buffer;
	uint32_t visibility_buffer;
};

typedef int Render_Scene_Handle;

struct Draw_Call
{
	const Mesh* mesh;
	Game_Shader* mat;
	int submesh;
	int mat_index;
	int object_index;
	uint64_t sort;
};

// defines shared resources used for gpu driven rendering
struct Shared_Gpu_Driven_Resources
{
public:
	void init();

	void build_draw_calls();

	void make_draw_calls_from(
		const Mesh* mesh,
		glm::mat4 transform,
		vector<Game_Shader*>& mat_list,
		const Animator* animator,
		bool casts_shadows,
		glm::vec4 colorparam);

	vector<Draw_Call> opaques;
	vector<Draw_Call> transparents;
	vector<Draw_Call> shadows;

	vector<glm::mat4x4> skinned_matricies;
	vector<Gpu_Object> gpu_objects;
	vector<Gpu_Material> scene_mats;

	uint32_t scene_mats_ssbo;
	uint32_t gpu_objs_ssbo;
	uint32_t anim_matrix_ssbo;
};

struct Render_Stats
{
	int textures_bound = 0;
	int shaders_bound = 0;
	int tris_drawn = 0;
	int draw_calls = 0;
};

class Renderer
{
public:
	Renderer();

	void Init();

	// editor mode doesn't draw UI and it calls the eddoc hook to draw custom stuff
	void scene_draw(bool editor_mode);
	void extract_objects();

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

	Texture white_texture;
	Texture black_texture;
	Texture flat_normal_texture;


	Texture3d perlin3d;
	
	int cubemap_index = 0;

	static const int MAX_SAMPLER_BINDINGS = 16;

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

		S_MDI_TESTING,


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

	struct framebuffers {
		fbohandle scene;
		fbohandle reflected_scene;
		fbohandle bloom;
	}fbo;

	struct textures {
		texhandle scene_color;
		texhandle scene_depthstencil;
		texhandle reflected_color;
		texhandle reflected_depth;

		texhandle bloom_chain[BLOOM_MIPS];
		glm::ivec2 bloom_chain_isize[BLOOM_MIPS];
		glm::vec2 bloom_chain_size[BLOOM_MIPS];
	}tex;

	struct uniform_buffers {
		bufferhandle current_frame;
	}ubo;

	struct buffers {
		bufferhandle default_vb;
	}buf;

	struct vertex_array_objects {
		vertexarrayhandle default_;
	}vao;

	bufferhandle active_constants_ubo = 0;
	
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

			stats.shaders_bound++;
		}
	}
	void set_shader(int index_into_shader_list) {
		ASSERT(index_into_shader_list >= 0 && index_into_shader_list < (sizeof shader_list) / (sizeof Shader));
		Shader& s = shader_list[index_into_shader_list];
		if (s.ID != cur_shader) {
			s.use();
			cur_shader = s.ID;
			stats.shaders_bound++;
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
	Shared_Gpu_Driven_Resources shared;
	std::vector<Draw_Model_Frontend_Params> immediate_draw_calls;
	int get_shader_index(const Mesh& part, const Game_Shader& gs, bool depth_pass);
	Render_Stats stats;
private:

	struct Sprite_Drawing_State {
		bool force_set = true;
		bool in_world_space = false;
		bool additive = false;
		uint32_t current_t = 0;
	}sprite_state;
	void draw_sprite_buffer();

	struct Model_Drawing_State {
		bool initial_set = true;
		uint32_t current_vao = -1;
		int current_shader = 0;	// S_ANIMATED, S_STATIC, ...
		bool initial_model = true;
		int current_alpha_state = 0;
		int current_backface_state = 0;
		bool is_water_reflection_pass = false;
		Render_Level_Params::Pass_Type pass = Render_Level_Params::OPAQUE;
	};

	void draw_model_real(const Draw_Call& dc,
		Model_Drawing_State& state);

	void upload_ubo_view_constants(uint32_t ubo, glm::vec4 custom_clip_plane = glm::vec4(0.0));

	void init_bloom_buffers();
	void render_bloom_chain();

	void planar_reflection_pass();

	void InitGlState();
	void InitFramebuffers();

	void DrawSkybox();

	void DrawEntBlobShadows();
	void AddBlobShadow(glm::vec3 org, glm::vec3 normal, float width);

	void set_shader_sampler_locations();
	void set_wind_constants();
	void set_water_constants();

	View_Setup current_frame_main_view;

	int cur_w = 0;
	int cur_h = 0;
	proghandle cur_shader = 0;
	texhandle cur_tex[MAX_SAMPLER_BINDINGS];

	MeshBuilder ui_builder;
	texhandle building_ui_texture;

	MeshBuilder shadowverts;

};

extern Renderer draw;