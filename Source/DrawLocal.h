#pragma once
#include "DrawPublic.h"
#include "Framework/Config.h"
#include "glm/glm.hpp"
#include "Framework/Util.h"
#include "Types.h"
#include "Level.h"
#include "GlmInclude.h"
#include "Framework/MeshBuilder.h"
#include "Shader.h"
#include "EnvProbe.h"
#include "Texture.h"
#include "../Shaders/SharedGpuTypes.txt"
#include "DrawTypedefs.h"
#include "RenderObj.h"
#include "Framework/FreeList.h"

#pragma optimize("", on);

class MeshPart;
class Model;
class Animator;
class Texture;
class Entity;
class Material;

const int BLOOM_MIPS = 6;

typedef int program_handle;

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
	void make_render_targets(bool initial, int w, int h);
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


// represents a singular call to glDrawElements() with same state and mesh, batchable
struct Mesh_Batch
{
	uint32_t first = 0;	// indexes into pass.sorted_list
	uint32_t count = 0;

	uint32_t shader_index = 0;	// indexes into shader_list[]
	const Material* material = nullptr;
};

// represents multiple mesh_batch calls packaged into one glMultidrawIndirect()
struct Multidraw_Batch
{
	uint32_t first = 0;
	uint32_t count = 0;
};

struct draw_call_key  {
	draw_call_key() {
		shader = blending = backface = texture = vao = mesh = 0;
		layer = 0;
	}

	// lowest
	uint64_t mesh : 19;
	uint64_t vao : 5;
	uint64_t texture : 19;
	uint64_t backface : 1;
	uint64_t blending : 3;
	uint64_t shader : 14;
	uint64_t layer : 3;		
	// highest
	
	uint64_t as_uint64() const{
		return *(reinterpret_cast<const uint64_t*>(this));
	}
};
static_assert(sizeof(draw_call_key) == 8, "key needs 8 bytes");


struct Pass_Object
{
	Pass_Object() {
		submesh_index = 0;
	}

	draw_call_key sort_key;
	const Material* material = nullptr;
	handle<Render_Object> render_obj{};	// entity instance
	uint32_t submesh_index;		// what submesh am i
};

// in the end: want a flat list of batches that are merged with neighbors
enum class pass_type
{
	OPAQUE,
	TRANSPARENT,
	DEPTH
};

 
typedef int passobj_handle;

class Render_Scene;
class Render_Pass
{
public:
	Render_Pass(pass_type type);


	pass_type type;

	//void delete_object(
	//	const Render_Object& proxy, 
	//	renderobj_handle handle,
	//	Material* material,
	//	uint32_t submesh,
	//	uint32_t layer);

	void make_batches(Render_Scene& scene);
	
	void add_object(
		const Render_Object& proxy, 
		handle<Render_Object> handle,
		Material* material,
		uint32_t submesh, 
		uint32_t layer);

	draw_call_key create_sort_key_from_obj(
		const Render_Object& proxy, 
		Material* material, 
		uint32_t submesh, 
		uint32_t layer);

	void clear() {
		objects.clear();
	}

	//std::vector<Pass_Object> deletions;
	//std::vector<Pass_Object> creations;


	std::vector<Pass_Object> objects;
	std::vector<Mesh_Batch> mesh_batches;
	std::vector<Multidraw_Batch> batches;
};

class Culling_Pass
{
public:
	bufferhandle visibility;
};

struct ROP_Internal
{
	Render_Object proxy;
	glm::mat4 inv_transform;
};

// cull main view
// first pass does objects visible last frame
// render visible ones last frame
// second pass works on objects not visible, check if they are this frame

// use the culling result to create draw calls for opaque, transparent

// shadow view culling
// only does frustum, cull objects for each cascade
// create draw calls for each cascade

struct Render_Lists
{
	void init(uint32_t drawidsz, uint32_t instbufsz);
	uint32_t indirect_drawid_buf_size=0;
	uint32_t indirect_instance_buf_size=0;


	std::vector<gpu::DrawElementsIndirectCommand> commands;
	bufferhandle gpu_command_list = 0;
	std::vector<int> command_count;
	bufferhandle gpu_command_count = 0;

