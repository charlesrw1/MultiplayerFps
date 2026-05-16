#include "DrawLocal.h"
#include "imgui.h"
#include "glm/gtc/type_ptr.hpp"
#include "GameEnginePublic.h"
#include <random>

#include "Assets/AssetDatabase.h"
#include "IGraphicsDevice.h"

void draw_hbao_menu() {
	ImGui::DragFloat("radius", &draw.ssao.tweak.radius, 0.05, 0.f);
	ImGui::DragFloat("sharpness", &draw.ssao.tweak.blur_sharpness, 0.05, 0);
	ImGui::DragFloat("bias", &draw.ssao.tweak.bias, 0.001, 0, 0.999);
	ImGui::DragFloat("intensity", &draw.ssao.tweak.intensity, 0.05, 0);
}

static const int NOISE_RES = 4;
static const int NUM_MRT = 8;

void SSAO_System::init() {

	texture.result_vts_handle = Texture::install_system("_ssao_result");
	texture.blur_vts_handle = Texture::install_system("_ssao_blur");
	texture.view_normal_vts_handle = Texture::install_system("_ssao_view_normal");
	texture.linear_depth_vts_handle = Texture::install_system("_linear_depth");

	Debug_Interface::get()->add_hook("hbao", draw_hbao_menu);

	make_render_targets(true, g_window_w.get_integer(), g_window_h.get_integer());
	reload_shaders();

	{
		CreateBufferArgs args;
		args.size = sizeof(gpu::HBAOData);
		args.flags = BUFFER_USE_DYNAMIC;
		ubo.data = gfx().create_buffer(args);
	}

	std::mt19937 rmt;

	const float numDir = 8; // keep in sync to glsl

	signed short hbaoRandomShort[RANDOM_ELEMENTS * 4];

	for (int i = 0; i < RANDOM_ELEMENTS; i++) {
		float Rand1 = static_cast<float>(rmt()) / 4294967296.0f;
		float Rand2 = static_cast<float>(rmt()) / 4294967296.0f;

		// Use random rotation angles in [0,2PI/NUM_DIRECTIONS)
		float Angle = glm::two_pi<float>() * Rand1 / numDir;
		random_elements[i].x = cosf(Angle);
		random_elements[i].y = sinf(Angle);
		random_elements[i].z = Rand2;
		random_elements[i].w = 0;
#define SCALE ((1 << 15))
		hbaoRandomShort[i * 4 + 0] = (signed short)(SCALE * random_elements[i].x);
		hbaoRandomShort[i * 4 + 1] = (signed short)(SCALE * random_elements[i].y);
		hbaoRandomShort[i * 4 + 2] = (signed short)(SCALE * random_elements[i].z);
		hbaoRandomShort[i * 4 + 3] = (signed short)(SCALE * random_elements[i].w);
#undef SCALE
	}

	{
		CreateTextureArgs args;
		args.format = GraphicsTextureFormat::rgba16_snorm;
		args.width = 4;
		args.height = 4;
		args.sampler_type = GraphicsSamplerType::NearestClamped;
		texture.random = gfx().create_texture(args);
		texture.random->sub_image_upload(0, 0, 0, 4, 4,
										 (int)sizeof(hbaoRandomShort), hbaoRandomShort);
	}
}

Shader make_program(const char* vert, const char* frag, const std::string& defines = "") {
	Shader ret{};
	Shader::compile(&ret, vert, frag, defines);
	return ret;
}

void SSAO_System::reload_shaders() {
	auto& prog_man = draw.get_prog_man();
	prog.hbao_calc = prog_man.create_raster("fullscreenquad.txt", "hbao/hbao.txt");
	prog.hbao_blur = prog_man.create_raster("fullscreenquad.txt", "hbao/hbaoblur.txt");
	prog.hbao_deinterleave = prog_man.create_raster("fullscreenquad.txt", "hbao/hbaodeinterleave.txt");
	prog.hbao_reinterleave = prog_man.create_raster("fullscreenquad.txt", "hbao/hbaoreinterleave.txt");
	prog.linearize_depth = prog_man.create_raster("fullscreenquad.txt", "hbao/linearizedepth.txt");
	prog.make_viewspace_normals = prog_man.create_raster("fullscreenquad.txt", "hbao/viewnormal.txt");
}

