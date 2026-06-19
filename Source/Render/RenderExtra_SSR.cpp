#include "RenderExtra.h"
#include "Render/DrawLocal.h"
#include "imgui.h"

void draw_imgui_for_cvar(ConfigVar& var) {
	ASSERT(var.get_name() != nullptr);
	auto flags = var.get_var_flags();
	if (flags & CVAR_BOOL) {
		bool b = var.get_bool();
		if (ImGui::Checkbox(var.get_name(), &b))
			var.set_bool(b);
	} else if (flags & CVAR_INTEGER) {
		int i = var.get_integer();
		if (flags & CVAR_UNBOUNDED) {
			if (ImGui::InputInt(var.get_name(), &i))
				var.set_integer(i);
		} else {
			if (ImGui::SliderInt(var.get_name(), &i, var.get_min_val(), var.get_max_val()))
				var.set_integer(i);
		}
	}
}

SSRSystem* SSRSystem::inst = nullptr;

SSRSystem::SSRSystem() {
	ASSERT(gfx_is_initialized());

	ssr_raytrace = draw.get_prog_man().create_raster("fullscreenquad.txt", "ssr_f.txt");
	ssr_resolve = draw.get_prog_man().create_raster("fullscreenquad.txt", "ssr_upsample.txt");
	ssr_temporal = draw.get_prog_man().create_raster("fullscreenquad.txt", "temporal_upsample_ssr.txt");
	gaussian_blur = draw.get_prog_man().create_raster("fullscreenquad.txt", "gaussian_blur.txt");
}

void SSRSystem::ensure_buffers(int w, int h) {
	auto create_buffer = [](IGraphicsTexture*& tex, int w, int h, GraphicsTextureFormat fmt = GraphicsTextureFormat::rgba16f) {
		if (tex) tex->release();
		CreateTextureArgs args;
		args.format = fmt;
		args.num_mip_maps = 1;
		args.width = w;
		args.height = h;
		args.sampler_type = GraphicsSamplerType::LinearDefault;
		tex = gfx().create_texture(args);
	};

	bool needs_recreate = !trace_buffer
		|| trace_buffer->get_size().x != w
		|| trace_buffer->get_size().y != h;

	if (needs_recreate) {
		create_buffer(trace_buffer, w, h);
		create_buffer(resolve_buffer, w, h);
	}

	const auto& vs = draw.current_frame_view;
	int half_w = vs.width / 2;
	int half_h = vs.height / 2;
	bool needs_blur_buf = !blur_intermediate
		|| blur_intermediate->get_size().x != half_w
		|| blur_intermediate->get_size().y != half_h;
	if (needs_blur_buf) {
		if (blur_intermediate) blur_intermediate->release();
		CreateTextureArgs args;
		args.format = GraphicsTextureFormat::r11f_g11f_b10f;
		args.num_mip_maps = 5;
		args.width = half_w;
		args.height = half_h;
		args.sampler_type = GraphicsSamplerType::LinearDefault;
		blur_intermediate = gfx().create_texture(args);
	}
}

