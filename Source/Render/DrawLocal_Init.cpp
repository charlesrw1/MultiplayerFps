#include "DrawLocal.h"
#include "Framework/Util.h"
#include "glad/glad.h"
#include "Render/Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include <SDL2/SDL.h>
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "tracy/public/tracy/Tracy.hpp"
#include <tracy/public/tracy/TracyOpenGL.hpp>
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"
#include <algorithm>

void Renderer::create_shaders() {
	ssao.reload_shaders();

	auto& prog_man = get_prog_man();

	prog.simple = prog_man.create_raster("MbSimpleV.txt", "MbSimpleF.txt");
	prog.simple_solid_color = prog_man.create_raster("MbSimpleV.txt", "MbSimpleF.txt", "USE_SOLID_COLOR");

	prog.tex_debug_2d = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_2D_VERSION");
	prog.tex_debug_2d_array = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_2D_ARRAY_VERSION");
	prog.tex_debug_cubemap = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_CUBEMAP_VERSION");
	prog.tex_debug_cubemap_array =
		prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_CUBEMAP_ARRAY_VERSION");

	// Bloom shaders
	prog.bloom_downsample = prog_man.create_raster("fullscreenquad.txt", "BloomDownsampleF.txt");
	prog.bloom_upsample = prog_man.create_raster("fullscreenquad.txt", "BloomUpsampleF.txt");
	prog.combine = prog_man.create_raster("fullscreenquad.txt", "CombineF.txt");
	prog.taa_resolve = prog_man.create_raster("fullscreenquad.txt", "TaaResolveF.txt");

	prog.mdi_testing = prog_man.create_raster("SimpleMeshV.txt", "UnlitF.txt", "MDI");

	prog.fullscreen_draw_texture = prog_man.create_raster("fullscreenquad.txt", "fullscreen_quad_textureF.txt");

	prog.light_accumulation_fullscreen =
		prog_man.create_raster("fullscreenquad.txt", "LightAccumulationFullScreen.txt", "SHADOWED");
	prog.light_accumulation_fullscreen_tiled =
		prog_man.create_raster("fullscreenquad.txt", "LightAccumulationFullScreen.txt", "SHADOWED,TILED 1");
	prog.light_accumulation_fullscreen_tiled2 =
		prog_man.create_raster("fullscreenquad.txt", "LightAccumulationFullScreen.txt", "SHADOWED,TILED 2");

	prog.sunlight_accumulation = prog_man.create_raster("fullscreenquad.txt", "SunLightAccumulationF.txt");
	prog.sunlight_accumulation_debug =
		prog_man.create_raster("fullscreenquad.txt", "SunLightAccumulationF.txt", "DEBUG");

	prog.ambient_accumulation = prog_man.create_raster("fullscreenquad.txt", "AmbientLightingF.txt");
	prog.reflection_accumulation = prog_man.create_raster("fullscreenquad.txt", "SampleCubemapsF.txt");

	prog.height_fog = prog_man.create_raster("fullscreenquad.txt", "HeightFogF.txt");
	prog.volfog_apply = prog_man.create_raster("fullscreenquad.txt", "VolfogApplyF.txt");

	// prog_man.create_single_file()
	// volumetric fog shaders
	volfog.prog.lightcalc = prog_man.create_compute("VfogScatteringC.txt");
	volfog.prog.raymarch = prog_man.create_compute("VfogRaymarchC.txt");

	glUseProgram(0);
}

void Renderer::reload_shaders() {
	assert(0);
	on_reload_shaders.invoke();

	ssao.reload_shaders();
	// prog_man.recompile_all();
}

