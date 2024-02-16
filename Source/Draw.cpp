#include "Draw.h"
#include "Util.h"
#include "glad/glad.h"
#include "Texture.h"
#include "Game_Engine.h"
#include "Profilier.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"

Renderer draw;

// Perlin noise generator taken from: https://www.shadertoy.com/view/slB3z3
uint32_t hash(uint32_t x, uint32_t seed) {
	const uint32_t m = 0x5bd1e995U;
	uint32_t hash = seed;
	// process input
	uint32_t k = x;
	k *= m;
	k ^= k >> 24;
	k *= m;
	hash *= m;
	hash ^= k;
	// some final mixing
	hash ^= hash >> 13;
	hash *= m;
	hash ^= hash >> 15;
	return hash;
}

// implementation of MurmurHash (https://sites.google.com/site/murmurhash/) for a  
// 3-dimensional unsigned integer input vector.

uint32_t hash(glm::uvec3 x, uint32_t seed) {
	const uint32_t m = 0x5bd1e995U;
	uint32_t hash = seed;
	// process first vector element
	uint32_t k = x.x;
	k *= m;
	k ^= k >> 24;
	k *= m;
	hash *= m;
	hash ^= k;
	// process second vector element
	k = x.y;
	k *= m;
	k ^= k >> 24;
	k *= m;
	hash *= m;
	hash ^= k;
	// process third vector element
	k = x.z;
	k *= m;
	k ^= k >> 24;
	k *= m;
	hash *= m;
	hash ^= k;
	// some final mixing
	hash ^= hash >> 13;
	hash *= m;
	hash ^= hash >> 15;
	return hash;
}


vec3 gradientDirection(uint32_t hash) {
	switch (int(hash) & 15) { // look at the last four bits to pick a gradient direction
	case 0:
		return vec3(1, 1, 0);
	case 1:
		return vec3(-1, 1, 0);
	case 2:
		return vec3(1, -1, 0);
	case 3:
		return vec3(-1, -1, 0);
	case 4:
		return vec3(1, 0, 1);
	case 5:
		return vec3(-1, 0, 1);
	case 6:
		return vec3(1, 0, -1);
	case 7:
		return vec3(-1, 0, -1);
	case 8:
		return vec3(0, 1, 1);
	case 9:
		return vec3(0, -1, 1);
	case 10:
		return vec3(0, 1, -1);
	case 11:
		return vec3(0, -1, -1);
	case 12:
		return vec3(1, 1, 0);
	case 13:
		return vec3(-1, 1, 0);
	case 14:
		return vec3(0, -1, 1);
	case 15:
		return vec3(0, -1, -1);
	}
}

float interpolate(float value1, float value2, float value3, float value4, float value5, float value6, float value7, float value8, vec3 t) {
	using glm::mix;
	return mix(
		mix(mix(value1, value2, t.x), mix(value3, value4, t.x), t.y),
		mix(mix(value5, value6, t.x), mix(value7, value8, t.x), t.y),
		t.z
	);
}

vec3 fade(vec3 t) {
	// 6t^5 - 15t^4 + 10t^3
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float perlinNoise(vec3 position, uint32_t seed) {
	vec3 floorPosition = floor(position);
	vec3 fractPosition = position - floorPosition;
	glm::uvec3 cellCoordinates = glm::uvec3(floorPosition);
	float value1 = dot(gradientDirection(hash(cellCoordinates, seed)), fractPosition);
	float value2 = dot(gradientDirection(hash((cellCoordinates + glm::uvec3(1, 0, 0)), seed)), fractPosition - vec3(1, 0, 0));
	float value3 = dot(gradientDirection(hash((cellCoordinates + glm::uvec3(0, 1, 0)), seed)), fractPosition - vec3(0, 1, 0));
	float value4 = dot(gradientDirection(hash((cellCoordinates + glm::uvec3(1, 1, 0)), seed)), fractPosition - vec3(1, 1, 0));
	float value5 = dot(gradientDirection(hash((cellCoordinates + glm::uvec3(0, 0, 1)), seed)), fractPosition - vec3(0, 0, 1));
	float value6 = dot(gradientDirection(hash((cellCoordinates + glm::uvec3(1, 0, 1)), seed)), fractPosition - vec3(1, 0, 1));
	float value7 = dot(gradientDirection(hash((cellCoordinates + glm::uvec3(0, 1, 1)), seed)), fractPosition - vec3(0, 1, 1));
	float value8 = dot(gradientDirection(hash((cellCoordinates + glm::uvec3(1, 1, 1)), seed)), fractPosition - vec3(1, 1, 1));
	return interpolate(value1, value2, value3, value4, value5, value6, value7, value8, fade(fractPosition));
}


Texture3d generate_perlin_3d(glm::ivec3 size, uint32_t seed, int octaves, int frequency, float persistence, float lacunarity)
{
	Texture3d newt;
	glGenTextures(1, &newt.id);
	newt.size = size;
	std::vector<uint8_t> values;
	values.resize(size.x * size.y * size.z);
	for (int z = 0; z < size.z; z++) {
		for (int y = 0; y < size.y; y++) {
			for (int x = 0; x < size.x; x++) {
				glm::ivec3 coord{ x,y,z };
				vec3 coordf = vec3(float(x) / size.x, float(y) / size.y, float(z) / size.z);
				float value = 0.0;
				float amplitude = 1.0;
				float current_frequency = float(frequency);
				uint32_t current_seed = seed;
				for (int oct = 0; oct < octaves; oct++) {
					current_seed = hash(current_seed, 0x0U);
					value += perlinNoise(coordf * current_frequency, current_seed) * amplitude;
					amplitude *= persistence;
					current_frequency *= lacunarity;
				}
				int ivalue = value * 255;
				glm::clamp(ivalue, 0, 255);
				values[size.y * size.x * z + size.x * y + x] = ivalue;
			}
		}
	}
	glBindTexture(GL_TEXTURE_3D, newt.id);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, size.x, size.y, size.y, 0, GL_RED, GL_UNSIGNED_BYTE, values.data());
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_3D, 0);

	return newt;
}


void Renderer::InitGlState()
{
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glClearColor(0.5f, 0.3f, 0.2f, 1.f);
	glEnable(GL_MULTISAMPLE);
	glDepthFunc(GL_LEQUAL);
}

