#include "DrawLocal.h"
#include "imgui.h"
#include "glad/glad.h"

#include "GameEnginePublic.h"

static const ivec3 volfog_sizes[] = {{0, 0, 0}, {160, 90, 128}, {80, 45, 64}};

struct Vfog_Light
{
	glm::vec4 position_type;
	glm::vec4 color;
	glm::vec4 direction_coneangle;
};
struct Vfog_Params
{
	glm::ivec4 volumesize;
	glm::vec4 volspread_frustumend;
	glm::vec4 reprojection;
	glm::mat4 last_frame_viewproj;
	glm::vec4 density_ani;
};

void Volumetric_Fog_System::init() {
	if (quality == 0)
		return;

	voltexturesize = volfog_sizes[1];

	glGenTextures(1, &texture.volume);
	glBindTexture(GL_TEXTURE_3D, texture.volume);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, voltexturesize.x, voltexturesize.y, voltexturesize.z, 0, GL_RGBA,
				 GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &texture.last_volume);
	glBindTexture(GL_TEXTURE_3D, texture.last_volume);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, voltexturesize.x, voltexturesize.y, voltexturesize.z, 0, GL_RGBA,
				 GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE); // REEE!!!!!!!!!!!!!

	glCreateBuffers(1, &buffer.param);

	glCheckError();
}

void vfogmenu() {
	ImGui::InputFloat("den", &draw.volfog.density);
	ImGui::InputFloat("falloff", &draw.volfog.falloff);
	ImGui::InputFloat("ofs", &draw.volfog.offset);

	ImGui::InputFloat("ani", &draw.volfog.anisotropy);
	ImGui::InputFloat("spread", &draw.volfog.spread);
}
ADD_TO_DEBUG_MENU(vfogmenu);
void Volumetric_Fog_System::compute() {
	if (!enable_volumetric_fog.get_bool())
		return;
#if 1
	GPUFUNCTIONSTART;

	static Vfog_Light light_buffer[64];
	int num_lights = 0;
	{
		// Level_Light& l = draw.dyn_light;
		// Vfog_Light& vfl = light_buffer[num_lights++];
		// float type = (l.type == LIGHT_POINT) ? 0.0 : 1.0;
		// vfl.position_type = vec4(l.position, type);
		// vfl.color = vec4(l.color, 0.0);
		// vfl.direction_coneangle = vec4(l.direction, l.spot_angle);
	}

	Vfog_Params params{};

	const auto& last_vs = draw.last_frame_main_view;

	params.volumesize = glm::ivec4(voltexturesize, 0);
	params.volspread_frustumend = vec4(spread, frustum_end, 0, 0);
	params.last_frame_viewproj = last_vs.viewproj;
	params.reprojection = vec4(temporal_sequence, 0.1, 0, 0);

	params.density_ani = glm::vec4(draw.volfog.density, draw.volfog.anisotropy, falloff, offset);

	glBindBuffer(GL_UNIFORM_BUFFER, buffer.param);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(Vfog_Params), &params, GL_DYNAMIC_DRAW);

	glBindBufferBase(GL_UNIFORM_BUFFER, 4, buffer.param);
	// glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, buffer.light);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, draw.ubo.current_frame);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, draw.buf.lighting_uniforms->get_internal_handle());

	glCheckError();
	ivec3 groups = ceil(vec3(voltexturesize) / vec3(8, 8, 1));
	{
		draw.get_device().set_shader(prog.lightcalc);
		// prog.lightcalc.use();

		// prog.lightcalc.set_mat4("InvViewProj", glm::inverse(draw.vs.viewproj));
		// prog.lightcalc.set_vec3("ViewPos", draw.vs.origin);
		// glUniform3i(glGetUniformLocation(prog.lightcalc.ID, "TextureSize"), voltexturesize.x, voltexturesize.y,
		// voltexturesize.z);

		// prog.lightcalc.set_float("znear", draw.vs.near);
		// prog.lightcalc.set_float("zfar", draw.vs.far);
		// prog.lightcalc.set_mat4("InvView", glm::inverse(draw.vs.view));
		// prog.lightcalc.set_mat4("InvProjection", glm::inverse(draw.vs.proj));

		// prog.lightcalc.set_float("density", draw.vfog.x);
		// prog.lightcalc.set_float("anisotropy", draw.vfog.y);
		// prog.lightcalc.set_vec3("ambient", draw.ambientvfog);

		// prog.lightcalc.set_vec3("spotlightpos", vec3(0, 2, 0));
		// prog.lightcalc.set_vec3("spotlightnormal", vec3(0, -1, 0));
		// prog.lightcalc.set_float("spotlightangle", 0.5);
		// prog.lightcalc.set_vec3("spotlightcolor", vec3(10.f));
		draw.bind_texture(0, texture.last_volume);
		// glActiveTexture(GL_TEXTURE0);
		// glBindTexture(GL_TEXTURE_3D, texture.last_volume);
		// glActiveTexture(GL_TEXTURE1);
		// glBindTexture(GL_TEXTURE_3D, draw.perlin3d.id);
		// prog.lightcalc.set_vec3("perlin_offset", glm::vec3(eng->get_game_time() * 0.2, 0, eng->get_game_time()));

		draw.shader().set_int("num_lights", draw.scene.light_list.objects.size());
		glCheckError();

		glBindImageTexture(2, texture.volume, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		draw.bind_texture_ptr(1, draw.spotShadows->get_atlas().get_atlas_texture());
		extern void set_shit_fuck();
		set_shit_fuck();
		draw.bind_texture_ptr(4, draw.ddgi->probe_irradiance);
		draw.bind_texture_ptr(5, draw.ddgi->probe_depth);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 15, draw.ddgi->ddgi_probe_avg_value->get_internal_handle());

		glDispatchCompute(groups.x, groups.y, groups.z);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glCheckError();
	}
	// static Config_Var* fog_raymarch = cfg.get_var("dbg/raymarch", "1");
	if (1) {
		draw.set_shader(prog.raymarch);
		// prog.raymarch.use();
		// prog.raymarch.set_float("znear", draw.vs.near);
		// prog.raymarch.set_float("zfar", draw.vs.far);
		//	glUniform3i(glGetUniformLocation(prog.raymarch.ID, "TextureSize"), voltexturesize.x, voltexturesize.y,
		// voltexturesize.z);

		glBindImageTexture(5, texture.last_volume, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glBindImageTexture(2, texture.volume, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);

		glDispatchCompute(groups.x, groups.y, 1);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glCheckError();

		// swap, rendering with voltexture
		std::swap(texture.volume, texture.last_volume);
	}

	temporal_sequence = (temporal_sequence + 1) % 16;
#endif
}
