#include "DrawLocal.h"
#include "Framework/Util.h"
#include "Render/Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include <SDL3/SDL.h>
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "Render/RenderWindowLocal.h"
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"
#include <algorithm>
void Renderer::draw_editor_ortho_grid(IGraphicsTexture* target) {
	GPU_SCOPE("draw_editor_ortho_grid");

	RenderPassState state;
	auto color_info = {ColorTargetInfo(target)};
	state.color_infos = color_info;
	gfx().set_render_pass(state);

	RenderPipelineState setup;
	setup.depth_testing = false;
	setup.depth_writes = false;
	setup.program = get_prog_man().get_obj(prog.editor_ortho_grid);
	setup.vao = get_empty_vao();
	gfx().set_pipeline(setup);

	gfx().bind_uniform_buffer_base(0, ubo.current_frame);
	bind_texture_ptr(0, tex.scene_depth);

	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}

void Renderer::scene_draw_internal(SceneDrawParamsEx params, View_Setup view) {
	RENDER_SCOPE("scene_draw_internal");

	current_time = params.time;

	mem_arena.free_bottom();
	stats = Render_Stats();
	gfx().reset_state_cache();

	if (view.width < 4 || view.height < 4) {
		sys_print(Error, "framebuffer too small for scene draw internal\n");
		return;
	}

	if (refresh_render_targets_next_frame || cur_w != view.width || cur_h != view.height)
		InitFramebuffers(true, view.width, view.height);

	current_frame_view = view;

	if (!params.draw_world && !params.draw_ui)
		return;
	else if (!params.draw_world && params.draw_ui) {

		const auto& view_to_use = current_frame_view;
		assert(cur_w == view_to_use.width && cur_h == view_to_use.height);
		// RenderPassSetup setup("composite", fbo.composite, true, true, 0, 0, view_to_use.width, view_to_use.height);
		// auto scope = device.start_render_pass(setup);

		RenderPassState pass_state;
		ColorTargetInfo composite_target(tex.output_composite);
		composite_target.wants_clear = true; // clear to black (default)
		auto color_infos = {composite_target};
		pass_state.color_infos = color_infos;
		gfx().set_render_pass(pass_state);

		// draw_ui_local.render();

		windowDrawer->render();
		gfx().rmlui_render(cur_w, cur_h, tex.output_composite);

		if (params.output_to_screen) {
			GPU_SCOPE("Blit_composite_to_backbuffer");

			GraphicsBlitInfo blitInfo;
			blitInfo.dest.w = blitInfo.src.w = cur_w;
			blitInfo.dest.h = blitInfo.src.h = cur_h;
			blitInfo.dest.texture = gfx().acquire_swapchain_texture();
			blitInfo.src.texture = tex.output_composite;
			blitInfo.filter = GraphicsFilterType::Nearest;
			gfx().blit_textures(blitInfo);
		}
		return;
	}
	upload_ubo_view_constants(current_frame_view, ubo.current_frame);
	shadowmap.update_matricies();
	scene.build_scene_data(params.skybox_only, params.is_editor, params.is_cubemap_view);
	upload_light_and_decal_buffers();

	volfog.compute();
	const bool is_wireframe_mode = r_debug_mode.get_integer() == gpu::DEBUG_WIREFRAME;

	// main level render

#if 0
	auto depth_prepass = [&]() {
		GPU_SCOPE("depth_prepass");


		const auto& view_to_use = current_frame_view;

		//RenderPassSetup setup("gbuffer", fbo.gbuffer, clear_framebuffer,clear_framebuffer, 0, 0, view_to_use.width, view_to_use.height);
		//auto scope = device.start_render_pass(setup);

		RenderPassState setup2;
		setup2.depth_info = tex.scene_depth;
		setup2.wants_depth_clear = true;
		gfx().set_render_pass(setup2);

		if (r_skip_depth_prepass.get_bool())
			return;	// do it here so framebuffer depth/color is cleared even if no prepass


		Render_Level_Params cmdparams(
			view_to_use,
			&scene.depth_prepass_rlist,
			&scene.depth_prepass,
			Render_Level_Params::OPAQUE
		);

		cmdparams.upload_constants = true;
		cmdparams.provied_constant_buffer = ubo.current_frame;
		cmdparams.draw_viewmodel = true;

		render_level_to_target(cmdparams);
	};
	depth_prepass();
#endif

	enum GbufferPassRenderType
	{
		GPRF_WIREFRAME_1,
		GPRF_WIREFRAME_2,
		GPRF_GBUFFER_1,
		GPRF_GBUFFER_2,
		GPRF_OVERDRAWVIS
	};

	auto gbuffer_pass = [&](GbufferPassRenderType type) {
		if (r_skip_gbuffer.get_bool())
			return;

		const auto& view_to_use = current_frame_view;

		const bool clear_color =
			(type == GPRF_WIREFRAME_1 || type == GPRF_GBUFFER_1 ||
			 type == GPRF_OVERDRAWVIS); // (!is_wireframe || !wireframe_secondpass) && !gbuffer_2nd;
		const bool clear_depth = (clear_color) && (type != GPRF_OVERDRAWVIS);

		// RenderPassSetup setup("gbuffer", fbo.gbuffer, clear_framebuffer,clear_framebuffer, 0, 0, view_to_use.width,
		// view_to_use.height); auto scope = device.start_render_pass(setup);

		RenderPassState setup2;
		std::array<ColorTargetInfo, 6> color_targets = {
			ColorTargetInfo(tex.scene_gbuffer0),   ColorTargetInfo(tex.scene_gbuffer1),
			ColorTargetInfo(tex.scene_gbuffer2),   ColorTargetInfo(tex.scene_color),
			ColorTargetInfo(tex.editor_id_buffer), ColorTargetInfo(tex.scene_motion),
		};

		if (clear_color) {
			const int dbg = r_debug_mode.get_integer();
			const bool is_wireframe = dbg == gpu::DEBUG_WIREFRAME;
			const bool is_debug = dbg != 0;
			const glm::vec4 bg_color =
				is_wireframe ? glm::vec4(0.12f, 0.12f, 0.12f, 1.0f) :
				is_debug ? glm::vec4(0, 0, 0, 1) :
				(view.is_ortho && params.is_editor) ? glm::vec4(0.12f, 0.12f, 0.12f, 1.0f) :
				glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
			for (auto& ct : color_targets) {
				ct.wants_clear = true;
				ct.clear_color = bg_color;
			}
			// motion buffer: 0.5 = no motion in the velocity encoding
			color_targets[5].clear_color = glm::vec4(0.5f, 0.5f, 0.0f, 1.0f);
		}

		std::span<const ColorTargetInfo> ct_span = color_targets;
		if (dont_attach_velocity.get_bool())
			ct_span = std::span<const ColorTargetInfo>(ct_span.data(), ct_span.size() - 1);

		setup2.color_infos = ct_span;
		setup2.depth_info = tex.scene_depth;
		setup2.wants_depth_clear = (clear_depth);
		gfx().set_render_pass(setup2);

		Render_Level_Params cmdparams(view_to_use, &scene.gbuffer_rlist, &scene.gbuffer_pass,
									  Render_Level_Params::OPAQUE);
		if (type == GPRF_GBUFFER_2 || type == GPRF_OVERDRAWVIS) {
			cmdparams.rl = nullptr;
		}

		cmdparams.upload_constants = true;
		cmdparams.provied_constant_buffer = ubo.current_frame;
		cmdparams.draw_viewmodel = true;
		cmdparams.wireframe_secondpass = (type == GPRF_WIREFRAME_2);
		cmdparams.is_wireframe_pass = (type == GPRF_WIREFRAME_1 || type == GPRF_WIREFRAME_2);

		render_level_to_target(cmdparams);

		if (!params.skybox_only)
			BuildSceneData_CpuFast::inst->do_gbuffer_draw(type == GPRF_OVERDRAWVIS);
		// GpuCullingTest::inst->dodraw();

		// Unity-style "shaded wireframe": redraw the same triangles in line-fill mode,
		// alpha-blended and depth-tested (no depth write) against the depth buffer just
		// written, so hidden-line removal falls out for free. Occlusion culling splits
		// the scene across GBUFFER_1 (last-frame-visible set, seeds the depth pyramid)
		// and GBUFFER_2 (re-culled remainder) with separate GPU cull buffers, so this
		// has to run right after each phase using that phase's own cull/visibility
		// data - a single pass after both would only see whichever buffer was drawn
		// last, missing every object exclusive to the other phase.
		if (r_debug_mode.get_integer() == gpu::DEBUG_ALBEDO &&
			(type == GPRF_GBUFFER_1 || type == GPRF_GBUFFER_2)) {
			gfx().set_polygon_fill_mode(GraphicsFillMode::Line);
			gfx().set_line_width(1.25f);

			Render_Level_Params wf_params = cmdparams;
			wf_params.wireframe_overlay_pass = true;
			render_level_to_target(wf_params);

			if (!params.skybox_only)
				BuildSceneData_CpuFast::inst->do_gbuffer_draw(false, true);

			gfx().set_polygon_fill_mode(GraphicsFillMode::Fill);
		}
	};

	if (is_wireframe_mode) {
		gfx().set_polygon_fill_mode(GraphicsFillMode::Line);
		gfx().set_line_width(1);
		gbuffer_pass(GPRF_WIREFRAME_1);
		gfx().set_polygon_fill_mode(GraphicsFillMode::Fill);
	} else {
		{
			GPU_SCOPE("gbuffer_pass_1");
			gbuffer_pass(GPRF_GBUFFER_1);
		}
		if (r_debug_mode.get_integer() == gpu::DEBUG_OVERDRAW) {
			gbuffer_pass(GPRF_OVERDRAWVIS);
		}
		if (!params.skybox_only)
			GpuCullingTest::inst->build_data_2(BuildSceneData_CpuFast::inst->get_cull_input());
		if (r_debug_mode.get_integer() != gpu::DEBUG_OVERDRAW) {
			GPU_SCOPE("gbuffer_pass_2");
			gbuffer_pass(GPRF_GBUFFER_2); // second gbuffer pass
		}
	}
	if (!params.skybox_only) {
		shadowmap.render_cascades();

		scene.update_spotlight_data();
	}
	// device.reset_states();

	deferred_decal_pass();
	// device.reset_states();

	if (enable_ssr.get_bool() && r_debug_mode.get_integer() == 0 && !params.skybox_only) {
		SSRSystem::inst->execute();
	} else {
		tex.reflection_accum->clear_image();
	}

	if (r_debug_mode.get_integer() == 0 && enable_ssao.get_bool() && !params.is_cubemap_view)
		ssao.render();

	if (r_debug_mode.get_integer() == 0 && !params.skybox_only)
		accumulate_gbuffer_lighting(params.is_cubemap_view);
	// STAMPS ON NORMALS IN GBUFFER0!
	auto copy_forward_to_temporary = [&]() {
		GPU_SCOPE("copy_forward_to_temporary");
		GraphicsBlitInfo blitInfo;
		blitInfo.set_width_height_both(cur_w, cur_h);
		blitInfo.src.texture = tex.scene_color;
		blitInfo.dest.texture = tex.scene_gbuffer0;
		gfx().blit_textures(blitInfo);
	};

	if (r_taa_enabled.get_bool())
		copy_forward_to_temporary();

	auto taa_resolve_pass = [&]() -> IGraphicsTexture* {
		RENDER_SCOPE("TaaResolve");
		bool wants_disable = disable_taa_this_frame || params.is_cubemap_view;
		disable_taa_this_frame = false;
		if (!r_taa_enabled.get_bool() || wants_disable) {
			return tex.scene_color;
		}

		gfx().bind_uniform_buffer_base(0, ubo.current_frame);

		auto color_infos = {ColorTargetInfo(tex.scene_gbuffer0)};
		RenderPassState pass;
		pass.color_infos = color_infos;
		gfx().set_render_pass(pass);

		RenderPipelineState state;
		state.program = get_prog_man().get_obj(prog.taa_resolve);
		state.vao = get_empty_vao();
		gfx().set_pipeline(state);

		extern ConfigVar r_taa_stationary_blend;
		extern ConfigVar r_taa_motion_blend;
		extern ConfigVar r_taa_adaptive_blend;
		extern ConfigVar r_taa_sharpness;

		gpu::TemporalParams tp{};
		tp.lastViewProj = last_frame_main_view.viewproj;
		tp.amt = r_taa_blend.get_float();
		tp.doc_mult = taa_doc_mult;
		tp.doc_vel_bias = taa_doc_vel_bias;
		tp.doc_bias = taa_doc_bias;
		tp.doc_pow = taa_doc_pow;
		tp.remove_flicker = r_taa_flicker_remove.get_bool();
		tp.use_reproject = r_taa_reproject.get_bool();
		tp.dilate_velocity = r_taa_dilate_velocity.get_bool();
		tp.stationary_blending = r_taa_stationary_blend.get_float();
		tp.motion_blending = r_taa_motion_blend.get_float();
		tp.sharpness = r_taa_sharpness.get_float();
		tp.adaptive_blend = r_taa_adaptive_blend.get_bool();
		ubo.temporal_params->upload(&tp, sizeof(tp));
		gfx().bind_uniform_buffer_base(7, ubo.temporal_params);

		bind_texture_ptr(0, tex.scene_color);
		bind_texture_ptr(1, tex.last_scene_color);
		bind_texture_ptr(2, tex.scene_depth);
		bind_texture_ptr(3, tex.scene_motion);
		bind_texture_ptr(4, tex.last_scene_motion);
		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);

		// blit from gbuffer0 to scene color
		GraphicsBlitInfo blitinfo;
		blitinfo.set_width_height_both(cur_w, cur_h);
		blitinfo.src.texture = tex.scene_gbuffer0;
		blitinfo.dest.texture = tex.scene_color;
		gfx().blit_textures(blitinfo);

		return tex.scene_gbuffer0;
	};
	IGraphicsTexture* const scene_color_handle = taa_resolve_pass();

	auto draw_forward_pass = [&]() {
		GPU_SCOPE("draw_forward_pass");

		const auto& view_to_use = current_frame_view;
		// RenderPassSetup setup("transparents", fbo.forward_render, false, false, 0, 0, view_to_use.width,
		// view_to_use.height); auto scope = device.start_render_pass(setup);

		RenderPassState state;
		auto color_info = {ColorTargetInfo(scene_color_handle)};
		state.depth_info = tex.scene_depth;
		state.color_infos = color_info;
		gfx().set_render_pass(state);

		Render_Level_Params params(view_to_use, &scene.transparent_rlist, &scene.transparent_pass,
								   Render_Level_Params::FORWARD_PASS);

		params.upload_constants = true;
		params.provied_constant_buffer = ubo.current_frame;
		params.draw_viewmodel = true;

		render_level_to_target(params);

		render_particles();
	};
	draw_forward_pass();

	if (!params.is_cubemap_view && r_debug_mode.get_integer() == 0)
		draw_height_fog(scene_color_handle);


	// cubemap views end here
	// dont need to draw post processing or UI stuff
	if (params.is_cubemap_view)
		return;

	if (params.is_editor) {
		GPU_SCOPE("editor_select_pass");

		auto create_editor_pass = [&]() {
			RenderPassState state;
			state.depth_info = tex.editor_selection_depth_buffer;
			state.wants_depth_clear = true;
			gfx().set_render_pass(state);
		};
		create_editor_pass();

		const auto& view_to_use = current_frame_view;
		// RenderPassSetup setup("editor-id", fbo.editorSelectionDepth, false, true/* clear depth*/, 0, 0,
		// view_to_use.width, view_to_use.height); auto scope = device.start_render_pass(setup);

		Render_Level_Params params(view_to_use, &scene.editor_sel_rlist, &scene.editor_sel_pass,
								   Render_Level_Params::DEPTH);
		params.provied_constant_buffer = ubo.current_frame;
		render_level_to_target(params);
	}

	gfx().reset_state_cache();

	// mesh builder stuff
	auto draw_mesh_builders = [&]() {
		GPU_SCOPE("draw_mesh_builders");
		const auto& view_to_use = current_frame_view;
		// RenderPassSetup setup("meshbuilders", fbo.forward_render, false, false, 0, 0, view_to_use.width,
		// view_to_use.height); auto scope = device.start_render_pass(setup);

		auto start_render_pass = [&]() {
			auto targets = {ColorTargetInfo(scene_color_handle)};
			RenderPassState rp;
			rp.color_infos = targets;
			rp.depth_info = tex.scene_depth;

			gfx().set_render_pass(rp);
		};
		start_render_pass();

		draw_meshbuilders();
	};
	draw_mesh_builders();

	// last_scene
	// scene
	// gbuffer0 = taa_resolve(scene, last_scene)
	// last_scene = blit(gbuffer0)
	// scene_color_handle = gbuffer0
	// render_transparents(scene_color_handle)

	const bool in_debug_mode = r_debug_mode.get_integer() != 0;

	// Bloom update
	if (!in_debug_mode)
		render_bloom_chain(scene_color_handle);

	// Auto-exposure (after bloom so method 0 can read bloom tail)
	if (!in_debug_mode) {
		const PostProcessParams ae_pp = PPManager::inst ? PPManager::inst->get_active() : PostProcessParams{};
		render_auto_exposure(scene_color_handle, ae_pp, params.dt);
	}

	IGraphicsTexture* read_from_texture = tex.output_composite;
	const auto& view_to_use = current_frame_view;
	assert(cur_w == view_to_use.width && cur_h == view_to_use.height);
	// RenderPassSetup setup("composite", fbo.composite, true, false, 0, 0, view_to_use.width, view_to_use.height);
	// auto scope = device.start_render_pass(setup);

	auto do_composite_pass = [&]() {
		GPU_SCOPE("composite_pass");
		auto set_composite_pass = [&]() {
			RenderPassState pass_state;
			ColorTargetInfo target(read_from_texture);
			target.wants_clear = true; // clear to black (default)
			auto color_infos = {target};
			pass_state.color_infos = color_infos;
			gfx().set_render_pass(pass_state);
		};
		set_composite_pass();

		RenderPipelineState state;
		state.program = get_prog_man().get_obj(prog.combine);
		state.vao = get_empty_vao();
		gfx().set_pipeline(state);

		PostProcessParams debug_pp;
		debug_pp.bloom_enabled = false;
		const PostProcessParams pp = in_debug_mode ? debug_pp : (PPManager::inst ? PPManager::inst->get_active() : PostProcessParams{});

		IGraphicsTexture* bloom_tex = tex.bloom_chain[0].texture;
		if (!enable_bloom.get_bool() || !pp.bloom_enabled)
			bloom_tex = black_texture;
		Texture* dirt_tex = pp.bloom_lens_dirt ? pp.bloom_lens_dirt : lens_dirt;
		bind_texture_ptr(0, scene_color_handle);
		bind_texture_ptr(1, bloom_tex);
		bind_texture_ptr(2, dirt_tex->gpu_ptr);
		// binding 3 is reserved (lens dirt was 2, keep old layout)
		// binding 4: adapted exposure texture (1x1 R16F); use previous ping so it's ready
		IGraphicsTexture* ae_tex = tex.ae_lum[ae_ping] ? tex.ae_lum[ae_ping] : black_texture;
		bind_texture_ptr(4, ae_tex);

		{
			gpu::LitCompositorParams lp{};
			lp.tonemap_type       = pp.tonemap_type;
			lp.contrast_tweak     = pp.contrast;
			lp.saturation_tweak   = pp.saturation;
			lp.bloom_lerp         = pp.bloom_intensity;
			lp.exposure           = pp.exposure;
			lp.vignette_intensity = pp.vignette_intensity;
			lp.vignette_falloff   = pp.vignette_falloff;
			lp.chromatic_ab       = pp.chromatic_ab;
			lp.grain_intensity    = pp.grain_intensity;
			lp.grain_size         = pp.grain_size;
			lp.sharpness          = pp.sharpness;
			lp.color_temp         = pp.color_temp;
			lp.grain_time         = (float)(SDL_GetTicks() * 0.001);
			lp.lgq_lift           = vec4(pp.lift,      0.f);
			lp.lgq_gamma          = vec4(pp.gamma_rgb, 0.f);
			lp.lgq_gain           = vec4(pp.gain,      0.f);
			lp.ae_enabled         = pp.auto_exposure ? 1 : 0;
			lp.lens_dirt_intensity = pp.bloom_lens_dirt_intensity;
			ubo.lit_compositor_params->upload(&lp, sizeof(lp));
			gfx().bind_uniform_buffer_base(7, ubo.lit_compositor_params);
		}

		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
	};
	do_composite_pass();

	if (params.is_editor && view.is_ortho)
		draw_editor_ortho_grid(read_from_texture);

	if (ddgi_test.get_bool())
		ddgi->render_probes();
	else if (ddgi_rt.get_bool())
		ddgi->render_rt();

	GpuCullingTest::inst->debug_overlay();

	auto post_process_stack = [&]() {
		GPU_SCOPE("post_process_stack");

		std::vector<MaterialInstance*> postProcesses;
		if (r_debug_mode.get_integer() == gpu::DEBUG_OVERDRAW) {
			auto mat = MaterialInstance::load("eng/overdraw_pp.mm");
			if (mat && mat->impl && mat->impl->gpu_buffer_offset != -1)
				postProcesses.push_back(mat);
		}

		static const int DEBUG_OUTLINED = 100; // uses objID; not in the gpu:: enum
		if (r_debug_mode.get_integer() == DEBUG_OUTLINED) {
			auto mat = g_assets.find<MaterialInstance>("eng/editorEdgeDetect.mm");
			if (mat.get() && mat->impl->gpu_buffer_offset != mat->impl->INVALID_MAPPING)
				postProcesses.push_back(mat.get());
		}
		if (params.is_editor) {
			postProcesses.push_back(matman.get_default_editor_sel_PP());
		}

		if (!r_no_postprocess.get_bool())
			read_from_texture = do_post_process_stack(postProcesses);
	};
	post_process_stack();

	auto do_ui_draw = [&]() {
		// UI
		if (params.draw_ui && !r_force_hide_ui.get_bool()) {
			windowDrawer->render();
			gfx().rmlui_render(cur_w, cur_h, read_from_texture);
		}
	};
	do_ui_draw();

	debug_tex_out.draw_out();

	tex.actual_output_composite = read_from_texture;
	if (params.output_to_screen) {
		GPU_SCOPE("Blit_composite_to_backbuffer");

		GraphicsBlitInfo blitInfo;
		blitInfo.dest.y = 0;
		blitInfo.dest.w = blitInfo.src.w = cur_w;
		blitInfo.dest.h = blitInfo.src.h = cur_h;
		blitInfo.dest.texture = gfx().acquire_swapchain_texture();
		blitInfo.src.texture = read_from_texture;
		blitInfo.filter = GraphicsFilterType::Nearest;
		gfx().blit_textures(blitInfo);
	}
}

