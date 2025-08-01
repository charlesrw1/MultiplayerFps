#pragma once
#include "Render/DrawPublic.h"

#include "Framework/Util.h"
#include "Framework/Config.h"
#include "Framework/MemArena.h"
#include "Framework/MulticastDelegate.h"
#include "Framework/FreeList.h"
#include "Framework/MeshBuilder.h"

// shared types with glsl shaders
#include "../Shaders/SharedGpuTypes.txt"

#include "glm/glm.hpp"
#include "Types.h"


#include "Render/Shader.h"
#include "Render/EnvProbe.h"
#include "Render/Texture.h"
#include "Render/DrawTypedefs.h"
#include "Render/RenderExtra.h"
#include "Render/MaterialLocal.h"
#include "Render/RenderScene.h"
#include "Framework/ConsoleCmdGroup.h"
#include <array>
#include "IGraphsDevice.h"

class MeshPart;
class Model;
class Animator;
class Texture;
class Entity;

extern ConfigVar draw_collision_tris;
extern ConfigVar draw_sv_colliders;
extern ConfigVar draw_viewmodel;
extern ConfigVar enable_vsync;
extern ConfigVar shadow_quality_setting;
extern ConfigVar enable_bloom;
extern ConfigVar enable_volumetric_fog;
extern ConfigVar enable_ssao;
extern ConfigVar use_halfres_reflections;
class RenderWindowBackendLocal;
struct Texture3d
{
	glm::ivec3 size;
	uint32_t id = 0;
};
Texture3d generate_perlin_3d(glm::ivec3 size, uint32_t seed, int octaves, int frequency, float persistence, float lacunarity);


struct Render_Lists;
class Render_Pass;
struct Render_Level_Params {

	enum Pass_Type { 
		OPAQUE, 
		FORWARD_PASS, 
		DEPTH, 
		SHADOWMAP 
	};
	Render_Level_Params(
		const View_Setup& view,
		Render_Lists* render_list,
		Render_Pass* render_pass,
		Pass_Type type
		) : view(view), rl(render_list), rp(render_pass), 

		pass(type)
	{

	}

	View_Setup view;

	float offset_poly_units = 1.1;

	Render_Lists* rl = nullptr;
	Render_Pass* rp = nullptr;

	Pass_Type pass = OPAQUE;
	bool draw_viewmodel = false;
	bool is_probe_render = false;
	bool is_water_reflection_pass = false;

	bool is_wireframe_pass = false;
	bool wireframe_secondpass = false;

	bool upload_constants = false;
	bufferhandle provied_constant_buffer = 0;

	// for cascade shadow map ortho!
	bool wants_non_reverse_z = false;
};

struct RenderPipelineState
{
	RenderPipelineState() = default;
	RenderPipelineState(
		bool backface_culling,
		bool cull_front_face,
		bool depth_testing,
		bool depth_less_than,
		bool depth_writes,
		blend_state blend_state,
		program_handle shader,
		vertexarrayhandle vao,
		fbohandle framebuffer);

	bool backface_culling = true;
	bool cull_front_face = false;
	bool depth_testing = true;
	bool depth_less_than = false;
	bool depth_writes = true;
	blend_state blend = blend_state::OPAQUE;
	program_handle program = 0;
	vertexarrayhandle vao = 0;
};

struct RenderPassSetup
{
	RenderPassSetup(
		const char* debug_name,
		fbohandle framebuffer,
		bool clear_color,
		bool clear_depth,
		int x,
		int y,
		int w,
		int h
	) : debug_name(debug_name), framebuffer(framebuffer), clear_color(clear_color), clear_depth(clear_depth),
		x(x), y(y), w(w), h(h) {}

	const char* debug_name = "";
	fbohandle framebuffer = 0;
	bool clear_color = false;
	bool clear_depth = false;
	float clear_depth_value = 0.0;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

struct GpuRenderPassScope
{
	GpuRenderPassScope& operator=(const GpuRenderPassScope& other) = delete;
	GpuRenderPassScope(const GpuRenderPassScope& other) = default;
private:
	GpuRenderPassScope(const RenderPassSetup& setup) : setup(setup) {};