void SSRSystem::do_gaussian_mipchain() {
	GPUSCOPESTART(ssr_gaussian_mipchain);

	const auto& viewsetup = draw.current_frame_view;
	auto& device = draw.get_device();
	const int num_mips = 5;

	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = draw.get_prog_man().get_obj(gaussian_blur);
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);

	glm::ivec2 src_size(viewsetup.width, viewsetup.height);

	for (int i = 0; i < num_mips; i++) {
		glm::ivec2 dst_size = glm::max(src_size / 2, glm::ivec2(1));
		glm::vec2 src_texel = 1.f / glm::vec2(src_size);

		// Horizontal blur: source → blur_intermediate mip i
		{
			auto targets = {ColorTargetInfo(blur_intermediate, -1, i)};
			RenderPassState rp;
			rp.color_infos = targets;
			gfx().set_render_pass(rp);
			device.set_viewport(0, 0, dst_size.x, dst_size.y);

			gpu::SsrParams sp{};
			sp.myimg_size = glm::vec2(1.f, 0.f);
			sp.inv_prev_size = src_texel;
			sp.mip_level = (i == 0) ? 0 : i - 1;
			draw.ubo.ssr_params->upload(&sp, sizeof(sp));
			gfx().bind_uniform_buffer_base(7, draw.ubo.ssr_params);

			if (i == 0)
				device.bind_texture(0, draw.tex.last_scene_color);
			else
				device.bind_texture(0, draw.tex.scene_color_mipchain);

			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
		}

		// Vertical blur: blur_intermediate mip i → scene_color_mipchain mip i
		{
			auto targets = {ColorTargetInfo(draw.tex.scene_color_mipchain, -1, i)};
			RenderPassState rp;
			rp.color_infos = targets;
			gfx().set_render_pass(rp);
			device.set_viewport(0, 0, dst_size.x, dst_size.y);

			gpu::SsrParams sp{};
			sp.myimg_size = glm::vec2(0.f, 1.f);
			sp.inv_prev_size = 1.f / glm::vec2(dst_size);
			sp.mip_level = i;
			draw.ubo.ssr_params->upload(&sp, sizeof(sp));
			gfx().bind_uniform_buffer_base(7, draw.ubo.ssr_params);

			device.bind_texture(0, blur_intermediate);

			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
		}

		src_size = dst_size;
	}
}

// Tweakables
static float ssr_roughness_fade = 0.4f;
static float ssr_brdf_bias = 0.82f;
static float ssr_anti_self_occ_bias = 0.1f;
static float ssr_edge_fade = 0.1f;
static float ssr_max_trace_samples = 60.f;
static float ssr_intensity = 1.0f;
static float ssr_fade_out_distance = 5000.f;
static float ssr_temporal_response = 0.85f;
static int   ssr_resolve_samples = 4;
static bool  ssr_fullres_trace = false;

void SSRSystem::do_raytrace() {
	GPUSCOPESTART(ssr_raytrace_pass);
	const auto& vs = draw.current_frame_view;

	auto& device = draw.get_device();
	auto targets = {ColorTargetInfo(trace_buffer)};
	RenderPassState rp;
	rp.color_infos = targets;
	gfx().set_render_pass(rp);

	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = draw.get_prog_man().get_obj(ssr_raytrace);
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);

	int trace_w = trace_buffer->get_size().x;
	int trace_h = trace_buffer->get_size().y;
	device.set_viewport(0, 0, trace_w, trace_h);

	{
		gpu::SsrParams sp{};
		sp.maxTraceSamples = ssr_max_trace_samples;
		sp.roughnessFade = ssr_roughness_fade;
		sp.brdfBias = ssr_brdf_bias;
		sp.worldAntiSelfOccBias = ssr_anti_self_occ_bias;
		sp.edgeFadeFactor = ssr_edge_fade;
		sp.temporalTime = float(temporalframe);
		sp.temporalEffect = 1.0f;
		sp.rayTraceStep = 1.0f / float(vs.width);
		sp.traceSizeMax = float(glm::max(trace_w, trace_h));
		sp.maxColorMiplevel = 3.f;
		sp.intensity = ssr_intensity;
		sp.fadeOutDistance = ssr_fade_out_distance;
		draw.ubo.ssr_params->upload(&sp, sizeof(sp));
		gfx().bind_uniform_buffer_base(7, draw.ubo.ssr_params);
	}

	device.bind_texture(0, draw.tex.scene_gbuffer0);
	device.bind_texture(1, draw.tex.scene_gbuffer1);
	device.bind_texture(2, draw.tex.scene_gbuffer2);
	device.bind_texture(3, draw.tex.scene_depth);
	device.bind_texture(4, draw.tex.scene_color_mipchain);

	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}

