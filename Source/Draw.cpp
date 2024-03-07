#include "Draw.h"
#include "Util.h"
#include "glad/glad.h"
#include "Texture.h"
#include "Game_Engine.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"

Renderer draw;

static const int ALBEDO1_LOC = 0;
static const int NORMAL1_LOC = 1;
static const int ROUGH1_LOC = 2;
static const int METAL1_LOC = 3;
static const int AO1_LOC = 4;
static const int ALBEDO2_LOC = 5;
static const int NORMAL2_LOC = 3; // if double blending, cant have metal or ao 
static const int ROUGH2_LOC = 4;
static const int LIGHTMAP_LOC = 6;
static const int SPECIAL_LOC = 7;

static const int IRRADIANCE_CM_LOC = 13;
static const int SPECULAR_CM_LOC = 9;
static const int BRDF_LUT_LOC = 10;
static const int SHADOWMAP_LOC = 11;
static const int CAUSTICS_LOC = 12;
static const int SSAO_TEX_LOC = 8;



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
	int tex = (mat) ? mat->gl_id : white_texture.gl_id;
	if ((in_world_space != sprite_state.in_world_space || tex != sprite_state.current_t
		|| additive != sprite_state.additive))
		draw_sprite_buffer();

	sprite_state.in_world_space = in_world_space;
	if (sprite_state.current_t != tex || sprite_state.force_set) {
		bind_texture(ALBEDO1_LOC, tex);
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
	ASSERT(bind >= 0 && bind < MAX_SAMPLER_BINDINGS);
	if (cur_tex[bind] != id) {
		//glActiveTexture(GL_TEXTURE0 + bind);
		//glBindTexture(target, id);
		glBindTextureUnit(bind, id);
		cur_tex[bind] = id;

		stats.textures_bound++;
	}
}


void Renderer::set_shader_sampler_locations()
{
	shader().set_int("basecolor", ALBEDO1_LOC);
	shader().set_int("roughness_tex", ROUGH1_LOC);
	shader().set_int("normal_tex", NORMAL1_LOC);
	shader().set_int("metalness_tex", METAL1_LOC);
	shader().set_int("ao_tex", AO1_LOC);

//	shader().set_int("basecolor2", ALBEDO2_LOC);
//	shader().set_int("auxcolor2", ROUGH2_LOC);
//	shader().set_int("normalmap2", NORMAL2_LOC);

	shader().set_int("special", SPECIAL_LOC);
	shader().set_int("lightmap", LIGHTMAP_LOC);

	// >>> PBR BRANCH
	shader().set_int("pbr_irradiance_cubemaps", IRRADIANCE_CM_LOC);
	shader().set_int("pbr_specular_cubemaps", SPECULAR_CM_LOC);
	shader().set_int("PBR_brdflut", BRDF_LUT_LOC);
	//shader().set_int("volumetric_fog", SAMPLER_LIGHTMAP + 4);
	shader().set_int("cascade_shadow_map", SHADOWMAP_LOC);

	shader().set_int("ssao_tex", SSAO_TEX_LOC);

	shader().set_int("caustictex", CAUSTICS_LOC);
}

void Renderer::set_depth_shader_constants()
{

}

void set_standard_draw_data(const Render_Level_Params& params)
{
	glCheckError();

	// >>> PBR BRANCH
	draw.bind_texture(IRRADIANCE_CM_LOC, draw.scene.levelcubemapirradiance_array);
	draw.bind_texture(SPECULAR_CM_LOC, draw.scene.levelcubemapspecular_array);

	glCheckError();
	draw.bind_texture(BRDF_LUT_LOC, EnviornmentMapHelper::get().integrator.lut_id);

	draw.bind_texture(SHADOWMAP_LOC, draw.shadowmap.shadow_map_array);

	glCheckError();

	//shader().set_vec4("aoproxy_sphere", vec4(eng->local_player().position + glm::vec3(0,aosphere.y,0), aosphere.x));
	//shader().set_float("aoproxy_scale_factor", aosphere.z);

	uint32_t ssao_tex = draw.ssao.fullres2;
	if (params.is_probe_render || params.is_water_reflection_pass) ssao_tex = draw.white_texture.gl_id;

	draw.bind_texture(SSAO_TEX_LOC, ssao_tex);
	glCheckError();

	draw.bind_texture(CAUSTICS_LOC, draw.casutics->gl_id);

	glCheckError();


	if (eng->level->lightmap)
		draw.bind_texture(LIGHTMAP_LOC, eng->level->lightmap->gl_id);
	else
		draw.bind_texture(LIGHTMAP_LOC, draw.white_texture.gl_id);

	//glActiveTexture(GL_TEXTURE0 + start + 4);
	//glBindTexture(GL_TEXTURE_3D, draw.volfog.voltexture);

	glCheckError();

	//glBindBufferBase(GL_UNIFORM_BUFFER, 4, draw.volfog.param_ubo);

	glCheckError();
	glBindBufferBase(GL_UNIFORM_BUFFER, 8, draw.shadowmap.csm_ubo);

	glCheckError();

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, draw.scene.cubemap_ssbo);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, draw.shared.scene_mats_ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, draw.shared.gpu_objs_ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, draw.shared.anim_matrix_ssbo);

	glCheckError();
}

void Renderer::set_shader_constants()
{
	shader().set_vec3("light_dir", glm::normalize(-vec3(1)));
}

static int combine_flags_type(int flags, int type, int flag_bits)
{
	return flags + (type >> flag_bits);
}

static const char* sdp_strs[] = {
	"ALPHATEST,",
	"NORMALMAPPED,",
	"LIGHTMAPPED,",
	"ANIMATED,",
	"VERTEX_COLOR,",
};

void compile_game_shaders(Shader* shader_start, int toggle_params, int mandatory_params, const char* other_defines, const char* vert, const char* frag)
{
	std::string shader_defines;
	int num = 1 << Renderer::NUM_SDP;
	int not_toggle = ~(toggle_params|mandatory_params);
	for (int i = 0; i < num; i++) {
		if ((i & not_toggle)!=0) continue;
		if (mandatory_params!=0 && (i & mandatory_params) == 0) continue;
		for (int j = 0; j < Renderer::NUM_SDP; j++) {
			if (i & (1 << j)) {
				shader_defines += sdp_strs[j];
			}
		}
		shader_defines += other_defines;
		if (!shader_defines.empty()) shader_defines.pop_back();	// remove comma

		Shader::compile(&shader_start[i], vert, frag, shader_defines);
		shader_defines.clear();
	}
}