void Renderer::upload_ubo_view_constants(const View_Setup& view_to_use, IGraphicsBuffer* ubo, bool wireframe_secondpass) {
	gpu::Ubo_View_Constants_Struct constants;
	auto& vs = view_to_use;
	constants.view = vs.view;
	constants.viewproj = vs.viewproj;
	constants.invview = glm::inverse(vs.view);
	constants.invproj = glm::inverse(vs.proj);
	constants.inv_viewproj = glm::inverse(vs.viewproj);
	constants.viewpos_time = glm::vec4(vs.origin, current_time);
	constants.viewfront = glm::vec4(vs.front, 0.0);
	constants.viewport_size = glm::vec4(vs.width, vs.height, 0, 0);
	constants.prev_viewproj = last_frame_main_view.viewproj;
	constants.near = vs.near;
	constants.far = vs.far;
	constants.shadowmap_epsilon = shadowmap.tweak.epsilon;
	constants.inv_scale_by_proj_distance = 1.0 / (2.0 * tan(vs.fov * 0.5));

	constants.fogcolor = vec4(vec3(0.7), 1);
	constants.fogparams = vec4(10, 30, 0, 0);

	const int num_lights = scene.light_list.objects.size();
	constants.numlights = num_lights;
	constants.numcubemaps = RenderGiManager::inst->get_num_cubemaps();

	constants.forcecubemap = -1.0;

	auto cur_jit = r_taa_manager.calc_frame_jitter(cur_w, cur_h);
	auto prev_jit = r_taa_manager.get_last_frame_jitter(cur_w, cur_h);
	if (r_taa_jitter_test.get_integer() == 1) {
		cur_jit *= -1;
	} else if (r_taa_jitter_test.get_integer() == 2) {
		cur_jit *= -1;
		prev_jit *= -1;
	} else if (r_taa_jitter_test.get_integer() == 3) {
		prev_jit *= -1;
	}

	constants.current_and_prev_jitter = glm::vec4(cur_jit.x, cur_jit.y, prev_jit.x, prev_jit.y);

	constants.debug_options = r_debug_mode.get_integer();
	if (r_debug_mode.get_integer() == DEBUG_OUTLINED)
		constants.debug_options = gpu::DEBUG_OBJID;

	constants.flags = 0;
	if (wireframe_secondpass)
		constants.flags |= (1 << 0);
	if (r_normal_shaded_debug.get_bool()) {
		constants.flags |= (1 << 1);
	}

	ASSERT(ubo != nullptr);
	ubo->sub_upload(&constants, sizeof(gpu::Ubo_View_Constants_Struct), 0);
}

Renderer::Renderer() {}

void debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
							GLchar const* message, void const* user_param) {
	auto const src_str = [source]() {
		switch (source) {
		case GL_DEBUG_SOURCE_API:
			return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
			return "WINDOW SYSTEM";
		case GL_DEBUG_SOURCE_SHADER_COMPILER:
			return "SHADER COMPILER";
		case GL_DEBUG_SOURCE_THIRD_PARTY:
			return "THIRD PARTY";
		case GL_DEBUG_SOURCE_APPLICATION:
			return "APPLICATION";
		case GL_DEBUG_SOURCE_OTHER:
			return "OTHER";
		}
		return "";
	}();

	auto const type_str = [type]() {
		switch (type) {
		case GL_DEBUG_TYPE_ERROR:
			return "ERROR";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			return "DEPRECATED_BEHAVIOR";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			return "UNDEFINED_BEHAVIOR";
		case GL_DEBUG_TYPE_PORTABILITY:
			return "PORTABILITY";
		case GL_DEBUG_TYPE_PERFORMANCE:
			return "PERFORMANCE";
		case GL_DEBUG_TYPE_MARKER:
			return "MARKER";
		case GL_DEBUG_TYPE_OTHER:
			return "OTHER";
		}
		return "";
	}();

	auto const severity_str = [severity]() {
		switch (severity) {
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			return "NOTIFICATION";
		case GL_DEBUG_SEVERITY_LOW:
			return "LOW";
		case GL_DEBUG_SEVERITY_MEDIUM:
			return "MEDIUM";
		case GL_DEBUG_SEVERITY_HIGH:
			return "HIGH";
		}
		return "";
	}();

	sys_print(Error, "%s, %s, %s, %d: %s\n", src_str, type_str, severity_str, id, message);
}

