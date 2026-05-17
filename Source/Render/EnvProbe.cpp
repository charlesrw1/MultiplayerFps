#include "EnvProbe.h"
#include "glad/glad.h"
#include <glm/ext.hpp>
#include <vector>
#include "Render/Texture.h"
#include "Framework/MeshBuilder.h"
#include "DrawLocal.h"
#include "IGraphicsDevice.h"
using glm::mat4;
using glm::vec3;

const int EnviornmentMapHelper::MAX_MIP_ROUGHNESS = Texture::get_mip_map_count(CUBEMAP_SIZE, CUBEMAP_SIZE);

EnviornmentMapHelper& EnviornmentMapHelper::get() {
	static EnviornmentMapHelper instance = EnviornmentMapHelper();
	return instance;
}
void EnviornmentMapHelper::init() {
	auto& prog_man = draw.get_prog_man();

	prefilter_irradiance = prog_man.create_raster("Helpers/EqrtCubemapV.txt", "Helpers/PrefilterIrradianceF.txt");

	prefilter_specular_new = prog_man.create_raster("Helpers/EqrtCubemapV.txt", "Helpers/PrefilterSpecularNewF.txt");

	static const float cube_verts[] = {
		-0.5f, -0.5f, -0.5f, 0.5f,	-0.5f, -0.5f, 0.5f,	 0.5f,	-0.5f,
		0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, -0.5f, -0.5f,

		-0.5f, -0.5f, 0.5f,	 0.5f,	-0.5f, 0.5f,  0.5f,	 0.5f,	0.5f,
		0.5f,  0.5f,  0.5f,	 -0.5f, 0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,

		-0.5f, 0.5f,  0.5f,	 -0.5f, 0.5f,  -0.5f, -0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,	0.5f,

		0.5f,  0.5f,  0.5f,	 0.5f,	0.5f,  -0.5f, 0.5f,	 -0.5f, -0.5f,
		0.5f,  -0.5f, -0.5f, 0.5f,	-0.5f, 0.5f,  0.5f,	 0.5f,	0.5f,

		-0.5f, -0.5f, -0.5f, 0.5f,	-0.5f, -0.5f, 0.5f,	 -0.5f, 0.5f,
		0.5f,  -0.5f, 0.5f,	 -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, -0.5f,

		-0.5f, 0.5f,  -0.5f, 0.5f,	0.5f,  -0.5f, 0.5f,	 0.5f,	0.5f,
		0.5f,  0.5f,  0.5f,	 -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,	-0.5f,
	};

	CreateBufferArgs bargs;
	bargs.flags = BUFFER_USE_AS_VB;
	vertex_buffer = gfx().create_buffer(bargs);
	vertex_buffer->upload(cube_verts, sizeof(cube_verts));
	auto cube_layout = {VertexLayout(0, 3, GraphicsVertexAttribType::float32, 3 * sizeof(float), 0)};
	CreateVertexInputArgs vargs;
	vargs.vertex = vertex_buffer;
	vargs.layout = cube_layout;
	vertex_input = gfx().create_vertex_input(vargs);

	cubemap_projection = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 100.f);
	cubemap_views[0] = glm::lookAt(vec3(0), vec3(1, 0, 0), vec3(0, -1, 0));
	cubemap_views[1] = glm::lookAt(vec3(0), vec3(-1, 0, 0), vec3(0, -1, 0));
	cubemap_views[2] = glm::lookAt(vec3(0), vec3(0, 1, 0), vec3(0, 0, 1));
	cubemap_views[3] = glm::lookAt(vec3(0), vec3(0, -1, 0), vec3(0, 0, -1));
	cubemap_views[4] = glm::lookAt(vec3(0), vec3(0, 0, 1), vec3(0, -1, 0));
	cubemap_views[5] = glm::lookAt(vec3(0), vec3(0, 0, -1), vec3(0, -1, 0));

	integrator.run();
}

#include "Texture.h"

