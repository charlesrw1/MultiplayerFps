#pragma once
// Umbrella header — includes all DrawLocal sub-headers plus the Renderer class.
// All existing `#include "Render/DrawLocal.h"` sites continue to work unchanged.

#include "Render/DrawPublic.h"

#include "Framework/Util.h"
#include "Framework/Config.h"
#include "Framework/MemArena.h"
#include "Framework/MulticastDelegate.h"
#include "Framework/FreeList.h"
#include "Framework/MeshBuilder.h"

// shared types with glsl shaders
#include "../Shaders/SharedGpuTypes.txt"
#include "../Shaders/ShaderBufferShared.txt"

#include "glm/glm.hpp"
#include "User_Camera.h"

#include "Render/IGraphicsDevice.h"
#include "Render/EnvProbe.h"
#include "Render/Texture.h"
#include "Render/DrawTypedefs.h"
#include "Render/RenderExtra.h"
#include "Render/MaterialLocal.h"
#include "Render/RenderScene.h"
#include "RT/RaytraceTest.h"

#include "Framework/ConsoleCmdGroup.h"
#include "Render/PPManager.h"
#include <array>
#include "IGraphicsDevice.h"

#include <span>

// ---------------------------------------------------------------------------
// LOD visibility helpers — used by DrawLocal_Scene.cpp and DrawLocal_CullShadow.cpp
// ---------------------------------------------------------------------------
inline void pack_input_lod_arr(uint8_t& out, bool is_vis, int8_t lod) {
	ASSERT(lod >= 0);
	out = uint8_t(is_vis) | uint8_t(lod << 1);
}
inline void split_input_lod_arr(uint8_t in, bool& is_vis, int8_t& lod) {
	ASSERT(true);
	is_vis = bool(in & 1);
	lod = int8_t(in >> 1);
}

// ---------------------------------------------------------------------------
// Sub-headers
// ---------------------------------------------------------------------------

#include "Render/DrawLocal_Device.h"   // Program_Manager, Render_Stats (RenderPipelineState now in IGraphicsDevice.h)
#include "Render/DrawLocal_Batching.h" // CPU-fast batching, GpuCullInput, BuildSceneData_CpuFast
#include "Render/DrawLocal_Helpers.h"  // Texture3d, Render_Level_Params, DecalBatcher, LightListCuller, etc.

class MeshPart;
class Model;
class Animator;
class Texture;
class Entity;
class RenderWindowBackendLocal;

// ---------------------------------------------------------------------------
// Renderer — main render pipeline
// ---------------------------------------------------------------------------

class Renderer : public RendererPublic
{
public:
	Renderer();

	// local delegates
	MulticastDelegate<int, int> on_viewport_size_changed; // hook up to change buffers etc.
	MulticastDelegate<> on_reload_shaders;				  // called before shaders are reloaded

	// ####################
	// # public interface #
	// ####################
	void init() final;
	void scene_draw(SceneDrawParamsEx params, View_Setup view) final;
	void sync_update() final;
	void on_level_start() final;
	void on_level_end() final;
	void reload_shaders() final;
	RenderScenePublic* get_scene() override { return &scene; }
	void bake_cubemaps() final {}
	uint32_t get_composite_output_texture_handle() final {
		if (!tex.actual_output_composite)
			return 0;
		// assert(tex.actual_output_composite);
		return tex.actual_output_composite->get_internal_handle();
	}
#ifdef EDITOR_BUILD
	handle<Render_Object> mouse_pick_scene_for_editor(int x, int y) final;
	std::vector<handle<Render_Object>> mouse_box_select_for_editor(int x, int y, int w, int h) final;
	float get_scene_depth_for_editor(int x, int y) final;
	void editor_render_thumbnail_for(Model* model, MaterialInstance* override_mat, int w, int h,
									 std::string path) final {
		matman.pre_render_update(); // hack fixme
		thumbnailRenderer->render(model, override_mat);
		thumbnailRenderer->output_to_path(path);
	}
	void editor_set_debug_overlay(const char* tex_name, float scale, float alpha, float mip) final;
	void editor_clear_debug_overlay() final;
	EditorDebugOverlayState editor_get_debug_overlay_state() const final;
#endif
	void pre_sync_update() final { matman.pre_render_update(); }

