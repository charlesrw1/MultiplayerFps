#pragma once
#include "DrawPublic.h"
#include "Framework/Config.h"
#include "glm/glm.hpp"
#include "Framework/Util.h"
#include "Types.h"
#include "GlmInclude.h"
#include "Framework/MeshBuilder.h"
#include "Shader.h"
#include "EnvProbe.h"
#include "Texture.h"
#include "../Shaders/SharedGpuTypes.txt"
#include "DrawTypedefs.h"
#include "RenderObj.h"
#include "Framework/FreeList.h"
#include "Render/RenderExtra.h"
#include "Render/Material.h"
#include "Framework/MemArena.h"
#pragma optimize("", on)

class MeshPart;
class Model;
class Animator;
class Texture;
class Entity;
class Material;

extern ConfigVar draw_collision_tris;
extern ConfigVar draw_sv_colliders;
extern ConfigVar draw_viewmodel;
extern ConfigVar enable_vsync;
extern ConfigVar shadow_quality_setting;
extern ConfigVar enable_bloom;
extern ConfigVar enable_volumetric_fog;
extern ConfigVar enable_ssao;
extern ConfigVar use_halfres_reflections;


const int BLOOM_MIPS = 6;

typedef int program_handle;

struct Texture3d
{
	glm::ivec3 size;
	uint32_t id = 0;
};
Texture3d generate_perlin_3d(glm::ivec3 size, uint32_t seed, int octaves, int frequency, float persistence, float lacunarity);


struct Render_Box_Cubemap
{
	vec3 boxmin;
	vec3 boxmax;
	vec3 probe_pos = vec3(0.f);
	int priority = 0;
	bool found_probe_flag = false;
	int id = -1;
};

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

// represents multiple Mesh_Batch calls packaged into one glMultidrawIndirect()
struct Multidraw_Batch
{
	uint32_t first = 0;
	uint32_t count = 0;
};

// represents one draw call of a mesh with a material and various state, sorted and put into Mesh_Batch's
struct draw_call_key  {
	draw_call_key() {
		shader = blending = backface = texture = vao = mesh = 0;
		distance = 0;
		layer = 0;
	}

	// lowest
	uint64_t distance : 14;
	uint64_t mesh : 14;
	uint64_t vao : 3;
	uint64_t texture : 14;
	uint64_t backface : 1;
	uint64_t blending : 3;
	uint64_t shader : 12;
	uint64_t layer : 3;
	// highest
	
	// :)
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
	OPAQUE,			// front to back sorting
	TRANSPARENT,	// back to front sorting
	DEPTH			// front to back sorting, ignores material textures unless alpha tested
};
 
typedef int passobj_handle;

// A render_pass is one collection of POSSIBLE draw calls
// A render_list is created from a pass which is the actual draw calls to submit along with extra buffers to facilitate it
class Render_Scene;
class Render_Pass
{
public:
	Render_Pass(pass_type type);

	void make_batches(Render_Scene& scene);
	
	void add_object(
		const Render_Object& proxy, 
		handle<Render_Object> handle,
		Material* material,
		uint32_t camera_dist,
		uint32_t submesh, 
		uint32_t layer);

	draw_call_key create_sort_key_from_obj(
		const Render_Object& proxy, 
		Material* material, 
		uint32_t camera_dist,
		uint32_t submesh, 
		uint32_t layer);

	void clear() {
		objects.clear();
	}

	const pass_type type{};					// modifies batching+sorting logic

	// all Render_Objects in the pass
	// there will likely be multiple Pass_Objects from 1 R_O like multiple submeshes and LODs all get added
	// this is the array that gets frustum + occlusion culled to make the final Render_Lists structure
	// this means static objects can cache LODs
	std::vector<handle<Render_Object>> high_level_objects_in_pass;

	std::vector<Pass_Object> objects;		// geometry + material id + object id
	std::vector<Mesh_Batch> mesh_batches;	// glDrawElementsIndirect()
	std::vector<Multidraw_Batch> batches;	// glMultiDrawElementsIndirect()
};

// RenderObject internal data
struct ROP_Internal
{
	Render_Object proxy;
	glm::mat4 inv_transform;
};

// RenderLight internal data
struct RL_Internal
{
	Render_Light light;
	// stuff like shadowmap indicies etc.
	int shadow_array_index = -1;
};

// cull main view
// first pass does objects visible last frame
// render visible ones last frame
// second pass works on objects not visible, check if they are this frame

// use the culling result to create draw calls for opaque, transparent

// shadow view culling
// only does frustum, cull objects for each cascade
// create draw calls for each cascade