void Renderer::draw_sprite_buffer()
{
	if (shadowverts.GetBaseVertex() == 0)
		return;
	shadowverts.End();
	if (sprite_state.in_world_space)
		shader().set_mat4("ViewProj", vs.viewproj);
	else
		shader().set_mat4("ViewProj", mat4(1));

	shadowverts.Draw(GL_TRIANGLES);
	shadowverts.Begin();

}
void Renderer::draw_sprite(glm::vec3 origin, Color32 color, glm::vec2 size, Texture* mat,
	bool billboard, bool in_world_space, bool additive, glm::vec3 orient_face)
{
	int tex = (mat) ? mat->gl_id : white_texture;
	if ((in_world_space != sprite_state.in_world_space || tex != sprite_state.current_t
		|| additive != sprite_state.additive))
		draw_sprite_buffer();

	sprite_state.in_world_space = in_world_space;
	if (sprite_state.current_t != tex || sprite_state.force_set) {
		bind_texture(BASE0_SAMPLER, tex);
		sprite_state.current_t = tex;
	}
	if (sprite_state.additive != additive || sprite_state.force_set) {
		if (additive) {
			glBlendFunc(GL_ONE, GL_ONE);
		}
		else {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		sprite_state.additive = additive;
	}
	sprite_state.force_set = false;

	MbVertex v[4];
	glm::vec3 side1;
	glm::vec3 side2;
	if (in_world_space)
	{
		if (billboard)
		{
			side1 = cross(draw.vs.front, vec3(0.f, 1.f, 0.f));
			side2 = cross(side1, draw.vs.front);
		}
		else
		{
			side1 = (glm::abs(orient_face.x) < 0.999) ? cross(orient_face, vec3(1, 0, 0)) : cross(orient_face, vec3(0, 1, 0));
			side2 = cross(side1, orient_face);
		}
	}
	else
	{
		side1 = glm::vec3(1, 0, 0);
		side2 = glm::vec3(0, 1, 0);
		glm::vec4 neworigin = vs.viewproj * vec4(origin, 1.0);
		neworigin /= neworigin.w;
		origin = neworigin;
	}
	int base = shadowverts.GetBaseVertex();
	glm::vec2 uvbase = glm::vec2(0);
	glm::vec2 uvsize = glm::vec2(1);

	v[0].position = origin - size.x * side1 + size.y * side2;
	v[3].position = origin + size.x * side1 + size.y * side2;
	v[2].position = origin + size.x * side1 - size.y * side2;
	v[1].position = origin - size.x * side1 - size.y * side2;
	v[0].uv = uvbase;
	v[3].uv = glm::vec2(uvbase.x + uvsize.x, uvbase.y);
	v[2].uv = uvbase + uvsize;
	v[1].uv = glm::vec2(uvbase.x, uvbase.y + uvsize.y);
	for (int j = 0; j < 4; j++) v[j].color = color;
	for (int j = 0; j < 4; j++)shadowverts.AddVertex(v[j]);
	shadowverts.AddQuad(base, base + 1, base + 2, base + 3);
}

void Renderer::AddBlobShadow(glm::vec3 org, glm::vec3 normal, float width)
{
	MbVertex corners[4];

	glm::vec3 side = (glm::abs(normal.x) < 0.999) ? cross(normal, vec3(1, 0, 0)) : cross(normal, vec3(0, 1, 0));
	side = glm::normalize(side);
	glm::vec3 side2 = cross(side, normal);

	float halfwidth = width / 2.f;

	for (int i = 0; i < 4; i++) corners[i].color = COLOR_BLACK;
	corners[0].position = org + side * halfwidth + side2 * halfwidth;
	corners[1].position = org - side * halfwidth + side2 * halfwidth;
	corners[2].position = org - side * halfwidth - side2 * halfwidth;
	corners[3].position = org + side * halfwidth - side2 * halfwidth;
	corners[0].uv = glm::vec2(0);
	corners[1].uv = glm::vec2(0, 1);
	corners[2].uv = glm::vec2(1, 1);
	corners[3].uv = glm::vec2(1, 0);
	int base = shadowverts.GetBaseVertex();
	for (int i = 0; i < 4; i++) {
		shadowverts.AddVertex(corners[i]);
	}
	shadowverts.AddQuad(base, base + 1, base + 2, base + 3);
}

void Renderer::bind_texture(int bind, int id)
{
	ASSERT(bind >= 0 && bind < NUM_SAMPLERS);
	if (cur_tex[bind] != id) {
		glActiveTexture(GL_TEXTURE0 + bind);
		glBindTexture(GL_TEXTURE_2D, id);
		cur_tex[bind] = id;
	}
}


void Renderer::set_shader_sampler_locations()
{
	shader().set_int("basecolor", BASE0_SAMPLER);
	shader().set_int("auxcolor", AUX0_SAMPLER);
	shader().set_int("basecolor2", BASE1_SAMPLER);
	shader().set_int("auxcolor2", AUX1_SAMPLER);
	shader().set_int("lightmap", LIGHTMAP_SAMPLER);
	shader().set_int("special", SPECIAL_SAMPLER);

	// >>> PBR BRANCH
	shader().set_int("pbr_irradiance_cubemaps", LIGHTMAP_SAMPLER + 1);
	shader().set_int("pbr_specular_cubemaps", LIGHTMAP_SAMPLER + 2);
	shader().set_int("PBR_brdflut", LIGHTMAP_SAMPLER + 3);
	shader().set_int("volumetric_fog", LIGHTMAP_SAMPLER + 4);
	shader().set_int("cascade_shadow_map", LIGHTMAP_SAMPLER + 5);
}

void Renderer::set_depth_shader_constants()
{

}

void set_standard_draw_data()
{
	int start = Renderer::LIGHTMAP_SAMPLER;
	// >>> PBR BRANCH
	glActiveTexture(GL_TEXTURE0 + start + 1);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, draw.scene.levelcubemapirradiance_array);
	glActiveTexture(GL_TEXTURE0 + start + 2);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, draw.scene.levelcubemapspecular_array);

	glActiveTexture(GL_TEXTURE0 + start + 3);
	glBindTexture(GL_TEXTURE_2D, EnviornmentMapHelper::get().integrator.lut_id);

	glActiveTexture(GL_TEXTURE0 + start + 5);
	glBindTexture(GL_TEXTURE_2D_ARRAY, draw.shadowmap.shadow_map_array);

	//shader().set_vec4("aoproxy_sphere", vec4(engine.local_player().position + glm::vec3(0,aosphere.y,0), aosphere.x));
	//shader().set_float("aoproxy_scale_factor", aosphere.z);

	glActiveTexture(GL_TEXTURE0 + start + 4);
	glBindTexture(GL_TEXTURE_3D, draw.volfog.voltexture);

	glBindBufferBase(GL_UNIFORM_BUFFER, 4, draw.volfog.param_ubo);
	glBindBufferBase(GL_UNIFORM_BUFFER, 8, draw.shadowmap.csm_ubo);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, draw.scene.cubemap_ssbo);

}

void Renderer::set_shader_constants()
{
	shader().set_vec3("light_dir", glm::normalize(-vec3(1)));
}

static int combine_flags_type(int flags, int type, int flag_bits)
{
	return flags + (type >> flag_bits);
}

void Renderer::reload_shaders()
{
	// >>> PBR BRANCH

	Shader::compile(&shade[S_SIMPLE], "MbSimpleV.txt", "MbSimpleF.txt");
	Shader::compile(&shade[S_TEXTURED], "MbTexturedV.txt", "MbTexturedF.txt");
	Shader::compile(&shade[S_TEXTURED3D], "MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE3D");
	Shader::compile(&shade[S_TEXTUREDARRAY], "MbTexturedV.txt", "MbTexturedF.txt", "TEXTUREARRAY");
	Shader::compile(&shade[S_SKYBOXCUBE], "MbSimpleV.txt", "SkyboxF.txt", "SKYBOX");


	const char* frag = "PbrBasicF.txt";
	Shader::compile(&shade[S_ANIMATED], "AnimBasicV.txt", frag, "ANIMATED");
	Shader::compile(&shade[S_STATIC], "AnimBasicV.txt", frag);
	Shader::compile(&shade[S_STATIC_AT], "AnimBasicV.txt", frag, "ALPHATEST");
	Shader::compile(&shade[S_WIND], "AnimBasicV.txt", frag, "WIND");
	Shader::compile(&shade[S_WIND_AT], "AnimBasicV.txt", frag, "WIND, ALPHATEST");
	Shader::compile(&shade[S_LIGHTMAPPED], "AnimBasicV.txt", frag, "LIGHTMAPPED");
	Shader::compile(&shade[S_LIGHTMAPPED_AT], "AnimBasicV.txt", frag, "LIGHTMAPPED, ALPHATEST");
	Shader::compile(&shade[S_LIGHTMAPPED_BLEND2], "AnimBasicV.txt", frag, "LIGHTMAPPED, BLEND2, VERTEX_COLOR");

	frag = "DepthF.txt";
	Shader::compile(&shade[S_ANIMATED_DEPTH], "AnimBasicV.txt", frag, "ANIMATED");
	Shader::compile(&shade[S_DEPTH], "AnimBasicV.txt", frag);
	Shader::compile(&shade[S_AT_DEPTH], "AnimBasicV.txt", frag, "ALPHATEST");
	Shader::compile(&shade[S_WIND_DEPTH], "AnimBasicV.txt", frag, "WIND");
	Shader::compile(&shade[S_WIND_AT_DEPTH], "AnimBasicV.txt", frag, "WIND, ALPHATEST");

	Shader::compile(&shade[S_PARTICLE_BASIC], "MbTexturedV.txt", "MbTexturedF.txt", "PARTICLE_SHADER");


	Shader::compile(&shade[S_BLOOM_DOWNSAMPLE], "MbTexturedV.txt", "BloomDownsampleF.txt");
	Shader::compile(&shade[S_BLOOM_UPSAMPLE], "MbTexturedV.txt", "BloomUpsampleF.txt");
	Shader::compile(&shade[S_COMBINE], "MbTexturedV.txt", "CombineF.txt");
	set_shader(shade[S_COMBINE]);
	shader().set_int("scene_lit", 0);
	shader().set_int("bloom", 1);
	shader().set_int("lens_dirt", 2);



	Shader::compute_compile(&volfog.lightcalc, "VfogScatteringC.txt");
	Shader::compute_compile(&volfog.raymarch, "VfogRaymarchC.txt");
	Shader::compute_compile(&volfog.reproject, "VfogScatteringC.txt", "REPROJECTION");
	volfog.lightcalc.use();
	volfog.lightcalc.set_int("previous_volume", 0);
	volfog.lightcalc.set_int("perlin_noise", 1);
	glUseProgram(0);


	glCheckError();

	int start = S_ANIMATED;
	int end = S_WIND_AT_DEPTH;
	for (int i = start; i <= end; i++) {
		set_shader(shade[i]);
		set_shader_sampler_locations();
	}
}

struct Ubo_View_Constants_Struct
{
	glm::mat4 view;
	glm::mat4 viewproj;
	glm::mat4 invview;
	glm::mat4 invproj;
	glm::vec4 viewpos_time;
	glm::vec4 viewfront;
	glm::vec4 viewport_size;

	glm::vec4 near_far_epsilon;
	
	glm::vec4 fogcolor;
	glm::vec4 fogparams;
	glm::vec4 directional_light_dir_and_used;
	glm::vec4 directional_light_color;

	glm::vec4 ncubemaps_nlights_forcecubemap;
};