// convolutes a rendered cubemap
void EnviornmentMapHelper::compute_specular_new(Texture* t // in-out cubemap, scene drawn to mip level 0
) {
	ASSERT(t && t->gpu_ptr);
	const glm::ivec2 size_vec = t->gpu_ptr->get_size();
	int size = size_vec.x;
	const int num_mips = Texture::get_mip_map_count(size, size);

	// Clamp sampling to mip 0 so the prefilter shader only ever reads the
	// freshly rendered scene level, never a higher mip we are about to write.
	t->gpu_ptr->set_mip_range(0, 0);

	auto& device = draw.get_device();
	device.reset_states();

	{
		RenderPipelineState state;
		state.depth_testing = state.depth_writes = false;
		state.vao = vertex_input->get_internal_handle();
		state.backface_culling = false;
		state.program = prefilter_specular_new;
		device.set_pipeline(state);
		auto shader = device.shader();

		gfx().bind_texture(0, t->gpu_ptr);

		for (int mip = 1 /* skip mip level 0*/; mip < num_mips; mip++) {
			size >>= 1;

			float roughness = (float)mip / (MAX_MIP_ROUGHNESS - 1);
			shader.set_float("roughness", roughness);

			for (int i = 0; i < 6; i++) {
				RenderPassState pass;
				auto color_infos = {ColorTargetInfo(t->gpu_ptr, i, mip)};
				pass.color_infos = color_infos;
				gfx().set_render_pass(pass);
				device.set_viewport(0, 0, size, size);

				shader.set_mat4("ViewProj", cubemap_projection * cubemap_views[i]);
				gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 36);
			}
		}
	}

	t->gpu_ptr->set_mip_range(0, num_mips);
	device.reset_states();
}

// causes pipeline stall to read back texture
void EnviornmentMapHelper::compute_irradiance_new(
	Texture* t,				  // in cubemap, scene draw to mip level 0
	glm::vec3 ambient_cube[6] // out 6 vec3s representing irradiance of cubemap
) {
	ASSERT(t && t->gpu_ptr);
	constexpr int irrad_size = 16;

	CreateTextureArgs tex_args;
	tex_args.type = GraphicsTextureType::tCubemap;
	tex_args.format = GraphicsTextureFormat::rgb16f;
	tex_args.width = irrad_size;
	tex_args.height = irrad_size;
	tex_args.num_mip_maps = 1;
	tex_args.sampler_type = GraphicsSamplerType::CubemapDefault;
	IGraphicsTexture* temp_tex = gfx().create_texture(tex_args);

	auto& device = draw.get_device();
	device.reset_states();
	{
		RenderPipelineState state;
		state.program = prefilter_irradiance;
		state.depth_testing = state.depth_writes = false;
		state.backface_culling = false;
		state.vao = vertex_input->get_internal_handle();
		device.set_pipeline(state);
		auto shader = device.shader();

		gfx().bind_texture(0, t->gpu_ptr);

		for (int i = 0; i < 6; i++) {
			RenderPassState pass;
			auto color_infos = {ColorTargetInfo(temp_tex, i, 0)};
			pass.color_infos = color_infos;
			gfx().set_render_pass(pass);

			shader.set_mat4("ViewProj", cubemap_projection * cubemap_views[i]);
			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 36);
		}
	}

	gfx().wait_for_gpu_idle();

	constexpr int face_floats = irrad_size * irrad_size * 3;
	std::vector<float> input(face_floats * 6);
	float weights[6];
	glm::vec3 dirs[6];
	for (int i = 0; i < 6; i++) {
		weights[i] = 0.f;
		ambient_cube[i] = glm::vec3(0.f);
		dirs[i] = -cubemap_views[i][2];

		const int ofs = i * face_floats;
		temp_tex->download(0, i, &input[ofs], face_floats * sizeof(float));
	}
	for (int i = 0; i < 6; i++) {
		glm::vec3 up = cubemap_views[i][1];
		glm::vec3 right = cubemap_views[i][0];

		const float inv_irrad_size = 1.0 / (irrad_size - 1);
		for (int y = 0; y < irrad_size; y++) {
			for (int x = 0; x < irrad_size; x++) {
				const float xf = (x * inv_irrad_size) * 2.0 - 1.0;
				const float yf = (y * inv_irrad_size) * 2.0 - 1.0;
				const int ofs = (i * irrad_size * irrad_size + y * irrad_size + x) * 3;
				glm::vec3 c = {input[ofs], input[ofs + 1], input[ofs + 2]};
				glm::vec3 v = glm::normalize(xf * right + yf * up + dirs[i]);
				for (int dir = i; dir < i + 1; dir++) {
					float weight = glm::max(dot(v, dirs[dir]), 0.0f);
					weight = weight * weight;
					weights[dir] += weight;
					ambient_cube[dir] += c * weight;
				}
			}
		}
	}
	for (int dir = 0; dir < 6; dir++) {
		ambient_cube[dir] /= weights[dir];
	}

	device.reset_states();
	safe_release(temp_tex);
}