void imgui_stat_hook() {
	auto& stats = draw.stats;
	ImGui::Text("Draw calls: %d", stats.total_draw_calls);
	ImGui::Text("Total tris: %d", stats.tris_drawn);
	ImGui::Text("Texture binds: %d", stats.texture_binds);
	ImGui::Text("Shader binds: %d", stats.program_changes);
	ImGui::Text("Vao binds: %d", stats.vertex_array_changes);
	ImGui::Text("Blend changes: %d", stats.blend_changes);
	ImGui::Separator();
	ImGui::Text("shadow objs: %d", stats.shadow_objs);
	ImGui::Text("shadow lights: %d", stats.shadow_lights);
	ImGui::Separator();
	auto& scene = draw.scene;
	auto cf = BuildSceneData_CpuFast::inst;
	ImGui::Text("depth batches: %d", (int)cf->get_num_depth_batches());
	ImGui::Text("opaque batches: %d", (int)cf->get_num_opaque_batches());
	ImGui::Text("cached model cmds: %d", (int)cf->get_num_cached_cmds());
	ImGui::Text("cached model/mats: %d", (int)cf->get_num_cached_mod_mats());
	ImGui::Text("transparent batches: %d", (int)scene.transparent_pass.batches.size());

	ImGui::Separator();
	ImGui::Text("total objects: %d", (int)scene.proxy_list.objects.size());
	ImGui::Text("total lights: %d", (int)scene.light_list.objects.size());
	ImGui::Text("total decals: %d", (int)scene.decal_list.objects.size());
	ImGui::Text("total meshbuilders: %d", (int)scene.meshbuilder_objs.objects.size());
	ImGui::Separator();
}

glm::vec2 Renderer::get_taa_jitter() const {
	return r_taa_manager.calc_frame_jitter(current_frame_view.width, current_frame_view.height);
}

void Renderer::check_hardware_options() {
	bool supports_compression = false;
	bool supports_sprase_tex = false;
	bool supports_filter_minmax = false;
	bool supports_atomic64 = false;
	bool supports_int64 = false;

	int num_extensions = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
	for (int i = 0; i < num_extensions; i++) {
		const char* ext = (char*)glGetStringi(GL_EXTENSIONS, i);
		if (strcmp(ext, "GL_ARB_sparse_texture") == 0)
			supports_sprase_tex = true;
		else if (strcmp(ext, "GL_EXT_texture_compression_s3tc") == 0)
			supports_compression = true;
		else if (strcmp(ext, "GL_ARB_texture_filter_minmax") == 0)
			supports_filter_minmax = true;
		else if (strcmp(ext, "GL_NV_shader_atomic_int64") == 0)
			supports_atomic64 = true;
		else if (strcmp(ext, "GL_ARB_gpu_shader_int64") == 0)
			supports_int64 = true;
	}

	sys_print(Debug, "###########################\n");
	sys_print(Debug, "#### Extension support ####\n");
	sys_print(Debug, "###########################\n");
	sys_print(Debug, "-GL_ARB_sparse_texture: %s\n", (supports_sprase_tex) ? "yes" : "no");
	sys_print(Debug, "-GL_ARB_texture_filter_minmax: %s\n", (supports_filter_minmax) ? "yes" : "no");
	sys_print(Debug, "-GL_EXT_texture_compression_s3tc: %s\n", (supports_compression) ? "yes" : "no");
	sys_print(Debug, "-GL_NV_shader_atomic_int64: %s\n", (supports_atomic64) ? "yes" : "no");
	sys_print(Debug, "-GL_ARB_gpu_shader_int64: %s\n", (supports_int64) ? "yes" : "no");

	if (!supports_compression) {
		Fatalf("Opengl driver needs GL_EXT_texture_compression_s3tc\n");
	}

	GLint binary_formats;
	glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &binary_formats);
	if (binary_formats == 0) {
		Fatalf("Opengl driver must support program binary. (GL_NUM_PROGRAM_BINARY_FORMATS>0)\n");
	}

	sys_print(Debug, "############################\n");
	sys_print(Debug, "#### GL Hardware Values ####\n");
	sys_print(Debug, "############################\n");
	int max_buffer_bindings = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_buffer_bindings);
	sys_print(Debug, "-GL_MAX_UNIFORM_BUFFER_BINDINGS: %d\n", max_buffer_bindings);
	int max_texture_units = 0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
	sys_print(Debug, "-GL_MAX_TEXTURE_IMAGE_UNITS: %d\n", max_texture_units);
	sys_print(Debug, "-GL_NUM_PROGRAM_BINARY_FORMATS: %d\n", binary_formats);
	int max_ssbos = 0;
	glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &max_ssbos);
	sys_print(Debug, "-GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS: %d\n", max_ssbos);
	glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &max_ssbos);
	sys_print(Debug, "-GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS: %d\n", max_ssbos);
	glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &max_ssbos);
	sys_print(Debug, "-GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS: %d\n", max_ssbos);
	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_ssbos);
	sys_print(Debug, "-GL_MAX_ARRAY_TEXTURE_LAYERS: %d\n", max_ssbos);
	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_ssbos);
	sys_print(Debug, "-GL_MAX_COLOR_ATTACHMENTS: %d\n", max_ssbos);

	sys_print(Debug, "\n");
}