// Render lists: represents opengl commands that have been uploaded (or kept CPU side)
//				 these are fed into glMultiDrawElementsIndirect()
//				 these are built around Render_Pass which contains the objects that will be renderered

// Gpu occlusion culling:	

// Reprsents a structure for storing DrawElementsIndirectCommands
// These can be cpu or gpu stored, when using gpu culling, the gpu buffer is culled and used
// This gets fed into "execute_render_lists"
struct Render_Lists
{
	void init(uint32_t drawidsz, uint32_t instbufsz);

	void build_from(Render_Pass& src,
		Free_List<ROP_Internal>& proxy_list);

	uint32_t indirect_drawid_buf_size=0;
	uint32_t indirect_instance_buf_size=0;

	// commands to input to glMultiDrawElementsIndirect
	std::vector<gpu::DrawElementsIndirectCommand> commands;
	bufferhandle gpu_command_list = 0;
	// command_count is the number of commands per glMultiDrawElementsIndirect command
	// for now its just set to batches[i].count in the Render_Pass
	// when calling glMDEI, the offset into commands is the summation of previous command counts essentially
	// it works like an indirection into commands
	std::vector<int> command_count;
	bufferhandle gpu_command_count = 0;

	// maps the gl_DrawID to submesh material (dynamically uniform for bindless)
	bufferhandle gldrawid_to_submesh_material;
	// maps gl_baseinstance + gl_instance to the render object instance (for transforms, animation, etc.)
	bufferhandle glinstance_to_instance;

	// where are we getting our objects from
	const Render_Pass* parent_pass = nullptr;
};

// For CPU culling
struct CpuRenderLists
{
	std::vector<gpu::DrawElementsIndirectCommand> commands;
	// maps the gl_DrawID to submesh material (dynamically uniform for bindless)
	bufferhandle gldrawid_to_submesh_material;
	// maps gl_baseinstance + gl_instance to the render object instance (for transforms, animation, etc.)
	bufferhandle glinstance_to_instance;
	const Render_Pass* parent_pass = nullptr;
};

// Optional GPU path
struct GpuRenderLists
{
	bufferhandle gpu_command_list = 0;
	bufferhandle gpu_command_count = 0;

	// maps the gl_DrawID to submesh material (dynamically uniform for bindless)
	bufferhandle gldrawid_to_submesh_material;
	// maps gl_baseinstance + gl_instance to the render object instance (for transforms, animation, etc.)
	bufferhandle glinstance_to_instance;
	const Render_Pass* parent_pass = nullptr;
};


// In theory render passes can be made once and cached if the object is static and do culling on the gpu
// Render lists are updated every frame though
// Render passes are  1 to 1 with render lists except gbuffer and shadow lists

// Render passes: (describes objects to be drawn in a render list with a material+geometry)
// Gbuffer objects
// Shadow casting objects
// Transparent objects
// Custom depth objects
// editor selected objects

// render lists: (contains glmultidrawelementsindirect commands, generated on cpu or on gpu)
// gbuffer pass 1 (all gbuffer objects get added to this)
//		gbuffer pass 1 gets culled to last HZB
// gbuffer pass 2 (objects that failed first HZB)
//		gets culled again to 2nd HZB
// shadow lists for N lights (cull per shadow caster, or render entire list per light)
// transparent list
// custom depth list
// editor selected list


// The lowest level data
// Litteraly just feeding vertex data and a shader into glMultiDrawElementsIndirect
// particles are just a dynamic mesh with their own handle into the proxy list
struct DrawCommand
{
	// handle into a vertex buffer
	// this is likely the same for all static meshes
	// but particles/meshbuilders might have unique vertex buffers
	// the handle points to a vao/vbo/ebo resource
	uint64_t vertex_buffer_handle = 0;

	// handle for a material to render this with
	uint64_t material_handle = 0;

	// per object data
	uint64_t object_handle = 0;
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

	handle<Render_Light> register_light() { return { 0 }; }
	void update_light(handle<Render_Light> handle, const Render_Light& proxy) {}
	void remove_light(handle<Render_Light> handle) {}
	const Render_Light& get_light(handle<Render_Light> handle) {
		return light_list.get(handle.id).light;
	}

	void build_scene_data();
	void upload_scene_materials();

	RL_Internal* get_main_directional_light();

	std::unique_ptr<Render_Pass> gbuffer;
	std::unique_ptr<Render_Lists> gbuffer1;				// main draw list, or 1st pass if using gpu culling
	std::unique_ptr<Render_Lists> gbuffer2;				// 2nd pass for new unoccluded objects if using gpu culling

	std::unique_ptr<Render_Pass> transparent_objs;
	std::unique_ptr<Render_Lists> transparents_ren_list;// draw in forward pass of transparents