void Renderer::upload_ubo_view_constants(uint32_t ubo)
{
	Ubo_View_Constants_Struct constants;
	constants.view = vs.view;
	constants.viewproj = vs.viewproj;
	constants.invview = glm::inverse(vs.view);
	constants.invproj = glm::inverse(vs.proj);
	constants.viewpos_time = glm::vec4(vs.origin, engine.time);
	constants.viewfront = glm::vec4(vs.front, 0.0);
	constants.viewport_size = glm::vec4(vs.width, vs.height, 0, 0);

	constants.near_far_epsilon = vec4(vs.near, vs.far, shadowmap.epsilon, 0.0);

	constants.fogcolor = vec4(vec3(0.7), 1);
	constants.fogparams = vec4(10, 30, 0, 0);

	if (scene.directional_index != -1) {
		auto& light = scene.lights[scene.directional_index];
		constants.directional_light_dir_and_used = vec4(light.normal, 0.0);
		constants.directional_light_dir_and_used.w = 1.0;
		constants.directional_light_color = vec4(light.color, 0.0);
	}
	else
		constants.directional_light_dir_and_used = vec4(1, 0, 0, 0);

	constants.ncubemaps_nlights_forcecubemap = vec4(scene.cubemaps.size(), scene.lights.size(), 0, 0);
	if (using_skybox_for_specular)
		constants.ncubemaps_nlights_forcecubemap.z = 0.0;
	else
		constants.ncubemaps_nlights_forcecubemap.z = -1.0;

	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof Ubo_View_Constants_Struct, &constants, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

const static int csm_resolutions[] = { 0, 256, 512, 1024 };


void Shadow_Map_System::init()
{
	make_csm_rendertargets();
	glGenBuffers(1, &csm_ubo);
	glGenBuffers(4, frame_view_ubos);
}
void Shadow_Map_System::make_csm_rendertargets()
{
	if (quality == 0)
		return;
	csm_resolution = csm_resolutions[(int)quality];

	glGenTextures(1, &shadow_map_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map_array);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, csm_resolution, csm_resolution, 4, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	bool hardware_filtering = true;
	if (hardware_filtering) {
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float bordercolor[] = { 1.0,1.0,1.0,1.0 };
	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, bordercolor);
	glCheckError();

	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glCheckError();

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_array, 0);
	glCheckError();

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		printf("GBuffer framebuffer not complete!\n");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	glCheckError();
}


static glm::vec4 CalcPlaneSplits(float near, float far, float log_lin_lerp)
{
	float zratio = far / near;
	float zrange = far - near;

	const float bias = 0.0001f;

	glm::vec4 planedistances;
	for (int i = 0; i < 4; i++)
	{
		float x = (i + 1) / 4.f;
		float log = near * pow(zratio, x);
		float linear = near + zrange * x;
		planedistances[i] = log_lin_lerp * (log - linear) + linear + bias;
	}

	return planedistances;
}

void Shadow_Map_System::update()
{
	if (draw.r_shadow_quality->integer < 0) cfg.set_var("r_shadow_quality", "0");
	else if (draw.r_shadow_quality->integer > 3) cfg.set_var("r_shadow_quality", "3");
	if (quality != draw.r_shadow_quality->integer) {
		quality = draw.r_shadow_quality->integer;
		targets_dirty = true;
	}

	if (targets_dirty) {
		glDeleteTextures(1, &shadow_map_array);
		glDeleteFramebuffers(1, &framebuffer);
		make_csm_rendertargets();
		targets_dirty = false;
	}
	if (quality == 0)
		return;
	if (draw.scene.directional_index == -1)
		return;

	GPUFUNCTIONSTART;

	glm::vec3 directional_dir = draw.scene.lights[draw.scene.directional_index].normal;

	const View_Setup& view = draw.vs;

	float near = view.near;
	float far = glm::min(view.far, max_shadow_dist);

	split_distances = CalcPlaneSplits(near, far, log_lin_lerp_factor);
	for (int i = 0; i < MAXCASCADES; i++)
		update_cascade(i, view, directional_dir);

	struct Shadowmap_Csm_Ubo_Struct
	{
		mat4 data[4];
		vec4 near_planes;
		vec4 far_planes;
	}upload_data;

	for (int i = 0; i < 4; i++) {
		upload_data.data[i] = matricies[i];
		upload_data.near_planes[i] = nearplanes[i];
		upload_data.far_planes[i] = farplanes[i];
	}

	glBindBuffer(GL_UNIFORM_BUFFER, csm_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof Shadowmap_Csm_Ubo_Struct, &upload_data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// now setup scene for rendering
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	for (int i = 0; i < 4; i++) {
		GPUSCOPESTART("Render csm layer");

		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_array, 0, i);

		Render_Level_Params params;
		params.output_framebuffer = framebuffer;
		params.pass = Render_Level_Params::SHADOWMAP;
		params.include_lightmapped = false;
		View_Setup setup;
		setup.width = csm_resolution;
		setup.height = csm_resolution;
		setup.near = nearplanes[i];
		setup.far = farplanes[i];
		setup.viewproj = matricies[i];
		setup.view = setup.proj = mat4(1);
		params.view = setup;
		params.cull_front_face = true;
		params.force_backface = true;
		params.provied_constant_buffer = frame_view_ubos[i];
		params.upload_constants = true;

		draw.render_level_to_target(params);
	}
}



static std::vector<vec3> GetFrustumCorners(const mat4& view, const mat4& projection)
{
	mat4 inv_viewproj = glm::inverse(projection * view);
	std::vector<vec3> corners;
	for (int x = 0; x < 2; x++) {
		for (int y = 0; y < 2; y++) {
			for (int z = 0; z < 2; z++) {
				vec4 ndc_coords = vec4(2 * x - 1, 2 * y - 1, 2 * z - 1, 1);
				vec4 world_space = inv_viewproj * ndc_coords;
				world_space /= world_space.w;
				corners.push_back(vec3(world_space));
			}
		}
	}

	return corners;
}


void Shadow_Map_System::update_cascade(int cascade_idx, const View_Setup& view, vec3 directionalDir)
{
	float far = split_distances[cascade_idx];
	float near = (cascade_idx == 0) ? view.near : split_distances[cascade_idx - 1];
	if (fit_to_scene)
		near = view.near;

	mat4 camera_cascaded_proj = glm::perspective(
		glm::radians(view.near),
		(float)view.width / view.height,
		near, far);
	// World space corners
	auto corners = GetFrustumCorners(view.view, camera_cascaded_proj);
	vec3 frustum_center = vec3(0);
	for (int i = 0; i < corners.size(); i++)
		frustum_center += corners[i];
	frustum_center /= 8.f;

	mat4 light_cascade_view = glm::lookAt(frustum_center - directionalDir, frustum_center, vec3(0, 1, 0));
	vec3 viewspace_min = vec3(INFINITY);
	vec3 viewspace_max = vec3(-INFINITY);
	if (reduce_shimmering)
	{
		float sphere_radius = 0.f;
		for (int i = 0; i < 8; i++) {
			float dist = glm::length(corners[i] - frustum_center);
			sphere_radius = glm::max(sphere_radius, dist);
		}
		sphere_radius = ceil(sphere_radius);
		vec3 world_max = frustum_center + vec3(sphere_radius);
		vec3 world_min = frustum_center - vec3(sphere_radius);

		vec3 v_max = light_cascade_view * vec4(world_max, 1.0);
		vec3 v_min = light_cascade_view * vec4(world_min, 1.0);
		viewspace_max = glm::max(v_max, v_min);
		viewspace_min = glm::min(v_max, v_min);
		viewspace_max = vec3(sphere_radius);
		viewspace_min = -viewspace_max;
	}
	else {
		for (int i = 0; i < 8; i++) {
			vec3 viewspace_corner = light_cascade_view * vec4(corners[i], 1.0);
			viewspace_min = glm::min(viewspace_min, viewspace_corner);
			viewspace_max = glm::max(viewspace_max, viewspace_corner);
		}

		// insert scaling for pcf filtering here

	}
	if (viewspace_min.z < 0)
		viewspace_min.z *= z_dist_scaling;
	else
		viewspace_min.z /= z_dist_scaling;

	if (viewspace_max.z < 0)
		viewspace_max.z /= z_dist_scaling;
	else
		viewspace_max.z *= z_dist_scaling;

	vec3 cascade_extent = viewspace_max - viewspace_min;

	mat4 light_cascade_proj = glm::ortho(viewspace_min.x, viewspace_max.x, viewspace_min.y, viewspace_max.y, viewspace_min.z, viewspace_max.z);
	mat4 shadow_matrix = light_cascade_proj * light_cascade_view;
	if (reduce_shimmering)
	{
		vec4 shadow_origin = vec4(0, 0, 0, 1);
		shadow_origin = shadow_matrix * shadow_origin;
		float w = shadow_origin.w;
		shadow_origin *= csm_resolution / 2.0f;

		vec4 rounded_origin = glm::round(shadow_origin);
		vec4 rounded_offset = rounded_origin - shadow_origin;
		rounded_offset *= 2.0f / csm_resolution;
		rounded_offset.z = 0;
		rounded_offset.w = 0;

		mat4 shadow_cascade_proj = light_cascade_proj;
		shadow_cascade_proj[3] += rounded_offset;

		shadow_matrix = shadow_cascade_proj * light_cascade_view;
	}


	matricies[cascade_idx] = shadow_matrix;// light_cascade_proj* light_cascade_view;
	nearplanes[cascade_idx] = viewspace_min.z;
	farplanes[cascade_idx] = viewspace_max.z;
}


static const ivec3 volfog_sizes[] = { {0,0,0},{160,90,128},{80,45,64} };

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
};

void Volumetric_Fog_System::init()
{
	if (quality == 0)
		return;

	voltexturesize = volfog_sizes[1];

	glGenTextures(1, &voltexture);
	glBindTexture(GL_TEXTURE_3D, voltexture);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, voltexturesize.x, voltexturesize.y, voltexturesize.z, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);

	glGenTextures(1, &voltexture_prev);
	glBindTexture(GL_TEXTURE_3D, voltexture_prev);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, voltexturesize.x, voltexturesize.y, voltexturesize.z, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);	// REEE!!!!!!!!!!!!!


	glGenBuffers(1, &light_ssbo);
	glGenBuffers(1, &param_ubo);

	glCheckError();
}