	const RenderPassSetup setup;
	friend class OpenglRenderDevice;
};
#include "glad/glad.h"

struct Render_Stats {
	int tris_drawn = 0;
	int total_draw_calls = 0;
	int program_changes = 0;
	int texture_binds = 0;
	int vertex_array_changes = 0;
	int blend_changes = 0;
	int framebuffer_changes = 0;
	int framebuffer_clears = 0;

	int shadow_objs = 0;
	int shadow_lights = 0;
};

// this caches programs
class IGraphicsProgram;
class Program_Manager
{
public:

	program_handle create_single_file(const std::string& shared_file, bool is_tesselation = false, const std::string& defines = {});
	program_handle create_raster(const std::string& frag, const std::string& vert, const std::string& defines = {});
	program_handle create_raster_geo(const std::string& frag, const std::string& vert, const std::string& geo = nullptr, const std::string& defines = {});
	program_handle create_compute(const std::string& compute, const std::string& defines = {});
	Shader get_obj(program_handle handle) const {
		assert(handle >= 0 && handle < programs.size());
		return programs[handle].shader_obj;
	}
	bool did_shader_fail(program_handle handle) const {
		assert(handle >= 0 && handle < programs.size());
		return programs.at(handle).compile_failed;
	}


	void recompile_all();

	struct program_def {
		std::string defines;
		std::string frag;
		std::string vert;
		std::string geo;
		bool is_compute = false;
		bool compile_failed = false;
		bool is_tesselation = false;

		bool is_shared() const { return !vert.empty() && frag.empty()&& !is_compute; }
		Shader shader_obj;
	};
	std::vector<program_def> programs;

	void recompile(program_handle handle) {
		if (handle >= 0 && handle < programs.size())
			recompile(programs[handle]);
		else
			sys_print(Warning, "Program_Manager::recompile: handle out of range\n");
	}
private:
	void recompile_shared(program_def& def);
	void recompile(program_def& def);
	void recompile_do(program_def& def);

};

class OpenglRenderDevice
{
public:
	OpenglRenderDevice() {
		memset(textures_bound, 0, sizeof(textures_bound));
	}
	Shader shader() const {
		if (active_program == -1) return Shader();
		return prog_man.get_obj(active_program);
	}
	Program_Manager& get_prog_man() {
		return prog_man;
	}

	void set_pipeline(const RenderPipelineState& pipeline);
	GpuRenderPassScope start_render_pass(const RenderPassSetup& setup);

	void draw_arrays(int mode, int first, int count) {
		activeStats.total_draw_calls++;
		glDrawArrays(mode, first, count);
	}
	void draw_elements_base_vertex(int mode, int count, int type, const void* indicies, int base_vertex) {
		activeStats.total_draw_calls++;
		glDrawElementsBaseVertex(mode, count, type, indicies, base_vertex);
	}
	void multi_draw_elements_indirect(int mode, int type, const void* indirect, int drawcount, int stride) {
		activeStats.total_draw_calls++;
		glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);
	}

	const Render_Stats& get_stats() {
		return lastStats;
	}

	void reset_states() {
		active_program = -1;
		invalidate_all();
	}
	void on_frame_start() {
		lastStats = activeStats;
		activeStats = Render_Stats();
	}

	void set_viewport(int x, int y, int w, int h) {
		glViewport(x, y, w, h);
	}
	void clear_framebuffer(bool clear_depth, bool clear_color, float depth_value = 0.f);