IGraphicsTexture* Renderer::do_post_process_stack(const std::vector<MaterialInstance*>& postProcessMats) {
	CPU_FUNCTION();

	auto renderToTexture = tex.output_composite_2;
	auto renderFromTexture = tex.output_composite;
	tex.actual_output_composite = renderFromTexture;
	for (int i = 0; i < postProcessMats.size(); i++) {
		if (!postProcessMats[i])
			continue;

		// Materials just created/modified this same frame (e.g. editor tool startup
		// assigning a default post-process material) can still be sitting in matman's
		// dirty list with unresolved texture_bindings -- flush is idempotent (no-op if
		// already clean), matching the self-heal get_texture_id_hash() does elsewhere.
		matman.flush_dirty_material(postProcessMats[i]);

		RenderPassState pass_setup;
		auto color_infos = {ColorTargetInfo(renderToTexture)};
		pass_setup.color_infos = color_infos;
		gfx().set_render_pass(pass_setup);

		tex.postProcessInput_vts_handle->update_specs_ptr(renderFromTexture);

		auto mat = postProcessMats[i];

		RenderPipelineState state;
		state.program = get_prog_man().get_obj(matman.get_mat_shader(nullptr, mat, 0));
		state.blend = mat->get_master_material()->blend;
		state.depth_testing = state.depth_writes = false;
		state.vao = get_empty_vao();
		state.backface_culling = false;
		gfx().set_pipeline(state);

		auto& texs = mat->impl->get_textures();

		for (int i = 0; i < texs.size(); i++) {
			bind_texture_ptr(i, texs[i]->gpu_ptr);
		}

		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);

		tex.actual_output_composite = renderToTexture;
		std::swap(renderFromTexture, renderToTexture);
	}

	return renderFromTexture;
}