void Volumetric_Fog_System::compute()
{
	if (!draw.r_volumetric_fog->integer)
		return;

	GPUFUNCTIONSTART;

	static Vfog_Light light_buffer[64];
	int num_lights = 0;
	{
		Level_Light& l = draw.dyn_light;
		Vfog_Light& vfl = light_buffer[num_lights++];
		float type = (l.type == LIGHT_POINT) ? 0.0 : 1.0;
		vfl.position_type = vec4(l.position, type);
		vfl.color = vec4(l.color, 0.0);
		vfl.direction_coneangle = vec4(l.direction, l.spot_angle);
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, light_ssbo);
	if (num_lights > 0)
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Vfog_Light) * num_lights, light_buffer, GL_DYNAMIC_DRAW);

	Vfog_Params params;
	params.volumesize = glm::ivec4(voltexturesize, 0);
	params.volspread_frustumend = vec4(spread, frustum_end, 0, 0);
	params.last_frame_viewproj = draw.lastframe_vs.viewproj;
	params.reprojection = vec4(temporal_sequence, 0.1, 0, 0);
	glBindBuffer(GL_UNIFORM_BUFFER, param_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof Vfog_Params, &params, GL_DYNAMIC_DRAW);


	glBindBufferBase(GL_UNIFORM_BUFFER, 4, param_ubo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, light_ssbo);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, draw.active_constants_ubo);
	glCheckError();
	ivec3 groups = ceil(vec3(voltexturesize) / vec3(8, 8, 1));
	{
		lightcalc.use();

		lightcalc.set_mat4("InvViewProj", glm::inverse(draw.vs.viewproj));
		lightcalc.set_vec3("ViewPos", draw.vs.origin);
		glUniform3i(glGetUniformLocation(lightcalc.ID, "TextureSize"), voltexturesize.x, voltexturesize.y, voltexturesize.z);

		lightcalc.set_float("znear", draw.vs.near);
		lightcalc.set_float("zfar", draw.vs.far);
		lightcalc.set_mat4("InvView", glm::inverse(draw.vs.view));
		lightcalc.set_mat4("InvProjection", glm::inverse(draw.vs.proj));

		lightcalc.set_float("density", draw.vfog.x);
		lightcalc.set_float("anisotropy", draw.vfog.y);
		lightcalc.set_vec3("ambient", draw.ambientvfog);

		lightcalc.set_vec3("spotlightpos", vec3(0, 2, 0));
		lightcalc.set_vec3("spotlightnormal", vec3(0, -1, 0));
		lightcalc.set_float("spotlightangle", 0.5);
		lightcalc.set_vec3("spotlightcolor", vec3(10.f));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D, voltexture_prev);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D, draw.perlin3d.id);
		lightcalc.set_vec3("perlin_offset", glm::vec3(engine.time * 0.2, 0, engine.time));


		lightcalc.set_int("num_lights", 0);
		glCheckError();

		glBindImageTexture(2, voltexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

		glDispatchCompute(groups.x, groups.y, groups.z);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glCheckError();

	}
	static Config_Var* fog_raymarch = cfg.get_var("dbg/raymarch", "1");
	if (fog_raymarch->integer) {
		raymarch.use();
		raymarch.set_float("znear", draw.vs.near);
		raymarch.set_float("zfar", draw.vs.far);
		glUniform3i(glGetUniformLocation(raymarch.ID, "TextureSize"), voltexturesize.x, voltexturesize.y, voltexturesize.z);

		glBindImageTexture(5, voltexture_prev, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glBindImageTexture(2, voltexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);

		glDispatchCompute(groups.x, groups.y, 1);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glCheckError();

		// swap, rendering with voltexture
		std::swap(voltexture, voltexture_prev);
	}


	temporal_sequence = (temporal_sequence + 1) % 16;
}



void Renderer::Init()
{
	glCheckError();
	InitGlState();
	reload_shaders();

	const uint8_t wdata[] = { 0xff,0xff,0xff };
	const uint8_t bdata[] = { 0x0,0x0,0x0 };
	glGenTextures(1, &white_texture);
	glBindTexture(GL_TEXTURE_2D, white_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, wdata);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);
	glGenTextures(1, &black_texture);
	glBindTexture(GL_TEXTURE_2D, black_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, bdata);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	r_draw_collision_tris = cfg.get_var("draw_collision_tris", "0");
	r_draw_sv_colliders = cfg.get_var("draw_sv_colliders", "0");
	r_draw_viewmodel = cfg.get_var("draw_viewmodel", "1");
	vsync = cfg.get_var("vsync", "1");
	r_bloom = cfg.get_var("r_bloom", "1");
	r_shadow_quality = cfg.get_var("r_shadow_quality", "1");
	r_volumetric_fog = cfg.get_var("r_volumetric_fog", "1");



	perlin3d = generate_perlin_3d({ 16,16,16 }, 0x578437adU, 4, 2, 0.4, 2.0);

	fbo.scene = 0;
	tex.scene_color = tex.scene_depthstencil = 0;
	InitFramebuffers();

	// >>> PBR BRANCH
	EnviornmentMapHelper::get().init();

	lens_dirt = mats.find_texture("lens_dirt.jpg");

	glGenBuffers(1, &ubo.current_frame);


	volfog.init();
	shadowmap.init();
}

void Renderer::InitFramebuffers()
{
	const int s_w = engine.window_w->integer;
	const int s_h = engine.window_h->integer;

	glDeleteTextures(1, &tex.scene_color);
	glGenTextures(1, &tex.scene_color);
	glBindTexture(GL_TEXTURE_2D, tex.scene_color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, s_w, s_h, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glCheckError();

	glDeleteTextures(1, &tex.scene_depthstencil);
	glGenTextures(1, &tex.scene_depthstencil);
	glBindTexture(GL_TEXTURE_2D, tex.scene_depthstencil);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, s_w, s_h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glCheckError();

	glDeleteFramebuffers(1, &fbo.scene);
	glGenFramebuffers(1, &fbo.scene);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.scene);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.scene_color, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex.scene_depthstencil, 0);
	glCheckError();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glCheckError();

	cur_w = s_w;
	cur_h = s_h;

	init_bloom_buffers();
}

void Renderer::init_bloom_buffers()
{
	glDeleteFramebuffers(1, &fbo.bloom);
	glDeleteTextures(BLOOM_MIPS, tex.bloom_chain);
	glDeleteRenderbuffers(1, &tex.bloom_depth);

	glGenFramebuffers(1, &fbo.bloom);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.bloom);

	glGenTextures(BLOOM_MIPS, tex.bloom_chain);
	int x = cur_w / 2;
	int y = cur_h / 2;
	float fx = x;
	float fy = y;
	for (int i = 0; i < BLOOM_MIPS; i++) {
		tex.bloom_chain_isize[i] = { x,y };
		tex.bloom_chain_size[i] = { fx,fy };
		glBindTexture(GL_TEXTURE_2D, tex.bloom_chain[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, x, y, 0, GL_RGB, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		x /= 2;
		y /= 2;
		fx *= 0.5;
		fy *= 0.5;
	}

	glGenRenderbuffers(1, &tex.bloom_depth);
	glBindRenderbuffer(GL_RENDERBUFFER, tex.bloom_depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, cur_w / 2, cur_h / 2);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, tex.bloom_depth);

	glCheckError();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


extern bool bloom_stop;

void Renderer::render_bloom_chain()
{
	GPUFUNCTIONSTART;

	if (!r_bloom->integer)
		return;


	glDisable(GL_CULL_FACE);

	MeshBuilder mb;
	mb.Begin();
	mb.Push2dQuad(vec2(-1, -1), vec2(2, 2));
	mb.End();

	glBindFramebuffer(GL_FRAMEBUFFER, fbo.bloom);
	glActiveTexture(GL_TEXTURE0);
	set_shader(shade[S_BLOOM_DOWNSAMPLE]);
	shader().set_mat4("Model", mat4(1));
	shader().set_mat4("ViewProj", mat4(1));
	float src_x = cur_w;
	float src_y = cur_h;

	glBindTexture(GL_TEXTURE_2D, tex.scene_color);
	glDisable(GL_DEPTH_TEST);
	glClearColor(0, 0, 0, 1);
	for (int i = 0; i < BLOOM_MIPS; i++)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.bloom_chain[i], 0);
		shader().set_vec2("srcResolution", vec2(src_x, src_y));
		shader().set_int("mipLevel", i);
		src_x = tex.bloom_chain_size[i].x;
		src_y = tex.bloom_chain_size[i].y;

		glViewport(0, 0, src_x, src_y);	// dest size
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		mb.Draw(GL_TRIANGLES);

		glBindTexture(GL_TEXTURE_2D, tex.bloom_chain[i]);
	}

	if (bloom_stop) {
		mb.Free();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	set_shader(shade[S_BLOOM_UPSAMPLE]);
	shader().set_mat4("Model", mat4(1));
	shader().set_mat4("ViewProj", mat4(1));
	for (int i = BLOOM_MIPS - 1; i > 0; i--)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.bloom_chain[i - 1], 0);
		vec2 destsize = tex.bloom_chain_size[i - 1];
		glViewport(0, 0, destsize.x, destsize.y);
		//glClear(GL_COLOR_BUFFER_BIT);
		glBindTexture(GL_TEXTURE_2D, tex.bloom_chain[i]);
		shader().set_float("filterRadius", 0.0001f);

		mb.Draw(GL_TRIANGLES);
	}
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	mb.Free();
	glEnable(GL_CULL_FACE);

	glCheckError();
}