	// ###################
	// # local interface #
	// ###################

	glm::vec2 get_taa_jitter() const;
	void check_hardware_options();
	void create_default_textures();

	void render_level_to_target(const Render_Level_Params& params);
	void render_particles();

	void accumulate_gbuffer_lighting(bool is_cubemap_view);
	void deferred_decal_pass();
	void draw_editor_ortho_grid(IGraphicsTexture* target);

	void create_shaders();

	void render_lists_old_way(Render_Lists& list, Render_Pass& pass, bool depth_test_enabled, bool force_show_backface,
							  bool depth_less_than_op, float poly_offset_factor = 0.f);
	void execute_render_lists(Render_Lists& lists, Render_Pass& pass, bool depth_test_enabled,
							  bool force_show_backfaces, bool depth_less_than_op,
							  float poly_offset_factor = 0.f);

	void scene_draw_internal(SceneDrawParamsEx params, View_Setup view);
	IGraphicsTexture* do_post_process_stack(const std::vector<MaterialInstance*>& stack);
	void check_cubemaps_dirty(); // render any cubemaps
	void update_cubemap_specular_irradiance(glm::vec3 ambientCube[6], Texture* cubemap, glm::vec3 position,
											bool skybox_only);

	uptr<ConsoleCmdGroup> consoleCommands;
	Memory_Arena mem_arena;

	Memory_Arena& get_arena() { return mem_arena; }

	// default textures
	IGraphicsTexture* white_texture{};
	IGraphicsTexture* black_texture{};
	IGraphicsTexture* flat_normal_texture{};

	struct programs
	{
		program_handle simple{};
		program_handle simple_solid_color{};
		// program_handle textured;
		// program_handle textured3d;
		// program_handle texturedarray;

		// program_handle particle_basic;
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
		program_handle tex_debug_cubemap_array{};

		program_handle sunlight_accumulation{};
		program_handle sunlight_accumulation_debug{};
		program_handle ambient_accumulation{};
		program_handle light_accumulation_fullscreen{};
		program_handle light_accumulation_fullscreen_tiled{};
		program_handle light_accumulation_fullscreen_tiled2{};

		program_handle fullscreen_draw_texture{};

		program_handle height_fog{};
		program_handle volfog_apply{};
		program_handle editor_ortho_grid{};
	} prog;

	struct textures
	{
		IGraphicsTexture* scene_color{};
		IGraphicsTexture* last_scene_color{};
		IGraphicsTexture* scene_depth{};

		IGraphicsTexture* scene_gbuffer0{}; // also used to resolve TAA into since its rgbf16 (and also used as a
											// texture to read from for transparent fx)
		IGraphicsTexture* scene_gbuffer1{};
		IGraphicsTexture* scene_gbuffer2{};

		IGraphicsTexture* scene_motion{};
		IGraphicsTexture* last_scene_motion{};

		// textures for ddgi rendering at half res
		// full res just writes to scene_color
		IGraphicsTexture* halfres_scene_color{};
		IGraphicsTexture* last_ddgi_accum{};
		IGraphicsTexture* ddgi_accum{};
		IGraphicsTexture* reflection_accum{};
		IGraphicsTexture* last_reflection_accum{};
		IGraphicsTexture* scene_color_mipchain{};

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

		// texhandle reflected_color{};
		// texhandle reflected_depth{};

		IGraphicsTexture* output_composite{};
		IGraphicsTexture* output_composite_2{};
		IGraphicsTexture* actual_output_composite{};

		struct BloomChain
		{
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
	} tex;

	struct uniform_buffers
	{
		IGraphicsBuffer* current_frame = nullptr;