	void bind_texture(int bind, int id);
	void bind_texture_ptr(int bind, IGraphicsTexture* ptr) {
		if(ptr)
			bind_texture(bind, ptr->get_internal_handle());
	}
	void set_shader(program_handle handle);
	void set_depth_write_enabled(bool enabled);
private:
	void set_vao(vertexarrayhandle vao);
	void set_blend_state(blend_state blend);
	void set_show_backfaces(bool show_backfaces);
	void set_depth_test_enabled(bool enabled);
	void set_cull_front_face(bool enabled);
	void set_depth_less_than(bool enable_less_than);
	void end_render_pass(GpuRenderPassScope& scope) {
		assert(in_render_pass);
		in_render_pass = false;
	}
	friend struct GpuRenderPassScope;

	static const int MAX_SAMPLER_BINDINGS = 32;
	program_handle active_program = -1;
	texhandle textures_bound[MAX_SAMPLER_BINDINGS];
	blend_state blending = blend_state::OPAQUE;
	bool show_backface = false;
	bool depth_test_enabled = true;
	bool depth_write_enabled = true;
	bool depth_less_than_enabled = true;
	bool cullfrontface = false;
	fbohandle current_framebuffer = 0;
	vertexarrayhandle current_vao = 0;

	bool in_render_pass = false;

	int framebuffer_changes = 0;
	Render_Stats activeStats;
	Render_Stats lastStats;

	enum invalid_bits {
		PROGRAM_BIT,
		BLENDING_BIT,
		BACKFACE_BIT,
		CULLFRONTFACE_BIT,
		DEPTHTEST_BIT,
		DEPTHWRITE_BIT,
		DEPTHLESS_THAN_BIT,
		VAO_BIT,
		FBO_BIT,
		TEXTURE0_BIT,
	};

	bool is_bit_invalid(uint32_t bit) { return invalid_bits & (1ull << bit); }
	void set_bit_valid(uint32_t bit) { invalid_bits &= ~(1ull << bit); }
	void set_bit_invalid(uint32_t bit) { invalid_bits |= (1ull << bit); }
	void invalidate_all() { invalid_bits = UINT64_MAX; }

	Program_Manager prog_man;
private:
	uint64_t invalid_bits = UINT32_MAX;
	friend class Renderer;
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
#ifdef EDITOR_BUILD
class ThumbnailRenderer {
public:
	ThumbnailRenderer(int size);
	void render(Model* m);
	void output_to_path(std::string path);
private:

	IGraphicsTexture* color{};
	IGraphicsTexture* depth{};


	Texture* vts_handle = nullptr;
	handle<Render_Object> object;
	Render_Pass pass;
	Render_Lists list;
	int size = 0;
};
#endif

class DecalBatcher
{
public:
	DecalBatcher();
	void build_batches();
	void draw_decals();
private:
	struct DecalObj {
		int orig_index = 0;
		int program = 0;
		int texture_set = 0;
		int sort_order = 0;

		MaterialInstance* the_material = nullptr;
	};
	struct DecalDraw {
		int count = 0;
		MaterialInstance* shared_pipeline_material = nullptr;
		program_handle the_program_to_use = 0;
	};

	std::vector<DecalDraw> draws;
	IGraphicsBuffer* multidraw_commands = nullptr;
	IGraphicsBuffer* indirection_buffer = nullptr;
};


class Renderer : public RendererPublic
{
public:
	Renderer();

	// local delegates
	MulticastDelegate<int, int> on_viewport_size_changed;	// hook up to change buffers etc.
	MulticastDelegate<> on_reload_shaders;	// called before shaders are reloaded


	// public interface
	void init() final;
	void scene_draw(SceneDrawParamsEx params, View_Setup view) final;
	void sync_update() final;
	void on_level_start() final;
	void on_level_end() final;
	void reload_shaders() final;
	RenderScenePublic* get_scene() override { return &scene; }
	void bake_cubemaps() final {}
	uint32_t get_composite_output_texture_handle() final {
		if (!tex.actual_output_composite) return 0;
		//assert(tex.actual_output_composite);
		return tex.actual_output_composite->get_internal_handle(); 
	}
#ifdef EDITOR_BUILD
	handle<Render_Object> mouse_pick_scene_for_editor(int x, int y) final;
	std::vector<handle<Render_Object>> mouse_box_select_for_editor(int x, int y, int w, int h) final;
	float get_scene_depth_for_editor(int x, int y) final;
	void editor_render_thumbnail_for(Model* model, int w, int h, std::string path) final {
		matman.pre_render_update();	// hack fixme
		thumbnailRenderer->render(model);
		thumbnailRenderer->output_to_path(path);
	}
#endif