	std::unique_ptr<Render_Pass> custom_depth;
	std::unique_ptr<Render_Lists> custom_depth_list;	// draw to custom depth buffer

	std::unique_ptr<Render_Pass> editor_selection;
	std::unique_ptr<Render_Lists> editor_sel_list;		// drawn to editor selection buffer

	std::unique_ptr<Render_Pass> shadow_casters;
	std::unique_ptr<Render_Lists> global_shadow_list;	// unculled shadow casters


	Render_Pass depth;			// vis/shadow objects, same as opaque but grouped with minimal draw clls
	Render_Pass opaque;			// opaque objects that have full sorting
	Render_Pass transparents;	// transparent objects added in back to front order

	Render_Lists vis_list;
	Render_Lists opaque_list;
	Render_Lists transparents_list;
	Render_Lists shadow_lists;

	bufferhandle gpu_skinned_mats_buffer = 0;
	bufferhandle gpu_render_instance_buffer = 0;
	bufferhandle gpu_render_material_buffer = 0;

	Free_List<ROP_Internal> proxy_list;
	Free_List<RL_Internal> light_list;

	bufferhandle light_ssbo;
	bufferhandle light_grid_ssbo;
	bufferhandle indirect_to_light_ssbo;

	// list of IBL cubemaps and boxes
	// list of irradiance probe volumes and boxes

	uint32_t skybox = 0;
	std::vector<Render_Box_Cubemap> cubemaps;
	uint32_t cubemap_ssbo;
	uint32_t levelcubemapirradiance_array = 0;
	uint32_t levelcubemapspecular_array = 0;
	int levelcubemap_num = 0;

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
	std::vector<program_def> programs;
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
		billboard_type = 0;
	}
	uint32_t shader_type : 22;
	uint32_t animated : 1;
	uint32_t alpha_tested : 1;
	uint32_t normal_mapped : 1;
	uint32_t vertex_colors : 1;
	uint32_t depth_only : 1;
	uint32_t dither : 1;
	uint32_t billboard_type : 2;
	uint32_t color_overlay : 1;
	uint32_t debug : 1;

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
	virtual void scene_draw(SceneDrawParamsEx params, View_Setup view, UIControl* gui, IEditorTool* tool) override;
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
	virtual handle<Render_Light> register_light(const Render_Light& l) override;
	virtual void update_light(handle<Render_Light> handle, const Render_Light& l) override;
	virtual void remove_light(handle<Render_Light>& handle) override;

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

	void scene_draw_presetup(const SceneDrawParamsEx& params, const View_Setup& view);
	void scene_draw_setup(const SceneDrawParamsEx& params, const View_Setup& view);

	void scene_draw_main();
	void scene_draw_gbuffer_pass();
	void scene_draw_aux_geom_passes();
	void scene_draw_decal_pass();
	void scene_draw_lighting_pass();
	void scene_draw_translucent_pass();

	void scene_draw_post();

	Memory_Arena mem_arena;

	Memory_Arena& get_arena() { return mem_arena; }

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

		fbohandle gbuffer{};	// 4 MRT (gbuffer0-2, scene_color)
		fbohandle forward_render{};	// scene_color, use for translucents
	}fbo;


	struct textures {
		texhandle scene_color{};
		texhandle scene_depthstencil{};

		texhandle scene_gbuffer0{};	
		texhandle scene_gbuffer1{};
		texhandle scene_gbuffer2{};

		// ----------------------------------------------------------------------------------
		// | gbuffer		|		X		|		Y		|		Z		|		A		|
		// ----------------------------------------------------------------------------------
		// | R10,G10,B10	|	NORMAL X	|	NORMAL Y	|	NORMAL Z	|				|
		// | R8G8B8A8		|	albedo R	|	albedo G	|	albedo B	|	AO			|
		// | R8G8B8A8		|	Metallic	|	Roughness	|	Custom		|	MatID		|
		// ----------------------------------------------------------------------------------

		// Emissive outputs to scene color

		// Scene color: RGBA16

		texhandle scene_custom_depthstencil{};
		texhandle editor_selection_buffer{};


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


	Render_Scene scene;

	program_handle get_mat_shader(bool is_animated, bool color_overlay, const Model* mod, const Material* gs, bool depth_pass, bool dither);
	
	Render_Stats stats;

	const View_Setup& get_current_frame_vs()const { return current_frame_main_view; }

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
	void render_lists_old_way(Render_Lists& list, Render_Pass& pass);

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

	// current world time for shaders/fx fed in by SceneParamsEx on draw_scene()
	float current_time = 0.0;
};

extern Renderer draw;