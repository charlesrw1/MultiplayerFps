#include "RenderExtra.h"
#include "Render/DrawLocal.h"
#include "imgui.h"
#include "Framework/ArenaAllocator.h"
#include "Framework/ArenaStd.h"

SSRSystem* SSRSystem::inst = nullptr;
SSRSystem::SSRSystem() {
	ASSERT(gfx_is_initialized());

	ssr_compute = draw.get_prog_man().create_raster("fullscreenquad.txt", "ssr_f.txt");
	hiz_downsample = draw.get_prog_man().create_compute("DepthPyramidC.txt");
	ssr_downsample = draw.get_prog_man().create_raster("fullscreenquad.txt", "ssr_downsample.txt");
	ssr_upsample = draw.get_prog_man().create_raster("fullscreenquad.txt", "ssr_upsample.txt");
	ssr_blur = draw.get_prog_man().create_raster("fullscreenquad.txt", "blur_ssr.txt");

	temporal_upsample = draw.get_prog_man().create_raster("fullscreenquad.txt", "temporal_upsample_ssr.txt");

	Texture::install_system("_depth_pyramid2");
	CreateSamplerArgs sargs;
	sargs.min_filter = GraphicsSamplerFilter::LinearMipmapNearest;
	sargs.mag_filter = GraphicsSamplerFilter::Linear;
	sargs.wrap       = GraphicsSamplerWrap::ClampToEdge;
	// max, takes the closest value to the camera. depth buffer stored in reverse-Z [1,0]
	sargs.reduction  = GraphicsSamplerReduction::Max;
	hiz_max_sampler  = gfx().create_sampler(sargs);
}

void SSRSystem::compute_depth() {
	GPUSCOPESTART(ssr_compute_depth_pyramid);
	ASSERT(depth_pyramid != nullptr);

	draw.get_device().set_shader(hiz_downsample);
	const int levels = Texture::get_mip_map_count(actual_depth_size.x, actual_depth_size.y);
	int width = actual_depth_size.x;
	int height = actual_depth_size.y;
	gfx().bind_sampler(0, hiz_max_sampler);
	for (int level = 0; level < levels; level++) {
		gfx().bind_image_for_compute(1, depth_pyramid, level, 0, GraphicsImageAccess::WriteOnly);
		if (level == 0)
			draw.get_device().bind_texture_ptr(0, draw.tex.scene_depth);
		else {
			draw.get_device().bind_texture_ptr(0, depth_pyramid);
		}

		int groups_x = glm::ceil(width / 32.f);
		int groups_y = glm::ceil(height / 32.f);
		draw.shader().set_float("width", width);
		draw.shader().set_float("height", height);
		const int level_to_sample = level == 0 ? 0 : level - 1;
		draw.shader().set_int("level", level_to_sample);

		gfx().dispatch_compute(groups_x, groups_y, 1);

		width /= 2.0;
		height /= 2.0;
		width = glm::max(width, 1);
		height = glm::max(height, 1);

		gfx().memory_barrier(BARRIER_SHADER_IMAGE_ACCESS | BARRIER_TEXTURE_FETCH);
	}

	gfx().bind_sampler(0, nullptr);
}

void SSRSystem::do_downsample() {
	ASSERT(ssr_downsample != 0);

	const auto& viewsetup = draw.current_frame_view;

	auto& device = draw.get_device();
	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = ssr_downsample;
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);
	const int num_mips = 5;
	glm::ivec2 size(viewsetup.width, viewsetup.height);
	glm::vec2 inv_presize = 1.f / glm::vec2(size);
	for (int i = 0; i < num_mips; i++) {
		size /= 2.f;
		auto targets = {ColorTargetInfo(draw.tex.scene_color_mipchain, -1, i)};
		RenderPassState rp;
		rp.color_infos = targets;
		gfx().set_render_pass(rp);
		int mip_to_fetch = (i == 0) ? 0 : i - 1;
		device.shader().set_int("mip_level", mip_to_fetch);
		device.shader().set_vec2("myimg_size", size);
		device.shader().set_vec2("inv_prev_size", inv_presize);
		if (i == 0)
			device.bind_texture_ptr(0, draw.tex.last_scene_color);
		else
			device.bind_texture_ptr(0, draw.tex.scene_color_mipchain);
		device.set_viewport(0, 0, size.x, size.y);
		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);

		inv_presize = 1.f / glm::vec2(size);
	}
}