void Renderer::DrawSkybox()
{
	static Config_Var* draw_skybox = cfg.get_var("r_skybox", "1");
	if (!draw_skybox->integer)
		return;

	MeshBuilder mb;
	mb.Begin();
	mb.PushSolidBox(-vec3(1), vec3(1), COLOR_WHITE);
	mb.End();

	set_shader(shade[S_SKYBOXCUBE]);
	glm::mat4 view = vs.view;
	view[3] = vec4(0, 0, 0, 1);	// remove translation
	shader().set_mat4("ViewProj", vs.proj * view);
	shader().set_vec2("screen_size", vec2(vs.width, vs.height));
	shader().set_int("volumetric_fog", 1);
	shader().set_int("cube", 0);


	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, scene.skybox);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, volfog.voltexture);


	glDisable(GL_CULL_FACE);
	mb.Draw(GL_TRIANGLES);
	glEnable(GL_CULL_FACE);
	mb.Free();
}

void Renderer::render_level_to_target(Render_Level_Params params)
{
	vs = params.view;

	if (params.force_skybox_probe)
		using_skybox_for_specular = true;

	static Config_Var* upload_ubo = cfg.get_var("dbg/uubo", "1");
	if (upload_ubo->integer) {
		uint32_t view_ubo = params.provied_constant_buffer;
		bool upload = params.upload_constants;
		if (params.provied_constant_buffer == 0) {
			view_ubo = ubo.current_frame;
			upload = true;
		}
		if (upload)
			upload_ubo_view_constants(view_ubo);
		active_constants_ubo = view_ubo;
	}

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, active_constants_ubo);
	set_standard_draw_data();

	glBindFramebuffer(GL_FRAMEBUFFER, params.output_framebuffer);
	glViewport(0, 0, vs.width, vs.height);
	if (params.clear_framebuffer) {
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	}

	if (params.cull_front_face) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(shadowmap.poly_factor, shadowmap.poly_units);
		glCullFace(GL_FRONT);
	}
	if (params.force_backface)
		glDisable(GL_CULL_FACE);


	if (params.draw_level) {
		if (params.pass == Render_Level_Params::STANDARD)
			DrawLevel();
		else
			DrawLevelDepth(params);
	}
	if (params.draw_ents)
		DrawEnts(params.pass);

	if (params.pass == Render_Level_Params::STANDARD) {
		DrawSkybox();
	}


	if (params.cull_front_face) {
		glDisable(GL_POLYGON_OFFSET_FILL);
		glCullFace(GL_BACK);
	}
	if (params.force_backface)
		glEnable(GL_CULL_FACE);

	using_skybox_for_specular = false;
}

void Renderer::ui_render()
{
	GPUFUNCTIONSTART;

	set_shader(shade[S_TEXTURED]);
	shader().set_mat4("Model", mat4(1));
	glm::mat4 proj = glm::ortho(0.f, (float)cur_w, -(float)cur_h, 0.f);
	shader().set_mat4("ViewProj", proj);
	building_ui_texture = 0;
	ui_builder.Begin();
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Texture* t = mats.find_texture("crosshair007.png");
	int centerx = cur_w / 2;
	int centery = cur_h / 2;

	float crosshair_scale = 0.7f;
	Color32 crosshair_color = { 0, 0xff, 0, 0xff };
	float width = t->width * crosshair_scale;
	float height = t->height * crosshair_scale;


	draw_rect(centerx - width / 2, centery - height / 2, width, height, crosshair_color, t, t->width, t->height);

	//draw_rect(0, 300, 300, 300, COLOR_WHITE, mats.find_for_name("tree_bark")->images[0],500,500,0,0);


	if (ui_builder.GetBaseVertex() > 0) {
		bind_texture(BASE0_SAMPLER, building_ui_texture);
		ui_builder.End();
		ui_builder.Draw(GL_TRIANGLES);
	}

	glCheckError();


	glDisable(GL_BLEND);
	if (0) {
		set_shader(shade[S_TEXTUREDARRAY]);
		glCheckError();

		shader().set_mat4("Model", mat4(1));
		glm::mat4 proj = glm::ortho(0.f, (float)cur_w, -(float)cur_h, 0.f);
		shader().set_mat4("ViewProj", mat4(1));
		shader().set_int("slice", (int)slice_3d);

		ui_builder.Begin();
		ui_builder.Push2dQuad(glm::vec2(-1, 1), glm::vec2(1, -1), glm::vec2(0, 0),
			glm::vec2(1, 1), COLOR_WHITE);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, shadowmap.shadow_map_array);

		glCheckError();

		ui_builder.End();
		ui_builder.Draw(GL_TRIANGLES);

		glCheckError();
	}



	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}

void Renderer::draw_rect(int x, int y, int w, int h, Color32 color, Texture* t, float srcw, float srch, float srcx, float srcy)
{
	h = -h;	// adjust for coordinates
	y = -y;

	int texnum = (t) ? t->gl_id : white_texture;
	float tw = (t) ? t->width : 1;
	float th = (t) ? t->height : 1;

	if (texnum != building_ui_texture && ui_builder.GetBaseVertex() > 0) {
		bind_texture(BASE0_SAMPLER, building_ui_texture);
		ui_builder.End();
		ui_builder.Draw(GL_TRIANGLES);
		ui_builder.Begin();
	}
	building_ui_texture = texnum;
	ui_builder.Push2dQuad(glm::vec2(x, y), glm::vec2(w, h), glm::vec2(srcx / tw, srcy / th),
		glm::vec2(srcw / tw, srch / th), color);
}

void Renderer::FrameDraw()
{
	GPUFUNCTIONSTART;

	cur_shader = 0;
	for (int i = 0; i < NUM_SAMPLERS; i++)
		cur_tex[i] = 0;
	if (cur_w != engine.window_w->integer || cur_h != engine.window_h->integer)
		InitFramebuffers();
	lastframe_vs = current_frame_main_view;
	current_frame_main_view = engine.local.last_view;
	vs = current_frame_main_view;

	// Shadow map updates
	shadowmap.update();

	vs = current_frame_main_view;
	upload_ubo_view_constants(ubo.current_frame);
	active_constants_ubo = ubo.current_frame;

	volfog.compute();

	// main level render
	{
		GPUSCOPESTART("Main level render");
		Render_Level_Params params;
		params.output_framebuffer = fbo.scene;
		params.view = current_frame_main_view;
		params.pass = Render_Level_Params::STANDARD;
		params.upload_constants = false;
		params.provied_constant_buffer = ubo.current_frame;
		render_level_to_target(params);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.scene);
	DrawEntBlobShadows();
	engine.local.pm.draw_particles();
	glCheckError();


	// Bloom update
	render_bloom_chain();

	int x = vs.width;
	int y = vs.height;
	MeshBuilder mb;
	mb.Begin();
	mb.Push2dQuad(vec2(-1, -1), vec2(2, 2));
	mb.End();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, cur_w, cur_h);
	glDisable(GL_CULL_FACE);
	set_shader(shade[S_COMBINE]);
	shader().set_mat4("Model", mat4(1));
	shader().set_mat4("ViewProj", mat4(1));
	uint32_t bloom_tex = tex.bloom_chain[0];
	if (!r_bloom->integer) bloom_tex = black_texture;
	bind_texture(0, tex.scene_color);
	bind_texture(1, bloom_tex);
	bind_texture(2, lens_dirt->gl_id);
	mb.Draw(GL_TRIANGLES);

	glEnable(GL_CULL_FACE);

	mb.Begin();
	if (r_draw_sv_colliders->integer == 1) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (engine.ents[i].type == ET_PLAYER) {
				AddPlayerDebugCapsule(engine.ents[i], &mb, COLOR_CYAN);
			}
		}
	}

	mb.End();
	set_shader(shade[S_SIMPLE]);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.f));

	if (r_draw_collision_tris->integer)
		DrawCollisionWorld(engine.level);

	mb.Draw(GL_LINES);


	//game.rays.End();
	//game.rays.Draw(GL_LINES);
	if (engine.is_host) {
		//phys_debug.End();
		//phys_debug.Draw(GL_LINES);
	}

	ui_render();

	mb.Free();

	cubemap_positions_debug();

	glCheckError();
	glClear(GL_DEPTH_BUFFER_BIT);

	// FIXME: ubo view constant buffer might be wrong since its changed around a lot (bad design)
	if (!engine.local.thirdperson_camera->integer && r_draw_viewmodel->integer)
		DrawPlayerViewmodel();
}

void Renderer::cubemap_positions_debug()
{
	set_shader(shade[S_SIMPLE]);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.f));

	MeshBuilder mb;
	mb.Begin();
	for (auto& cube : scene.cubemaps) {
		mb.PushLineBox(cube.boxmin, cube.boxmax, COLOR_CYAN);
		mb.AddSphere(cube.probe_pos, 1.0, 5, 5, COLOR_RED);
	}
	mb.End();
	glDisable(GL_DEPTH_TEST);
	mb.Draw(GL_LINES);
	glEnable(GL_DEPTH_TEST);
	mb.Free();
}