	void check_hardware_options();
	void create_default_textures();

	void render_level_to_target(const Render_Level_Params& params);
	void render_particles();

	void accumulate_gbuffer_lighting(bool is_cubemap_view);
	void deferred_decal_pass();

	void create_shaders();

	void render_lists_old_way(Render_Lists& list, Render_Pass& pass, bool depth_test_enabled, bool force_show_backface, bool depth_less_than_op);
	void execute_render_lists(Render_Lists& lists, Render_Pass& pass, 
		bool depth_test_enabled,
		bool force_show_backfaces,
		bool depth_less_than_op
	);

	void scene_draw_internal(SceneDrawParamsEx params, View_Setup view);
	IGraphicsTexture* do_post_process_stack(const std::vector<MaterialInstance*>& stack);
	void check_cubemaps_dirty();	// render any cubemaps
	void update_cubemap_specular_irradiance(glm::vec3 ambientCube[6], Texture* cubemap, glm::vec3 position, bool skybox_only);

	uptr<ConsoleCmdGroup> consoleCommands;
	Memory_Arena mem_arena;

	Memory_Arena& get_arena() { return mem_arena; }
	
	// default textuers
	IGraphicsTexture* white_texture{};
	IGraphicsTexture* black_texture{};
	IGraphicsTexture* flat_normal_texture{};

	struct programs
	{
		program_handle simple{};
		program_handle simple_solid_color{};
		//program_handle textured;
		//program_handle textured3d;
		//program_handle texturedarray;

		//program_handle particle_basic;
		program_handle bloom_downsample{};
		program_handle taa_resolve{};
		program_handle bloom_upsample{};
		program_handle combine{};
		
		program_handle mdi_testing{};

		// depth pyramid compute shader
		program_handle cCreateDepthPyramid{};

		program_handle tex_debug_2d{};
		program_handle tex_debug_2d_array{};
		program_handle tex_debug_cubemap{};

		program_handle light_accumulation_shadow_cookie{};
		program_handle light_accumulation_shadowed{};
		program_handle light_accumulation{};
		program_handle sunlight_accumulation{};
		program_handle sunlight_accumulation_debug{};
		program_handle ambient_accumulation{};
		program_handle reflection_accumulation{};

		program_handle height_fog{};
	}prog;

	
	struct textures {
		IGraphicsTexture* scene_color{};
		IGraphicsTexture* last_scene_color{};
		IGraphicsTexture* scene_depth{};

		IGraphicsTexture* scene_gbuffer0{};	// also used to resolve TAA into since its rgbf16 (and also used as a texture to read from for transparent fx)
		IGraphicsTexture* scene_gbuffer1{};
		IGraphicsTexture* scene_gbuffer2{};

		IGraphicsTexture* scene_motion{};
		IGraphicsTexture* last_scene_motion{};



		// ----------------------------------------------------------------------------------
		// | gbuffer		|		X		|		Y		|		Z		|		A		|
		// ----------------------------------------------------------------------------------
		// | RGB16F			|	NORMAL X	|	NORMAL Y	|	NORMAL Z	|				|
		// | R8G8B8A8		|	albedo R	|	albedo G	|	albedo B	|	AO			|
		// | R8G8B8A8		|	Metallic	|	Roughness	|	Custom		|	MatID		|
		// ----------------------------------------------------------------------------------

		// Emissive outputs to scene color
		// Scene color: RGBA16
		// Storing normals in rgb16f, can/should optimize this down later