void SSRSystem::do_resolve() {
	GPUSCOPESTART(ssr_resolve_pass);

	auto& device = draw.get_device();
	auto targets = {ColorTargetInfo(resolve_buffer)};
	RenderPassState rp;
	rp.color_infos = targets;
	gfx().set_render_pass(rp);

	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = draw.get_prog_man().get_obj(ssr_resolve);
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);

	int w = resolve_buffer->get_size().x;
	int h = resolve_buffer->get_size().y;
	device.set_viewport(0, 0, w, h);

	{
		gpu::SsrParams sp{};
		sp.ssrTexelSize = glm::vec2(1.f / w, 1.f / h);
		sp.temporalTime = float(temporalframe);
		sp.intensity = ssr_intensity;
		sp.resolveSamples = glm::clamp(ssr_resolve_samples, 1, 8);
		draw.ubo.ssr_params->upload(&sp, sizeof(sp));
		gfx().bind_uniform_buffer_base(7, draw.ubo.ssr_params);
	}

	device.bind_texture(0, trace_buffer);

	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}

void SSRSystem::do_temporal() {
	GPUSCOPESTART(ssr_temporal_pass);

	auto& device = draw.get_device();

	auto last_reflect = draw.tex.last_reflection_accum;
	if (draw.wants_disable_temporal_effects_this_frame())
		last_reflect = draw.black_texture;

	auto targets = {ColorTargetInfo(draw.tex.reflection_accum)};
	RenderPassState rp;
	rp.color_infos = targets;
	gfx().set_render_pass(rp);

	RenderPipelineState state{};
	state.vao = draw.get_empty_vao();
	state.blend = BlendState::OPAQUE;
	state.program = draw.get_prog_man().get_obj(ssr_temporal);
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);

	const auto& vs = draw.current_frame_view;
	device.set_viewport(0, 0, vs.width, vs.height);

	{
		gpu::SsrParams sp{};
		sp.ssrTexelSize = glm::vec2(1.f / vs.width, 1.f / vs.height);
		sp.temporalResponse = ssr_temporal_response;
		draw.ubo.ssr_params->upload(&sp, sizeof(sp));
		gfx().bind_uniform_buffer_base(7, draw.ubo.ssr_params);
	}

	draw.bind_texture_ptr(0, resolve_buffer);
	draw.bind_texture_ptr(1, last_reflect);
	draw.bind_texture_ptr(2, draw.tex.scene_motion);

	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}

void imgui_menu_ssr() {
	ImGui::DragFloat("roughness_fade", &ssr_roughness_fade, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("brdf_bias", &ssr_brdf_bias, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("anti_self_occ_bias", &ssr_anti_self_occ_bias, 0.001f, 0.0f, 1.0f);
	ImGui::DragFloat("edge_fade", &ssr_edge_fade, 0.01f, 0.0f, 0.5f);
	ImGui::DragFloat("max_trace_samples", &ssr_max_trace_samples, 1.f, 10.f, 200.f);
	ImGui::DragFloat("intensity", &ssr_intensity, 0.01f, 0.0f, 2.0f);
	ImGui::DragFloat("fade_out_distance", &ssr_fade_out_distance, 10.f, 100.f, 50000.f);
	ImGui::DragFloat("temporal_response", &ssr_temporal_response, 0.01f, 0.0f, 1.0f);
	ImGui::SliderInt("resolve_samples", &ssr_resolve_samples, 1, 8);
	ImGui::Checkbox("fullres_trace", &ssr_fullres_trace);
}
ADD_TO_DEBUG_MENU(imgui_menu_ssr);

void SSRSystem::execute() {
	GPUSCOPESTART(ssr_system_execute);
	ASSERT(SSRSystem::inst != nullptr);

	const auto& vs = draw.current_frame_view;
	int trace_w = ssr_fullres_trace ? vs.width : vs.width / 2;
	int trace_h = ssr_fullres_trace ? vs.height : vs.height / 2;
	ensure_buffers(trace_w, trace_h);

	std::swap(draw.tex.reflection_accum, draw.tex.last_reflection_accum);
	temporalframe = (temporalframe + 1) % 2048;

	do_gaussian_mipchain();
	do_raytrace();
	do_resolve();
	do_temporal();

	// Restore full-res viewport for subsequent passes (SSAO, DDGI lighting)
	gfx().set_viewport(0, 0, vs.width, vs.height);
}