Shader& Renderer::shader()
{
	static Shader stemp;
	stemp.ID = cur_shader;
	return stemp;	// Shader is just a wrapper around an id anyways
}

void Renderer::DrawEntBlobShadows()
{
	shadowverts.Begin();

	for (int i = 0; i < MAX_GAME_ENTS; i++)
	{
		Entity* e = &engine.ents[i];
		if (!e->active()) continue;

		RayHit rh;
		Ray r;
		r.pos = e->position + glm::vec3(0, 0.1f, 0);
		r.dir = glm::vec3(0, -1, 0);
		rh = engine.phys.trace_ray(r, i, PF_WORLD);

		if (rh.dist < 0)
			continue;

		AddBlobShadow(rh.pos + vec3(0, 0.05, 0), rh.normal, CHAR_HITBOX_RADIUS * 4.5f);
	}
	glCheckError();

	shadowverts.End();
	glCheckError();

	set_shader(shade[S_PARTICLE_BASIC]);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.0));
	shader().set_vec4("tint_color", vec4(0, 0, 0, 1));
	glCheckError();

	bind_texture(0, engine.media.blob_shadow->gl_id);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	shadowverts.Draw(GL_TRIANGLES);

	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);

	cur_shader = -1;
	glCheckError();

}

// shader_key 8 bits
// 3 bits
// animated/not animated
// lightmapped/not lightmapped
// alpha test/not alpha test

// 2wayblend, windsway, none


#define BADSHADERIF(x) if(x) { sys_print("bad shader combo %s\n", #x); return; }
#define WHITEIFNULL(x) ((x)?x->gl_id : white_texture)
void Renderer::draw_model_real(Model_Drawing_State* s, const Model* m, int part_n, glm::mat4 transform,
	const Entity* e, const Animator* a, Game_Shader* override_mat)
{
	const MeshPart& part = m->parts[part_n];
	const Game_Shader& gs = (override_mat) ? *override_mat : *m->materials.at(part.material_idx);
	const bool is_transparent = gs.alpha_type == Game_Shader::A_ADD || gs.alpha_type == Game_Shader::A_BLEND;
	if (is_transparent != s->is_transparent_pass)
		return;
	const bool is_animated = (a);
	const bool is_lightmapped = part.has_lightmap_coords();
	const bool is_at = gs.alpha_type == Game_Shader::A_TEST;
	const bool has_colors = part.has_colors();
	const bool show_backface = gs.backface;
	// ahhhhhhh
	if (is_lightmapped) {
		BADSHADERIF(is_animated);
		BADSHADERIF(gs.shader_type == Game_Shader::S_WINDSWAY);
		if (gs.shader_type == Game_Shader::S_2WAYBLEND) {
			set_shader(shade[S_LIGHTMAPPED_BLEND2]);
			bind_texture(BASE1_SAMPLER, WHITEIFNULL(gs.images[gs.BASE2]));
			bind_texture(SPECIAL_SAMPLER, WHITEIFNULL(gs.images[gs.SPECIAL]));
		}
		else if (is_at)
			set_shader(shade[S_LIGHTMAPPED_AT]);
		else
			set_shader(shade[S_LIGHTMAPPED]);

		bind_texture(LIGHTMAP_SAMPLER, WHITEIFNULL(engine.level->lightmap));
		bind_texture(BASE0_SAMPLER, WHITEIFNULL(gs.images[gs.BASE1]));
	}
	else {
		if (is_animated) {
			BADSHADERIF(gs.shader_type == Game_Shader::S_WINDSWAY);
			if (is_at) {
				BADSHADERIF(0);
			}
			else
				set_shader(shade[S_ANIMATED]);

			if (s->set_model_params) {
				const std::vector<mat4>& bones = a->GetBones();
				const uint32_t bone_matrix_loc = glGetUniformLocation(shader().ID, "BoneTransform[0]");
				for (int j = 0; j < bones.size(); j++)
					glUniformMatrix4fv(bone_matrix_loc + j, 1, GL_FALSE, glm::value_ptr(bones[j]));
				glCheckError();
			}

		}
		else {
			if (gs.shader_type == Game_Shader::S_WINDSWAY) {
				if (is_at)
					set_shader(shade[S_WIND_AT]);
				else
					set_shader(shade[S_WIND]);
			}
			else {
				if (is_at)
					set_shader(shade[S_STATIC_AT]);
				else
					set_shader(shade[S_STATIC]);
			}
		}

		bind_texture(BASE0_SAMPLER, WHITEIFNULL(gs.images[gs.BASE1]));
	}

	shader().set_mat4("Model", transform);
	shader().set_mat4("InverseModel", glm::inverse(transform));

	glBindVertexArray(part.vao);
	glDrawElements(GL_TRIANGLES, part.element_count, part.element_type, (void*)part.element_offset);
}


void Renderer::DrawModel(Render_Level_Params::Pass_Type pass, const Model* m, mat4 transform, const Animator* a, float rough, float metal)
{
	ASSERT(m);
	const bool isanimated = a != nullptr;

	if (pass == Render_Level_Params::DEPTH || pass == Render_Level_Params::SHADOWMAP) {
		if (isanimated)
			set_shader(shade[S_ANIMATED_DEPTH]);
		else
			set_shader(shade[S_DEPTH]);

		set_depth_shader_constants();
	}
	else {
		if (isanimated)
			set_shader(shade[S_ANIMATED]);
		else
			set_shader(shade[S_STATIC]);

		set_shader_constants();
		shader().set_float("in_roughness", rough);
		shader().set_float("in_metalness", metal);
	}


	glCheckError();
	shader().set_mat4("Model", transform);
	shader().set_mat4("InverseModel", glm::inverse(transform));


	if (isanimated) {
		const std::vector<mat4>& bones = a->GetBones();
		const uint32_t bone_matrix_loc = glGetUniformLocation(shader().ID, "BoneTransform[0]");
		for (int j = 0; j < bones.size(); j++)
			glUniformMatrix4fv(bone_matrix_loc + j, 1, GL_FALSE, glm::value_ptr(bones[j]));
		glCheckError();
	}

	for (int i = 0; i < m->parts.size(); i++)
	{
		const MeshPart* part = &m->parts[i];
		const Game_Shader* mm = nullptr;
		if (part->material_idx != -1)
			mm = m->materials.at(part->material_idx);
		if (pass == Render_Level_Params::STANDARD || (mm && mm->alpha_type == Game_Shader::A_TEST)) {
			if (part->material_idx == -1) {
				bind_texture(0, white_texture);
			}
			else {
				const Game_Shader* mm = m->materials.at(part->material_idx);
				shader().set_bool("has_uv_scroll", false);
				if (mm->images[Game_Shader::BASE1])
					bind_texture(BASE0_SAMPLER, mm->images[Game_Shader::BASE1]->gl_id);
				else
					bind_texture(BASE0_SAMPLER, white_texture);
			}
		}

		glBindVertexArray(part->vao);
		glDrawElements(GL_TRIANGLES, part->element_count, part->element_type, (void*)part->element_offset);
	}

}




void Renderer::DrawEnts(Render_Level_Params::Pass_Type pass)
{
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		auto& ent = engine.get_ent(i);
		//auto& ent = cgame->entities[i];
		if (!ent.active())
			continue;
		if (!ent.model)
			continue;

		if (i == engine.player_num() && !engine.local.thirdperson_camera->integer)
			continue;

		mat4 model;
		if (ent.using_interpolated_pos_and_rot)
			model = glm::translate(mat4(1), ent.local_sv_interpolated_pos);
		else
			model = glm::translate(mat4(1), ent.position);
		model = model * glm::eulerAngleXYZ(ent.rotation.x, ent.rotation.y, ent.rotation.z);
		model = glm::scale(model, vec3(1.f));

		const Animator* a = (ent.model->animations) ? &ent.anim : nullptr;
		DrawModel(pass, ent.model, model, a);


		if (ent.type == ET_PLAYER && a && ent.inv.active_item != Game_Inventory::UNEQUIP) {

			static Config_Var* m24 = cfg.get_var("dbg_m24", "1");

			Game_Item_Stats& stat = get_item_stats()[ent.inv.active_item];


			Model* m = FindOrLoadModel(stat.world_model);
			if (!m) continue;

			int index = ent.model->BoneForName("weapon");
			int index2 = ent.model->BoneForName("magazine");
			glm::mat4 rotate = glm::rotate(mat4(1), HALFPI, vec3(1, 0, 0));
			if (index == -1 || index2 == -1) {
				sys_print("no weapon bone\n");
				continue;
			}
			const Bone& b = ent.model->bones.at(index);
			glm::mat4 transform = a->GetBones()[index];
			transform = model * transform * mat4(b.posematrix) * rotate;
			DrawModel(pass, m, transform);

			if (stat.category == ITEM_CAT_RIFLE) {
				std::string mod = stat.world_model;
				mod = mod.substr(0, mod.rfind('.'));
				mod += "_mag.glb";
				Model* mag_mod = FindOrLoadModel(mod.c_str());
				if (mag_mod) {
					const Bone& mag_bone = ent.model->bones.at(index2);
					transform = a->GetBones()[index2];
					transform = model * transform * mat4(mag_bone.posematrix) * rotate;
					DrawModel(pass, mag_mod, transform);
				}
			}
		}
	}


	//Model* sphere = FindOrLoadModel("sphere.glb");
	//for (int x = 0; x < 10; x++) {
	//	for (int y = 0; y < 10; y++) {
	//		mat4 transform = glm::translate(mat4(1), vec3((x - 5) * 0.75, 0, (y - 5) * 0.75));
	//		transform = glm::scale(transform, vec3(0.4));
	//		DrawModel(Render_Level_Params::STANDARD, sphere, transform, nullptr, (9 - x) / 9.f, (9 - y) / 9.f);
	//	}
	//}

	DrawModel(Render_Level_Params::STANDARD, FindOrLoadModel("sphere.glb"), glm::scale(glm::translate(mat4(1), vec3(0, 2, 0)),vec3(2)), nullptr, 0.0, 1.0);


}


