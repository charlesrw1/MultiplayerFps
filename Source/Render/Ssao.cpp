#include "DrawLocal.h"
#include "imgui.h"
#include "glad/glad.h"
#include "glm/gtc/type_ptr.hpp"
#include "GameEnginePublic.h"
#include <random>

#include "Assets/AssetDatabase.h"


void draw_hbao_menu()
{
	ImGui::DragFloat("radius", &draw.ssao.tweak.radius, 0.05, 0.f);
	ImGui::DragFloat("sharpness", &draw.ssao.tweak.blur_sharpness, 0.05, 0);
	ImGui::DragFloat("bias", &draw.ssao.tweak.bias, 0.001,0, 0.999);
	ImGui::DragFloat("intensity", &draw.ssao.tweak.intensity, 0.05, 0);
}

static const int NOISE_RES = 4;
static const int NUM_MRT = 8;

void SSAO_System::init()
{

	texture.result_vts_handle = Texture::install_system("_ssao_result");
	texture.blur_vts_handle = Texture::install_system("_ssao_blur");
	texture.view_normal_vts_handle = Texture::install_system("_ssao_view_normal");
	texture.linear_depth_vts_handle = Texture::install_system("_linear_depth");

	Debug_Interface::get()->add_hook("hbao", draw_hbao_menu);

	make_render_targets(true, g_window_w.get_integer(), g_window_h.get_integer());
	reload_shaders();

	glCreateBuffers(1, &ubo.data);
	glNamedBufferStorage(ubo.data, sizeof(gpu::HBAOData), nullptr, GL_DYNAMIC_STORAGE_BIT);

	std::mt19937 rmt;

	float numDir = 8;  // keep in sync to glsl

	signed short hbaoRandomShort[RANDOM_ELEMENTS * 4];

	for (int i = 0; i < RANDOM_ELEMENTS; i++)
	{
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

	glCreateTextures(GL_TEXTURE_2D, 1, &texture.random);
	glTextureStorage2D(texture.random, 1, GL_RGBA16_SNORM, 4, 4);
	glTextureSubImage2D(texture.random, 0, 0, 0, 4, 4, GL_RGBA, GL_SHORT, hbaoRandomShort);
	glTextureParameteri(texture.random, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(texture.random, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

Shader make_program(const char* vert, const char* frag, const std::string& defines = "")
{
	Shader ret{};
	Shader::compile(&ret, vert, frag, defines);
	return ret;
}


void SSAO_System::reload_shaders()
{
	auto& prog_man = draw.get_prog_man();
	prog.hbao_calc= prog_man.create_raster_geo( "fullscreenquad.txt", "hbao/hbao.txt", "hbao/hbaoG.txt", {});
	prog.hbao_blur = prog_man.create_raster("fullscreenquad.txt", "hbao/hbaoblur.txt");
	prog.hbao_deinterleave = prog_man.create_raster("fullscreenquad.txt", "hbao/hbaodeinterleave.txt");
	prog.hbao_reinterleave = prog_man.create_raster("fullscreenquad.txt", "hbao/hbaoreinterleave.txt");
	prog.linearize_depth = prog_man.create_raster("fullscreenquad.txt", "hbao/linearizedepth.txt");
	prog.make_viewspace_normals = prog_man.create_raster("fullscreenquad.txt", "hbao/viewnormal.txt");
}


void SSAO_System::make_render_targets(bool initial, int width, int height)
{
	if (!initial) {
		glDeleteTextures(1, &texture.depthlinear);
		glDeleteFramebuffers(1, &fbo.depthlinear);
		glDeleteTextures(1, &texture.viewnormal);
		glDeleteFramebuffers(1, &fbo.viewnormal);

		glDeleteTextures(1, &texture.result);
		glDeleteTextures(1, &texture.blur);

		glDeleteFramebuffers(1, &fbo.finalresolve);

		glDeleteFramebuffers(1, &fbo.hbao2_deinterleave);

		glDeleteTextures(1, &texture.deptharray);


		for (int i = 0; i < RANDOM_ELEMENTS; i++) {
			glDeleteTextures(1, &texture.depthview[i]);
		}

		glDeleteTextures(1, &texture.resultarray);


		glDeleteFramebuffers(1, &fbo.hbao2_calc);
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &texture.depthlinear);
	glTextureStorage2D(texture.depthlinear, 1, GL_RG32F, width, height);
	glTextureParameteri(texture.depthlinear, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture.depthlinear, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture.depthlinear, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(texture.depthlinear, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glCreateFramebuffers(1, &fbo.depthlinear);
	glNamedFramebufferTexture(fbo.depthlinear, GL_COLOR_ATTACHMENT0, texture.depthlinear, 0);

	glCreateTextures(GL_TEXTURE_2D, 1, &texture.viewnormal);
	glTextureStorage2D(texture.viewnormal, 1, GL_RGBA8, width, height);
	glTextureParameteri(texture.viewnormal, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture.viewnormal, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture.viewnormal, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(texture.viewnormal, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glCreateFramebuffers(1, &fbo.viewnormal);
	glNamedFramebufferTexture(fbo.viewnormal, GL_COLOR_ATTACHMENT0, texture.viewnormal, 0);

	glCreateTextures(GL_TEXTURE_2D, 1, &texture.result);
	glTextureStorage2D(texture.result, 1, GL_RG16F, width, height);
	glTextureParameteri(texture.result, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture.result, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glCreateTextures(GL_TEXTURE_2D, 1, &texture.blur);
	glTextureStorage2D(texture.blur, 1, GL_RG16F, width, height);
	glTextureParameteri(texture.blur, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture.blur, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glCreateFramebuffers(1, &fbo.finalresolve);
	glNamedFramebufferTexture(fbo.finalresolve, GL_COLOR_ATTACHMENT0, texture.result, 0);
	glNamedFramebufferTexture(fbo.finalresolve, GL_COLOR_ATTACHMENT1, texture.blur, 0);

	GLenum drawbuffers[NUM_MRT];
	for (int layer = 0; layer < NUM_MRT; layer++)
		drawbuffers[layer] = GL_COLOR_ATTACHMENT0 + layer;
	glCreateFramebuffers(1, &fbo.hbao2_deinterleave);
	glNamedFramebufferDrawBuffers(fbo.hbao2_deinterleave, NUM_MRT, drawbuffers);

	int quarterWidth = ((width + 3) / 4);
	int quarterHeight = ((height + 3) / 4);


	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texture.deptharray);
	glTextureStorage3D(texture.deptharray, 1, GL_R32F, quarterWidth, quarterHeight, RANDOM_ELEMENTS);
	glTextureParameteri(texture.deptharray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture.deptharray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture.deptharray, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(texture.deptharray, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	for (int i = 0; i < RANDOM_ELEMENTS; i++) {
		glGenTextures(1, &texture.depthview[i]);
		glTextureView(texture.depthview[i], GL_TEXTURE_2D, texture.deptharray, GL_R32F, 0, 1, i, 1);
		glBindTexture(GL_TEXTURE_2D, texture.depthview[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}


	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texture.resultarray);
	glTextureStorage3D(texture.resultarray, 1, GL_RG16F, quarterWidth, quarterHeight, RANDOM_ELEMENTS);
	glTextureParameteri(texture.resultarray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture.resultarray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture.resultarray, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(texture.resultarray, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glCreateFramebuffers(1, &fbo.hbao2_calc);
	glNamedFramebufferTexture(fbo.hbao2_calc, GL_COLOR_ATTACHMENT0, texture.resultarray, 0);

	// render viewspace normals and linear depth
	// deinterleave
	// render hbao for each layer
	// reinterleave
	// blur

	this->width = width;
	this->height = height;

	texture.blur_vts_handle->update_specs(texture.result, width, height, 2, {});
	texture.result_vts_handle->update_specs(texture.blur, width, height, 2, {});
	texture.view_normal_vts_handle->update_specs(texture.viewnormal, width, height, 4, {});
	texture.linear_depth_vts_handle->update_specs(texture.depthlinear, width, height, 4, {});
}

#define USE_AO_LAYERED_SINGLEPASS 2
const int HBAO_RANDOM_ELEMENTS = 4 * 4;

void SSAO_System::update_ubo()
{
	// projection

	const auto& viewsetup = draw.current_frame_view;

	mat4 proj_matrix = viewsetup.proj;
	float proj_fov = viewsetup.fov;

	const float* P = glm::value_ptr(proj_matrix);

	float projInfoPerspective[] = {
		2.0f / (P[4 * 0 + 0]),                  // (x) * (R - L)/N
		2.0f / (P[4 * 1 + 1]),                  // (y) * (T - B)/N
		-(1.0f - P[4 * 2 + 0]) / P[4 * 0 + 0],  // L/N
		-(1.0f + P[4 * 2 + 1]) / P[4 * 1 + 1],  // B/N
	};

	float projInfoOrtho[] = {
		2.0f / (P[4 * 0 + 0]),                  // ((x) * R - L)
		2.0f / (P[4 * 1 + 1]),                  // ((y) * T - B)
		-(1.0f + P[4 * 3 + 0]) / P[4 * 0 + 0],  // L
		-(1.0f - P[4 * 3 + 1]) / P[4 * 1 + 1],  // B
	};

	int useOrtho = false;
	data.projOrtho = useOrtho;
	data.projInfo = useOrtho ? glm::make_vec4(projInfoOrtho) : glm::make_vec4(projInfoPerspective);

	float projScale;
	if (useOrtho)
	{
		projScale = float(height) / (projInfoOrtho[1]);
	}
	else
	{
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
	for (int i = 0; i < HBAO_RANDOM_ELEMENTS; i++)
	{
		data.float2Offsets[i] = vec4(float(i % 4) + 0.5f, float(i / 4) + 0.5f, 0.0f, 0.0f);
		data.jitters[i] = random_elements[i];
	}
#endif

	glNamedBufferSubData(ubo.data, 0, sizeof(gpu::HBAOData), &data);
}
ConfigVar r_ssao_blur("r.ssao_blur", "1", CVAR_BOOL | CVAR_DEV,"option to disable ssao blur for debug");
void SSAO_System::render()
{

	GPUFUNCTIONSTART;
	const auto& viewsetup = draw.current_frame_view;
	int v_w = viewsetup.width;
	int v_h = viewsetup.height;


	if (width != v_w || height != v_h)
		make_render_targets(false, v_w,v_h);

	update_ubo();

	const int quarterWidth = ((width + 3) / 4);
	const int quarterHeight = ((height + 3) / 4);

	//*glViewport(0, 0, width, height);
	auto& device = draw.get_device();

	// linearize depth, writes to texture.depthlinear
	{
		RenderPassSetup setup("depthlinear", fbo.depthlinear, false, false, 0, 0, width, height);
		auto scope = device.start_render_pass(setup);

		RenderPipelineState state;
		state.vao = draw.get_empty_vao();
		state.depth_testing = state.depth_writes = false;
		state.program = prog.linearize_depth;
		device.set_pipeline(state);
		auto shader = device.shader();

		float near = viewsetup.near;
		float far = viewsetup.far;
		//*glBindFramebuffer(GL_FRAMEBUFFER, fbo.depthlinear);
		//prog.linearize_depth.use();
		shader.set_vec4("clipInfo", glm::vec4(
			near * far,
			near - far,
			far,
			1.0
		));
		shader.set_float("zNear", near);


		glBindTextureUnit(0, draw.tex.scene_depth);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glCheckError();
	}

	// create viewspace normals, writes to texture.viewnormal
	{
		RenderPassSetup setup("viewnormal", fbo.viewnormal, false, false, 0, 0, width, height);
		auto scope = device.start_render_pass(setup);

		RenderPipelineState state;
		state.vao = draw.get_empty_vao();
		state.depth_testing = state.depth_writes = false;
		state.program = prog.make_viewspace_normals;
		device.set_pipeline(state);
		auto shader = device.shader();

		//*glBindFramebuffer(GL_FRAMEBUFFER, fbo.viewnormal);
		//prog.make_viewspace_normals.use();
		shader.set_int("projOrtho", 0);
		shader.set_vec4("projInfo", data.projInfo);
		shader.set_vec2("InvFullResolution", data.InvFullResolution);
		glBindTextureUnit(0, texture.depthlinear);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	// deinterleave, writes to texture.deptharray
	{
		RenderPassSetup setup("hbao2_deinterleave", fbo.hbao2_deinterleave, false, false, 0, 0, quarterWidth, quarterHeight);
		auto scope = device.start_render_pass(setup);

		RenderPipelineState state;
		state.vao = draw.get_empty_vao();
		state.depth_testing = state.depth_writes = false;
		state.program = prog.hbao_deinterleave;
		device.set_pipeline(state);
		auto shader = device.shader();

		//*glBindFramebuffer(GL_FRAMEBUFFER, fbo.hbao2_deinterleave);
		//*glViewport(0, 0, quarterWidth, quarterHeight);
		glBindTextureUnit(0, texture.depthlinear);
		//prog.hbao_deinterleave.use();
		// two passes
		for (int i = 0; i < RANDOM_ELEMENTS; i += NUM_MRT) {
			shader.set_vec4("info", glm::vec4(
				float(i % 4) + 0.5f,
				float(i / 4) + 0.5f,
				data.InvFullResolution.x,
				data.InvFullResolution.y
			));

			for (int layer = 0; layer < NUM_MRT; layer++)
				glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + layer, texture.depthview[i + layer], 0);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	}

	// calculate hbao, writes to texture.resultarray
	{
		RenderPassSetup setup("hbao2_calc", fbo.hbao2_calc, false, false, 0, 0, quarterWidth, quarterHeight);
		auto scope = device.start_render_pass(setup);

		RenderPipelineState state;
		state.vao = draw.get_empty_vao();
		state.depth_testing = state.depth_writes = false;
		state.program = prog.hbao_calc;
		device.set_pipeline(state);
		auto shader = device.shader();

		//*glBindFramebuffer(GL_FRAMEBUFFER, fbo.hbao2_calc);
		//*glViewport(0, 0, quarterWidth, quarterHeight);
		glBindTextureUnit(0, texture.deptharray);
		glBindTextureUnit(1, texture.viewnormal);

		glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo.data);

		//prog.hbao_calc.use();

		glDrawArrays(GL_TRIANGLES, 0, 3 * RANDOM_ELEMENTS);
	}

	{
		RenderPassSetup setup("finalresolve+blur", fbo.finalresolve, false, false, 0, 0, width, height);
		auto scope = device.start_render_pass(setup);

		// reinterleave, writes to texture.result
		{
			RenderPipelineState state;
			state.vao = draw.get_empty_vao();
			state.depth_testing = state.depth_writes = false;
			state.program = prog.hbao_reinterleave;
			device.set_pipeline(state);
			auto shader = device.shader();

			//*glBindFramebuffer(GL_FRAMEBUFFER, fbo.finalresolve);
			//*glViewport(0, 0, width, height);
			glNamedFramebufferDrawBuffer(fbo.finalresolve, GL_COLOR_ATTACHMENT0);
			//*glDrawBuffer(GL_COLOR_ATTACHMENT0);
			//prog.hbao_reinterleave.use();

			glBindTextureUnit(0, texture.resultarray);

			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		// depth aware blur, writes to texture.result
		if(r_ssao_blur.get_bool())
		{
			RenderPipelineState state;
			state.vao = draw.get_empty_vao();
			state.depth_testing = state.depth_writes = false;
			state.program = prog.hbao_blur;
			device.set_pipeline(state);
			auto shader = device.shader();

			//prog.hbao_blur.use();
			// framebuffer = fbo.finalresolve
			glNamedFramebufferDrawBuffer(fbo.finalresolve, GL_COLOR_ATTACHMENT1);
			//*glDrawBuffer(GL_COLOR_ATTACHMENT1);
			glBindTextureUnit(0, texture.result);
			shader.set_float("g_Sharpness",
				tweak.blur_sharpness);
			shader.set_vec2("g_InvResolutionDirection", glm::vec2(
				1.0f / float(width),
				0
			));
			glDrawArrays(GL_TRIANGLES, 0, 3);	// read from .result and write to .blur

			glNamedFramebufferDrawBuffer(fbo.finalresolve, GL_COLOR_ATTACHMENT0);
			//*glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glBindTextureUnit(0, texture.blur);
			shader.set_vec2("g_InvResolutionDirection", glm::vec2(
				0,
				1.0f / float(height)
			));
			glDrawArrays(GL_TRIANGLES, 0, 3);	// read from .blur and write to .result
		}
	}

	device.reset_states();
	//*glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//glEnable(GL_DEPTH_TEST);
	//glUseProgram(0);
}