void BRDFIntegration::drawdebug() {

	return;
#if 0
    MeshBuilder mb;
    mb.Begin();
    mb.Push2dQuad(vec2(0, 0), vec2(1, 1));
    mb.End();

    Renderer& d = draw;
    glCheckError();

    glCheckError();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glCheckError();
    glViewport(0, 0, 512, 512);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glCheckError();
    glDisable(GL_CULL_FACE);
    Shader& s = d.shader_list[d.S_TEXTURED];
    s.use();
    Texture* t = mats.find_texture("frog.jpg");
    glCheckError();
    glBindTexture(GL_TEXTURE_2D, t->get_internal_render_handle());
    glCheckError();
    s.set_mat4("Model", mat4(1));
    glCheckError();
    s.set_mat4("ViewProj", mat4(1));
    mb.Draw(GL_TRIANGLES);
    glCheckError();

    glEnable(GL_CULL_FACE);
    glCheckError();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    mb.Free();
    glCheckError();
#endif
}
#include "Framework/MeshBuilderImpl.h"
void BRDFIntegration::run() {
	static const float quad_verts[] = {-1, -1, 0, 1,  -1, 0, 1, 1, 0,

									   -1, -1, 0, -1, 1,  0, 1, 1, 0};

	safe_release(integrate_shader);
	integrate_shader = gfx().create_shader_vert_frag("MbTexturedV.txt", "Helpers/PreIntegrateF.txt");
	const int LUT_SIZE = EnviornmentMapHelper::BRDF_PREINTEGRATE_LUT_SIZE;
	glGenBuffers(1, &quadvbo);
	glGenVertexArrays(1, &quadvao);
	glBindVertexArray(quadvao);
	glBindBuffer(GL_ARRAY_BUFFER, quadvbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glGenTextures(1, &lut_id);
	glBindTexture(GL_TEXTURE_2D, lut_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, LUT_SIZE, LUT_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lut_id, 0);

	glGenTextures(1, &depth);
	glBindTexture(GL_TEXTURE_2D, depth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, LUT_SIZE, LUT_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		printf("Framebuffer not complete!\n");

	MeshBuilderDD dd;
	MeshBuilder mb;
	mb.Begin();
	mb.Push2dQuad(vec2(-1, 1), vec2(2, -2));
	mb.End();
	dd.init_from(mb);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glCheckError();
	glViewport(0, 0, 512, 512);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glCheckError();
	glDisable(GL_CULL_FACE);
	IGraphicsShader* s = integrate_shader;
	s->use();

	s->set_mat4("Model", mat4(1));
	glCheckError();
	s->set_mat4("ViewProj", mat4(1));
	dd.draw(MeshBuilderDD::TRIANGLES);
	glCheckError();

	glEnable(GL_CULL_FACE);
	glCheckError();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	dd.free();
	glCheckError();
}