void SSAO_System::make_render_targets(bool initial, int width, int height) {
	if (!initial) {
		safe_release(texture.depthlinear);
		safe_release(texture.viewnormal);
		safe_release(texture.blurred);
		safe_release(texture.result);
		safe_release(texture.deptharray);
		safe_release(texture.resultarray);
	}

	{
		CreateTextureArgs args;
		args.width = width;
		args.height = height;
		args.format = GraphicsTextureFormat::rg32f;
		args.sampler_type = GraphicsSamplerType::NearestClamped;
		texture.depthlinear = gfx().create_texture(args);
	}
	{
		CreateTextureArgs args;
		args.width = width;
		args.height = height;
		args.format = GraphicsTextureFormat::rgba8;
		args.sampler_type = GraphicsSamplerType::NearestClamped;
		texture.viewnormal = gfx().create_texture(args);
	}
	{
		CreateTextureArgs args;
		args.width = width;
		args.height = height;
		args.format = GraphicsTextureFormat::rg16f;
		args.sampler_type = GraphicsSamplerType::LinearClamped;
		texture.result = gfx().create_texture(args);
		texture.blurred = gfx().create_texture(args);
	}

	const int quarterWidth = ((width + 3) / 4);
	const int quarterHeight = ((height + 3) / 4);

	{
		CreateTextureArgs args;
		args.type = GraphicsTextureType::t2DArray;
		args.width = quarterWidth;
		args.height = quarterHeight;
		args.depth_3d = RANDOM_ELEMENTS;
		args.format = GraphicsTextureFormat::r32f;
		args.sampler_type = GraphicsSamplerType::NearestClamped;
		texture.deptharray = gfx().create_texture(args);
	}
	{
		CreateTextureArgs args;
		args.type = GraphicsTextureType::t2DArray;
		args.width = quarterWidth;
		args.height = quarterHeight;
		args.depth_3d = RANDOM_ELEMENTS;
		args.format = GraphicsTextureFormat::rg16f;
		args.sampler_type = GraphicsSamplerType::NearestClamped;
		texture.resultarray = gfx().create_texture(args);
	}

	this->width = width;
	this->height = height;

	texture.blur_vts_handle->update_specs_ptr(texture.result);
	texture.result_vts_handle->update_specs_ptr(texture.blurred);
	texture.view_normal_vts_handle->update_specs_ptr(texture.viewnormal);
	// texture.linear_depth_vts_handle->update_specs(texture.depthlinear, ...) -- not wired
}

#define USE_AO_LAYERED_SINGLEPASS 2
const int HBAO_RANDOM_ELEMENTS = 4 * 4;