void Renderer::reload_shaders()
{

	// meshbuilder shaders
	Shader::compile(&shader_list[S_SIMPLE], "MbSimpleV.txt", "MbSimpleF.txt");
	Shader::compile(&shader_list[S_TEXTURED], "MbTexturedV.txt", "MbTexturedF.txt");
	Shader::compile(&shader_list[S_TEXTURED3D], "MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE3D");
	Shader::compile(&shader_list[S_TEXTUREDARRAY], "MbTexturedV.txt", "MbTexturedF.txt", "TEXTUREARRAY");
	Shader::compile(&shader_list[S_SKYBOXCUBE], "MbSimpleV.txt", "SkyboxF.txt", "SKYBOX");
	Shader::compile(&shader_list[S_PARTICLE_BASIC], "MbTexturedV.txt", "MbTexturedF.txt", "PARTICLE_SHADER");

	// model shaders
	int num_per_shader = (1 << NUM_SDP);
	int standard_toggle_mask = (1 << SDP_ANIMATED) | (1 << SDP_ALPHATESTED) | (1 << SDP_LIGHTMAPPED) | (1<<SDP_NORMALMAPPED);
	int wind_mask = (1 << SDP_ALPHATESTED) | (1 << SDP_ALPHATESTED) | (1<<SDP_NORMALMAPPED);
	int blend_toggle_mask = (1 << SDP_LIGHTMAPPED) | (1<<SDP_NORMALMAPPED);

	compile_game_shaders(&shader_list[NUM_NON_MODEL_SHADERS + MSHADER_STANDARD * num_per_shader], standard_toggle_mask, 0, "", "AnimBasicV.txt", "PbrBasicF.txt");
	compile_game_shaders(&shader_list[NUM_NON_MODEL_SHADERS + MSHADER_MULTIBLEND * num_per_shader], blend_toggle_mask, (1<<SDP_VERTEXCOLORS), "BLEND2,", "AnimBasicV.txt", "PbrBasicF.txt");
	compile_game_shaders(&shader_list[NUM_NON_MODEL_SHADERS + MSHADER_WIND * num_per_shader], wind_mask, 0, "WIND,", "AnimBasicV.txt", "PbrBasicF.txt");


	// depth shaders for models
	const char* frag = "DepthF.txt";
	Shader::compile(&shader_list[S_ANIMATED_DEPTH], "AnimBasicV.txt", frag, "ANIMATED");
	Shader::compile(&shader_list[S_DEPTH], "AnimBasicV.txt", frag);
	Shader::compile(&shader_list[S_AT_DEPTH], "AnimBasicV.txt", frag, "ALPHATEST");
	Shader::compile(&shader_list[S_WIND_DEPTH], "AnimBasicV.txt", frag, "WIND");
	Shader::compile(&shader_list[S_WIND_AT_DEPTH], "AnimBasicV.txt", frag, "WIND, ALPHATEST");


	// Bloom shaders
	Shader::compile(&shader_list[S_BLOOM_DOWNSAMPLE], "MbTexturedV.txt", "BloomDownsampleF.txt");
	Shader::compile(&shader_list[S_BLOOM_UPSAMPLE], "MbTexturedV.txt", "BloomUpsampleF.txt");
	Shader::compile(&shader_list[S_COMBINE], "MbTexturedV.txt", "CombineF.txt");
	set_shader(shader_list[S_COMBINE]);
	shader().set_int("scene_lit", 0);
	shader().set_int("bloom", 1);
	shader().set_int("lens_dirt", 2);


	// Ssao shaders
	Shader::compile(&shader_list[S_SSAO], "MbTexturedV.txt", "SsaoLOGF.txt");
	Shader::compile(&shader_list[S_XBLUR], "MbTexturedV.txt", "BilateralBlurF.txt");
	Shader::compile(&shader_list[S_YBLUR], "MbTexturedV.txt", "BilateralBlurF.txt", "YBLUR");
	set_shader(S_SSAO);
	shader().set_int("scene_depth", 0);
	shader().set_int("noise_texture", 1);
	set_shader(S_XBLUR);
	shader().set_int("input", 0);
	shader().set_int("scene_depth", 1);
	set_shader(S_YBLUR);
	shader().set_int("input", 0);
	shader().set_int("scene_depth", 1);



	Shader::compile(&shader_list[S_MDI_TESTING], "SimpleMeshV.txt", "UnlitF.txt");


	// volumetric fog shaders
	Shader::compute_compile(&volfog.lightcalc, "VfogScatteringC.txt");
	Shader::compute_compile(&volfog.raymarch, "VfogRaymarchC.txt");
	Shader::compute_compile(&volfog.reproject, "VfogScatteringC.txt", "REPROJECTION");
	volfog.lightcalc.use();
	volfog.lightcalc.set_int("previous_volume", 0);
	volfog.lightcalc.set_int("perlin_noise", 1);


	// water shader
	Shader::compile(&shader_list[S_WATER], "AnimBasicV.txt", "WaterF.txt", "VERTEX_COLOR, NORMALMAPPED");
	set_shader(S_WATER);
	set_shader_sampler_locations();


	// set sampler locations for all model shaders
	int start = NUM_NON_MODEL_SHADERS;
	int end = start + NUM_MST * num_per_shader;
	for (int i = start; i < end; i++) {
		if (shader_list[i].ID != 0) {
			set_shader(shader_list[i]);
			set_shader_sampler_locations();
		}
	}
	
	glCheckError();
	glUseProgram(0);
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

	glm::vec4 custom_clip_plane;
};

void Renderer::upload_ubo_view_constants(uint32_t ubo, glm::vec4 custom_clip_plane)
{
	Ubo_View_Constants_Struct constants;
	constants.view = vs.view;
	constants.viewproj = vs.viewproj;
	constants.invview = glm::inverse(vs.view);
	constants.invproj = glm::inverse(vs.proj);
	constants.viewpos_time = glm::vec4(vs.origin, eng->time);
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

	constants.custom_clip_plane = custom_clip_plane;

	glNamedBufferData(ubo, sizeof Ubo_View_Constants_Struct, &constants, GL_DYNAMIC_DRAW);
}

const static int csm_resolutions[] = { 0, 256, 512, 1024 };


void Shadow_Map_System::init()
{
	make_csm_rendertargets();
	glCreateBuffers(1, &csm_ubo);
	glCreateBuffers(4, frame_view_ubos);
}
void Shadow_Map_System::make_csm_rendertargets()
{
	if (quality == 0)
		return;
	csm_resolution = csm_resolutions[(int)quality];

	glGenTextures(1, &shadow_map_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_map_array);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, csm_resolution, csm_resolution, 4, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	bool hardware_filtering = false;
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
	int setting = draw.shadow_quality_setting.integer();
	if (setting < 0) setting = 0;
	else if (setting > 3) setting = 3;
	draw.shadow_quality_setting.integer() = setting;

	if (quality != setting) {
		quality = setting;
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

	{
		GPUSCOPESTART("Csm setup");

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

		glNamedBufferData(csm_ubo, sizeof Shadowmap_Csm_Ubo_Struct, &upload_data, GL_DYNAMIC_DRAW);
	}
	// now setup scene for rendering
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	{
		GPUSCOPESTART("Render csm layer");
		for (int i = 0; i < 4; i++) {

			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_map_array, 0, i);

			Render_Level_Params params;
			params.output_framebuffer = framebuffer;
			params.pass = Render_Level_Params::SHADOWMAP;
			View_Setup setup;
			setup.width = csm_resolution;
			setup.height = csm_resolution;
			setup.near = nearplanes[i];
			setup.far = farplanes[i];
			setup.viewproj = matricies[i];
			setup.view = setup.proj = mat4(1);
			params.view = setup;
			params.provied_constant_buffer = frame_view_ubos[i];
			params.upload_constants = true;

			draw.render_level_to_target(params);
		}
	}
}