	std::vector<uint32_t> instance_to_instance;
	std::vector<uint32_t> draw_to_material;

	// maps the gl_DrawID to submesh material (dynamically uniform for bindless)
	bufferhandle gldrawid_to_submesh_material;
	// maps gl_baseinstance + gl_instance to the render object instance (for transforms, animation, etc.)
	bufferhandle glinstance_to_instance;
};

class Render_Scene
{
public:
	Render_Scene();

	void init();

	handle<Render_Object> register_renderable();
	void update(handle<Render_Object> handle, const Render_Object& proxy);
	void remove(handle<Render_Object> handle);
	const Render_Object& get(handle<Render_Object> handle) {
		return proxy_list.get(handle.id).proxy;
	}

	void build_scene_data();
	void build_render_list(Render_Lists& list, Render_Pass& src);
	void upload_scene_materials();

	Render_Pass depth;			// vis/shadow objects, same as opaque but grouped with minimal draw clls
	Render_Pass opaque;			// opaque objects that have full sorting
	Render_Pass transparents;	// transparent objects added in back to front order

	Render_Lists vis_list;
	Render_Lists opaque_list;
	Render_Lists transparents_list;
	Render_Lists shadow_lists;	// one for each cascade

	std::vector<glm::mat4x4> skinned_matricies_vec;
	std::vector<gpu::Object_Instance> gpu_objects;
	std::vector<gpu::Material_Data> scene_mats_vec;
	bufferhandle gpu_skinned_mats_buffer = 0;
	bufferhandle gpu_render_instance_buffer = 0;
	bufferhandle gpu_render_material_buffer = 0;

	Culling_Pass main_view;

	Free_List<ROP_Internal> proxy_list;

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

struct Render_Stats
{
	int textures_bound = 0;
	int shaders_bound = 0;
	int tris_drawn = 0;
	int draw_calls = 0;
	int vaos_bound = 0;
	int blend_changes = 0;
};

class Program_Manager
{
public:
	program_handle create_raster(const char* frag, const char* vert, const std::string& defines = {});
	program_handle create_raster_geo(const char* frag, const char* vert, const char* geo = nullptr, const std::string& defines = {});
	program_handle create_compute(const char* compute, const std::string& defines = {});
	Shader get_obj(program_handle handle) {
		return programs[handle].shader_obj;
	}
	void recompile_all();

	struct program_def {
		std::string defines;
		const char* frag = nullptr;
		const char* vert = nullptr;
		const char* geo = nullptr;
		bool is_compute = false;
		bool compile_failed = false;
		Shader shader_obj;
	};
	vector<program_def> programs;
private:
	void recompile(program_def& def);
};

struct shader_key
{
	shader_key() {
		shader_type = 0;
		depth_only = 0;
		alpha_tested = 0;
		normal_mapped = 0;
		animated = 0;
		vertex_colors = 0;
		dither = 0;
	}
	uint32_t shader_type : 26;
	uint32_t animated : 1;
	uint32_t alpha_tested : 1;
	uint32_t normal_mapped : 1;
	uint32_t vertex_colors : 1;
	uint32_t depth_only : 1;
	uint32_t dither : 1;

	uint32_t as_uint32() const {
		return *((uint32_t*)this);
	}
};
static_assert(sizeof(shader_key) == 4, "shader key needs 4 bytes");

class Material_Shader_Table
{
public:
	Material_Shader_Table();
	struct material_shader_internal {
		shader_key key;
		program_handle handle = -1;
	};

	program_handle lookup(shader_key key); 
	void insert(shader_key key, program_handle handle);

	std::vector<material_shader_internal> shader_hash_map;
};

class Renderer : public RendererPublic
{
public:
	Renderer();

	// public interface
	virtual void init() override;
	virtual void scene_draw(View_Setup view, special_render_mode mode) override;
	virtual void on_level_start() override;
	virtual void on_level_end() override;
	virtual void reload_shaders() override;
	virtual handle<Render_Object> register_obj() override;
	virtual void update_obj(handle<Render_Object> handle, const Render_Object& proxy) override;
	virtual void remove_obj(handle<Render_Object>& handle) override;
	virtual uint32_t get_composite_output_texture_handle() override {
		ASSERT(tex.output_composite!=0);
		return tex.output_composite;
	}