static float lod_force = 1.0;
static bool debug_toggle = false;
ConfigVar r_ssr_res("r.ssr_res", "0", CVAR_INTEGER, "", 0, 2); // 0=full,1=half,2=quarter
ConfigVar r_ssr_num_samples("r.ssr_num_samples", "0", CVAR_INTEGER, "", 1, 4);

void SSRSystem::do_upsample() {
	GPUFUNCTIONSTART;
	ASSERT(ssr_upsample != 0);

	const auto& viewsetup = draw.current_frame_view;

	auto& device = draw.get_device();
	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = ssr_upsample;
	state.blend = BlendState::ADD;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);
	glm::ivec2 size(viewsetup.width, viewsetup.height);
	glm::vec2 inv_presize = 1.f / glm::vec2(size);

	device.bind_texture_ptr(0, draw.tex.halfres_scene_color);
	device.bind_texture_ptr(1, draw.tex.scene_gbuffer0);
	device.bind_texture_ptr(2, draw.tex.last_scene_color);
	device.bind_texture_ptr(3, draw.tex.scene_gbuffer2);
	device.bind_texture_ptr(4, draw.tex.scene_depth);
	device.bind_texture(5, EnviornmentMapHelper::get().integrator.get_texture());
	device.bind_texture_ptr(6, draw.tex.scene_color_mipchain);

	static int frame = 0;
	device.shader().set_int("temporal_frame", (frame++) % 4);
	device.shader().set_bool("debug_toggle", debug_toggle);
	device.shader().set_int("res_mode", r_ssr_res.get_integer());
	device.shader().set_int("num_samples_to_get", r_ssr_num_samples.get_integer());

	auto targets = {ColorTargetInfo(draw.tex.last_reflection_accum)};
	RenderPassState rp;
	rp.color_infos = targets;
	gfx().set_render_pass(rp);
	device.set_viewport(0, 0, viewsetup.width, viewsetup.height);

	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
	return;
	// now do blur and output

	state.vao = draw.get_empty_vao();
	state.program = ssr_blur;
	state.blend = BlendState::ADD;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);
	device.shader().set_vec2("texelSize", inv_presize);

	auto targets2 = {ColorTargetInfo(draw.tex.scene_color)};
	rp;
	rp.color_infos = targets2;
	gfx().set_render_pass(rp);
	device.set_viewport(0, 0, viewsetup.width, viewsetup.height);
	device.bind_texture_ptr(0, draw.tex.ddgi_accum);
	device.bind_texture_ptr(1, draw.tex.scene_depth);

	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}

ConfigVar ssr_temporal_blend("r.ssr_temporal_blend", "0.75", CVAR_FLOAT, "", 0, 1);
static float ssr_nonoccluded_weight_mult = 0.6;

