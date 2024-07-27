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
#include "Framework/FreeList.h"
#include "Render/RenderExtra.h"
#include "Render/Material.h"
#include "Framework/MemArena.h"

#include "Framework/MulticastDelegate.h"

#include "Render/RenderScene.h"

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

struct Texture3d
{
	glm::ivec3 size;
	uint32_t id = 0;
};
Texture3d generate_perlin_3d(glm::ivec3 size, uint32_t seed, int octaves, int frequency, float persistence, float lacunarity);

class Render_Lists;
class Render_Pass;
struct Render_Level_Params {

	enum Pass_Type { 
		OPAQUE, 
		TRANSLUCENT, 
		DEPTH, 
		SHADOWMAP 
	};
	Render_Level_Params(
		const View_Setup& view,
		Render_Lists* render_list,
		Render_Pass* render_pass,
		uint32_t output_framebuffer,
		bool clear_framebuffer,
		Pass_Type type
		) : view(view), rl(render_list), rp(render_pass), 
		clear_framebuffer(clear_framebuffer),
		output_framebuffer(output_framebuffer)
	{

	}

	View_Setup view;
	uint32_t output_framebuffer;
	bool clear_framebuffer = true;

	Render_Lists* rl = nullptr;
	Render_Pass* rp = nullptr;

	Pass_Type pass = OPAQUE;
	bool draw_viewmodel = false;
	bool is_probe_render = false;
	bool is_water_reflection_pass = false;

	bool has_clip_plane = false;
	vec4 custom_clip_plane = vec4(0.f);
	bool upload_constants = false;
	uint32_t provied_constant_buffer = 0;
};


// Render lists: represents opengl commands that have been uploaded (or kept CPU side)
//				 these are fed into glMultiDrawElementsIndirect()
//				 these are built around Render_Pass which contains the objects that will be renderered

// Gpu occlusion culling:	

// Reprsents a structure for storing DrawElementsIndirectCommands
// These can be cpu or gpu stored, when using gpu culling, the gpu buffer is culled and used
// This gets fed into "execute_render_lists"


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
		material_id = 0;
		depth_only = 0;
		animated = 0;
		editor_id = 0;
		dither = 0;
		debug = 0;
	}
	uint32_t material_id : 27;
	uint32_t animated : 1;
	uint32_t editor_id : 1;
	uint32_t depth_only : 1;
	uint32_t dither : 1;
	uint32_t debug : 1;

	uint32_t as_uint32() const {
		return *((uint32_t*)this);
	}
};
static_assert(sizeof(shader_key) == 4, "shader key needs 4 bytes");



// opaque materials get one path
// transparent materials get another path
// post process materials get another
// (different output targets, different inputs)

// materials also vary depending on context:
// is it animated?
// is it outputting editor id?

// so step 1: determine WHAT material we have (ie what shader)
//		if this is a depth pass, then get a special shader that is more easily batched
// now we have the shader, the options function like "#ifdefs for the shader" so check if the shader already exists first

// Essentially we want a hashmap of uint64(shader id, shader parameters) maped to a shader object
// to make sorting them easier, each shader object is also assigned a program id the first time its created (thus we can store shader id under, say, 16 bits)
// hashmap<uint64_t, {glShader, uint16}> shaderid_to_shaderobj
// and vector<uint64_t> (maps from uint16 back to shader key)
// if the shader doesnt modify verticies then it can get the uber depth shader, if it does, then it gets its own depth shader but fragment part is simplified
// if its alpha tested, then the depth material gets its own shader
// thus basic opaques can be merged, but anything else cant be merged in the depth pass


class Material_Shader_Table
{
public:
	Material_Shader_Table();

	program_handle lookup(shader_key key); 
	void insert(shader_key key, program_handle handle);

	std::unordered_map<uint32_t, program_handle> shader_key_to_program_handle;
};


class DepthPyramid
{
public:
	void init();
	void free();

	void dispatch_depth_pyramid_creation();

	void on_viewport_size_changed(int x, int y);

	Texture* depth_pyramid = nullptr;
};

class DebuggingTextureOutput
{
public:
	void draw_out();

	Texture* output_tex = nullptr;
	float alpha = 1.0;
	float mip = 0.0;
	float scale = 1.0;
	bool explicit_texel = false;
};


const uint32_t MAX_BLOOM_MIPS = 6;

class Renderer : public RendererPublic
{
public:
	Renderer();

	// local delegates
	MulticastDelegate<int, int> on_viewport_size_changed;	// hook up to change buffers etc.

	// public interface
	virtual void init() override;
	virtual void scene_draw(SceneDrawParamsEx params, View_Setup view, UIControl* gui, IEditorTool* tool) override;
	virtual void on_level_start() override;
	virtual void on_level_end() override;
	virtual void reload_shaders() override;
	virtual RenderScenePublic* get_scene() override { return &scene; }
	virtual void bake_cubemaps() override {}
	virtual uint32_t get_composite_output_texture_handle() override { return tex.output_composite; }

	void check_hardware_options();
	void create_default_textures();

	void render_level_to_target(const Render_Level_Params& params);
	void accumulate_gbuffer_lighting();

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
		program_handle simple{};
		//program_handle textured;
		//program_handle textured3d;
		//program_handle texturedarray;
		program_handle skybox{};

		//program_handle particle_basic;
		program_handle bloom_downsample{};
		program_handle bloom_upsample{};
		program_handle combine{};
		program_handle hbao{};
		program_handle xblur{};
		program_handle yblur{};
		program_handle mdi_testing{};

		// depth pyramid compute shader
		program_handle cCreateDepthPyramid{};

		program_handle tex_debug_2d{};
		program_handle tex_debug_2d_array{};
	}prog;

	struct framebuffers {
		//fbohandle reflected_scene{};
		fbohandle bloom{};
		fbohandle composite{};

		fbohandle gbuffer{};	// 4 MRT (gbuffer0-2, scene_color)
		fbohandle forward_render{};	// scene_color, use for translucents
	}fbo;


	struct textures {
		texhandle scene_color{};
		texhandle scene_depth{};

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


		//texhandle reflected_color{};
		//texhandle reflected_depth{};
		
		texhandle output_composite{};

		texhandle bloom_chain[MAX_BLOOM_MIPS];
		glm::ivec2 bloom_chain_isize[MAX_BLOOM_MIPS];
		glm::vec2 bloom_chain_size[MAX_BLOOM_MIPS];
		uint32_t number_bloom_mips = 0;

		// "virtual texture system" handles, does that even make sense?
		Texture* bloom_vts_handle = nullptr;
		Texture* scene_color_vts_handle = nullptr;
		Texture* scene_depth_vts_handle = nullptr;
		Texture* gbuffer0_vts_handle = nullptr;
		Texture* gbuffer1_vts_handle = nullptr;
		Texture* gbuffer2_vts_handle = nullptr;
	}tex;

	struct uniform_buffers {
		bufferhandle current_frame{};
	}ubo;

	struct buffers {
		bufferhandle default_vb{};
	}buf;

	struct vertex_array_objects {
		vertexarrayhandle default_{};
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

	Texture* lens_dirt = nullptr;

	SSAO_System ssao;
	Shadow_Map_System shadowmap;
	Volumetric_Fog_System volfog;
	DepthPyramid depth_pyramid_maker;
	DebuggingTextureOutput debug_tex_out;

	float slice_3d=0.0;

	Render_Scene scene;

	program_handle get_mat_shader(bool is_animated, const Model* mod, const Material* gs, bool depth_pass, bool dither);
	
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