	void render_level_to_target(Render_Level_Params params);

	void draw_text();
	void draw_rect(int x, int y, int width, int height, Color32 color, Texture* texture=nullptr, 
		float srcw=0, float srch=0, float srcx=0, float srcy=0);	// src* are in pixel coords

	void create_shaders();

	void ui_render();

	void render_world_cubemap(vec3 position, uint32_t fbo, uint32_t texture, int size);

	void cubemap_positions_debug();

	void execute_render_lists(Render_Lists& lists, Render_Pass& pass);

	void AddPlayerDebugCapsule(Entity& e, MeshBuilder* mb, Color32 color);

	Texture white_texture;
	Texture black_texture;
	Texture flat_normal_texture;
	Texture3d perlin3d;
	
	int cubemap_index = 0;
	static const int MAX_SAMPLER_BINDINGS = 16;

	Program_Manager prog_man;
	Material_Shader_Table mat_table;
	struct programs
	{
		program_handle simple;
		program_handle textured;
		program_handle textured3d;
		program_handle texturedarray;
		program_handle skybox;

		program_handle particle_basic;
		program_handle bloom_downsample;
		program_handle bloom_upsample;
		program_handle combine;
		program_handle hbao;
		program_handle xblur;
		program_handle yblur;
		program_handle mdi_testing;
	}prog;

	struct framebuffers {
		fbohandle scene;
		fbohandle reflected_scene;
		fbohandle bloom;
		fbohandle composite = 0;
	}fbo;

	struct textures {
		texhandle scene_color;
		texhandle scene_depthstencil;
		texhandle reflected_color;
		texhandle reflected_depth;
		texhandle output_composite = 0;

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

	void bind_vao(uint32_t vao);
	void bind_texture(int bind, int id);
	void set_shader(program_handle handle);
	void set_blend_state(blend_state blend);
	void set_show_backfaces(bool show_backfaces);

	Shader shader();

	void draw_sprite(glm::vec3 pos, Color32 color, glm::vec2 size, Texture* mat, 
		bool billboard, bool in_world_space, bool additive, glm::vec3 orient_face);

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

	program_handle get_mat_shader(bool is_animated, const Mesh& part, const Material& gs, bool depth_pass, bool dither);
	
	Render_Stats stats;

	void set_shader_sampler_locations();
	View_Setup current_frame_main_view;
private:

	struct Sprite_Drawing_State {
		bool force_set = true;
		bool in_world_space = false;
		bool additive = false;
		uint32_t current_t = 0;
	}sprite_state;
	void draw_sprite_buffer();

	void upload_ubo_view_constants(uint32_t ubo, glm::vec4 custom_clip_plane = glm::vec4(0.0));

	void init_bloom_buffers();
	void render_bloom_chain();

	void planar_reflection_pass();

	void InitGlState();
	void InitFramebuffers(bool create_composite_texture, int s_w, int s_h);

	void DrawSkybox();

	void DrawEntBlobShadows();
	void AddBlobShadow(glm::vec3 org, glm::vec3 normal, float width);

	void set_wind_constants();
	void set_water_constants();


	int cur_w = 0;
	int cur_h = 0;


	struct Opengl_State_Machine
	{
		program_handle active_program = -1;
		texhandle textures_bound[16];
		blend_state blending = blend_state::OPAQUE;
		bool backface_state = false;
		uint32_t current_vao = 0;

		enum invalid_bits {
			PROGRAM_BIT,
			BLENDING_BIT,
			BACKFACE_BIT,
			VAO_BIT,
			TEXTURE0_BIT,
		};
		uint32_t invalid_bits = UINT32_MAX;

		bool is_bit_invalid(uint32_t bit) { return invalid_bits & (1 << bit); }
		void set_bit_valid(uint32_t bit) { invalid_bits &= ~(1 << bit); }
		void set_bit_invalid(uint32_t bit) { invalid_bits |= (1 << bit); }
		void invalidate_all() { invalid_bits = UINT32_MAX; }
	};

	Opengl_State_Machine state_machine;

	MeshBuilder ui_builder;
	texhandle building_ui_texture;

	MeshBuilder shadowverts;

};

extern Renderer draw;