void SSRSystem::do_temporal() {
	ASSERT(temporal_upsample != 0);

	auto& device = draw.get_device();

	auto last_reflect = draw.tex.last_reflection_accum;
	if (draw.wants_disable_temporal_effects_this_frame())
		last_reflect = draw.black_texture;

	RenderPassState rp;

	// now do temporal upsample
	auto targets2 = {ColorTargetInfo(draw.tex.reflection_accum)};
	rp.color_infos = targets2;
	gfx().set_render_pass(rp);

	RenderPipelineState state{};
	state.blend = BlendState::OPAQUE;
	state.program = temporal_upsample;
	device.set_pipeline(state);
	auto& vs = draw.current_frame_view;
	device.set_viewport(0, 0, vs.width, vs.height);
	draw.bind_texture_ptr(0, draw.tex.halfres_scene_color);
	draw.bind_texture_ptr(1, last_reflect);
	draw.bind_texture_ptr(2, draw.tex.scene_depth);
	draw.bind_texture_ptr(3, draw.tex.scene_motion);
	draw.bind_texture_ptr(4, draw.tex.last_scene_motion);

	// FIXME
	extern ConfigVar r_taa_blend;
	extern ConfigVar r_taa_flicker_remove;
	extern ConfigVar r_taa_reproject;
	extern ConfigVar r_taa_dilate_velocity;
	extern float taa_doc_mult;
	extern float taa_doc_vel_bias;
	extern float taa_doc_bias;
	extern float taa_doc_pow;

	auto the_shader = device.shader();
	the_shader.set_float("amt", ssr_temporal_blend.get_float());
	the_shader.set_bool("remove_flicker", r_taa_flicker_remove.get_bool());
	the_shader.set_mat4("lastViewProj", draw.last_frame_main_view.viewproj);
	the_shader.set_bool("use_reproject", r_taa_reproject.get_bool());
	the_shader.set_float("doc_mult", taa_doc_mult);
	the_shader.set_float("doc_vel_bias", taa_doc_vel_bias);
	the_shader.set_float("doc_bias", taa_doc_bias);
	the_shader.set_float("doc_pow", taa_doc_pow);
	the_shader.set_bool("dilate_velocity", r_taa_dilate_velocity.get_bool());
	the_shader.set_ivec2("halfres_texel_offset", get_frame_offset());
	the_shader.set_float("ssr_nonoccluded_weight_mult", ssr_nonoccluded_weight_mult);
	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
}

static int max_steps = 40;
static float bias = 0.05;
static float step_size = 0.2;
static float max_dist = 100.0;
static float max_thick = 0.07;

void draw_imgui_for_cvar(ConfigVar& var) {
	ASSERT(var.get_name() != nullptr);

	auto flags = var.get_var_flags();
	if (flags & CVAR_BOOL) {
		bool b = var.get_bool();
		if (ImGui::Checkbox(var.get_name(), &b)) {
			var.set_bool(b);
		}
	} else if (flags & CVAR_INTEGER) {
		int i = var.get_integer();
		if (flags & CVAR_UNBOUNDED) {
			if (ImGui::InputInt(var.get_name(), &i)) {
				var.set_integer(i);
			}
		} else {
			int min = var.get_min_val();
			int max = var.get_max_val();
			if (ImGui::SliderInt(var.get_name(), &i, min, max)) {
				var.set_integer(i);
			}
		}
	}
}

static float ssr_brdf_bias = 1.0;
static float ssr_mip_bias = 2;
static float ssr_max_roughness = 0.7;
static int random_repeat = 32;
static int traces_per_pixel = 1;

void imgui_menu_ssr() {
	ImGui::InputFloat("bias", &bias);
	ImGui::InputFloat("step_size", &step_size);
	ImGui::InputFloat("max_dist", &max_dist);
	ImGui::InputFloat("max_thick", &max_thick);
	ImGui::InputFloat("lod_force", &lod_force);
	ImGui::Checkbox("debug_toggle", &debug_toggle);
	ImGui::InputInt("max_steps", &max_steps);
	ImGui::DragFloat("ssr_nonoccluded_weight_mult", &ssr_nonoccluded_weight_mult, 0.01, 0, 1);
	ImGui::DragFloat("ssr_brdf_bias", &ssr_brdf_bias, 0.01, 0.5, 1);
	ImGui::DragFloat("ssr_mip_bias", &ssr_mip_bias, 0.1, 1.0, 9);
	ImGui::DragFloat("ssr_max_roughness", &ssr_max_roughness, 0.05, 0.0, 1.0);
	ImGui::SliderInt("traces_per_pixel", &traces_per_pixel, 1, 4);
	ImGui::InputInt("random_repeat", &random_repeat);

	draw_imgui_for_cvar(r_ssr_res);
	draw_imgui_for_cvar(r_ssr_num_samples);
}
ADD_TO_DEBUG_MENU(imgui_menu_ssr);