static glm::vec3* GetFrustumCorners(const mat4& view, const mat4& projection)
{
	mat4 inv_viewproj = glm::inverse(projection * view);
	static glm::vec3 corners[8];
	int i = 0;
	for (int x = 0; x < 2; x++) {
		for (int y = 0; y < 2; y++) {
			for (int z = 0; z < 2; z++) {
				vec4 ndc_coords = vec4(2 * x - 1, 2 * y - 1, 2 * z - 1, 1);
				vec4 world_space = inv_viewproj * ndc_coords;
				world_space /= world_space.w;
				corners[i++] = world_space;
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
	glm::vec3* corners = GetFrustumCorners(view.view, camera_cascaded_proj);
	vec3 frustum_center = vec3(0);
	for (int i = 0; i < 8; i++)
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


	glCreateBuffers(1, &light_ssbo);
	glCreateBuffers(1, &param_ubo);

	glCheckError();
}

void Volumetric_Fog_System::compute()
{
	if (!draw.enable_volumetric_fog.integer())
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
		lightcalc.set_vec3("perlin_offset", glm::vec3(eng->time * 0.2, 0, eng->time));


		lightcalc.set_int("num_lights", 0);
		glCheckError();

		glBindImageTexture(2, voltexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

		glDispatchCompute(groups.x, groups.y, groups.z);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glCheckError();

	}
	//static Config_Var* fog_raymarch = cfg.get_var("dbg/raymarch", "1");
	if (1) {
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

Renderer::Renderer()
	: draw_collision_tris("gpu.draw_collision_tris", 0),
	draw_sv_colliders("gpu.draw_colliders",0),
	draw_viewmodel("gpu.draw_viewmodel", 0),
	enable_vsync("gpu.vsync",0),
	enable_bloom("gpu.bloom",1),
	shadow_quality_setting("gpu.shadow_quality",1, (int)CVar_Flags::INTEGER),
	enable_volumetric_fog("gpu.volfog",1),
	enable_ssao("gpu.ssao",1),
	use_halfres_reflections("gpu.halfreswater",1)
{

}

void debug_message_callback(GLenum source, GLenum type, GLuint id, 
	GLenum severity, GLsizei length, GLchar const* message, void const* user_param)
{
	auto const src_str = [source]() {
		switch (source)
		{
		case GL_DEBUG_SOURCE_API: return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "WINDOW SYSTEM";
		case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER COMPILER";
		case GL_DEBUG_SOURCE_THIRD_PARTY: return "THIRD PARTY";
		case GL_DEBUG_SOURCE_APPLICATION: return "APPLICATION";
		case GL_DEBUG_SOURCE_OTHER: return "OTHER";
		}
	}();

	auto const type_str = [type]() {
		switch (type)
		{
		case GL_DEBUG_TYPE_ERROR: return "ERROR";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "UNDEFINED_BEHAVIOR";
		case GL_DEBUG_TYPE_PORTABILITY: return "PORTABILITY";
		case GL_DEBUG_TYPE_PERFORMANCE: return "PERFORMANCE";
		case GL_DEBUG_TYPE_MARKER: return "MARKER";
		case GL_DEBUG_TYPE_OTHER: return "OTHER";
		}
	}();

	auto const severity_str = [severity]() {
		switch (severity) {
		case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
		case GL_DEBUG_SEVERITY_LOW: return "LOW";
		case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
		case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
		}
	}();

	printf("%s, %s, %s, %d: %s\n", src_str, type_str, severity_str, id, message);
}

void imgui_stat_hook()
{
	ImGui::Text("Draw calls: %d", draw.stats.draw_calls);
	ImGui::Text("Total tris: %d", draw.stats.tris_drawn);
	ImGui::Text("Texture binds: %d", draw.stats.textures_bound);
	ImGui::Text("Shader binds: %d", draw.stats.shaders_bound);

}

void Renderer::Init()
{
	bool supports_compression = false;
	bool supports_sprase_tex = false;
	bool supports_bindless = false;
	bool supports_filter_minmax = false;

#ifdef _DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(debug_message_callback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE); 
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

	int num_extensions = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
	for (int i = 0; i < num_extensions; i++) {
		const char* ext = (char*)glGetStringi(GL_EXTENSIONS, i);
		if (strcmp(ext, "GL_ARB_bindless_texture") == 0) supports_bindless = true;
		else if (strcmp(ext, "GL_ARB_sparse_texture") == 0)supports_sprase_tex = true;
		else if (strcmp(ext, "GL_EXT_texture_compression_s3tc") == 0)supports_compression = true;
		else if (strcmp(ext, "GL_ARB_texture_filter_minmax") == 0)supports_filter_minmax = true;
	}

	printf("Extension support:\n");
	printf("-GL_ARB_bindless_texture: %s\n", (supports_bindless)?"yes":"no");
	printf("-GL_ARB_sparse_texture: %s\n", (supports_sprase_tex) ? "yes" : "no");
	printf("-GL_ARB_texture_filter_minmax: %s\n", (supports_filter_minmax) ? "yes" : "no");

	if (!supports_compression) {
		Fatalf("Opengl driver needs GL_EXT_texture_compression_s3tc\n");
	}
	
	InitGlState();

	shared.init();

	float start = GetTime();
	reload_shaders();
	printf("compiled shaders in %f\n", (float)GetTime() - start);

	const uint8_t wdata[] = { 0xff,0xff,0xff };
	const uint8_t bdata[] = { 0x0,0x0,0x0 };
	const uint8_t normaldata[] = { 128,128,255 };
	
	glCreateTextures(GL_TEXTURE_2D, 1, &white_texture.gl_id);
	glTextureStorage2D(white_texture.gl_id, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(white_texture.gl_id, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, wdata);
	glTextureParameteri(white_texture.gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(white_texture.gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateTextureMipmap(white_texture.gl_id);

	glCreateTextures(GL_TEXTURE_2D, 1, &black_texture.gl_id);
	glTextureStorage2D(black_texture.gl_id, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(black_texture.gl_id, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, wdata);
	glTextureParameteri(black_texture.gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(black_texture.gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateTextureMipmap(black_texture.gl_id);

	glCreateTextures(GL_TEXTURE_2D, 1, &flat_normal_texture.gl_id);
	glTextureStorage2D(flat_normal_texture.gl_id, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(flat_normal_texture.gl_id, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, wdata);
	glTextureParameteri(flat_normal_texture.gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(flat_normal_texture.gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateTextureMipmap(flat_normal_texture.gl_id);

	glCreateBuffers(1, &ubo.current_frame);

	perlin3d = generate_perlin_3d({ 16,16,16 }, 0x578437adU, 4, 2, 0.4, 2.0);

	fbo.scene = 0;
	fbo.reflected_scene = 0;
	tex.scene_color = tex.scene_depthstencil = 0;
	tex.reflected_color = tex.reflected_depth = 0;
	InitFramebuffers();

	EnviornmentMapHelper::get().init();
	volfog.init();
	shadowmap.init();
	ssao.init();
	
	lens_dirt = mats.find_texture("lens_dirt.jpg");
	casutics = mats.find_texture("caustics.png");
	waternormal = mats.find_texture("waternormal.png");

	Debug_Interface::get()->add_hook("Render stats", imgui_stat_hook);
}

void Renderer::InitFramebuffers()
{
	const int s_w = eng->window_w.integer();
	const int s_h = eng->window_h.integer();

	glDeleteTextures(1, &tex.scene_color);

	glCreateTextures(GL_TEXTURE_2D, 1, &tex.scene_color);
	glTextureStorage2D(tex.scene_color, 1, GL_RGBA16F, s_w, s_h);
	glTextureParameteri(tex.scene_color, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(tex.scene_color, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(tex.scene_color, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(tex.scene_color, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDeleteTextures(1, &tex.scene_depthstencil);
	glCreateTextures(GL_TEXTURE_2D, 1, &tex.scene_depthstencil);
	glTextureStorage2D(tex.scene_depthstencil, 1, GL_DEPTH_COMPONENT24, s_w, s_h);
	glTextureParameteri(tex.scene_depthstencil, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(tex.scene_depthstencil, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(tex.scene_depthstencil, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(tex.scene_depthstencil, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDeleteFramebuffers(1, &fbo.scene);
	glCreateFramebuffers(1, &fbo.scene);
	glNamedFramebufferTexture(fbo.scene, GL_COLOR_ATTACHMENT0, tex.scene_color, 0);
	glNamedFramebufferTexture(fbo.scene, GL_DEPTH_ATTACHMENT, tex.scene_depthstencil, 0);
	
	unsigned int attachments[1] = { GL_COLOR_ATTACHMENT0 };
	glNamedFramebufferDrawBuffers(fbo.scene, 1, attachments);
	
	glCheckError();

	glDeleteTextures(1, &tex.reflected_color);
	glCreateTextures(GL_TEXTURE_2D, 1, &tex.reflected_color);
	ivec2 reflect_size = ivec2(s_w, s_h);
	if (use_halfres_reflections.integer()) reflect_size /= 2;
	glTextureStorage2D(tex.reflected_color, 1, GL_RGBA16F, reflect_size.x, reflect_size.y);
	glTextureParameteri(tex.reflected_color, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(tex.reflected_color, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(tex.reflected_color, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(tex.reflected_color, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDeleteTextures(1, &tex.reflected_depth);
	glCreateTextures(GL_TEXTURE_2D, 1, &tex.reflected_depth);
	glTextureStorage2D(tex.reflected_depth, 1, GL_DEPTH_COMPONENT24, reflect_size.x, reflect_size.y);
	glTextureParameteri(tex.reflected_depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(tex.reflected_depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(tex.reflected_depth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(tex.reflected_depth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDeleteFramebuffers(1, &fbo.reflected_scene);
	glCreateFramebuffers(1, &fbo.reflected_scene);
	glNamedFramebufferTexture(fbo.reflected_scene, GL_COLOR_ATTACHMENT0, tex.reflected_color, 0);
	glNamedFramebufferTexture(fbo.reflected_scene, GL_DEPTH_ATTACHMENT, tex.reflected_depth, 0);

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

	if (!enable_bloom.integer())
		return;


	MeshBuilder mb;
	mb.Begin();
	mb.Push2dQuad(vec2(-1, -1), vec2(2, 2));
	mb.End();

	glBindFramebuffer(GL_FRAMEBUFFER, fbo.bloom);
	glActiveTexture(GL_TEXTURE0);
	set_shader(S_BLOOM_DOWNSAMPLE);
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

	set_shader(S_BLOOM_UPSAMPLE);
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

	glCheckError();
}

void Renderer::DrawSkybox()
{
	MeshBuilder mb;
	mb.Begin();
	mb.PushSolidBox(-vec3(1), vec3(1), COLOR_WHITE);
	mb.End();

	set_shader(S_SKYBOXCUBE);
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

	if (params.is_probe_render)
		using_skybox_for_specular = true;

	if (1) {
		uint32_t view_ubo = params.provied_constant_buffer;
		bool upload = params.upload_constants;
		if (params.provied_constant_buffer == 0) {
			view_ubo = ubo.current_frame;
			upload = true;
		}
		if (upload)
			upload_ubo_view_constants(view_ubo, params.custom_clip_plane);
		active_constants_ubo = view_ubo;
	}

	glCheckError();

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, active_constants_ubo);
	
	glCheckError();
	
	set_standard_draw_data(params);


	glBindFramebuffer(GL_FRAMEBUFFER, params.output_framebuffer);
	glViewport(0, 0, vs.width, vs.height);
	if (params.clear_framebuffer) {
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	}

	if (params.pass == Render_Level_Params::SHADOWMAP) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(shadowmap.poly_factor, shadowmap.poly_units);
		glCullFace(GL_FRONT);
		glDisable(GL_CULL_FACE);
	}


	if (params.has_clip_plane) {
		glEnable(GL_CLIP_DISTANCE0);
	}
	
	std::vector<Draw_Call>* list;
	if (params.is_water_reflection_pass) list = &shared.opaques;
	else if (params.pass == params.SHADOWMAP) list = &shared.shadows;
	else if (params.pass == params.TRANSLUCENT) list = &shared.transparents;
	else  list = &shared.opaques;

	{
		Model_Drawing_State state;
		for (int d = 0; d < list->size(); d++) {
			Draw_Call& dc = (*list)[d];
			draw_model_real(dc, state);
		}
	}

	glCheckError();

	if (params.pass == Render_Level_Params::OPAQUE) {
		glDepthFunc(GL_LEQUAL);	// for post z prepass
		DrawSkybox();
	}

	glCheckError();


	if (params.pass == Render_Level_Params::SHADOWMAP) {
		glDisable(GL_POLYGON_OFFSET_FILL);
		glCullFace(GL_BACK);
		glEnable(GL_CULL_FACE);
	}

	if (params.has_clip_plane)
		glDisable(GL_CLIP_DISTANCE0);

	glCheckError();

	using_skybox_for_specular = false;
}

void Renderer::ui_render()
{
	GPUFUNCTIONSTART;


	set_shader(S_TEXTURED);
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
		bind_texture(ALBEDO1_LOC, building_ui_texture);
		ui_builder.End();
		ui_builder.Draw(GL_TRIANGLES);
	}

	glCheckError();


	glDisable(GL_BLEND);
	if (0) {
		set_shader(S_TEXTURED);
		glCheckError();

		shader().set_mat4("Model", mat4(1));
		glm::mat4 proj = glm::ortho(0.f, (float)cur_w, -(float)cur_h, 0.f);
		shader().set_mat4("ViewProj", mat4(1));
		shader().set_int("slice", (int)slice_3d);

		ui_builder.Begin();
		ui_builder.Push2dQuad(glm::vec2(-1, 1), glm::vec2(1, -1), glm::vec2(0, 0),
			glm::vec2(1, 1), COLOR_WHITE);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex.scene_depthstencil);

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

	int texnum = (t) ? t->gl_id : white_texture.gl_id;
	float tw = (t) ? t->width : 1;
	float th = (t) ? t->height : 1;

	if (texnum != building_ui_texture && ui_builder.GetBaseVertex() > 0) {
		bind_texture(ALBEDO1_LOC, building_ui_texture);
		ui_builder.End();
		ui_builder.Draw(GL_TRIANGLES);
		ui_builder.Begin();
	}
	building_ui_texture = texnum;
	ui_builder.Push2dQuad(glm::vec2(x, y), glm::vec2(w, h), glm::vec2(srcx / tw, srcy / th),
		glm::vec2(srcw / tw, srch / th), color);
}

void Shared_Gpu_Driven_Resources::init()
{
	glCreateBuffers(1, &anim_matrix_ssbo);
	glCreateBuffers(1, &gpu_objs_ssbo);
	glCreateBuffers(1, &scene_mats_ssbo);
}

void Shared_Gpu_Driven_Resources::make_draw_calls_from(
	const Mesh* mesh,
	glm::mat4 transform,
	vector<Game_Shader*>& mat_list,
	const Animator* animator,
	bool casts_shadows,
	glm::vec4 colorparam)
{
	Gpu_Object obj;
	
	if (animator) {
		obj.anim_matrix_offset = skinned_matricies.size();
		auto& mats = animator->GetBones();
		for (int i = 0; i < animator->model->bones.size(); i++) {
			skinned_matricies.push_back(mats[i]);
		}
	}

	obj.model = transform;
	obj.invmodel = glm::inverse(transform);
	obj.color_val = colorparam;

	int obj_idx = gpu_objects.size();
	gpu_objects.push_back(obj);

	for (int s = 0; s < mesh->parts.size(); s++) {
		Draw_Call dc;
		dc.mat = mat_list[mesh->parts[s].material_idx];
		if (1) {
			// map it to a buffer
			Gpu_Material gpumat;
			gpumat.diffuse_tint = dc.mat->diffuse_tint;
			gpumat.rough_mult = dc.mat->roughness_mult;
			gpumat.metal_mult = dc.mat->metalness_mult;
			gpumat.rough_remap_x = dc.mat->roughness_remap_range.x;
			gpumat.rough_remap_y = dc.mat->roughness_remap_range.y;

			dc.mat_index = scene_mats.size();
			scene_mats.push_back(gpumat);
		}
		dc.object_index = obj_idx;
		dc.submesh = s;
		dc.mesh = mesh;

		int shade_index = draw.get_shader_index(*mesh, *dc.mat, false);
		int tex_index = (dc.mat->images[0]) ? dc.mat->images[0]->gl_id : 0;
		int alpha_and_other = dc.mat->backface | dc.mat->alpha_type;
		int vertfmt = mesh->format;
		dc.sort = (shade_index << 38) | (alpha_and_other<<36) | (tex_index<<4) | vertfmt;

		if (dc.mat->is_translucent()) {
			transparents.push_back(dc);
		}
		else {
			opaques.push_back(dc);
			if (casts_shadows) {
				shade_index = draw.get_shader_index(*mesh, *dc.mat, true);
				dc.sort = (shade_index << 38) | (alpha_and_other << 36) | (tex_index << 4) | vertfmt;
				shadows.push_back(dc);
			}
		}
	}
}
#include <algorithm>
void Shared_Gpu_Driven_Resources::build_draw_calls()
{
	skinned_matricies.clear();
	gpu_objects.clear();
	opaques.clear();
	transparents.clear();
	shadows.clear();
	scene_mats.clear();

	//int scene_pre_size = scene_mats.size();

	Level* level = eng->level;

	for (int m = 0; m < level->static_mesh_objs.size(); m++) {
		Static_Mesh_Object& smo = level->static_mesh_objs[m];
		ASSERT(smo.model);
		make_draw_calls_from(
			&smo.model->mesh,
			smo.transform,
			smo.model->mats,
			nullptr,
			true,
			glm::vec4(1.f)
		);
	}
	for (int m = 0; m < level->level_prefab->nodes.size(); m++) {
		auto& node = level->level_prefab->nodes[m];
		auto& mesh = level->level_prefab->meshes[node.mesh_idx];
		make_draw_calls_from(
			&mesh,
			node.transform,
			level->level_prefab->mats,
			nullptr,
			true,
			glm::vec4(1.f)
		);
	}

	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		auto& ent = eng->get_ent(i);
		//auto& ent = cgame->entities[i];
		if (!ent.active())
			continue;
		if (!ent.model)
			continue;

		if (i == eng->player_num() && !eng->local.thirdperson_camera.integer())
			continue;

		mat4 model;
		if (ent.using_interpolated_pos_and_rot)
			model = glm::translate(mat4(1), ent.local_sv_interpolated_pos);
		else
			model = glm::translate(mat4(1), ent.position);
		model = model * glm::eulerAngleXYZ(ent.rotation.x, ent.rotation.y, ent.rotation.z);
		model = glm::scale(model, vec3(1.f));

		Animator* a = (ent.model->animations) ? &ent.anim : nullptr;

		make_draw_calls_from(&ent.model->mesh, model, ent.model->mats, a, true, glm::vec4(1.f));

		if (ent.type == ET_PLAYER && a && ent.inv.active_item != Game_Inventory::UNEQUIP) {

			Game_Item_Stats& stat = get_item_stats()[ent.inv.active_item];


			Model* m = FindOrLoadModel(stat.world_model);
			if (!m) continue;

			int index = ent.model->bone_for_name("weapon");
			int index2 = ent.model->bone_for_name("magazine");
			glm::mat4 rotate = glm::rotate(mat4(1), HALFPI, vec3(1, 0, 0));
			if (index == -1 || index2 == -1) {
				sys_print("no weapon bone\n");
				continue;
			}
			const Bone& b = ent.model->bones.at(index);
			glm::mat4 transform = a->GetBones()[index];
			transform = model * transform * mat4(b.posematrix) * rotate;
			make_draw_calls_from(&m->mesh, transform,m->mats, nullptr, true, glm::vec4(1.f));

			//if (stat.category == ITEM_CAT_RIFLE) {
			//	std::string mod = stat.world_model;
			//	mod = mod.substr(0, mod.rfind('.'));
			//	mod += "_mag.glb";
			//	Model* mag_mod = FindOrLoadModel(mod.c_str());
			//	if (mag_mod) {
			//		const Bone& mag_bone = ent.model->bones.at(index2);
			//		transform = a->GetBones()[index2];
			//		transform = model * transform * mat4(mag_bone.posematrix) * rotate;
			//		//DrawModel(pass, mag_mod, transform);
			//
			//		draw_model_real(mag_mod->mesh, mag_mod->mats, transform, nullptr, nullptr, state);
			//	}
			//}
		}
	}
	for (int i = 0; i < draw.immediate_draw_calls.size(); i++) {
		auto& call = draw.immediate_draw_calls[i];
		make_draw_calls_from(&call.model->mesh,call.transform,call.model->mats, nullptr,true, glm::vec4(1.f));
	}
	draw.immediate_draw_calls.clear();

	if (eng->local.thirdperson_camera.integer() == 0 && draw.draw_viewmodel.integer() == 1)
	{
		mat4 invview = glm::inverse(draw.vs.view);

		Game_Local* gamel = &eng->local;
		mat4 model2 = glm::translate(invview, vec3(0.18, -0.18, -0.25) + gamel->viewmodel_offsets + gamel->viewmodel_recoil_ofs);
		model2 = glm::scale(model2, glm::vec3(gamel->vm_scale.x));


		model2 = glm::translate(model2, gamel->vm_offset);
		model2 = model2 * glm::eulerAngleY(PI + PI / 128.f);

		make_draw_calls_from(&eng->local.viewmodel->mesh, model2,
			eng->local.viewmodel->mats, &eng->local.viewmodel_animator, false, glm::vec4(1.f));
	}


	glNamedBufferData(scene_mats_ssbo, sizeof Gpu_Material * scene_mats.size(), scene_mats.data(), GL_STATIC_DRAW);
	glNamedBufferData(anim_matrix_ssbo, sizeof glm::mat4 * skinned_matricies.size(), skinned_matricies.data(), GL_DYNAMIC_DRAW);
	glNamedBufferData(gpu_objs_ssbo, sizeof Gpu_Object * gpu_objects.size(), gpu_objects.data(), GL_DYNAMIC_DRAW);

	auto compare = [](const Draw_Call& a, const Draw_Call& b) {
		return a.sort < b.sort;
	};

	std::sort(shadows.begin(), shadows.end(), compare);
	std::sort(opaques.begin(), opaques.end(), compare);
	std::sort(transparents.begin(), transparents.end(), compare);
}



void Renderer::extract_objects()
{
	GPUFUNCTIONSTART;
	shared.build_draw_calls();
}


// drawing structure
typedef  struct {
	GLuint  count;
	GLuint  primCount;
	GLuint  firstIndex;
	GLint   baseVertex;
	GLuint  baseInstance;
} DrawElementsIndirectCommand;

struct DrawCallObject
{
	int index_to_obj_data;			// ObjLevelData
	int index_to_mat_data;			// Gpu_Mat
	int index_to_mesh_draw_call;	// This indexes to the DrawElementsIndirectCommand for the mesh
	int padding;
};

struct ObjLevelData
{
	glm::mat4x4 transform;
	vec4 origin_and_radius;
	int animation_start;
	float boundsx;
	float boundsy;
	float boundsz;
	vec4 color_paraml;
};




template<typename T>
struct Gpu_Buf
{
	uint32_t handle;
	uint32_t allocated;
	uint32_t used;
};

struct Mesh_Pass_Mdi_Batch
{
	Game_Shader* material;
	int vert_fmt;
	int start;
	int end;
};

struct High_Level_Render_Object
{
	Mesh* mesh;
	Game_Shader* material;
	int obj_data_index = 0;
	int draw_calls_start = 0;
	int draw_calls_count = 0;
};

struct Culling_Data_Ubo
{
	mat4 view;
	mat4 proj;
	float frustum[4];
	float znear;
	float zfar;
	int num_calls;
	int enable_culling;
};


class Persistently_Mapped_Buffer
{
public:
	// initializes with a tripple buffered buffer of size size_per_buffer
	// call this again if you need to resize the buffer
	void init(uint32_t size_per_buffer) {
		if (buffer != 0) {
			glUnmapNamedBuffer(buffer);
			glDeleteBuffers(1, &buffer);
		}
		glCreateBuffers(1, &buffer);

		this->size_per_buffer = size_per_buffer;
		total_size = size_per_buffer * 3;
		
		uint32_t mapflags = GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT;
		glNamedBufferStorage(buffer, total_size, nullptr, mapflags);
		ptr = glMapNamedBufferRange(buffer, 0, total_size, mapflags);
		
		syncobjs[0] = syncobjs[1] = syncobjs[2] = 0;
		index = 0;
	}

	// waits for fence and returns the pointer to the area good for writing
	// only call this once per frame to get pointer as it increments what sub-buffer it uses
	void* wait_and_get_write_ptr() {
		index = (index + 1) % 3;

		if (syncobjs[index] != 0) {
			GLbitfield waitFlags = 0;
			GLuint64 waitDuration = 0;
			while (1) {
				GLenum waitRet = glClientWaitSync(syncobjs[index], waitFlags, waitDuration);
				if (waitRet == GL_ALREADY_SIGNALED || waitRet == GL_CONDITION_SATISFIED) {
					return (char*)ptr + size_per_buffer * index;
				}

				if (waitRet == GL_WAIT_FAILED) {
					assert(!"Not sure what to do here. Probably raise an exception or something.");
					return nullptr;
				}

				// After the first time, need to start flushing, and wait for a looong time.
				waitFlags = GL_SYNC_FLUSH_COMMANDS_BIT;
				waitDuration = 100'000'000;
			}
		}
		return (char*)ptr + size_per_buffer * index;
	}

	// call this after you are done writing
	void lock_current_range() {
		syncobjs[index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	}

	// binds the currently used range to binding point
	void bind_buffer(GLenum target, int binding) {
		uintptr_t offset = get_offset();
		uintptr_t size = size_per_buffer;
		glBindBufferRange(target, binding, buffer, offset, size);
	}

	uintptr_t get_offset() {
		return  (index * size_per_buffer);
	}

	uint32_t get_handle() {
		return buffer;
	}

	uint32_t get_buffer_size() {
		return size_per_buffer;
	}

	GLsync syncobjs[3];
	void* ptr;
	int index;
	uint32_t size_per_buffer = 0;
	uint32_t total_size;
	uint32_t buffer = 0;
};

// Initialization: 
// 
// final_draw_calls is initialized to all the possible meshes of the objects
//	on cpu: set element offset, element count, vertex base
// 
// obj_data is initialized and updated to the objects in the scene
// 
// draw_calls are created once when the ObjLevelData is created and aren't 
// changed unless a material change happens or the obj is deleted
// 
// draw_call_indirection is set to the size of draw_calls
//
// Per frame:
// gpu culling executes for each draw call, performs frustum/hi-z (with two pass system later)
// then, it runs a compaction parallel prefix whatever sum on the final_draw_calls, which should compact them and remove 0 draw call meshes
// next, it fills the draw_call_indirection buffer
// the draw_call_indirection buffer is used when drawing an object by indexing it with baseinstance_id + instance_id
// this then gives you an integer indexinto the draw_calls structure
//
// so to fill out this draw_call_indirection buffer, the gpu needs to set the baseinstance_id + instance_offset location of 
// the indirection buffer to the index of the draw_call
// since the parallel prefix sum was done, each DrawElementsIndirectCommand now has the correct baseinstance_id

struct Mesh_Pass_Mdi
{
	vector<High_Level_Render_Object> high_level_objs;
	vector<ObjLevelData> obj_data;		// 1 obj data creates n draw calls per material
	vector<DrawCallObject> draw_calls;
	vector<Gpu_Material> materials;
	vector<Mesh_Pass_Mdi_Batch> batches;

	Gpu_Buf<ObjLevelData> gpu_objs;
	Gpu_Buf<DrawCallObject> gpu_draws;
	Gpu_Buf<Gpu_Material> gpu_materials;
	Gpu_Buf<uint32_t> draw_call_indirection;
	Gpu_Buf<DrawElementsIndirectCommand> final_draw_calls;

	// draw element indirect commands
	Persistently_Mapped_Buffer dei_cmds;
	// draw call objects
	Persistently_Mapped_Buffer drawcalls;
	uint32_t drawid_to_obj;



	uint32_t culling_data_ubo;


	int add_object(Mesh* mesh, Game_Shader* mat, glm::mat4 transform) {
		
		ObjLevelData obj;
		obj.animation_start = 0;
		obj.transform = transform;
		obj.origin_and_radius = glm::vec4(mesh->aabb.get_center(), mesh->aabb.longest_axis() * 0.5f);
		int obj_index = obj_data.size();
		obj_data.push_back(obj);


		int draw_call_start = draw_calls.size();
		for (int i = 0; i < mesh->parts.size(); i++) {
			DrawCallObject call;
			call.index_to_mesh_draw_call = obj_index;
			call.index_to_obj_data = obj_index;
			call.index_to_mat_data = 0;
			draw_calls.push_back(call);
		}

		High_Level_Render_Object high_level_obj = {};
		high_level_obj.draw_calls_count = mesh->parts.size();
		high_level_obj.draw_calls_start = draw_call_start;
		high_level_obj.material = mat;
		high_level_obj.mesh = mesh;
		high_level_obj.obj_data_index = obj_index;

		high_level_objs.push_back(high_level_obj);
		return high_level_objs.size() - 1;
	}

	void init() {
		glCreateBuffers(1, &culling_data_ubo);
		glCreateBuffers(1, &gpu_objs.handle);
		glCreateBuffers(1, &gpu_materials.handle);
		glCreateBuffers(1, &gpu_draws.handle);
		glCreateBuffers(1, &draw_call_indirection.handle);
		glCreateBuffers(1, &final_draw_calls.handle);
		GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		
	}
	void update_and_cull() {
		Culling_Data_Ubo ubo;
		ubo.enable_culling = true;
		ubo.znear = draw.vs.near;
		ubo.zfar = draw.vs.far;
		ubo.view = draw.vs.view;
		ubo.proj = draw.vs.proj;
		ubo.num_calls = draw_calls.size();


	}
};

bool use_simple_sphere = false;
int num_batches_to_render = 2;
int num_prims_per_batch = 5;
bool use_persistent_mapped = true;
static const int MAX_OBJECTS = 27'000;
bool mdi_naive = false;
bool use_storage = false;
bool wireframe_mdi = false;
void mdi_test_imgui()
{
	ImGui::DragInt("batches", &num_batches_to_render, 1.f,0, 1000);
	ImGui::DragInt("prims", &num_prims_per_batch, 1.f, 0, 10000);
	ImGui::Checkbox("use_persistent_mapped", &use_persistent_mapped);
	ImGui::Checkbox("storage", &use_storage);
	ImGui::Checkbox("naive", &mdi_naive);
	ImGui::Checkbox("simple_sphere", &use_simple_sphere);
	ImGui::Checkbox("wireframe", &wireframe_mdi);

	if (num_batches_to_render * num_prims_per_batch > MAX_OBJECTS) {
		num_prims_per_batch = MAX_OBJECTS /num_batches_to_render;
	}
}

extern bool use_32_bit_indicies;
#include "Meshlet.h"
void multidraw_testing()
{
	GPUFUNCTIONSTART;

	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);

	static bool has_initialized = false;

	static Model* spherelod0;
	static Model* spherelod1;
	static Persistently_Mapped_Buffer mdi_buffer_pm;
	static Persistently_Mapped_Buffer mdi_buffer2_pm;
	static Persistently_Mapped_Buffer mdi_command_buf_pm;
	static Shader naiveshader;
	const static int MAX_DRAW_CALLS = MAX_OBJECTS;

	static vector<glm::mat4> matricies;

	static Chunked_Model* meshlet_model;

	if (!has_initialized) {

		meshlet_model = get_chunked_mod("monkey.glb");

		for (int y = 0; y < 30; y++) {
			for (int z = 0; z < 30; z++) {
				for (int x = 0; x < 30; x++) {
					matricies.push_back(glm::scale(glm::translate(glm::mat4(1), glm::vec3(x, y, z)*0.9f),glm::vec3(0.5)));

				}
			}
		}
		Texture* textures[4];
		textures[0] = mats.find_texture("dumb/123.jpg");
		textures[1] = mats.find_texture("dumb/1675880726829067.jpg");
		textures[2] = mats.find_texture("dumb/1646062318546.jpg");
		textures[3] = mats.find_texture("dumb/1674915612177470.jpg");

		// persistent mapped
		{
			Random r(23);
			mdi_buffer_pm.init(sizeof(glm::uvec4) * MAX_DRAW_CALLS);
			glm::uvec4* data = (glm::uvec4*)mdi_buffer_pm.wait_and_get_write_ptr();
			for (int i = 0; i < MAX_DRAW_CALLS; i++) {
				Texture* t = textures[r.RandI(0, 3)];
				data[i] = glm::uvec4(t->bindless_handle & UINT32_MAX, t->bindless_handle >> 32,0, 0);
			}
			mdi_buffer_pm.lock_current_range();

			mdi_buffer2_pm.init(sizeof(glm::mat4) * MAX_DRAW_CALLS);
			glm::mat4* mats = (glm::mat4*)mdi_buffer2_pm.wait_and_get_write_ptr();
			for (int i = 0; i < MAX_DRAW_CALLS; i++)
				mats[i] = matricies[i];
			mdi_buffer2_pm.lock_current_range();

			mdi_command_buf_pm.init(MAX_DRAW_CALLS * sizeof(DrawElementsIndirectCommand));
		}

		Shader::compile(&naiveshader, "SimpleMeshV.txt", "UnlitF.txt", "NAIVE");

		spherelod0 = mods.find_or_load("monkey.glb");
		spherelod1 = mods.find_or_load("spherelod1.glb");
		Debug_Interface::get()->add_hook("mdi testing", mdi_test_imgui);
		has_initialized = true;
	}



	Model* sphere = (use_simple_sphere) ? spherelod1 : spherelod0;

	GLenum index_type = (use_32_bit_indicies) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
	bool meshlet = true;
	if (mdi_naive) {

		naiveshader.use();

		mdi_buffer_pm.bind_buffer(GL_SHADER_STORAGE_BUFFER, 3);
		if (!meshlet) {
			glBindVertexArray(sphere->mesh.vao);
			int base_vertex = sphere->mesh.parts[0].base_vertex + sphere->mesh.merged_vert_offset;
			int count = sphere->mesh.parts[0].element_count;
			int first_index = sphere->mesh.parts[0].element_offset + sphere->mesh.merged_index_pointer;
			for (int i = 0; i < num_batches_to_render * num_prims_per_batch; i++) {
				naiveshader.set_mat4("model", matricies[i]);
				naiveshader.set_int("integer", i);
				glDrawElementsBaseVertex(GL_TRIANGLES, count, index_type, (void*)first_index, base_vertex);
			}
		}
		else {
			glBindVertexArray(meshlet_model->vao);

			if (wireframe_mdi)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			for (int i = 0; i < num_batches_to_render * num_prims_per_batch; i++) {
				naiveshader.set_mat4("model", matricies[i]);

				Submesh& submesh = meshlet_model->model->mesh.parts[0];
				Mesh& orgmesh = meshlet_model->model->mesh;
				for (int j = 0; j < meshlet_model->chunks.size(); j++) {
					naiveshader.set_int("integer", j);
					Chunk& chunk = meshlet_model->chunks[j];

					int base_vertex = submesh.base_vertex + orgmesh.merged_vert_offset;
					int count = chunk.index_count;
					int first_index = chunk.index_offset;
					
					glDrawElementsBaseVertex(GL_TRIANGLES, count, index_type, (void*)first_index, base_vertex);
				}
			}

			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		}

	}
	else {

		DrawElementsIndirectCommand* cmds =  (DrawElementsIndirectCommand*)mdi_command_buf_pm.wait_and_get_write_ptr();
		int num_commands = num_batches_to_render;
		for (int i = 0; i < num_commands; i++) {
			DrawElementsIndirectCommand& cmd = cmds[i];
			cmd.baseInstance = i * num_prims_per_batch;
			cmd.baseVertex = sphere->mesh.parts[0].base_vertex + sphere->mesh.merged_vert_offset;
			cmd.count = sphere->mesh.parts[0].element_count;
			cmd.firstIndex = sphere->mesh.parts[0].element_offset + sphere->mesh.merged_index_pointer;
			cmd.firstIndex /= (use_32_bit_indicies)?4:2;
			cmd.primCount = num_prims_per_batch;
		}
		mdi_command_buf_pm.lock_current_range();

		mdi_buffer2_pm.bind_buffer(GL_SHADER_STORAGE_BUFFER, 2);
		mdi_buffer_pm.bind_buffer(GL_SHADER_STORAGE_BUFFER, 3);
		draw.set_shader(draw.S_MDI_TESTING);

		glBindVertexArray(sphere->mesh.vao);
		
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mdi_command_buf_pm.get_handle());

		glMultiDrawElementsIndirect(GL_TRIANGLES, index_type, (void*)mdi_command_buf_pm.get_offset(), num_commands, sizeof(DrawElementsIndirectCommand));
	}
	draw.stats.tris_drawn = sphere->mesh.parts[0].element_count * num_batches_to_render * num_prims_per_batch / 3;
}


#include "EditorDoc.h"
void Renderer::scene_draw(bool editor_mode)
{
	GPUFUNCTIONSTART;

	stats = {};

	cur_shader = 0;
	for (int i = 0; i < MAX_SAMPLER_BINDINGS; i++)
		cur_tex[i] = 0;
	if (cur_w != eng->window_w.integer() || cur_h != eng->window_h.integer())
		InitFramebuffers();
	lastframe_vs = current_frame_main_view;

	if (editor_mode)
		current_frame_main_view = eng->eddoc->get_vs();
	else
		current_frame_main_view = eng->local.last_view;

	if (enable_vsync.integer())
		SDL_GL_SetSwapInterval(1);
	else
		SDL_GL_SetSwapInterval(0);

	vs = current_frame_main_view;
	upload_ubo_view_constants(ubo.current_frame);
	active_constants_ubo = ubo.current_frame;

	immediate_draw_calls.clear();
	if (editor_mode)
		eng->eddoc->scene_draw_callback();

	if (!editor_mode) {
		extract_objects();

		shadowmap.update();

		//volfog.compute();

		// depth prepass
		{
			GPUSCOPESTART("Depth prepass");
			Render_Level_Params params;
			params.output_framebuffer = fbo.scene;
			params.view = current_frame_main_view;
			params.pass = Render_Level_Params::DEPTH;
			params.upload_constants = false;
			params.provied_constant_buffer = ubo.current_frame;
			params.draw_viewmodel = true;
			render_level_to_target(params);
		}

		// render ssao using prepass buffer
		if (enable_ssao.integer())
			ssao.render();

		// planar reflection render
		{
			GPUSCOPESTART("Planar reflection");
			planar_reflection_pass();
		}

		// main level render
		{
			GPUSCOPESTART("Main level render opaques");
			Render_Level_Params params;
			params.output_framebuffer = fbo.scene;
			params.view = current_frame_main_view;
			params.pass = Render_Level_Params::OPAQUE;
			params.clear_framebuffer = false;
			params.upload_constants = true;
			params.provied_constant_buffer = ubo.current_frame;
			params.draw_viewmodel = true;

			glDepthMask(GL_FALSE);
			glDepthFunc(GL_EQUAL);
			render_level_to_target(params);
			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);
		}
		{
			GPUSCOPESTART("Main level translucents");
			Render_Level_Params params;
			params.output_framebuffer = fbo.scene;
			params.view = current_frame_main_view;
			params.pass = Render_Level_Params::TRANSLUCENT;
			params.clear_framebuffer = false;
			params.upload_constants = true;
			params.provied_constant_buffer = ubo.current_frame;
			params.draw_viewmodel = true;
			glDepthMask(GL_FALSE);
			render_level_to_target(params);
			glDepthMask(GL_TRUE);
		}
	}
	else {
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, active_constants_ubo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo.scene);
		glDisable(GL_BLEND);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fbo.scene);
	multidraw_testing();

	DrawEntBlobShadows();
	eng->local.pm.draw_particles();



	set_shader(S_SIMPLE);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.f));

	if (editor_mode)
		eng->eddoc->overlays_draw();

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

	set_shader(S_COMBINE);
	shader().set_mat4("Model", mat4(1));
	shader().set_mat4("ViewProj", mat4(1));
	uint32_t bloom_tex = tex.bloom_chain[0];
	if (!enable_bloom.integer()) bloom_tex = black_texture.gl_id;
	bind_texture(0, tex.scene_color);
	bind_texture(1, bloom_tex);
	bind_texture(2, lens_dirt->gl_id);
	mb.Draw(GL_TRIANGLES);

	mb.Begin();
	if (draw_sv_colliders.integer()) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (eng->ents[i].type == ET_PLAYER) {
				AddPlayerDebugCapsule(eng->ents[i], &mb, COLOR_CYAN);
			}
		}
	}

	mb.End();
	set_shader(S_SIMPLE);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.f));

	if (draw_collision_tris.integer())
		DrawCollisionWorld(eng->level);

	mb.Draw(GL_LINES);

	//game.rays.End();
	//game.rays.Draw(GL_LINES);
	if (eng->is_host) {
		//phys_debug.End();
		//phys_debug.Draw(GL_LINES);
	}
	
	if(!editor_mode)
		ui_render();

	mb.Free();

	//cubemap_positions_debug();

	glCheckError();
	glClear(GL_DEPTH_BUFFER_BIT);

	// FIXME: ubo view constant buffer might be wrong since its changed around a lot (bad design)
}

void Renderer::cubemap_positions_debug()
{
	set_shader(S_SIMPLE);
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
		Entity* e = &eng->ents[i];
		if (!e->active()) continue;

		RayHit rh;
		Ray r;
		r.pos = e->position + glm::vec3(0, 0.1f, 0);
		r.dir = glm::vec3(0, -1, 0);
		rh = eng->phys.trace_ray(r, i, PF_WORLD);

		if (rh.dist < 0)
			continue;

		AddBlobShadow(rh.pos + vec3(0, 0.05, 0), rh.normal, CHAR_HITBOX_RADIUS * 4.5f);
	}
	glCheckError();

	shadowverts.End();
	glCheckError();

	set_shader(S_PARTICLE_BASIC);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.0));
	shader().set_vec4("tint_color", vec4(0, 0, 0, 1));
	glCheckError();

	bind_texture(0, eng->media.blob_shadow->gl_id);
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

void Renderer::draw_model_immediate(Draw_Model_Frontend_Params params)
{
	immediate_draw_calls.push_back(params);
}



float wsheight = 3.0;
float wsradius = 1.5;
float wsstartheight = 0.2;
float wsstartradius = 0.2;
vec3 wswind_dir = glm::vec3(1, 0, 0);
float speed = 1.0;

void Renderer::set_wind_constants()
{
	shader().set_float("time", eng->time);
	shader().set_float("height", wsheight);
	shader().set_float("radius", wsradius);
	shader().set_float("startheight", wsstartheight);
	shader().set_float("startradius", wsstartradius);
	shader().set_vec3("wind_dir", wswind_dir);
	shader().set_float("speed", speed);
}

void Renderer::set_water_constants()
{
	// use sampler1 for reflection map, sampler2 for the depth of the current render
	bind_texture(ALBEDO1_LOC, tex.reflected_color);
	bind_texture(ROUGH1_LOC, tex.scene_depthstencil);
	bind_texture(NORMAL1_LOC, waternormal->gl_id);
	bind_texture(SPECIAL_LOC, tex.reflected_depth);
}

int Renderer::get_shader_index(const Mesh& mesh, const Game_Shader& gs, bool depth_pass)
{
	bool is_alpha_test = gs.alpha_type == gs.A_TEST;
	bool is_lightmapped = mesh.has_lightmap_coords();
	bool has_colors = mesh.has_colors();
	int shader_type = gs.shader_type;
	bool is_normal_mapped = mesh.has_tangents();
	bool is_animated = mesh.has_bones();

	int shader_index = -1;

	using sl = Renderer::Shader_List;
	if (depth_pass) {
		if (shader_type == gs.S_WINDSWAY) {
			shader_index = sl::S_WIND_DEPTH;
			if (is_alpha_test)
				shader_index = sl::S_WIND_AT_DEPTH;
		}
		else {
			if (is_animated) {
				shader_index = sl::S_ANIMATED_DEPTH;
			}
			else {
				if (is_alpha_test)
					shader_index = sl::S_AT_DEPTH;
				else
					shader_index = sl::S_DEPTH;
			}
		}
	}
	else {
		int param_bitmask = 0;
		if (is_animated) param_bitmask |= (1 << SDP_ANIMATED);
		if (is_alpha_test) param_bitmask |= (1 << SDP_ALPHATESTED);
		if (is_normal_mapped) param_bitmask |= (1 << SDP_NORMALMAPPED);
		if (is_lightmapped) param_bitmask |= (1 << SDP_LIGHTMAPPED);
		if (has_colors) param_bitmask |= (1 << SDP_VERTEXCOLORS);

		int shader_type = gs.shader_type;

		if (shader_type == Game_Shader::S_DEFAULT) {
			shader_type = MSHADER_STANDARD;
			param_bitmask &= ~(1 << SDP_VERTEXCOLORS);
		}
		else if (shader_type == Game_Shader::S_2WAYBLEND) {
			shader_type = MSHADER_MULTIBLEND;

			if (!gs.references[0] || !gs.references[1])
				return -1;
		}
		else if (shader_type == Game_Shader::S_WINDSWAY) {
			shader_type = MSHADER_WIND;
			param_bitmask &= ~(1 << SDP_VERTEXCOLORS);
		}

		if (shader_type == Game_Shader::S_WATER) {
			shader_index = S_WATER;
		}
		else {
			shader_index = NUM_NON_MODEL_SHADERS + (shader_type) * (1 << NUM_SDP) + param_bitmask;
		}
	}

	if (shader_list[shader_index].ID == 0)
		shader_index = -1;

	return shader_index;
}

#define SET_OR_USE_FALLBACK(texture, where, fallback) \
if(gs->images[texture]) bind_texture(where, gs->images[texture]->gl_id); \
else bind_texture(where, fallback.gl_id);


// this function sucks so bad
void Renderer::draw_model_real(const Draw_Call& dc,
	Model_Drawing_State& state)
{
	const Submesh& mp = dc.mesh->parts[dc.submesh];
	Game_Shader* gs = dc.mat;
	bool is_animated = dc.mesh->has_bones();
	bool is_depth = state.pass == Render_Level_Params::SHADOWMAP || state.pass == Render_Level_Params::DEPTH;
	int next_shader = get_shader_index(*dc.mesh, *gs, is_depth);

	bool is_water = gs->shader_type == Game_Shader::S_WATER;

	if (next_shader == -1) return;

	// water only renders in the real geometry pass
	if (state.is_water_reflection_pass && is_water)
		return;

	if (state.initial_set || next_shader != state.current_shader) {
		state.current_shader = next_shader;

		set_shader(next_shader);

		if (gs->shader_type == Game_Shader::S_WINDSWAY) {
			set_wind_constants();
		}
		else if (is_water) {
			set_water_constants();
		}
		set_shader_constants();
	}

	if (state.initial_set || state.current_alpha_state != gs->alpha_type) {
		state.current_alpha_state = gs->alpha_type;
		if (state.current_alpha_state == Game_Shader::A_NONE || state.current_alpha_state == Game_Shader::A_TEST) {
			glDisable(GL_BLEND);
		}
		else if (state.current_alpha_state == Game_Shader::A_BLEND) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else if (state.current_alpha_state == Game_Shader::A_ADD) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
		}
	}
	if (state.initial_set || state.current_backface_state != (int)gs->backface) {
		state.current_backface_state = gs->backface;
		if (state.current_backface_state)
			glDisable(GL_CULL_FACE);
		else
			glEnable(GL_CULL_FACE);
	}


	// ill find a better way maybe
	bool shader_doesnt_need_the_textures = is_water || is_depth;

	if (!shader_doesnt_need_the_textures) {

		if (gs->shader_type == Game_Shader::S_2WAYBLEND) {
			Game_Shader* blend1 = gs->references[0];
			Game_Shader* blend2 = gs->references[1];
			if (blend1->images[Game_Shader::DIFFUSE]) bind_texture(ALBEDO1_LOC, blend1->images[Game_Shader::DIFFUSE]->gl_id);
			else bind_texture(ALBEDO1_LOC, white_texture.gl_id);
			if (blend2->images[Game_Shader::DIFFUSE]) bind_texture(ALBEDO2_LOC, blend2->images[Game_Shader::DIFFUSE]->gl_id);
			else bind_texture(ALBEDO2_LOC, white_texture.gl_id);
			if (blend1->images[Game_Shader::ROUGHNESS]) bind_texture(ROUGH1_LOC, blend1->images[Game_Shader::ROUGHNESS]->gl_id);
			else bind_texture(ROUGH1_LOC, white_texture.gl_id);
			if (blend2->images[Game_Shader::ROUGHNESS]) bind_texture(ROUGH2_LOC, blend2->images[Game_Shader::ROUGHNESS]->gl_id);
			else bind_texture(ROUGH2_LOC, white_texture.gl_id);

			if (dc.mesh->has_tangents()) {
				if (blend1->images[Game_Shader::NORMAL]) bind_texture(NORMAL1_LOC, blend1->images[Game_Shader::NORMAL]->gl_id);
				else bind_texture(NORMAL1_LOC, flat_normal_texture.gl_id);
				if (blend2->images[Game_Shader::NORMAL]) bind_texture(NORMAL2_LOC, blend2->images[Game_Shader::NORMAL]->gl_id);
				else bind_texture(NORMAL2_LOC, flat_normal_texture.gl_id);
			}

			SET_OR_USE_FALLBACK(Game_Shader::SPECIAL, SPECIAL_LOC, white_texture);
		}
		else {
			SET_OR_USE_FALLBACK(Game_Shader::DIFFUSE, ALBEDO1_LOC, white_texture);
			SET_OR_USE_FALLBACK(Game_Shader::ROUGHNESS, ROUGH1_LOC, white_texture);
			SET_OR_USE_FALLBACK(Game_Shader::AO, AO1_LOC, white_texture);
			SET_OR_USE_FALLBACK(Game_Shader::METAL, METAL1_LOC, white_texture);

			if (dc.mesh->has_tangents()) {
				SET_OR_USE_FALLBACK(Game_Shader::NORMAL, NORMAL1_LOC, flat_normal_texture);
			}
		}
	}

	shader().set_int("obj_index", dc.object_index);
	shader().set_int("obj_mat_index", dc.mat_index);
	
	if (state.current_vao != dc.mesh->vao) {
		glBindVertexArray(dc.mesh->vao);
		state.current_vao = dc.mesh->vao;
	}
	GLenum index_type = (use_32_bit_indicies) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

	glDrawElementsBaseVertex(
		GL_TRIANGLES,
		mp.element_count,
		index_type,
		(void*)(dc.mesh->merged_index_pointer + mp.element_offset),
		dc.mesh->merged_vert_offset + mp.base_vertex
	);
	stats.draw_calls++;
	stats.tris_drawn += mp.element_count / 3;

	state.initial_set = false;
}

void Renderer::planar_reflection_pass()
{
	glm::vec3 plane_n = glm::vec3(0, 1, 0);
	float plane_d = 2.0;

	View_Setup setup = current_frame_main_view;

	// X-\     /-
	//	  -\ /-
	//	----O----------
	//    -/
	// C-/
	setup.front.y *= -1;
	float dist_to_plane = current_frame_main_view.origin.y + plane_d;
	setup.origin.y -= dist_to_plane * 2.0f;
	setup.view = glm::lookAt(setup.origin, setup.origin + setup.front, glm::vec3(0, 1, 0));
	setup.viewproj = setup.proj * setup.view;
	if (use_halfres_reflections.integer()) {
		setup.width /= 2;
		setup.height /= 2;
	}
	
	Render_Level_Params params;
	params.view = setup;
	params.has_clip_plane = true;
	params.custom_clip_plane = vec4(plane_n, plane_d);
	params.pass = params.OPAQUE;
	params.clear_framebuffer = true;
	params.output_framebuffer = fbo.reflected_scene;
	params.draw_viewmodel = false;
	params.upload_constants = true;
	params.is_water_reflection_pass = true;

	render_level_to_target(params);
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
		params.clear_framebuffer = true;
		params.provied_constant_buffer = 0;
		params.pass = Render_Level_Params::OPAQUE;
		params.upload_constants = true;
		params.is_probe_render = true;

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
	scene.cubemap_ssbo = 0;
	scene.levelcubemapirradiance_array = scene.levelcubemapspecular_array = 0;
	scene.directional_index = -1;

	// add the lights
	auto& llights = eng->level->lights;
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

	auto& espawns = eng->level->espawns;
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

	scene.skybox = helper.create_from_file("hdr_sky.hdr").original_cubemap;
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

	glCreateBuffers(1, &scene.cubemap_ssbo);
	glNamedBufferData(scene.cubemap_ssbo, (sizeof Cubemap_Ssbo_Struct)* scene.cubemaps.size(), probes, GL_STATIC_DRAW);
}

#include "glm/gtc/random.hpp"

float ourLerp(float a, float b, float f)
{
	return a + f * (b - a);
}
#include <random>
void SSAO_System::init()
{
	glGenFramebuffers(1, &fbo);

	std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
	std::default_random_engine generator;
	int kernel_samples =16;
	for (unsigned int i = 0; i < kernel_samples; ++i)
	{
		glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
		sample = glm::normalize(sample);
		sample *= randomFloats(generator);
		float scale = float(i) / kernel_samples;

		// scale samples s.t. they're more aligned to center of kernel
		scale = ourLerp(0.1f, 1.0f, scale * scale);
		sample *= scale;
		samples[i]=sample;
	}


	std::vector<glm::vec3> ssaoNoise;
	for (unsigned int i = 0; i < 16; i++)
	{
		glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f); // rotate around z-axis (in tangent space)
		ssaoNoise.push_back(noise);
	}
	glGenTextures(1, &noise_tex2);
	glBindTexture(GL_TEXTURE_2D, noise_tex2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

}

void SSAO_System::make_render_targets()
{
	glDeleteRenderbuffers(1, &rbo);
	glDeleteTextures(1, &halfres_texture);
	glDeleteTextures(1, &fullres1);
	glDeleteTextures(1, &fullres2);

	width = eng->window_w.integer();
	height = eng->window_h.integer();

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	res_scale = 1;	// halfres breaks ssao and I'm tired of trying to fix it..., something to do with projection matrix

	glGenTextures(1, &halfres_texture);
	glBindTexture(GL_TEXTURE_2D, halfres_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width/res_scale, height/res_scale, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glGenTextures(1, &fullres1);
	glBindTexture(GL_TEXTURE_2D, fullres1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glGenTextures(1, &fullres2);
	glBindTexture(GL_TEXTURE_2D, fullres2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);


	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);


	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO_System::render()
{
	GPUFUNCTIONSTART;

	if (width != eng->window_w.integer() || height != eng->window_h.integer())
		make_render_targets();


	MeshBuilder quad;
	quad.Begin();
	quad.Push2dQuad(vec2(-1, -1), vec2(2, 2));
	quad.End();

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, halfres_texture, 0);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glViewport(0, 0, width/res_scale, height/res_scale);

	draw.set_shader(Renderer::S_SSAO);

	draw.shader().set_mat4("Model", mat4(1));
	draw.shader().set_mat4("ViewProj", mat4(1));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, draw.tex.scene_depthstencil);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, noise_tex2);

	float fovRad = draw.vs.fov;

	vec2 LinMAD;
	vec2 ao_res = vec2(width/(res_scale), height/(res_scale));
	//ao_res = glm::floor(ao_res);
	vec2 inv_res = 1.f / ao_res;
	float ao_aspect_ratio = ao_res.x / ao_res.y;


	float near = draw.vs.near, far = draw.vs.far;
	LinMAD[0] = (near - far) / (2.0f * near * far);
	LinMAD[1] = (near + far) / (2.0f * near * far);



	draw.shader().set_vec2("AORes", ao_res);
	draw.shader().set_vec2("InvAORes", inv_res);

	vec2 noise_scale = ao_res / vec2(4.f);

	draw.shader().set_vec2("NoiseScale", noise_scale);

	draw.shader().set_mat4("projection", draw.vs.proj);
		//float aspect = (width / res_scale) / float(height / res_scale);
		//glm::mat4 newproj = glm::perspective(draw.vs.fov, aspect, draw.vs.near, draw.vs.far);

	draw.shader().set_mat4("invprojection", glm::inverse(draw.vs.proj));

	uint32_t id = draw.shader_list[Renderer::S_SSAO].ID;
	uint32_t loc = glGetUniformLocation(id, "samples[0]");
	for (int j = 0; j < 64; j++)
		glUniform3f(loc+j, samples[j].x, samples[j].y, samples[j].z);

	quad.Draw(GL_TRIANGLES);
	if (1) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fullres1, 0);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		draw.set_shader(Renderer::S_XBLUR);
		draw.shader().set_vec2("InvFullRes", 1.f / vec2(width, height));
		draw.shader().set_vec2("LinMAD", LinMAD);
		draw.shader().set_mat4("Model", mat4(1));
		draw.shader().set_mat4("ViewProj", mat4(1));
		draw.bind_texture(0, halfres_texture);
		draw.bind_texture(1, draw.tex.scene_depthstencil);
		quad.Draw(GL_TRIANGLES);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fullres2, 0);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		draw.set_shader(Renderer::S_YBLUR);
		draw.shader().set_vec2("InvFullRes", 1.f / vec2(width, height));
		draw.shader().set_vec2("LinMAD", LinMAD);
		draw.shader().set_mat4("Model", mat4(1));
		draw.shader().set_mat4("ViewProj", mat4(1));
		draw.bind_texture(0, fullres1);
		draw.bind_texture(1, draw.tex.scene_depthstencil);
		quad.Draw(GL_TRIANGLES);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glCheckError();

	quad.Free();
}