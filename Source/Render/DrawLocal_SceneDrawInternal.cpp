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
#include "tracy/public/tracy/Tracy.hpp"
#include <tracy/public/tracy/TracyOpenGL.hpp>
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"
#include <algorithm>
void Renderer::scene_draw_internal(SceneDrawParamsEx params, View_Setup view) {
	// TracyGpuZone("scene_draw_internal");
	// ZoneScoped;
	GPUSCOPESTART(scene_draw_internal_scope);

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

		if (params.output_to_screen) {
			GPUSCOPESTART(Blit_composite_to_backbuffer);

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
		GPUSCOPESTART(depth_prepass_scope);


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

		// Per-attachment clear-to-gray (previously RenderPassState::use_gray_clear).
		if (clear_color) {
			for (auto& ct : color_targets) {
				ct.wants_clear = true;
				ct.clear_color = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
			}
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
	};

	if (is_wireframe_mode) {
		gfx().set_polygon_fill_mode(GraphicsFillMode::Line);
		gfx().set_line_width(3);
		gbuffer_pass(GPRF_WIREFRAME_1);
		gfx().set_line_width(1);
		gbuffer_pass(GPRF_WIREFRAME_2);
		gfx().set_polygon_fill_mode(GraphicsFillMode::Fill);
	} else {
		{
			GPUSCOPESTART(gbuffer_pass_scope1);
			gbuffer_pass(GPRF_GBUFFER_1);
		}
		if (r_debug_mode.get_integer() == gpu::DEBUG_OVERDRAW) {
			gbuffer_pass(GPRF_OVERDRAWVIS);
		}
		if (!params.skybox_only)
			GpuCullingTest::inst->build_data_2(BuildSceneData_CpuFast::inst->get_cull_input());
		if (r_debug_mode.get_integer() != gpu::DEBUG_OVERDRAW) {
			GPUSCOPESTART(gbuffer_pass_scope2);
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
		SSRSystem::inst->execute_compute();
	}

	if (r_debug_mode.get_integer() == 0 && enable_ssao.get_bool() && !params.is_cubemap_view)
		ssao.render();

	if (r_debug_mode.get_integer() == 0 && !params.skybox_only)
		accumulate_gbuffer_lighting(params.is_cubemap_view);
	// STAMPS ON NORMALS IN GBUFFER0!
	auto copy_forward_to_temporary = [&]() {
		GPUSCOPESTART(copy_forward_to_temporary_scope);
		GraphicsBlitInfo blitInfo;
		blitInfo.set_width_height_both(cur_w, cur_h);
		blitInfo.src.texture = tex.scene_color;
		blitInfo.dest.texture = tex.scene_gbuffer0;
		gfx().blit_textures(blitInfo);
	};

	if (r_taa_enabled.get_bool())
		copy_forward_to_temporary();

	auto taa_resolve_pass = [&]() -> IGraphicsTexture* {
		GPUSCOPESTART(TaaResolve);
		ZoneScopedN("TaaResolve");

		if (!r_taa_enabled.get_bool() || params.is_cubemap_view) {
			disable_taa_this_frame = false;
			return tex.scene_color;
		}

		// On camera cut, still run the shader but with zero blend to prime history
		float blend_strength = 1.0f;
		if (disable_taa_this_frame) {
			blend_strength = 0.0f;
			disable_taa_this_frame = false;
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
		extern ConfigVar r_taa_sharpness;

		gpu::TemporalParams tp{};
		tp.lastViewProj = last_frame_main_view.viewproj;
		tp.doc_mult = taa_doc_mult;
		tp.doc_vel_bias = taa_doc_vel_bias;
		tp.doc_bias = taa_doc_bias;
		tp.doc_pow = taa_doc_pow;
		tp.stationary_blending = r_taa_stationary_blend.get_float() * blend_strength;
		tp.motion_blending = r_taa_motion_blend.get_float() * blend_strength;
		tp.sharpness = r_taa_sharpness.get_float();
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
		GPUSCOPESTART(draw_forward_pass_scope);

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

	// no fog in cubemaps?
	if (!params.is_cubemap_view)
		draw_height_fog(scene_color_handle);


	// cubemap views end here
	// dont need to draw post processing or UI stuff
	if (params.is_cubemap_view)
		return;

	if (params.is_editor) {
		GPUSCOPESTART(editor_select_pass_scope);

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
		GPUSCOPESTART(draw_mesh_builders);
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

	// Bloom update
	render_bloom_chain(scene_color_handle);

	IGraphicsTexture* read_from_texture = tex.output_composite;
	const auto& view_to_use = current_frame_view;
	assert(cur_w == view_to_use.width && cur_h == view_to_use.height);
	// RenderPassSetup setup("composite", fbo.composite, true, false, 0, 0, view_to_use.width, view_to_use.height);
	// auto scope = device.start_render_pass(setup);

	auto do_composite_pass = [&]() {
		GPUSCOPESTART(composite_pass_scope);
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

		IGraphicsTexture* bloom_tex = tex.bloom_chain[0].texture;
		if (!enable_bloom.get_bool())
			bloom_tex = black_texture;
		bind_texture_ptr(0, scene_color_handle);
		bind_texture_ptr(1, bloom_tex);
		bind_texture_ptr(2, lens_dirt->gpu_ptr);

		{
			gpu::LitCompositorParams lp{};
			lp.tonemap_type     = pp_tonemap_type;
			lp.contrast_tweak   = pp_contrast;
			lp.saturation_tweak = pp_saturation;
			lp.bloom_lerp       = pp_bloom_add;
			lp.exposure         = pp_exposure;
			ubo.lit_compositor_params->upload(&lp, sizeof(lp));
			gfx().bind_uniform_buffer_base(7, ubo.lit_compositor_params);
		}

		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
	};
	do_composite_pass();

	if (ddgi_test.get_bool())
		ddgi->render_probes();
	else if (ddgi_rt.get_bool())
		ddgi->render_rt();

	GpuCullingTest::inst->debug_overlay();

	auto post_process_stack = [&]() {
		GPUSCOPESTART(post_process_stack_scope);

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
		}
	};
	do_ui_draw();

	debug_tex_out.draw_out();

	tex.actual_output_composite = read_from_texture;
	if (params.output_to_screen) {
		GPUSCOPESTART(Blit_composite_to_backbuffer);

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
	ZoneScoped;

	auto renderToTexture = tex.output_composite_2;
	auto renderFromTexture = tex.output_composite;
	tex.actual_output_composite = renderFromTexture;
	for (int i = 0; i < postProcessMats.size(); i++) {
		if (!postProcessMats[i])
			continue;

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