void SSRSystem::execute_compute() {
	GPUSCOPESTART(ssr_system_execute);
	ASSERT(SSRSystem::inst != nullptr);

	// swap here
	std::swap(draw.tex.reflection_accum, draw.tex.last_reflection_accum);

	increment_temporal_frame();

	// compute depth pyramid
	const auto& viewsetup = draw.current_frame_view;
	int v_w = viewsetup.width / 2;
	int v_h = viewsetup.height / 2;
	if (depth_size.x != v_w || depth_size.y != v_h)
		init_depth_pyramid(v_w, v_h);
	// compute_depth();

	do_downsample();

	// compute ssr
	auto& device = draw.get_device();
	auto targets = {ColorTargetInfo(draw.tex.halfres_scene_color, -1, 0)};
	RenderPassState rp;
	rp.color_infos = targets;
	gfx().set_render_pass(rp);

	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.program = ssr_compute;
	state.blend = BlendState::OPAQUE;
	state.depth_testing = false;
	state.depth_writes = false;
	device.set_pipeline(state);
	device.shader().set_int("MAX_STEPS", max_steps);
	device.shader().set_float("max_distance", max_dist);
	device.shader().set_float("bias", bias);
	device.shader().set_float("step_size", step_size);
	device.shader().set_float("max_thickness", max_thick);
	device.shader().set_bool("debug_toggle", debug_toggle);
	auto time = GetTime();
	static int index = 0;
	device.shader().set_float("temporalTime", float(index++));
	r_ssr_res.set_integer(2);
	device.shader().set_int("res_mode", r_ssr_res.get_integer());
	device.shader().set_float("ssr_max_roughness", ssr_max_roughness);
	device.shader().set_float("ssr_brdf_bias", ssr_brdf_bias);
	device.shader().set_float("ssr_mip_bias", ssr_mip_bias);
	device.shader().set_int("random_repeat", random_repeat);
	device.shader().set_int("traces_per_pixel", traces_per_pixel);
	device.shader().set_mat4("lastViewProj", draw.last_frame_main_view.viewproj);

	glm::ivec2 texel_offset{};
	if (r_ssr_res.get_integer() == 1) {
		texel_offset.y = index % 2;
		device.set_viewport(0, 0, viewsetup.width, viewsetup.height / 2);
	} else if (r_ssr_res.get_integer() == 2) {
		texel_offset.y = index % 2;
		texel_offset.x = (index % 4) / 2;
		device.set_viewport(0, 0, viewsetup.width / 2, viewsetup.height / 2);
	} else {
		device.set_viewport(0, 0, viewsetup.width, viewsetup.height);
	}
	device.shader().set_ivec2("texel_offset", get_frame_offset());

	device.shader().set_mat4("g_proj", viewsetup.proj);
	device.bind_texture_ptr(0, draw.tex.scene_gbuffer0);
	device.bind_texture_ptr(1, draw.tex.scene_gbuffer1);
	device.bind_texture_ptr(2, draw.tex.scene_gbuffer2);
	gfx().bind_sampler(3, hiz_max_sampler);
	device.bind_texture_ptr(3, depth_pyramid);
	device.bind_texture_ptr(4, draw.tex.scene_depth);
	device.bind_texture_ptr(5, draw.tex.last_scene_color);
	device.bind_texture_ptr(6, draw.tex.scene_color_mipchain);

	gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
	gfx().bind_sampler(3, nullptr);

	// do_upsample();

	do_temporal();
}

extern uint32_t previousPow2(uint32_t v);
void SSRSystem::init_depth_pyramid(int w, int h) {
	ASSERT(w > 0 && h > 0);

	depth_size = {w, h};
	if (depth_pyramid)
		depth_pyramid->release();

	auto actual_width = previousPow2(w) * 2;
	auto actual_height = previousPow2(h) * 2;
	actual_depth_size = {actual_width, actual_height};

	CreateTextureArgs args;
	args.num_mip_maps = Texture::get_mip_map_count(actual_width, actual_height);
	args.width = actual_width;
	args.height = actual_height;
	args.type = GraphicsTextureType::t2D;
	args.sampler_type = GraphicsSamplerType::NearestClamped;
	args.format = GraphicsTextureFormat::r32f;

	depth_pyramid = gfx().create_texture(args);

	auto t = Texture::load("_depth_pyramid2");
	t->update_specs_ptr(depth_pyramid);
}