		IGraphicsTexture* scene_custom_depthstencil{};
		IGraphicsTexture* editor_selection_depth_buffer{};
		IGraphicsTexture* editor_id_buffer{};


		//texhandle reflected_color{};
		//texhandle reflected_depth{};
		
		IGraphicsTexture* output_composite{};
		IGraphicsTexture* output_composite_2{};
		IGraphicsTexture* actual_output_composite{};

		struct BloomChain {
			IGraphicsTexture* texture = nullptr;
			glm::ivec2 isize = {};
			glm::vec2 fsize = glm::vec2(0.0);
		};
		std::array<BloomChain, MAX_BLOOM_MIPS> bloom_chain = {};
		int number_bloom_mips = 0;

		// "virtual texture system" handles, does that even make sense?
		Texture* bloom_vts_handle = nullptr;
		Texture* scene_color_vts_handle = nullptr;
		Texture* scene_depth_vts_handle = nullptr;
		Texture* gbuffer0_vts_handle = nullptr;
		Texture* gbuffer1_vts_handle = nullptr;
		Texture* gbuffer2_vts_handle = nullptr;
		Texture* editorid_vts_handle = nullptr;
		Texture* editorSel_vts_handle = nullptr;
		Texture* postProcessInput_vts_handle = nullptr;
		Texture* scene_motion_vts_handle = nullptr;

		Texture* read_scene_color_for_transparents_handle = nullptr;
	}tex;

	struct uniform_buffers {
		bufferhandle current_frame{};
	}ubo;

	struct buffers {
		bufferhandle default_vb{};

		IGraphicsBuffer* lighting_uniforms{};
		IGraphicsBuffer* decal_uniforms{};
		IGraphicsBuffer* fog_uniforms{};
	}buf;

	struct vertex_array_objects {
		vertexarrayhandle default_{};
	}vao;

	vertexarrayhandle get_empty_vao() {
		return vao.default_;
	}
	
	const View_Setup& get_current_frame_vs() const { return current_frame_view; }
	View_Setup current_frame_view;
	View_Setup last_frame_main_view;

	// graphics_settings

	void bind_vao(uint32_t vao);
	void bind_texture(int bind, int id);
	void bind_texture_ptr(int bind, IGraphicsTexture* ptr) {
		if(ptr)
			bind_texture(bind, ptr->get_internal_handle());
	}

	void set_shader(program_handle handle);
	void set_blend_state(blend_state blend);
	void set_show_backfaces(bool show_backfaces);
	Shader shader();

	void draw_meshbuilders();

	Texture* lens_dirt = nullptr;

	SSAO_System ssao;
	CascadeShadowMapSystem shadowmap;
	Volumetric_Fog_System volfog;
	std::unique_ptr<ShadowMapManager> spotShadows;
	std::unique_ptr<DecalBatcher> decalBatcher;

	DebuggingTextureOutput debug_tex_out;

	Render_Scene scene;
	
	Render_Stats stats;


	OpenglRenderDevice& get_device() {
		return device;
	}
	Program_Manager& get_prog_man() {
		return device.get_prog_man();
	}
private:
	RenderWindowBackendLocal* windowDrawer = nullptr;
#ifdef EDITOR_BUILD
	std::unique_ptr<ThumbnailRenderer> thumbnailRenderer;
#endif

	void upload_ubo_view_constants(const View_Setup& view, bufferhandle ubo, bool wireframe_secondpass = false);

	void upload_light_and_decal_buffers();

	void init_bloom_buffers();
	void render_bloom_chain(texhandle scene_color_handle);



	void InitGlState();
	void InitFramebuffers(bool create_composite_texture, int s_w, int s_h);

	void draw_height_fog();


	int cur_w = 0;
	int cur_h = 0;

	OpenglRenderDevice device;

	// current world time for shaders/fx fed in by SceneParamsEx on draw_scene()
	float current_time = 0.0;

	bool refresh_render_targets_next_frame = false;
	bool disable_taa_this_frame = false;
};

extern Renderer draw;