float wsheight = 3.0;
float wsradius = 1.5;
float wsstartheight = 0.2;
float wsstartradius = 0.2;
vec3 wswind_dir = glm::vec3(1, 0, 0);
float speed = 1.0;

void Renderer::set_wind_constants()
{
	shader().set_float("time", engine.time);
	shader().set_float("height", wsheight);
	shader().set_float("radius", wsradius);
	shader().set_float("startheight", wsstartheight);
	shader().set_float("startradius", wsstartradius);
	shader().set_vec3("wind_dir", wswind_dir);
	shader().set_float("speed", speed);
}

void Renderer::DrawLevelDepth(const Render_Level_Params& params)
{
	const Level* level = engine.level;
	bool force_set = true;
	int backfaces = -1;

	glDisable(GL_BLEND);
	for (int m = 0; m < level->instances.size(); m++) {
		const Level::StaticInstance& sm = level->instances[m];
		if (sm.collision_only) continue;
		ASSERT(level->static_meshes[sm.model_index]);
		Model& model = *level->static_meshes[sm.model_index];

		for (int p = 0; p < model.parts.size(); p++) {
			MeshPart& mp = model.parts[p];
			Game_Shader* gs = (mp.material_idx != -1) ? model.materials.at(mp.material_idx) : &mats.fallback;
			if (!(gs->alpha_type == Game_Shader::A_NONE || gs->alpha_type == Game_Shader::A_TEST))
				continue;

			bool mat_is_at = gs->alpha_type == gs->A_TEST;
			bool mat_lightmapped = mp.has_lightmap_coords();
			bool mat_colors = mp.has_colors();
			int mat_shader_type = gs->shader_type;

			if (!params.include_lightmapped && mat_lightmapped)
				continue;

			if (mat_shader_type == Game_Shader::S_WINDSWAY)
			{
				if (mat_is_at)
					set_shader(shade[S_WIND_AT_DEPTH]);
				else
					set_shader(shade[S_WIND_DEPTH]);

				set_wind_constants();
			}
			else
			{
				if (mat_is_at)
					set_shader(shade[S_AT_DEPTH]);
				else
					set_shader(shade[S_DEPTH]);
			}

			set_depth_shader_constants();

			if (backfaces != (int)gs->backface) {
				backfaces = gs->backface;
				if (backfaces) {
					glDisable(GL_CULL_FACE);
				}
				else
					glEnable(GL_CULL_FACE);
			}

			shader().set_mat4("Model", sm.transform);
			shader().set_mat4("InverseModel", glm::inverse(sm.transform));

			if (gs->alpha_type == Game_Shader::A_TEST) {

				if (gs->images[gs->BASE1])
					bind_texture(BASE0_SAMPLER, gs->images[gs->BASE1]->gl_id);
				else
					bind_texture(BASE0_SAMPLER, white_texture);
			}
			glBindVertexArray(mp.vao);
			glDrawElements(GL_TRIANGLES, mp.element_count, mp.element_type, (void*)mp.element_offset);
		}
	}

}

void Renderer::DrawLevel()
{
	const Level* level = engine.level;

	bool force_set = true;
	bool is_alpha_test = false;
	bool is_lightmapped = false;
	int last_alpha = -1;
	int backfaces = -1;

	int shader_type = Game_Shader::S_DEFAULT;

	for (int m = 0; m < level->instances.size(); m++) {
		const Level::StaticInstance& sm = level->instances[m];
		if (sm.collision_only) continue;
		ASSERT(level->static_meshes[sm.model_index]);
		Model& model = *level->static_meshes[sm.model_index];

		for (int p = 0; p < model.parts.size(); p++) {
			MeshPart& mp = model.parts[p];
			Game_Shader* gs = (mp.material_idx != -1) ? model.materials.at(mp.material_idx) : &mats.fallback;
			bool mat_is_at = gs->alpha_type == gs->A_TEST;
			bool mat_lightmapped = mp.has_lightmap_coords();
			bool mat_colors = mp.has_colors();
			int mat_shader_type = gs->shader_type;
			if (force_set || is_alpha_test != mat_is_at || is_lightmapped != mat_lightmapped || shader_type != mat_shader_type) {
				is_alpha_test = mat_is_at;
				is_lightmapped = mat_lightmapped;
				shader_type = mat_shader_type;
				force_set = false;

				if (shader_type == gs->S_2WAYBLEND && (!is_lightmapped || !mat_colors))
					force_set = true;

				if (force_set)
					continue;

				if (shader_type == gs->S_DEFAULT) {
					if (is_lightmapped)
						set_shader(shade[S_LIGHTMAPPED]);
					else if (!is_lightmapped && !is_alpha_test)
						set_shader(shade[S_STATIC]);
					else if (!is_lightmapped && is_alpha_test)
						set_shader(shade[S_STATIC_AT]);
				}
				else if (shader_type == gs->S_2WAYBLEND) {
					set_shader(shade[S_LIGHTMAPPED_BLEND2]);
				}
				else if (shader_type == gs->S_WINDSWAY) {
					if (is_alpha_test)
						set_shader(shade[S_WIND_AT]);
					else
						set_shader(shade[S_WIND]);

					set_wind_constants();
				}

				set_shader_constants();

				if (is_lightmapped) {
					if (engine.level->lightmap)
						bind_texture(LIGHTMAP_SAMPLER, engine.level->lightmap->gl_id);
					else
						bind_texture(LIGHTMAP_SAMPLER, white_texture);
				}
			}

			if (last_alpha != gs->alpha_type) {
				last_alpha = gs->alpha_type;
				if (last_alpha == Game_Shader::A_NONE || last_alpha == Game_Shader::A_TEST) {
					glDisable(GL_BLEND);
				}
				else if (last_alpha == Game_Shader::A_BLEND) {
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				}
				else if (last_alpha == Game_Shader::A_ADD) {
					glEnable(GL_BLEND);
					glBlendFunc(GL_ONE, GL_ONE);
				}
			}
			if (backfaces != (int)gs->backface) {
				backfaces = gs->backface;
				if (backfaces) {
					glDisable(GL_CULL_FACE);
				}
				else
					glEnable(GL_CULL_FACE);
			}

			shader().set_mat4("Model", sm.transform);
			shader().set_mat4("InverseModel", glm::inverse(sm.transform));

			shader().set_bool("has_glassfresnel", gs->fresnel_transparency);
			shader().set_float("glassfresnel_opacity", 0.6f);

			shader().set_float("in_roughness", rough);
			shader().set_float("in_metalness", metal);

			shader().set_bool("no_light", gs->emmisive);

			bool has_uv_scroll = gs->uscroll != 0.f || gs->vscroll != 0.f;
			shader().set_bool("has_uv_scroll", has_uv_scroll);
			shader().set_vec2("uv_scroll_offset", glm::vec2(gs->uscroll, gs->vscroll) * (float)engine.time);

			if (gs->images[gs->BASE1])
				bind_texture(BASE0_SAMPLER, gs->images[gs->BASE1]->gl_id);
			else
				bind_texture(BASE0_SAMPLER, white_texture);
			if (shader_type == Game_Shader::S_2WAYBLEND) {
				bind_texture(BASE1_SAMPLER, gs->images[gs->BASE2]->gl_id);
				bind_texture(SPECIAL_SAMPLER, gs->images[gs->SPECIAL]->gl_id);
			}

			glBindVertexArray(mp.vao);
			glDrawElements(GL_TRIANGLES, mp.element_count, mp.element_type, (void*)mp.element_offset);
		}
	}
}


void Renderer::AddPlayerDebugCapsule(Entity& e, MeshBuilder* mb, Color32 color)
{
	vec3 origin = e.position;
	Capsule c;
	c.base = origin;
	c.tip = origin + vec3(0, (false) ? CHAR_CROUCING_HB_HEIGHT : CHAR_STANDING_HB_HEIGHT, 0);
	c.radius = CHAR_HITBOX_RADIUS;
	float radius = CHAR_HITBOX_RADIUS;
	vec3 a, b;
	c.GetSphereCenters(a, b);
	mb->AddSphere(a, radius, 10, 7, color);
	mb->AddSphere(b, radius, 10, 7, color);
	mb->AddSphere((a + b) / 2.f, (c.tip.y - c.base.y) / 2.f, 10, 7, COLOR_RED);
}