void Renderer::create_default_textures() {
	const uint8_t wdata[] = {0xff, 0xff, 0xff, 255};
	const uint8_t bdata[] = {0x0, 0x0, 0x0, 255};
	const uint8_t normaldata[] = {128, 128, 255, 255};

	auto create_defeault = [](IGraphicsTexture*& handle, const uint8_t* data) -> void {
		CreateTextureArgs args;
		args.width = args.height = 1;
		args.num_mip_maps = 1;
		args.sampler_type = GraphicsSamplerType::LinearDefault;
		args.format = GraphicsTextureFormat::rgba8;
		handle = gfx().create_texture(args);
		handle->sub_image_upload(0, 0, 0, 1, 1, sizeof(uint8_t) * 4, data);
	};
	create_defeault(white_texture, wdata);
	create_defeault(black_texture, bdata);
	create_defeault(flat_normal_texture, normaldata);

	auto white_tex = Texture::install_system("_white");
	auto black_tex = Texture::install_system("_black");
#ifdef EDITOR_BUILD
	white_tex->hasSimplifiedColor = true;
	white_tex->simplifiedColor = COLOR_WHITE;
	black_tex->hasSimplifiedColor = true;
	black_tex->simplifiedColor = COLOR_BLACK;
#endif
	auto flat_normal = Texture::install_system("_flat_normal");

	white_tex->update_specs_ptr(white_texture);
	black_tex->update_specs_ptr(black_texture);
	flat_normal->update_specs_ptr(flat_normal_texture);

	// create the "virtual texture system" handles so materials/debuging can reference these like a normal texture
	tex.bloom_vts_handle = Texture::install_system("_bloom_result");
	tex.scene_color_vts_handle = Texture::install_system("_scene_color");
	tex.scene_depth_vts_handle = Texture::install_system("_scene_depth");
	tex.gbuffer0_vts_handle = Texture::install_system("_gbuffer0");
	tex.gbuffer1_vts_handle = Texture::install_system("_gbuffer1");
	tex.gbuffer2_vts_handle = Texture::install_system("_gbuffer2");
	tex.editorid_vts_handle = Texture::install_system("_editorid");
	tex.editorSel_vts_handle = Texture::install_system("_editorSelDepth");
	tex.postProcessInput_vts_handle = Texture::install_system("_PostProcessInput");
	tex.scene_motion_vts_handle = Texture::install_system("_scene_motion");
	Texture::install_system("_halfres_scene_color");
	Texture::install_system("_ddgi_accum");
	Texture::install_system("_ddgi_accum_prev");
	Texture::install_system("_ssr");

	tex.read_scene_color_for_transparents_handle = Texture::install_system("_read_scene_color");
}