void SSAO_System::update_ubo() {
	// projection

	const auto& viewsetup = draw.current_frame_view;

	mat4 proj_matrix = viewsetup.proj;
	float proj_fov = viewsetup.fov;

	const float* P = glm::value_ptr(proj_matrix);

	float projInfoPerspective[] = {
		2.0f / (P[4 * 0 + 0]),				   // (x) * (R - L)/N
		2.0f / (P[4 * 1 + 1]),				   // (y) * (T - B)/N
		-(1.0f - P[4 * 2 + 0]) / P[4 * 0 + 0], // L/N
		-(1.0f + P[4 * 2 + 1]) / P[4 * 1 + 1], // B/N
	};

	float projInfoOrtho[] = {
		2.0f / (P[4 * 0 + 0]),				   // ((x) * R - L)
		2.0f / (P[4 * 1 + 1]),				   // ((y) * T - B)
		-(1.0f + P[4 * 3 + 0]) / P[4 * 0 + 0], // L
		-(1.0f - P[4 * 3 + 1]) / P[4 * 1 + 1], // B
	};

	int useOrtho = false;
	data.projOrtho = useOrtho;
	data.projInfo = useOrtho ? glm::make_vec4(projInfoOrtho) : glm::make_vec4(projInfoPerspective);

	float projScale;
	if (useOrtho) {
		projScale = float(height) / (projInfoOrtho[1]);
	} else {
		projScale = float(height) / (tanf(proj_fov * 0.5f) * 2.0f);
	}

	// radius
	float meters2viewspace = 1.0f;
	float R = tweak.radius * meters2viewspace;
	data.R2 = R * R;
	data.NegInvR2 = -1.0f / data.R2;
	data.RadiusToScreen = R * 0.5f * projScale;

	// ao
	data.PowExponent = std::max(tweak.intensity, 0.0f);
	data.NDotVBias = std::min(std::max(0.0f, tweak.bias), 1.0f);
	data.AOMultiplier = 1.0f / (1.0f - data.NDotVBias);

	// resolution
	int quarterWidth = ((width + 3) / 4);
	int quarterHeight = ((height + 3) / 4);

	data.InvQuarterResolution = vec2(1.0f / float(quarterWidth), 1.0f / float(quarterHeight));
	data.InvFullResolution = vec2(1.0f / float(width), 1.0f / float(height));

#if USE_AO_LAYERED_SINGLEPASS
	for (int i = 0; i < HBAO_RANDOM_ELEMENTS; i++) {
		data.float2Offsets[i] = vec4(float(i % 4) + 0.5f, float(i / 4) + 0.5f, 0.0f, 0.0f);
		data.jitters[i] = random_elements[i];
	}
#endif

	ubo.data->sub_upload(&data, (int)sizeof(gpu::HBAOData), 0);
}
ConfigVar r_ssao_blur("r.ssao_blur", "1", CVAR_BOOL | CVAR_DEV, "option to disable ssao blur for debug");
void SSAO_System::render() {

	GPUFUNCTIONSTART;
	const auto& viewsetup = draw.current_frame_view;
	int v_w = viewsetup.width;
	int v_h = viewsetup.height;

	if (width != v_w || height != v_h)
		make_render_targets(false, v_w, v_h);

	update_ubo();

	const int quarterWidth = ((width + 3) / 4);
	const int quarterHeight = ((height + 3) / 4);

	auto& device = draw.get_device();

	// linearize depth, writes to texture.depthlinear
	{
		auto targets = {ColorTargetInfo(texture.depthlinear)};
		RenderPassState pass;
		pass.color_infos = targets;
		gfx().set_render_pass(pass);

		RenderPipelineState state;
		state.vao = draw.get_empty_vao();
		state.depth_testing = state.depth_writes = false;
		state.program = prog.linearize_depth;
		device.set_pipeline(state);
		auto shader = device.shader();

		float near = viewsetup.near;
		float far = viewsetup.far;
		shader.set_vec4("clipInfo", glm::vec4(near * far, near - far, far, 1.0));
		shader.set_float("zNear", near);

		gfx().bind_texture(0, draw.tex.scene_depth);
		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);

		glCheckError();
	}

	// create viewspace normals, writes to texture.viewnormal
	{
		auto targets = {ColorTargetInfo(texture.viewnormal)};
		RenderPassState pass;
		pass.color_infos = targets;
		gfx().set_render_pass(pass);

		RenderPipelineState state;
		state.vao = draw.get_empty_vao();
		state.depth_testing = state.depth_writes = false;
		state.program = prog.make_viewspace_normals;
		device.set_pipeline(state);
		auto shader = device.shader();

		shader.set_int("projOrtho", 0);
		shader.set_vec4("projInfo", data.projInfo);
		shader.set_vec2("InvFullResolution", data.InvFullResolution);
		gfx().bind_texture(0, texture.depthlinear);
		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
	}

	// deinterleave, writes to texture.deptharray (each call attaches NUM_MRT
	// distinct array layers as separate color attachments and one quad fills them all).
	{
		RenderPipelineState state;
		state.vao = draw.get_empty_vao();
		state.depth_testing = state.depth_writes = false;
		state.program = prog.hbao_deinterleave;
		device.set_pipeline(state);
		auto shader = device.shader();

		gfx().bind_texture(0, texture.depthlinear);
		for (int i = 0; i < RANDOM_ELEMENTS; i += NUM_MRT) {
			ColorTargetInfo targets_arr[NUM_MRT] = {
				ColorTargetInfo(texture.deptharray, i + 0, 0),
				ColorTargetInfo(texture.deptharray, i + 1, 0),
				ColorTargetInfo(texture.deptharray, i + 2, 0),
				ColorTargetInfo(texture.deptharray, i + 3, 0),
				ColorTargetInfo(texture.deptharray, i + 4, 0),
				ColorTargetInfo(texture.deptharray, i + 5, 0),
				ColorTargetInfo(texture.deptharray, i + 6, 0),
				ColorTargetInfo(texture.deptharray, i + 7, 0),
			};
			RenderPassState pass;
			pass.color_infos = std::span<const ColorTargetInfo>(targets_arr, NUM_MRT);
			gfx().set_render_pass(pass);

			shader.set_vec4("info", glm::vec4(float(i % 4) + 0.5f, float(i / 4) + 0.5f, data.InvFullResolution.x,
											  data.InvFullResolution.y));
			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
		}
	}

	// calculate hbao, writes to texture.resultarray (one layer at a time).
	{
		RenderPipelineState state;
		state.vao = draw.get_empty_vao();
		state.depth_testing = state.depth_writes = false;
		state.program = prog.hbao_calc;
		device.set_pipeline(state);
		auto shader = device.shader();

		gfx().bind_texture(0, texture.deptharray);
		gfx().bind_texture(1, texture.viewnormal);
		gfx().bind_uniform_buffer_base(0, ubo.data);

		for (int i = 0; i < RANDOM_ELEMENTS; i++) {
			auto targets = {ColorTargetInfo(texture.resultarray, i, 0)};
			RenderPassState pass;
			pass.color_infos = targets;
			gfx().set_render_pass(pass);

			shader.set_uint("primitive_id_custom", i);
			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
		}
	}

	// reinterleave, writes to texture.result
	{
		auto targets = {ColorTargetInfo(texture.result)};
		RenderPassState pass;
		pass.color_infos = targets;
		gfx().set_render_pass(pass);

		RenderPipelineState state;
		state.vao = draw.get_empty_vao();
		state.depth_testing = state.depth_writes = false;
		state.program = prog.hbao_reinterleave;
		device.set_pipeline(state);

		gfx().bind_texture(0, texture.resultarray);
		gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
	}

	// depth aware blur — bounce result→blurred (horizontal), then blurred→result (vertical).
	if (r_ssao_blur.get_bool()) {
		RenderPipelineState state;
		state.vao = draw.get_empty_vao();
		state.depth_testing = state.depth_writes = false;
		state.program = prog.hbao_blur;

		// horizontal pass: read .result, write .blurred
		{
			auto targets = {ColorTargetInfo(texture.blurred)};
			RenderPassState pass;
			pass.color_infos = targets;
			gfx().set_render_pass(pass);

			device.set_pipeline(state);
			auto shader = device.shader();
			gfx().bind_texture(0, texture.result);
			shader.set_float("g_Sharpness", tweak.blur_sharpness);
			shader.set_vec2("g_InvResolutionDirection", glm::vec2(1.0f / float(width), 0));
			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
		}

		// vertical pass: read .blurred, write .result
		{
			auto targets = {ColorTargetInfo(texture.result)};
			RenderPassState pass;
			pass.color_infos = targets;
			gfx().set_render_pass(pass);

			device.set_pipeline(state);
			auto shader = device.shader();
			gfx().bind_texture(0, texture.blurred);
			shader.set_float("g_Sharpness", tweak.blur_sharpness);
			shader.set_vec2("g_InvResolutionDirection", glm::vec2(0, 1.0f / float(height)));
			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
		}
	}

	device.reset_states();
}