void Renderer::DrawPlayerViewmodel()
{
	mat4 invview = glm::inverse(vs.view);

	Game_Local* gamel = &engine.local;
	mat4 model2 = glm::translate(invview, vec3(0.18, -0.18, -0.25) + gamel->viewmodel_offsets + gamel->viewmodel_recoil_ofs);
	//model2 = model2 * glm::scale(model2, glm::vec3(gamel->vm_scale.x));


	model2 = glm::translate(model2, gamel->vm_offset);
	model2 = model2 * glm::eulerAngleY(PI + PI / 128.f);

	cur_shader = -1;

	set_standard_draw_data();
	DrawModel(Render_Level_Params::STANDARD, engine.local.viewmodel, model2, &engine.local.viewmodel_animator);
}


void get_view_mat(int idx, glm::vec3 pos, glm::mat4& view, glm::vec3& front)
{
	vec3 up = vec3(0, -1, 0);
	switch (idx)
	{
	case 0:
		front = vec3(1, 0, 0);
		break;
	case 1:
		front = vec3(-1, 0, 0);
		break;
	case 2:
		front = vec3(0, 1, 0);
		up = vec3(0, 0, 1);
		break;
	case 3:
		front = vec3(0, -1, 0);
		up = vec3(0, 0, -1);
		break;
	case 4:
		front = vec3(0, 0, 1);
		break;
	case 5:
		front = vec3(0, 0, -1);
		break;
	}
	view = glm::lookAt(pos, pos+front, up);
}

void Renderer::render_world_cubemap(vec3 probe_pos, uint32_t fbo, uint32_t texture, int size)
{
	glCheckError();
	auto& helper = EnviornmentMapHelper::get();


	for (int i = 0; i < 6; i++) {
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, i);

		View_Setup cubemap_view;
		get_view_mat(i, probe_pos, cubemap_view.view, cubemap_view.front);
		
		cubemap_view.origin = probe_pos;
		cubemap_view.width = size;
		cubemap_view.height = size;
		cubemap_view.far = 100.f;
		cubemap_view.near = 0.1f;
		cubemap_view.proj = helper.cubemap_projection;
		cubemap_view.viewproj = cubemap_view.proj * cubemap_view.view;

		glCheckError();

		Render_Level_Params params;
		params.view = cubemap_view;
		params.output_framebuffer = fbo;
		params.draw_level = true;
		params.draw_ents = false;
		params.clear_framebuffer = true;
		params.provied_constant_buffer = 0;
		params.pass = Render_Level_Params::STANDARD;
		params.upload_constants = true;
		params.force_skybox_probe = true;

		render_level_to_target(params);
		glCheckError();

	}
}

void Renderer::on_level_start()
{
	scene.cubemaps.clear();
	scene.lights.clear();
	glDeleteBuffers(1, &scene.cubemap_ssbo);
	glDeleteBuffers(1, &scene.light_ssbo);
	glDeleteTextures(1, &scene.levelcubemapirradiance_array);
	glDeleteTextures(1, &scene.levelcubemapspecular_array);
	glDeleteTextures(1, &scene.skybox);
	scene.directional_index = -1;

	// add the lights
	auto& llights = engine.level->lights;
	for (int i = 0; i < llights.size(); i++) {
		Level_Light& ll = llights[i];
		if (ll.type == LIGHT_DIRECTIONAL && scene.directional_index == -1) {
			scene.directional_index = i;
		}
		Render_Light rl;
		rl.type = ll.type;
		rl.position = ll.position;
		rl.color = ll.color;
		rl.normal = ll.direction;
		rl.conemin = rl.conemax = ll.spot_angle;
		rl.casts_shadow = false;

		scene.lights.push_back(rl);
	}


	// Render cubemaps
	scene.cubemaps.push_back({});		// skybox probe
	scene.cubemaps[0].found_probe_flag = true;

	auto& espawns = engine.level->espawns;
	for (int i = 0; i < espawns.size(); i++) {
		if (espawns[i].classname == "cubemap_box") {
			Render_Box_Cubemap bc;
			bc.boxmin = espawns[i].position - espawns[i].scale;
			bc.boxmax = espawns[i].position + espawns[i].scale;

			if (espawns[i].name.rfind("_!p1") != std::string::npos)
				bc.priority = 1;
			else if (espawns[i].name.rfind("_!p-1") != std::string::npos)
				bc.priority = -1;

			size_t idfind = espawns[i].name.find("_!i:");
			if (idfind != std::string::npos) {
				bc.id = std::atoi(espawns[i].name.substr(idfind + 4).c_str());
			}

			scene.cubemaps.push_back(bc);

			if (scene.cubemaps.size() >= 128) break;
		}
	}
	for (int i = 0; i < espawns.size(); i++) {
		if (espawns[i].classname == "cubemap") {
			bool found = false;
			int id = -1;
			size_t idfind = espawns[i].name.find("_!i:");
			if (idfind != std::string::npos) {
				id = std::atoi(espawns[i].name.substr(idfind + 4).c_str());
			}

			for (int j = 0; j < scene.cubemaps.size(); j++) {
				auto& bc = scene.cubemaps[j];

				if (id != bc.id) continue;

				Bounds b(bc.boxmin, bc.boxmax);
				if (!b.inside(espawns[i].position, 0.0))
					continue;
				if (bc.found_probe_flag) {
					sys_print("Cubemap box with 2 probes\n");
					continue;
				}
				found = true;
				bc.found_probe_flag = true;
				bc.probe_pos = espawns[i].position;
			}
			if (!found) {
				sys_print("Cubemap not inside box\n");
			}
		}
	}
	for (int i = 0; i < scene.cubemaps.size(); i++) {
		if (!scene.cubemaps[i].found_probe_flag) {
			scene.cubemaps.erase(scene.cubemaps.begin() + i);
			i--;
		}
	}

	scene.levelcubemap_num = scene.cubemaps.size();
	glGenTextures(1, &scene.levelcubemapspecular_array);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, scene.levelcubemapspecular_array);
	glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_R11F_G11F_B10F, CUBEMAP_SIZE, CUBEMAP_SIZE, 6 * scene.levelcubemap_num, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP_ARRAY);
	glCheckError();
	glGenTextures(1, &scene.levelcubemapirradiance_array);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, scene.levelcubemapirradiance_array);
	glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_R11F_G11F_B10F, 32, 32, 6 * scene.levelcubemap_num, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glCheckError();

	uint32_t cmfbo, cmrbo;
	glGenFramebuffers(1, &cmfbo);
	glGenRenderbuffers(1, &cmrbo);
	glBindFramebuffer(GL_FRAMEBUFFER, cmfbo);
	glBindRenderbuffer(GL_RENDERBUFFER, cmrbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, CUBEMAP_SIZE, CUBEMAP_SIZE);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, cmrbo);

	uint32_t temp_buffer;
	glGenTextures(1, &temp_buffer);
	glBindTexture(GL_TEXTURE_CUBE_MAP, temp_buffer);
	for (int i = 0; i < 6; i++)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, CUBEMAP_SIZE, CUBEMAP_SIZE, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	draw.using_skybox_for_specular = true;
	auto& helper = EnviornmentMapHelper::get();

	scene.skybox = helper.create_from_file("hdr_sky2.hdr").original_cubemap;
	// CUBEMAP_SIZE isnt the size of skybox, but its unused anyways
	helper.convolute_irradiance_array(scene.skybox, CUBEMAP_SIZE, scene.levelcubemapirradiance_array, 0, 32);
	helper.compute_specular_array(scene.skybox, CUBEMAP_SIZE, scene.levelcubemapspecular_array, 0, CUBEMAP_SIZE);


	for (int i = 1; i < scene.cubemaps.size(); i++) {
		Render_Box_Cubemap& bc = scene.cubemaps[i];

		render_world_cubemap(bc.probe_pos, cmfbo, temp_buffer, CUBEMAP_SIZE);

		helper.convolute_irradiance_array(temp_buffer, CUBEMAP_SIZE, scene.levelcubemapirradiance_array, i, 32);
		helper.compute_specular_array(temp_buffer, CUBEMAP_SIZE, scene.levelcubemapspecular_array, i, CUBEMAP_SIZE);

		glCheckError();
	}
	draw.using_skybox_for_specular = false;

	glDeleteRenderbuffers(1, &cmrbo);
	glDeleteFramebuffers(1, &cmfbo);
	glDeleteTextures(1, &temp_buffer);


	glGenBuffers(1, &scene.cubemap_ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, scene.cubemap_ssbo);

	struct Cubemap_Ssbo_Struct {
		glm::vec4 bmin;
		glm::vec4 bmax;
		glm::vec4 probepos_priority;
	};
	static Cubemap_Ssbo_Struct probes[128];
	for (int i = 0; i < 128 && i < scene.cubemaps.size(); i++) {
		probes[i].bmin = vec4(scene.cubemaps[i].boxmin,0.0);
		probes[i].bmax = vec4(scene.cubemaps[i].boxmax,0.0);
		probes[i].probepos_priority = vec4(scene.cubemaps[i].probe_pos, scene.cubemaps[i].priority);
	}

	glBufferData(GL_SHADER_STORAGE_BUFFER, (sizeof Cubemap_Ssbo_Struct)* scene.cubemaps.size(), probes, GL_STATIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