extern int total_gfx_mem_usage;
void Renderer::init() {
	sys_print(Info, "--------- Initializing Renderer ---------\n");

	double start = GetTime();
	auto print_time = [&](const char* msg) {
		double now = GetTime();
		// printf("-----TIME %s %f\n", msg, float(now - start));
		sys_print(Debug, "init % s in % fs\n", msg, float(now - start));
		start = now;
	};

	// gfx_init_opengl(window) is called by the engine right after SDL_CreateWindow
	// (init_sdl_window in EngineMain_Init.cpp) — by the time the renderer comes
	// up the device is already live.
	ASSERT(gfx_is_initialized());

	print_time("draw:device");

	// Check hardware settings like extension availibility
	check_hardware_options();

	// Enable debug output on debug builds
	if (enable_gl_debug_output.get_bool()) {
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(debug_message_callback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	}

	InitGlState();

	print_time("draw:init_state");

	BuildSceneData_CpuFast::inst = new BuildSceneData_CpuFast;
	windowDrawer = new RenderWindowBackendLocal();
	RenderWindowBackend::inst = windowDrawer;

	mem_arena.init("RenderTemp", renderer_memory_arena_size.get_integer());
	// Init scene draw buffers
	scene.init();
	print_time("draw:arena");

	create_shaders();
	print_time("draw:create_shaders");

	create_default_textures();
	{
		CreateBufferArgs args;
		args.size = sizeof(gpu::Ubo_View_Constants_Struct);
		args.flags = BUFFER_USE_DYNAMIC;
		ubo.current_frame = gfx().create_buffer(args);
	}
	InitFramebuffers(true, g_window_w.get_integer(), g_window_h.get_integer());
	print_time("draw:buffers");

	ASSERT(!RenderGiManager::inst);
	RenderGiManager::inst = new RenderGiManager;
	GpuCullingTest::inst = new GpuCullingTest;
	LightCookieAtlas::inst = new LightCookieAtlas;
	SSRSystem::inst = new SSRSystem;

	EnviornmentMapHelper::get().init();
	print_time("draw:env_map");

	volfog.init();
	shadowmap.init();
	ssao.init();
	print_time("draw:miscinit");
	// lens_dirt = &whg_assets.find<Texture>("eng/lens_dirt_fine.png").get();
	lens_dirt = Texture::load("_white");
	print_time("draw:lensdirt");

	glGenVertexArrays(1, &vao.default_);
	glCreateBuffers(1, &buf.default_vb);
	glNamedBufferStorage(buf.default_vb, 12 * 3, nullptr, 0);
	glBindVertexArray(vao.default_);
	glBindBuffer(GL_ARRAY_BUFFER, buf.default_vb);
	glBindVertexArray(0);

	auto create_uniform_buffer = [&](IGraphicsBuffer*& ptr) {
		CreateBufferArgs args;
		args.flags = BUFFER_USE_DYNAMIC;
		ptr = gfx().create_buffer(args);
	};
	create_uniform_buffer(buf.lighting_uniforms);
	create_uniform_buffer(buf.decal_uniforms);
	create_uniform_buffer(buf.fog_uniforms);

	on_level_start();
	Debug_Interface::get()->add_hook("Render stats", imgui_stat_hook);
	// auto brdf_lut = Texture::install_system("_brdf_lut");
	// brdf_lut->gl_id = EnviornmentMapHelper::get().integrator.lut_id;
	// brdf_lut->width = EnviornmentMapHelper::BRDF_PREINTEGRATE_LUT_SIZE;
	// brdf_lut->height = EnviornmentMapHelper::BRDF_PREINTEGRATE_LUT_SIZE;
	// brdf_lut->type = Texture_Type::TEXTYPE_2D;
	// FIXME
	consoleCommands = ConsoleCmdGroup::create("");
	consoleCommands->add("print_gfx_mem", [](const Cmd_Args&) { sys_print(Info, "%d\n", total_gfx_mem_usage); });
	consoleCommands->add("cot", [this](const Cmd_Args& args) { debug_tex_out.output_tex = nullptr; });
	consoleCommands->add("ot", [this](const Cmd_Args& args) {
		static const char* usage_str = "Usage: ot <scale:float> <alpha:float> <mip/slice:float> <texture_name>\n";
		if (args.size() != 2) {
			sys_print(Info, usage_str);
			return;
		}
		const char* texture_name = args.at(1);

		debug_tex_out.output_tex = g_assets.find<Texture>(texture_name).get();
		debug_tex_out.scale = 1.f;
		debug_tex_out.alpha = 1.f;
		debug_tex_out.mip = 1.f;

		if (!debug_tex_out.output_tex) {
			sys_print(Error, "output_texture: couldn't find texture %s\n", texture_name);
		}
	});
	consoleCommands->add("test_mode", [this](const Cmd_Args& args) {
		if (args.size() != 2)
			return;
		int i = atoi(args.at(1));
		if (i == 0) {
			dont_use_mdi.set_bool(false);
			r_no_batching.set_bool(false);
			r_better_depth_batching.set_bool(true);
			r_debug_skip_build_scene_data.set_bool(false);
		}
		if (i == 1) {
			dont_use_mdi.set_bool(true);
			r_no_batching.set_bool(true);
			r_better_depth_batching.set_bool(true);
			r_debug_skip_build_scene_data.set_bool(false);
		}
		if (i == 2) {
			dont_use_mdi.set_bool(true);
			r_no_batching.set_bool(true);
			r_better_depth_batching.set_bool(true);
			r_debug_skip_build_scene_data.set_bool(true);
		}
	});
	consoleCommands->add("otex", [this](const Cmd_Args& args) {
		static const char* usage_str = "Usage: otex <scale:float> <alpha:float> <mip/slice:float> <texture_name>\n";
		if (args.size() != 5) {
			sys_print(Info, usage_str);
			return;
		}
		float scale = atof(args.at(1));
		float alpha = atof(args.at(2));
		float mip = atof(args.at(3));
		const char* texture_name = args.at(4);
		debug_tex_out.output_tex = g_assets.find<Texture>(texture_name).get();
		debug_tex_out.scale = scale;
		debug_tex_out.alpha = alpha;
		debug_tex_out.mip = mip;
		if (!debug_tex_out.output_tex) {
			sys_print(Error, "output_texture: couldn't find texture %s\n", texture_name);
		}
	});

	spotShadows = std::make_unique<ShadowMapManager>();
	decalBatcher = std::make_unique<DecalBatcher>();
	lightListCuller = std::make_unique<LightListCuller>();
	ddgi = std::make_unique<DdgiTesting>();
#ifdef EDITOR_BUILD
	thumbnailRenderer = std::make_unique<ThumbnailRenderer>(128);
#endif
	print_time("draw:objects");

	consoleCommands->add("build-ddgi", [this](const Cmd_Args& args) { ddgi->execute(); });
}

void Renderer::InitFramebuffers(bool create_composite_texture, int s_w, int s_h) {
	s_w = std::min(s_w, 4000);
	s_h = std::min(s_h, 4000);

	refresh_render_targets_next_frame = false;
	disable_taa_this_frame = true;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	auto delete_and_create_texture = [&](IGraphicsTexture*& ptr, GraphicsTextureFormat format) {
		safe_release(ptr);

		CreateTextureArgs args;
		args.format = format;
		args.num_mip_maps = 1;
		args.width = s_w;
		args.height = s_h;
		args.sampler_type = GraphicsSamplerType::NearestClamped;
		ptr = gfx().create_texture(args);
	};
	auto delete_and_create_texture_halfresmips = [&](IGraphicsTexture*& ptr, GraphicsTextureFormat format,
													 int num_mips) {
		safe_release(ptr);

		CreateTextureArgs args;
		args.format = format;
		args.num_mip_maps = num_mips;
		args.width = s_w / 2;
		args.height = s_h / 2;
		args.sampler_type = GraphicsSamplerType::LinearDefault;
		ptr = gfx().create_texture(args);
	};

	auto delete_and_create_texture_half_res = [&](IGraphicsTexture*& ptr, GraphicsTextureFormat format) {
		safe_release(ptr);

		CreateTextureArgs args;
		args.format = format;
		args.num_mip_maps = 1;
		args.width = s_w / 2;
		args.height = s_h / 2;
		args.sampler_type = GraphicsSamplerType::NearestClamped;
		ptr = gfx().create_texture(args);
	};

	using gtf = GraphicsTextureFormat;
	delete_and_create_texture(tex.scene_color, gtf::rgb16f);
	tex.scene_color->clear_image();

	delete_and_create_texture_half_res(tex.halfres_scene_color, gtf::rgb16f);
	Texture::load("_halfres_scene_color")->update_specs_ptr(tex.halfres_scene_color);
	delete_and_create_texture(tex.ddgi_accum, gtf::r11f_g11f_b10f);
	delete_and_create_texture(tex.last_ddgi_accum, gtf::r11f_g11f_b10f);
	Texture::load("_ddgi_accum")->update_specs_ptr(tex.ddgi_accum);
	Texture::load("_ddgi_accum_prev")->update_specs_ptr(tex.last_ddgi_accum);
	tex.ddgi_accum->clear_image();
	tex.last_ddgi_accum->clear_image();

	delete_and_create_texture_halfresmips(tex.scene_color_mipchain, gtf::r11f_g11f_b10f, 5);
	delete_and_create_texture(tex.reflection_accum, gtf::r11f_g11f_b10f);
	delete_and_create_texture(tex.last_reflection_accum, gtf::r11f_g11f_b10f);
	Texture::load("_ssr")->update_specs_ptr(tex.reflection_accum);
	tex.reflection_accum->clear_image();
	tex.last_reflection_accum->clear_image();

	// last frame, for TAA
	delete_and_create_texture(tex.last_scene_color, gtf::rgb16f);
	tex.last_scene_color->clear_image();

	// Main scene depth
	delete_and_create_texture(tex.scene_depth, gtf::depth32f);
	// for mouse picking
	delete_and_create_texture(tex.editor_id_buffer, gtf::rgba8);

	delete_and_create_texture(tex.editor_selection_depth_buffer, gtf::depth32f);

	delete_and_create_texture(tex.scene_gbuffer0, gtf::rgb16f);
	delete_and_create_texture(tex.scene_gbuffer1, gtf::rgba8);
	delete_and_create_texture(tex.scene_gbuffer2, gtf::rgba8);

	const gtf scene_motion_format = (r_taa_32f.get_bool()) ? gtf::rg32f : gtf::rg16f;

	delete_and_create_texture(tex.scene_motion, scene_motion_format);

	delete_and_create_texture(tex.last_scene_motion, scene_motion_format);
	tex.last_scene_motion->clear_image();

	delete_and_create_texture(tex.output_composite, gtf::rgb8);
	delete_and_create_texture(tex.output_composite_2, gtf::rgb8);
	tex.actual_output_composite = tex.output_composite;

	cur_w = s_w;
	cur_h = s_h;

	// Update vts handles
	tex.scene_color_vts_handle->update_specs_ptr(tex.scene_color);
	tex.scene_depth_vts_handle->update_specs_ptr(tex.scene_depth);
	tex.gbuffer0_vts_handle->update_specs_ptr(tex.scene_gbuffer0);
	tex.gbuffer1_vts_handle->update_specs_ptr(tex.scene_gbuffer1);
	tex.gbuffer2_vts_handle->update_specs_ptr(tex.scene_gbuffer2);
	tex.editorid_vts_handle->update_specs_ptr(tex.editor_id_buffer);
	tex.editorSel_vts_handle->update_specs_ptr(tex.editor_selection_depth_buffer);
	tex.scene_motion_vts_handle->update_specs_ptr(tex.scene_motion);

	tex.read_scene_color_for_transparents_handle->update_specs_ptr(tex.scene_gbuffer0);

	// Also update bloom buffers (this can be elsewhere)
	init_bloom_buffers();

	// alert any observers that they need to update their buffer sizes (like SSAO, etc.)
	on_viewport_size_changed.invoke(cur_w, cur_h);
}

void Renderer::init_bloom_buffers() {

	int x = cur_w / 2;
	int y = cur_h / 2;
	tex.number_bloom_mips = glm::min((int)MAX_BLOOM_MIPS, Texture::get_mip_map_count(x, y));
	// glCreateTextures(GL_TEXTURE_2D, tex.number_bloom_mips, tex.bloom_chain);

	float fx = x;
	float fy = y;
	for (int i = 0; i < tex.number_bloom_mips; i++) {
		auto& bc = tex.bloom_chain[i];

		CreateTextureArgs args;
		args.width = x;
		args.height = y;
		args.format = GraphicsTextureFormat::r11f_g11f_b10f;
		args.num_mip_maps = 1;
		args.sampler_type = GraphicsSamplerType::LinearClamped;
		safe_release(bc.texture);
		bc.texture = gfx().create_texture(args);

		bc.isize = {x, y};
		bc.fsize = {fx, fy};
		// glTextureStorage2D(tex.bloom_chain[i], 1, GL_R11F_G11F_B10F, x, y);
		// glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		// glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		// glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		x /= 2;
		y /= 2;
		fx *= 0.5;
		fy *= 0.5;
	}

	tex.bloom_vts_handle->update_specs_ptr(tex.bloom_chain[0].texture);
}