		// Per-pass params (Phase 2a manual UBO migration; binding 7 is the
		// "per-pass push-constant analog" slot — see [[rendering/gfx_abstraction#2a]]).
		IGraphicsBuffer* bloom_params = nullptr;
		IGraphicsBuffer* temporal_params = nullptr;  // group C: TAA / DDGI / SSR temporal upsample
		IGraphicsBuffer* cull_params = nullptr;      // group F: cull / MDI compact / depth pyramid / debugCull
		IGraphicsBuffer* ssr_params = nullptr;       // group D: ssr_f / ssr_downsample / ssr_upsample
		IGraphicsBuffer* bake_params = nullptr;      // group J: probe / GI bake (trace + gather + probeLumCalc)
		IGraphicsBuffer* lit_compositor_params = nullptr; // group A: sun + ambient + tiled-light + combine
		IGraphicsBuffer* ddgi_runtime_params = nullptr;   // group E: ddgiShadeF + ddgi_apply_upsampled + reflectionShared
		IGraphicsBuffer* ssao_hbao_params = nullptr;      // group H: linearizedepth + viewnormal + hbao + hbaoblur + hbaodeinterleave
	} ubo;

	struct buffers
	{
		IGraphicsBuffer* default_vb{};

		IGraphicsBuffer* lighting_uniforms{};
		IGraphicsBuffer* decal_uniforms{};
		IGraphicsBuffer* fog_uniforms{};
	} buf;

	struct vertex_array_objects
	{
		IGraphicsVertexInput* default_{};
	} vao;

	IGraphicsVertexInput* get_empty_vao() { return vao.default_; }

	const View_Setup& get_current_frame_vs() const { return current_frame_view; }
	View_Setup current_frame_view;
	View_Setup last_frame_main_view;

	// graphics_settings

	void bind_texture_ptr(int bind, IGraphicsTexture* ptr);
	IGraphicsShader* shader();

	void draw_meshbuilders();

	Texture* lens_dirt = nullptr;

	std::unique_ptr<PPManager> pp_manager;

	SSAO_System ssao;
	CascadeShadowMapSystem shadowmap;
	Volumetric_Fog_System volfog;
	std::unique_ptr<ShadowMapManager> spotShadows;
	std::unique_ptr<DecalBatcher> decalBatcher;
	std::unique_ptr<LightListCuller> lightListCuller;
	std::unique_ptr<DdgiTesting> ddgi;

	DebuggingTextureOutput debug_tex_out;

	Render_Scene scene;

	Render_Stats stats;

	Program_Manager& get_prog_man() { return prog_man; }
	// Transitional shim — the OpenGL state cache used to live on a separate
	// OpenglRenderDevice class; Phase 2c folded it into the IGraphicsDevice
	// backend. Callers prefer gfx() directly; this stays so the migration
	// can land in one commit.
	IGraphicsDevice& get_device() { return gfx(); }

	// Per-frame stats lifecycle. Called from the renderer at the start of a
	// new frame; clears Renderer::stats and tells the backend to forget any
	// cached state (post-imgui, post-compute paths that bypass set_pipeline).
	void on_frame_start();

	bool wants_disable_temporal_effects_this_frame() const { return disable_taa_this_frame; }

private:
	RenderWindowBackendLocal* windowDrawer = nullptr;
#ifdef EDITOR_BUILD
	std::unique_ptr<ThumbnailRenderer> thumbnailRenderer;
#endif

	void upload_ubo_view_constants(const View_Setup& view, IGraphicsBuffer* ubo, bool wireframe_secondpass = false);

	void upload_light_and_decal_buffers();

	void init_bloom_buffers();
	void render_bloom_chain(IGraphicsTexture* scene_color);

	void InitFramebuffers(bool create_composite_texture, int s_w, int s_h);

	void draw_height_fog(IGraphicsTexture* target);

	int cur_w = 0;
	int cur_h = 0;

	Program_Manager prog_man;

	// current world time for shaders/fx fed in by SceneParamsEx on draw_scene()
	float current_time = 0.0;

	bool refresh_render_targets_next_frame = false;
	bool disable_taa_this_frame = false;
};

extern Renderer draw;
