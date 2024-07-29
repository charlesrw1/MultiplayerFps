#include "DrawLocal.h"

#include "Framework/Util.h"
#include "glad/glad.h"
#include "Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Physics/Physics2.h"	// for g_physics->debug_draw()
#include "UI.h"					// for gui->ui_paint()
#include "IEditorTool.h"		// for overlay_draw()

#include "Debug.h"

#include <SDL2/SDL.h>

#include "Level.h"

//#pragma optimize("", off)

extern ConfigVar g_window_w;
extern ConfigVar g_window_h;
extern ConfigVar g_window_fullscreen;

Renderer draw;
RendererPublic* idraw = &draw;


static const int IRRADIANCE_CM_LOC = 13;
static const int SPECULAR_CM_LOC = 9;
static const int BRDF_LUT_LOC = 10;
static const int SHADOWMAP_LOC = 11;
static const int CAUSTICS_LOC = 12;
static const int SSAO_TEX_LOC = 8;

extern ConfigVar g_debug_skeletons;


ConfigVar draw_viewmodel("r.draw_viewmodel","1",CVAR_BOOL);
ConfigVar enable_vsync("r.enable_vsync","1",CVAR_BOOL);
ConfigVar shadow_quality_setting("r.shadow_setting","0",CVAR_INTEGER,0,3);
ConfigVar enable_bloom("r.bloom","1",CVAR_BOOL);
ConfigVar enable_volumetric_fog("r.vol_fog","0",CVAR_BOOL);
ConfigVar enable_ssao("r.ssao","1",CVAR_BOOL);
ConfigVar use_halfres_reflections("r.halfres_reflections","1",CVAR_BOOL);
ConfigVar dont_use_mdi("r.dont_use_mdi", "0", CVAR_BOOL|CVAR_DEV);


DECLARE_ENGINE_CMD(output_texture)
{
	static const char* usage_str = "Usage: output_texture <scale:float> <alpha:float> <mip/slice:float> <texture_name>\n";
	if (args.size() != 5) {
		sys_print(usage_str);
		return;
	}

	float scale = atof(args.at(1));
	float alpha = atof(args.at(2));
	float mip = atof(args.at(3));
	const char* texture_name = args.at(4);

	draw.debug_tex_out.output_tex = g_imgs.find_texture(texture_name);
	draw.debug_tex_out.scale = scale;
	draw.debug_tex_out.alpha = alpha;
	draw.debug_tex_out.mip = mip;


	if (!draw.debug_tex_out.output_tex) {
		sys_print("output_texture: couldn't find texture %s\n", texture_name);
	}
}
DECLARE_ENGINE_CMD(clear_output_texture)
{
	draw.debug_tex_out.output_tex = nullptr;
}


/*
Defined in Shaders/SharedGpuTypes.txt

const uint DEBUG_NONE = 0;
const uint DEBUG_NORMAL = 1;
const uint DEBUG_MATID = 2;
const uint DEBUG_SHADERID = 3;
const uint DEBUG_WIREFRAME = 4;
const uint DEBUG_ALBEDO = 5;
const uint DEBUG_DIFFUSE = 6;
const uint DEBUG_SPECULAR = 7;
const uint DEBUG_OBJID = 8;
const uint DEBUG_LIGHTING_ONLY = 9;
*/

ConfigVar r_debug_mode("r.debug_mode", "0", CVAR_INTEGER | CVAR_DEV, 0, 9);

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
	ASSERT(0);

	int tex = (mat) ? mat->gl_id : white_texture.gl_id;
	if ((in_world_space != sprite_state.in_world_space || tex != sprite_state.current_t
		|| additive != sprite_state.additive))
		draw_sprite_buffer();

	sprite_state.in_world_space = in_world_space;
	if (sprite_state.current_t != tex || sprite_state.force_set) {
		//bind_texture(ALBEDO1_LOC, tex);
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

void Renderer::bind_texture(int bind, int id)
{
	ASSERT(bind >= 0 && bind < MAX_SAMPLER_BINDINGS);
	bool invalid = state_machine.is_bit_invalid(Opengl_State_Machine::TEXTURE0_BIT + bind);
	if (invalid || state_machine.textures_bound[bind] != id) {
		state_machine.set_bit_valid(Opengl_State_Machine::TEXTURE0_BIT + bind);
		glBindTextureUnit(bind, id);
		state_machine.textures_bound[bind] = id;
		stats.textures_bound++;
	}
}



void set_standard_draw_data(const Render_Level_Params& params)
{
	glCheckError();

	// >>> PBR BRANCH
	draw.bind_texture(IRRADIANCE_CM_LOC, draw.scene.levelcubemapirradiance_array);
	draw.bind_texture(SPECULAR_CM_LOC, draw.scene.levelcubemapspecular_array);

	glCheckError();
	draw.bind_texture(BRDF_LUT_LOC, EnviornmentMapHelper::get().integrator.lut_id);

	draw.bind_texture(SHADOWMAP_LOC, draw.shadowmap.texture.shadow_array);

	glCheckError();

	//shader().set_vec4("aoproxy_sphere", vec4(eng->local_player().position + glm::vec3(0,aosphere.y,0), aosphere.x));
	//shader().set_float("aoproxy_scale_factor", aosphere.z);

	uint32_t ssao_tex = draw.ssao.texture.result;
	if (params.is_probe_render || params.is_water_reflection_pass) ssao_tex = draw.white_texture.gl_id;

	draw.bind_texture(SSAO_TEX_LOC, ssao_tex);
	glCheckError();

	//draw.bind_texture(CAUSTICS_LOC, draw.casutics->gl_id);

	glCheckError();


	//if (eng->level && eng->level->lightmap)
	//	draw.bind_texture(LIGHTMAP_LOC, eng->level->lightmap->gl_id);
	//else
		//draw.bind_texture(LIGHTMAP_LOC, draw.white_texture.gl_id);

	//glActiveTexture(GL_TEXTURE0 + start + 4);
	//glBindTexture(GL_TEXTURE_3D, draw.volfog.voltexture);

	glCheckError();

	//glBindBufferBase(GL_UNIFORM_BUFFER, 4, draw.volfog.param_ubo);

	glCheckError();
	glBindBufferBase(GL_UNIFORM_BUFFER, 8, draw.shadowmap.ubo.info);

	glCheckError();

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, draw.scene.cubemap_ssbo);


	glCheckError();
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
program_handle Program_Manager::create_single_file(const char* shared_file, const std::string& defines)
{
	program_def def;
	def.vert = shared_file;
	def.frag = nullptr;
	def.defines = defines;
	def.is_compute = false;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
program_handle Program_Manager::create_raster(const char* vert, const char* frag, const std::string& defines)
{
	program_def def;
	def.vert = vert;
	def.frag = frag;
	def.defines = defines;
	def.is_compute = false;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
program_handle Program_Manager::create_raster_geo(const char* vert, const char* frag, const char* geo, const std::string& defines)
{
	program_def def;
	def.vert = vert;
	def.frag = frag;
	def.geo = geo;
	def.defines = defines;
	def.is_compute = false;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
program_handle Program_Manager::create_compute(const char* compute, const std::string& defines)
{
	program_def def;
	def.vert = compute;
	def.defines = defines;
	def.is_compute = true;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
void Program_Manager::recompile_all()
{
	for (int i = 0; i < programs.size(); i++)
		recompile(programs[i]);
}

void Program_Manager::recompile(program_def& def)
{
	if (def.is_compute) {
		def.compile_failed = Shader::compute_compile(&def.shader_obj, def.vert, def.defines) 
			!= ShaderResult::SHADER_SUCCESS;
	}
	else if (def.is_shared()) {
		def.compile_failed = Shader::compile_vert_frag_single_file(&def.shader_obj, def.vert, def.defines)!=ShaderResult::SHADER_SUCCESS;
	}
	else {
		if (def.geo)
			def.compile_failed = !Shader::compile(def.shader_obj, def.vert, def.frag, def.geo, def.defines);
		else
			def.compile_failed = Shader::compile(&def.shader_obj, def.vert, def.frag, def.defines) != ShaderResult::SHADER_SUCCESS;
	}
}

Material_Shader_Table::Material_Shader_Table() 
{

}

program_handle Material_Shader_Table::lookup(shader_key key)
{
	uint32_t key32 = key.as_uint32();
	auto find = shader_key_to_program_handle.find(key32);
	return find == shader_key_to_program_handle.end() ? -1 : find->second;
}
void Material_Shader_Table::insert(shader_key key, program_handle handle)
{
	shader_key_to_program_handle.insert({ key.as_uint32(), handle });
}


void Renderer::bind_vao(uint32_t vao)
{
	bool invalid = state_machine.is_bit_invalid(Opengl_State_Machine::VAO_BIT);
	if (invalid || vao != state_machine.current_vao) {
		state_machine.set_bit_valid(Opengl_State_Machine::VAO_BIT);
		state_machine.current_vao = vao;
		glBindVertexArray(vao);
		stats.vaos_bound++;
	}
}

void Renderer::set_blend_state(blend_state blend)
{
	bool invalid = state_machine.is_bit_invalid(Opengl_State_Machine::BLENDING_BIT);
	if (invalid || blend != state_machine.blending) {
		if (blend == blend_state::OPAQUE) 
			glDisable(GL_BLEND);
		else if (blend == blend_state::ADD) {
			if (invalid || state_machine.blending == blend_state::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
		}
		else if (blend == blend_state::BLEND) {
			if (invalid || state_machine.blending == blend_state::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		state_machine.blending = blend;
		state_machine.set_bit_valid(Opengl_State_Machine::BLENDING_BIT);
		stats.blend_changes++;
	}
}
void Renderer::set_show_backfaces(bool show_backfaces)
{
	bool invalid = state_machine.is_bit_invalid(Opengl_State_Machine::BACKFACE_BIT);
	if (invalid || show_backfaces != state_machine.backface_state) {
		if (show_backfaces)
			glDisable(GL_CULL_FACE);
		else
			glEnable(GL_CULL_FACE);
		state_machine.set_bit_valid(Opengl_State_Machine::BACKFACE_BIT);
		state_machine.backface_state = show_backfaces;
	}
}

void Renderer::set_shader(program_handle handle)
{
	if (handle == -1) {
		state_machine.active_program = handle;
		glUseProgram(0);
	}
	bool invalid = state_machine.is_bit_invalid(Opengl_State_Machine::PROGRAM_BIT);
	if (invalid || handle != state_machine.active_program) {
		state_machine.set_bit_valid(Opengl_State_Machine::PROGRAM_BIT);
		state_machine.active_program = handle;
		prog_man.get_obj(handle).use();
		stats.shaders_bound++;
	}
}


static Shader meshlet_reset_pre_inst;
static Shader meshlet_reset_post_inst;
static Shader meshlet_inst_cull;
static Shader meshlet_meshlet_cull;
static Shader meshlet_compact;


static Shader naiveshader;
static Shader naiveshader2;
static Shader mdi_meshlet_cull_shader;
static Shader mdi_meshlet_zero_bufs;

void Renderer::create_shaders()
{
	ssao.reload_shaders();
	//Shader::compile(&naiveshader, "SimpleMeshV.txt", "UnlitF.txt", "NAIVE");
	//Shader::compile(&naiveshader2, "SimpleMeshV.txt", "UnlitF.txt", "NAIVE2");

	//Shader::compile(&mdi_meshlet_cull_shader, "SimpleMeshV.txt", "UnlitF.txt", "MDICULL");


	//Shader::compute_compile(&meshlet_inst_cull, "Meshlets/meshlets.txt", "INSTANCE_CULLING");
	//Shader::compute_compile(&meshlet_meshlet_cull, "Meshlets/meshlets.txt", "MESHLET_CULLING");
	//Shader::compute_compile(&meshlet_reset_pre_inst, "Meshlets/reset.txt", "RESET_PRE_INSTANCES");
	//Shader::compute_compile(&meshlet_reset_post_inst, "Meshlets/reset.txt", "RESET_POST_INSTANCES");
	//Shader::compute_compile(&mdi_meshlet_zero_bufs, "Meshlets/zerobuf.txt");
	//Shader::compute_compile(&meshlet_compact, "Meshlets/compact.txt");


	prog.simple = prog_man.create_raster("MbSimpleV.txt", "MbSimpleF.txt");
	//prog.textured = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt");
	//prog.textured3d = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE3D");
	//prog.texturedarray = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTUREARRAY");
	prog.skybox = prog_man.create_raster("MbSimpleV.txt", "SkyboxF.txt", "SKYBOX");
	//prog.particle_basic = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "PARTICLE_SHADER");
	prog.tex_debug_2d = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_2D_VERSION");
	prog.tex_debug_2d_array = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_2D_ARRAY_VERSION");

	// Bloom shaders
	prog.bloom_downsample = prog_man.create_raster("fullscreenquad.txt", "BloomDownsampleF.txt");
	prog.bloom_upsample = prog_man.create_raster("fullscreenquad.txt", "BloomUpsampleF.txt");
	prog.combine = prog_man.create_raster("fullscreenquad.txt", "CombineF.txt");
	prog.hbao = prog_man.create_raster("MbTexturedV.txt", "HbaoF.txt");
	prog.xblur = prog_man.create_raster("MbTexturedV.txt", "BilateralBlurF.txt");
	prog.yblur = prog_man.create_raster("MbTexturedV.txt", "BilateralBlurF.txt", "YBLUR");

	set_shader(prog.xblur);
	shader().set_int("input_img", 0);
	shader().set_int("scene_depth", 1);
	set_shader(prog.yblur);
	shader().set_int("input_img", 0);
	shader().set_int("scene_depth", 1);
	set_shader(prog.hbao);
	shader().set_int("scene_depth", 0);
	shader().set_int("noise_texture", 1);


	prog.mdi_testing = prog_man.create_raster("SimpleMeshV.txt", "UnlitF.txt", "MDI");

	prog.light_accumulation = prog_man.create_raster("LightAccumulationV.txt", "LightAccumulationF.txt");
	prog.sunlight_accumulation = prog_man.create_raster("fullscreenquad.txt", "SunLightAccumulationF.txt");

	// volumetric fog shaders
	Shader::compute_compile(&volfog.prog.lightcalc, "VfogScatteringC.txt");
	Shader::compute_compile(&volfog.prog.raymarch, "VfogRaymarchC.txt");
	Shader::compute_compile(&volfog.prog.reproject, "VfogScatteringC.txt", "REPROJECTION");
	volfog.prog.lightcalc.use();
	volfog.prog.lightcalc.set_int("previous_volume", 0);
	volfog.prog.lightcalc.set_int("perlin_noise", 1);


	glCheckError();
	glUseProgram(0);



}

void Renderer::reload_shaders()
{
	on_reload_shaders.invoke();

	ssao.reload_shaders();
	prog_man.recompile_all();

	set_shader(prog.xblur);
	shader().set_int("input_img", 0);
	shader().set_int("scene_depth", 1);
	set_shader(prog.yblur);
	shader().set_int("input_img", 0);
	shader().set_int("scene_depth", 1);
	set_shader(prog.hbao);
	shader().set_int("scene_depth", 0);
	shader().set_int("noise_texture", 1);
}


void Renderer::upload_ubo_view_constants(uint32_t ubo, glm::vec4 custom_clip_plane)
{
	gpu::Ubo_View_Constants_Struct constants;
	constants.view = vs.view;
	constants.viewproj = vs.viewproj;
	constants.invview = glm::inverse(vs.view);
	constants.invproj = glm::inverse(vs.proj);
	constants.inv_viewproj = glm::inverse(vs.viewproj);
	constants.viewpos_time = glm::vec4(vs.origin, this->current_time);
	constants.viewfront = glm::vec4(vs.front, 0.0);
	constants.viewport_size = glm::vec4(vs.width, vs.height, 0, 0);

	constants.near = vs.near;
	constants.far = vs.far;
	constants.shadowmap_epsilon = shadowmap.tweak.epsilon;
	constants.inv_scale_by_proj_distance = 1.0 / (2.0 * tan(vs.fov * 0.5));

	constants.fogcolor = vec4(vec3(0.7), 1);
	constants.fogparams = vec4(10, 30, 0, 0);

	constants.directional_light_dir_and_used = vec4(1, 0, 0, 0);

	constants.numcubemaps = scene.cubemaps.size();

	if (using_skybox_for_specular)
		constants.forcecubemap = 0.0;
	else
		constants.forcecubemap = -1.0;

	constants.custom_clip_plane = custom_clip_plane;

	constants.debug_options = r_debug_mode.get_integer();

	glNamedBufferData(ubo, sizeof gpu::Ubo_View_Constants_Struct, &constants, GL_DYNAMIC_DRAW);
}

Renderer::Renderer()
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

	sys_print("!!! %s, %s, %s, %d: %s\n", src_str, type_str, severity_str, id, message);
}

void imgui_stat_hook()
{
	ImGui::Text("Draw calls: %d", draw.stats.draw_calls);
	ImGui::Text("Total tris: %d", draw.stats.tris_drawn);
	ImGui::Text("Texture binds: %d", draw.stats.textures_bound);
	ImGui::Text("Shader binds: %d", draw.stats.shaders_bound);
	ImGui::Text("Vao binds: %d", draw.stats.vaos_bound);
	ImGui::Text("Blend changes: %d", draw.stats.blend_changes);

	ImGui::Text("opaque batches: %d", (int)draw.scene.gbuffer_pass.batches.size());
	ImGui::Text("depth batches: %d", (int)draw.scene.shadow_pass.batches.size());
	ImGui::Text("transparent batches: %d", (int)draw.scene.transparent_pass.batches.size());

	ImGui::Text("total objects: %d", (int)draw.scene.proxy_list.objects.size());
	ImGui::Text("opaque mesh batches: %d", (int)draw.scene.gbuffer_pass.mesh_batches.size());
}

void Renderer::check_hardware_options()
{
	bool supports_compression = false;
	bool supports_sprase_tex = false;
	bool supports_bindless = false;
	bool supports_filter_minmax = false;
	bool supports_atomic64 = false;
	bool supports_int64 = false;

	int num_extensions = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
	for (int i = 0; i < num_extensions; i++) {
		const char* ext = (char*)glGetStringi(GL_EXTENSIONS, i);
		if (strcmp(ext, "GL_ARB_bindless_texture") == 0) supports_bindless = true;
		else if (strcmp(ext, "GL_ARB_sparse_texture") == 0)supports_sprase_tex = true;
		else if (strcmp(ext, "GL_EXT_texture_compression_s3tc") == 0)supports_compression = true;
		else if (strcmp(ext, "GL_ARB_texture_filter_minmax") == 0)supports_filter_minmax = true;
		else if (strcmp(ext, "GL_NV_shader_atomic_int64") == 0) supports_atomic64 = true;
		else if (strcmp(ext, "GL_ARB_gpu_shader_int64") == 0) supports_int64 = true;

	}

	sys_print("``` ==== Extension support ====\n");
	sys_print("-GL_ARB_bindless_texture: %s\n", (supports_bindless) ? "yes" : "no");
	sys_print("-GL_ARB_sparse_texture: %s\n", (supports_sprase_tex) ? "yes" : "no");
	sys_print("-GL_ARB_texture_filter_minmax: %s\n", (supports_filter_minmax) ? "yes" : "no");
	sys_print("-GL_EXT_texture_compression_s3tc: %s\n", (supports_compression) ? "yes" : "no");
	sys_print("-GL_NV_shader_atomic_int64: %s\n", (supports_atomic64) ? "yes" : "no");
	sys_print("-GL_ARB_gpu_shader_int64: %s\n", (supports_int64) ? "yes" : "no");

	if (!supports_compression) {
		Fatalf("Opengl driver needs GL_EXT_texture_compression_s3tc\n");
	}
	sys_print("\n");

	sys_print("``` ==== GL Hardware Values ====\n");
	int max_buffer_bindings = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_buffer_bindings);
	sys_print("-GL_MAX_UNIFORM_BUFFER_BINDINGS: %d\n", max_buffer_bindings);
	int max_texture_units = 0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
	sys_print("-GL_MAX_TEXTURE_IMAGE_UNITS: %d\n", max_texture_units);
	sys_print("\n");
}

void Renderer::create_default_textures()
{
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
	glTextureSubImage2D(black_texture.gl_id, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, bdata);
	glTextureParameteri(black_texture.gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(black_texture.gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateTextureMipmap(black_texture.gl_id);

	glCreateTextures(GL_TEXTURE_2D, 1, &flat_normal_texture.gl_id);
	glTextureStorage2D(flat_normal_texture.gl_id, 1, GL_RGB8, 1, 1);
	glTextureSubImage2D(flat_normal_texture.gl_id, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, normaldata);
	glTextureParameteri(flat_normal_texture.gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(flat_normal_texture.gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateTextureMipmap(flat_normal_texture.gl_id);

	// this is sort of leaking memory but not really, these are persitant over lifetime of program
	// anybody (like materials) can reference them with strings which is the point

	auto white_tex = g_imgs.install_system_texture("_white");
	auto black_tex = g_imgs.install_system_texture("_black");
	auto flat_normal = g_imgs.install_system_texture("_flat_normal");

	white_tex->update_specs(white_texture.gl_id, 1, 1, 3, {});
	black_tex->update_specs(black_texture.gl_id, 1, 1, 3, {});
	flat_normal->update_specs(flat_normal_texture.gl_id, 1, 1, 3, {});

	// create the "virtual texture system" handles so materials/debuging can reference these
	tex.bloom_vts_handle = g_imgs.install_system_texture("_bloom_result");
	tex.scene_color_vts_handle = g_imgs.install_system_texture("_scene_color");
	tex.scene_depth_vts_handle = g_imgs.install_system_texture("_scene_depth");
	tex.gbuffer0_vts_handle = g_imgs.install_system_texture("_gbuffer0");
	tex.gbuffer1_vts_handle = g_imgs.install_system_texture("_gbuffer1");
	tex.gbuffer2_vts_handle = g_imgs.install_system_texture("_gbuffer2");
	tex.editorid_vts_handle = g_imgs.install_system_texture("_editorid");

}

void Renderer::init()
{
	sys_print("--------- Initializing Renderer ---------\n");

	// Check hardware settings like extension availibility
	check_hardware_options();

	// Enable debug output on debug builds
#ifdef _DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(debug_message_callback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
	InitGlState();

	// Initialize memory arena, 4mb
	mem_arena.init("Render Temp", 4'000'000);

	// Init scene draw buffers
	scene.init();

	create_shaders();
	
	create_default_textures();

	glCreateBuffers(1, &ubo.current_frame);

	depth_pyramid_maker.init();

	InitFramebuffers(true, g_window_w.get_integer(), g_window_h.get_integer());

	EnviornmentMapHelper::get().init();
	volfog.init();
	shadowmap.init();
	ssao.init();

	lens_dirt = g_imgs.find_texture("lens_dirt.jpg");

	glGenVertexArrays(1, &vao.default_);
	glCreateBuffers(1, &buf.default_vb);
	glNamedBufferStorage(buf.default_vb, 12 * 3, nullptr, 0);

	on_level_start();

	Debug_Interface::get()->add_hook("Render stats", imgui_stat_hook);

	Random random(4523456);
	//for (int i = 0; i < 20; i++) {
	//	float box = 5.0;
	//	glm::vec3 pos(random.RandF(-box, box), random.RandF(-box, box), random.RandF(-box, box));
	//	float radius = random.RandF(3.0, 10.0);
	//	Render_Light rl;
	//	rl.radius = radius;
	//	rl.position = pos;
	//	scene.register_light(rl);
	//}
}


void Renderer::InitFramebuffers(bool create_composite_texture, int s_w, int s_h)
{
	auto set_default_parameters = [](uint32_t handle) {
		glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	};

	auto create_and_delete_texture = [](uint32_t& texture) {
		glDeleteTextures(1, &texture);
		glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	};

	auto create_and_delete_fb = [](uint32_t & framebuffer) {
		glDeleteFramebuffers(1, &framebuffer);
		glCreateFramebuffers(1, &framebuffer);
	};


	// Main accumulation buffer, 16 bit color
	create_and_delete_texture(tex.scene_color);
	glTextureStorage2D(tex.scene_color, 1, GL_RGBA16F, s_w, s_h);
	set_default_parameters(tex.scene_color);

	// Main scene depth
	create_and_delete_texture(tex.scene_depth);
	glTextureStorage2D(tex.scene_depth, 1, GL_DEPTH_COMPONENT24, s_w, s_h);
	set_default_parameters(tex.scene_depth);

	// Create forward render framebuffer
	// Transparents and other immediate stuff get rendered to this
	create_and_delete_fb(fbo.forward_render);
	glNamedFramebufferTexture(fbo.forward_render, GL_COLOR_ATTACHMENT0, tex.scene_color, 0);
	glNamedFramebufferTexture(fbo.forward_render, GL_DEPTH_ATTACHMENT, tex.scene_depth, 0);
	unsigned int attachments[1] = { GL_COLOR_ATTACHMENT0 };
	glNamedFramebufferDrawBuffers(fbo.forward_render, 1, attachments);
	
	// Gbuffer textures
	// See the comment above these var's decleration in DrawLocal.h for details
	create_and_delete_texture(tex.scene_gbuffer0);
	glTextureStorage2D(tex.scene_gbuffer0, 1, GL_RGB16F, s_w, s_h);
	set_default_parameters(tex.scene_gbuffer0);

	create_and_delete_texture(tex.scene_gbuffer1);
	glTextureStorage2D(tex.scene_gbuffer1, 1, GL_RGBA8, s_w, s_h);
	set_default_parameters(tex.scene_gbuffer1);

	create_and_delete_texture(tex.scene_gbuffer2);
	glTextureStorage2D(tex.scene_gbuffer2, 1, GL_RGBA8, s_w, s_h);
	set_default_parameters(tex.scene_gbuffer2);

	// for mouse picking
	create_and_delete_texture(tex.editor_id_buffer);
	glTextureStorage2D(tex.editor_id_buffer, 1, GL_RGBA8, s_w, s_h);
	set_default_parameters(tex.editor_id_buffer);

	// Create Gbuffer
	// outputs to 4 render targets: gbuffer 0,1,2 and scene_color for emissives
	create_and_delete_fb(fbo.gbuffer);
	glNamedFramebufferTexture(fbo.gbuffer, GL_DEPTH_ATTACHMENT, tex.scene_depth, 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT0, tex.scene_gbuffer0, 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT1, tex.scene_gbuffer1, 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT2, tex.scene_gbuffer2, 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT3, tex.scene_color, 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT4, tex.editor_id_buffer, 0);

	const uint32_t gbuffer_attach_count = 5;
	unsigned int gbuffer_attachments[gbuffer_attach_count] = { 
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2,
		GL_COLOR_ATTACHMENT3,
		GL_COLOR_ATTACHMENT4,
	};
	glNamedFramebufferDrawBuffers(fbo.gbuffer, gbuffer_attach_count, gbuffer_attachments);

	// Composite textures
	create_and_delete_fb(fbo.composite);
	create_and_delete_texture(tex.output_composite);
	glTextureStorage2D(tex.output_composite, 1, GL_RGB8, s_w, s_h);
	set_default_parameters(tex.output_composite);
	glNamedFramebufferTexture(fbo.composite, GL_COLOR_ATTACHMENT0, tex.output_composite, 0);

	cur_w = s_w;
	cur_h = s_h;

	// Update vts handles
	tex.scene_color_vts_handle->update_specs(tex.scene_color, s_w, s_h, 4, {});
	tex.scene_depth_vts_handle->update_specs(tex.scene_depth, s_w, s_h, 4, {});
	tex.gbuffer0_vts_handle->update_specs(tex.scene_gbuffer0, s_w, s_h, 3, {});
	tex.gbuffer1_vts_handle->update_specs(tex.scene_gbuffer1, s_w, s_h, 3, {});
	tex.gbuffer2_vts_handle->update_specs(tex.scene_gbuffer2, s_w, s_h, 3, {});
	tex.editorid_vts_handle->update_specs(tex.editor_id_buffer, s_w, s_h, 4, {});


	// Also update bloom buffers (this can be elsewhere)
	init_bloom_buffers();

	// alert any observers that they need to update their buffer sizes (like SSAO, etc.)
	on_viewport_size_changed.invoke(cur_w, cur_h);
}

void Renderer::init_bloom_buffers()
{
	glDeleteFramebuffers(1, &fbo.bloom);
	if(tex.number_bloom_mips>0)
		glDeleteTextures(tex.number_bloom_mips, tex.bloom_chain);
	glCreateFramebuffers(1, &fbo.bloom);
	
	int x = cur_w / 2;
	int y = cur_h / 2;
	tex.number_bloom_mips = glm::min((int)MAX_BLOOM_MIPS, get_mip_map_count(x, y));
	glCreateTextures(GL_TEXTURE_2D, tex.number_bloom_mips, tex.bloom_chain);

	float fx = x;
	float fy = y;
	for (int i = 0; i < tex.number_bloom_mips; i++) {
		tex.bloom_chain_isize[i] = { x,y };
		tex.bloom_chain_size[i] = { fx,fy };
		glTextureStorage2D(tex.bloom_chain[i], 1, GL_R11F_G11F_B10F, x, y);
		glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		x /= 2;
		y /= 2;
		fx *= 0.5;
		fy *= 0.5;
	}

	tex.bloom_vts_handle->update_specs(tex.bloom_chain[0], cur_w / 2, cur_h / 2, 3, {});
}

void Renderer::render_bloom_chain()
{
	GPUFUNCTIONSTART;

	glBindVertexArray(vao.default_);
	// to prevent crashes??
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexBuffer(0, buf.default_vb, 0, 0);
	glBindVertexBuffer(1, buf.default_vb, 0, 0);
	glBindVertexBuffer(2, buf.default_vb, 0, 0);


	if (!enable_bloom.get_bool())
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo.bloom);
	set_shader(prog.bloom_downsample);
	float src_x = cur_w;
	float src_y = cur_h;

	glBindTextureUnit(0, tex.scene_color);
	glClearColor(0, 0, 0, 1);
	for (int i = 0; i < tex.number_bloom_mips; i++)
	{
		glNamedFramebufferTexture(fbo.bloom, GL_COLOR_ATTACHMENT0, tex.bloom_chain[i], 0);

		shader().set_vec2("srcResolution", vec2(src_x, src_y));
		shader().set_int("mipLevel", i);
		src_x = tex.bloom_chain_size[i].x;
		src_y = tex.bloom_chain_size[i].y;

		glViewport(0, 0, src_x, src_y);	// dest size
		glClear(GL_COLOR_BUFFER_BIT);
		
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glBindTextureUnit(0, tex.bloom_chain[i]);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	set_shader(prog.bloom_upsample);
	for (int i = tex.number_bloom_mips - 1; i > 0; i--)
	{
		glNamedFramebufferTexture(fbo.bloom, GL_COLOR_ATTACHMENT0, tex.bloom_chain[i - 1], 0);

		vec2 destsize = tex.bloom_chain_size[i - 1];
		glViewport(0, 0, destsize.x, destsize.y);
		glBindTextureUnit(0, tex.bloom_chain[i]);
		shader().set_float("filterRadius", 0.0001f);

		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glCheckError();
}

void Renderer::DrawSkybox()
{
	MeshBuilder mb;
	mb.Begin();
	mb.PushSolidBox(-vec3(1), vec3(1), COLOR_WHITE);
	mb.End();

	set_shader(prog.skybox);
	glm::mat4 view = vs.view;
	view[3] = vec4(0, 0, 0, 1);	// remove translation
	shader().set_mat4("ViewProj", vs.proj * view);
	shader().set_vec2("screen_size", vec2(vs.width, vs.height));
	shader().set_int("volumetric_fog", 1);
	shader().set_int("cube", 0);


	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, scene.skybox);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, volfog.texture.volume);


	glDisable(GL_CULL_FACE);
	mb.Draw(GL_TRIANGLES);
	glEnable(GL_CULL_FACE);
	mb.Free();
}

#define SET_OR_USE_FALLBACK(texture, where, fallback) \
if(mat->images[(int)texture]) bind_texture(where, mat->images[(int)texture]->gl_id); \
else bind_texture(where, fallback.gl_id);

void Renderer::execute_render_lists(Render_Lists& list, Render_Pass& pass)
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, scene.gpu_render_instance_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, scene.gpu_skinned_mats_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, matman.get_gpu_material_buffer());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, list.glinstance_to_instance);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, list.gldrawid_to_submesh_material);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	int offset = 0;
	for (int i = 0; i < pass.batches.size(); i++) {

		auto& batch = pass.batches[i];


		int count = list.command_count[i];

		// static cast, dangerous kinda but not really
		const MaterialInstanceLocal* mat = (MaterialInstanceLocal*)pass.mesh_batches[pass.batches[i].first].material;
		draw_call_key batch_key = pass.objects[pass.mesh_batches[pass.batches[i].first].first].sort_key;

		program_handle program = (program_handle)batch_key.shader;
		blend_state blend = (blend_state)batch_key.blending;
		bool backface = batch_key.backface;
		uint32_t layer = batch_key.layer;
		int format = batch_key.vao;

		assert(program >= 0 && program < prog_man.programs.size());

		set_shader(program);

		bind_vao(mods.get_vao(true/* animated */));

		set_show_backfaces(backface);
		set_blend_state(blend);

		shader().set_int("indirect_material_offset", offset);

		auto& textures = mat->get_textures();

		for (int i = 0; i < textures.size(); i++) {
			bind_texture(i, textures[i]->gl_id);
		}

		const GLenum index_type = (mods.get_index_type_size() == 4) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			index_type,
			(void*)(list.commands.data() + offset),
			count,
			sizeof(gpu::DrawElementsIndirectCommand)
		);

		offset += count;

		stats.draw_calls++;
	}
}

void Renderer::render_lists_old_way(Render_Lists& list, Render_Pass& pass)
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, scene.gpu_render_instance_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, scene.gpu_skinned_mats_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, matman.get_gpu_material_buffer());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, list.glinstance_to_instance);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, list.gldrawid_to_submesh_material);

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	int offset = 0;
	for (int i = 0; i < pass.batches.size(); i++) {
		
		auto& batch = pass.batches[i];
		int count = list.command_count[i];

		for (int dc = 0; dc < batch.count; dc++) {
			auto& cmd = list.commands[offset + dc];

			const MaterialInstanceLocal* mat = (MaterialInstanceLocal*)pass.mesh_batches[pass.batches[i].first].material;
			draw_call_key batch_key = pass.objects[pass.mesh_batches[pass.batches[i].first].first].sort_key;

			program_handle program = (program_handle)batch_key.shader;
			blend_state blend = (blend_state)batch_key.blending;
			bool backface = batch_key.backface;
			uint32_t layer = batch_key.layer;
			int format = batch_key.vao;

			assert(program >= 0 && program < prog_man.programs.size());

			set_shader(program);

			bind_vao(mods.get_vao(true/* animated */));

			set_show_backfaces(backface);
			set_blend_state(blend);

			auto& textures = mat->get_textures();

			for (int i = 0; i < textures.size(); i++) {
				bind_texture(i, textures[i]->gl_id);
			}

			shader().set_int("indirect_material_offset", offset);


			const GLenum index_type = (mods.get_index_type_size()==4) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

			glDrawElementsBaseVertex(
				GL_TRIANGLES,
				cmd.count,
				index_type,
				(void*)(cmd.firstIndex * mods.get_index_type_size()),
				cmd.baseVertex
			);

			stats.draw_calls++;
		}

		offset += count;

	}
}

void Renderer::render_level_to_target(const Render_Level_Params& params)
{
	vs = params.view;

	state_machine.invalidate_all();

	if (params.is_probe_render)
		using_skybox_for_specular = true;

	{
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

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, active_constants_ubo);
	
	
	set_standard_draw_data(params);


	glBindFramebuffer(GL_FRAMEBUFFER, params.output_framebuffer);
	glViewport(0, 0, vs.width, vs.height);
	if (params.clear_framebuffer) {
		glClearColor(0.f, 0.0f, 0.f, 1.f);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	}

	if (params.pass == Render_Level_Params::SHADOWMAP) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(shadowmap.tweak.poly_factor, shadowmap.tweak.poly_units);
		glCullFace(GL_FRONT);
		glDisable(GL_CULL_FACE);
	}


	if (params.has_clip_plane) {
		glEnable(GL_CLIP_DISTANCE0);
	}
	
	
	{
		//Model_Drawing_State state;
		//for (int d = 0; d < list->size(); d++) {
		//	Draw_Call& dc = (*list)[d];
		//	draw_model_real(dc, state);
		//}

		// renderdoc seems to hate mdi for some reason, so heres an option to disable it
		if(dont_use_mdi.get_bool())
			render_lists_old_way(*params.rl, *params.rp);
		else
			execute_render_lists(*params.rl, *params.rp);
	}

	if (params.pass == Render_Level_Params::SHADOWMAP) {
		glDisable(GL_POLYGON_OFFSET_FILL);
		glCullFace(GL_BACK);
		glEnable(GL_CULL_FACE);
	}

	if (params.has_clip_plane)
		glDisable(GL_CLIP_DISTANCE0);

	using_skybox_for_specular = false;
}

void Renderer::ui_render()
{
	GPUFUNCTIONSTART;

	return;

	//set_shader(prog.textured3d);
	shader().set_mat4("Model", mat4(1));
	glm::mat4 proj = glm::ortho(0.f, (float)cur_w, -(float)cur_h, 0.f);
	shader().set_mat4("ViewProj", proj);
	building_ui_texture = 0;
	ui_builder.Begin();
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Texture* t = g_imgs.find_texture("crosshair007.png");
	int centerx = cur_w / 2;
	int centery = cur_h / 2;

	float crosshair_scale = 0.7f;
	Color32 crosshair_color = { 0, 0xff, 0, 0xff };
	float width = t->width * crosshair_scale;
	float height = t->height * crosshair_scale;


	draw_rect(centerx - width / 2, centery - height / 2, width, height, crosshair_color, t, t->width, t->height);

	//draw_rect(0, 300, 300, 300, COLOR_WHITE, mats.find_for_name("tree_bark")->images[0],500,500,0,0);

	ASSERT(0);
	//if (ui_builder.GetBaseVertex() > 0) {
	//	bind_texture(ALBEDO1_LOC, building_ui_texture);
	//	ui_builder.End();
	//	ui_builder.Draw(GL_TRIANGLES);
	//}

	glCheckError();


	glDisable(GL_BLEND);
	if (0) {
		//set_shader(prog.textured3d);
		glCheckError();

		shader().set_mat4("Model", mat4(1));
		glm::mat4 proj = glm::ortho(0.f, (float)cur_w, -(float)cur_h, 0.f);
		shader().set_mat4("ViewProj", mat4(1));
		shader().set_int("slice", (int)slice_3d);

		ui_builder.Begin();
		ui_builder.Push2dQuad(glm::vec2(-1, 1), glm::vec2(1, -1), glm::vec2(0, 0),
			glm::vec2(1, 1), COLOR_WHITE);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex.scene_depth);

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
		bind_texture(0, building_ui_texture);
		ui_builder.End();
		ui_builder.Draw(GL_TRIANGLES);
		ui_builder.Begin();
	}
	building_ui_texture = texnum;
	ui_builder.Push2dQuad(glm::vec2(x, y), glm::vec2(w, h), glm::vec2(srcx / tw, srcy / th),
		glm::vec2(srcw / tw, srch / th), color);
}


void draw_skeleton(const AnimatorInstance* a,float line_len,const mat4& transform)
{
	auto& bones = a->get_global_bonemats();
	auto model = a->get_model();
	if (!model || !model->get_skel())
		return;
	
	auto skel = model->get_skel();
	for (int index = 0; index < skel->get_num_bones(); index++) {
		vec3 org = transform * bones[index][3];
		Color32 colors[] = { COLOR_RED,COLOR_GREEN,COLOR_BLUE };
		for (int i = 0; i < 3; i++) {
			vec3 dir = mat3(transform) * bones[index][i];
			dir = normalize(dir);
			Debug::add_line(org, org + dir * line_len, colors[i],-1.f,false);
		}
		const int parent = skel->get_bone_parent(index);
		if (parent != -1) {
			vec3 parent_org = transform * bones[parent][3];
			Debug::add_line(org, parent_org, COLOR_PINK,-1.f,false);
		}
	}
}


#include <algorithm>


#if 0
void Render_Pass::remove_from_batch(passobj_handle handle)
{
	Pass_Object& obj = pass_objects.get(handle);
	int batch_idx = obj.batch_index;
	Batch& batch = batches.at(batch_idx);

	assert(obj.index_in_batch <= batch.pass_obj_count);
	batch_draw_list.at(batch.pass_obj_start + obj.index_in_batch) = batch_draw_list.at(batch.pass_obj_start + batch.pass_obj_count - 1);
	batch_draw_list.at(batch.pass_obj_start + batch.pass_obj_count - 1) = -1;

	batch.pass_obj_count -= 1;
}

void Render_Pass::update_batches()
{
	// if any new/removed/refreshed objects
	if (refresh_queue.empty() && creation_queue.empty() && deletion_queue.empty())
		return;

	// for removed objects
	// find batch and decrment object count
	for (auto& objhandle : deletion_queue) {
		remove_from_batch(objhandle);
		pass_objects.free(objhandle);
	}

	// for new and refreshed objects
	// compute hash for state
	// try to find existing batch with binary search
	// if it exists and has space, then add to it

	for (auto& objhandle : refresh_queue) {

	}

	for (auto& objhandle : creation_queue) {

	}

	// for new/refreshed objects that are left
	// create new batches and add objects to end of obj list
	
	// sort new batches
	// merge new batches into main batches (now all sorted)

	// compute merged_batches

	// recompute draw call buffer, one draw call per geometry, and add submesh instances with dc index and instance index

	// now: have merged batch list with a start and end count draw calls
	// have a list of submesh instances which index into draw calls and instance list for transform
}
#endif

Render_Pass::Render_Pass(pass_type type) : type(type) {}

draw_call_key Render_Pass::create_sort_key_from_obj(
	const Render_Object& proxy, 
	const MaterialInstanceLocal* material,
	uint32_t camera_dist, 
	uint32_t submesh, 
	uint32_t layer,
	bool is_editor_mode
)
{
	draw_call_key key;

	key.shader = matman.get_mat_shader(
		proxy.animator!=nullptr, 
		proxy.model, material, 
		(type == pass_type::DEPTH),
		false,
		is_editor_mode,
		r_debug_mode.get_integer()!=0
	);
	const MasterMaterial* mm = material->get_master_material();

	key.blending = (uint64_t)mm->blend;
	key.backface = mm->backface;
	key.texture = material->unique_id;
	key.vao = 0;// (uint64_t)proxy.mesh->format;
	key.mesh = proxy.model->get_uid();
	key.layer = layer;
	key.distance = camera_dist;

	return key;
}

//void Render_Pass::delete_object(
//	const Render_Object& proxy, 
//	renderobj_handle handle, 
//	Material* material,
//	uint32_t submesh,
//	uint32_t layer) {
//	
//	Pass_Object obj;
//	obj.sort_key = create_sort_key_from_obj(proxy, material, submesh, layer);
//	obj.render_obj = handle;
//	obj.submesh_index = submesh;
//	deletions.push_back(obj);
//}
void Render_Pass::add_object(
	const Render_Object& proxy, 
	handle<Render_Object> handle,
	const MaterialInstanceLocal* material,
	uint32_t camera_dist,
	uint32_t submesh,
	uint32_t lod,
	uint32_t layer,
	bool is_editor_mode) {
	ASSERT(handle.is_valid() && "null handle");
	ASSERT(material && "null material");
	Pass_Object obj;
	obj.sort_key = create_sort_key_from_obj(proxy, material,camera_dist, submesh, layer, is_editor_mode);
	obj.render_obj = handle;
	obj.submesh_index = submesh;
	obj.material = material;
	obj.lod_index = lod;
	uint32_t size = high_level_objects_in_pass.size();
	if (size==0||high_level_objects_in_pass[size-1].id != handle.id)
		high_level_objects_in_pass.push_back(handle);
	obj.hl_obj_index = high_level_objects_in_pass.size()-1;

	// ensure this material maps to a gpu material
	ASSERT(material->gpu_buffer_offset != MaterialInstanceLocal::INVALID_MAPPING);
	objects.push_back(obj);
}
#include <iterator>
void Render_Pass::make_batches(Render_Scene& scene)
{
	const auto& merge_functor = [](const Pass_Object& a, const Pass_Object& b)
	{
		if (a.sort_key.as_uint64() < b.sort_key.as_uint64()) return true;
		else if (a.sort_key.as_uint64() == b.sort_key.as_uint64()) 
			return  a.submesh_index < b.submesh_index;
		else return false;
	};

	// objects were added correctly in back to front order, just sort by layer
	const auto& sort_functor_transparent = [](const Pass_Object& a, const Pass_Object& b)
	{
		if (a.sort_key.distance > b.sort_key.distance) return true;
		else if (a.sort_key.as_uint64() == b.sort_key.as_uint64())
			return a.submesh_index < b.submesh_index;
		else return false;
	};

	if (type == pass_type::TRANSPARENT)
		std::sort(objects.begin(), objects.end(), sort_functor_transparent);
	else
		std::sort(objects.begin(), objects.end(), merge_functor);

	batches.clear();
	mesh_batches.clear();

	if (objects.empty()) 
		return;

	{
		const auto& functor = [](int first, Pass_Object* po, const Render_Object* rop) -> Mesh_Batch
		{
			Mesh_Batch batch;
			batch.first = first;
			batch.count = 1;
			//auto& mats = rop->mats;
			int index = rop->model->get_part(po->submesh_index).material_idx;// rop->mesh->parts.at(po->submesh_index).material_idx;
			batch.material = po->material;
			batch.shader_index = po->sort_key.shader;
			return batch;
		};

		// build mesh batches first
		Pass_Object* batch_obj = &objects[0];
		const Render_Object* batch_proxy = &scene.get(batch_obj->render_obj);
		Mesh_Batch batch = functor(0, batch_obj, batch_proxy);
		batch_obj->batch_idx = 0;

		for (int i = 1; i < objects.size(); i++) {
			Pass_Object* this_obj = &objects[i];
			const Render_Object* this_proxy = &scene.get(this_obj->render_obj);
			bool can_be_merged
				= this_obj->sort_key.as_uint64() == batch_obj->sort_key.as_uint64()
				&& this_obj->submesh_index == batch_obj->submesh_index && type != pass_type::TRANSPARENT;	// dont merge transparent meshes into instances
			if (can_be_merged)
				batch.count++;
			else {
				mesh_batches.push_back(batch);
				batch = functor(i, this_obj, this_proxy);
				batch_obj = this_obj;
				batch_proxy = this_proxy;
			}
			this_obj->batch_idx = mesh_batches.size();
		}
		mesh_batches.push_back(batch);
	}

	Multidraw_Batch batch;
	batch.first = 0;
	batch.count = 1;
	
	Mesh_Batch* mesh_batch = &mesh_batches[0];
	Pass_Object* batch_obj = &objects[mesh_batch->first];
	const Render_Object* batch_proxy = &scene.get(batch_obj->render_obj);
	for (int i = 1; i < mesh_batches.size(); i++)
	{
		Mesh_Batch* this_batch = &mesh_batches[i];
		Pass_Object* this_obj = &objects[this_batch->first];
		const Render_Object* this_proxy = &scene.get(this_obj->render_obj);

		bool batch_this = false;

		bool same_layer = batch_obj->sort_key.layer == this_obj->sort_key.layer;
		bool same_vao = batch_obj->sort_key.vao == this_obj->sort_key.vao;
		bool same_material = batch_obj->sort_key.texture == this_obj->sort_key.texture;
		bool same_shader = batch_obj->sort_key.shader == this_obj->sort_key.shader;
		bool same_other_state = batch_obj->sort_key.blending == this_obj->sort_key.blending 
			&& batch_obj->sort_key.backface == this_obj->sort_key.blending;

		//if (type == pass_type::OPAQUE || type == pass_type::TRANSPARENT) {
			if (same_vao && same_material && same_other_state && same_shader && same_layer)
				batch_this = true;	// can batch with different meshes
			else
				batch_this = false;

		//}
		//else if (type == pass_type::DEPTH){
		//	// can batch across texture changes as long as its not alpha tested
		//	if (same_shader && same_vao && same_other_state && !this_batch->material->alpha_tested)
		//		batch_this = true;
		//	else
		//		batch_this = false;
		//}

		if (batch_this) {
			batch.count += 1;
		}
		else {
			batches.push_back(batch);
			batch.count = 1;
			batch.first = i;

			mesh_batch = this_batch;
			batch_obj = this_obj;
			batch_proxy = this_proxy;
		}
	}

	batches.push_back(batch);
}



Render_Scene::Render_Scene() 
	: gbuffer_pass(pass_type::OPAQUE),
	transparent_pass(pass_type::TRANSPARENT),
	shadow_pass(pass_type::DEPTH),
	editor_sel_pass(pass_type::OPAQUE)
{

}

void Render_Lists::init(uint32_t drawbufsz, uint32_t instbufsz)
{
	indirect_drawid_buf_size = drawbufsz;
	indirect_instance_buf_size = instbufsz;

	glCreateBuffers(1, &gldrawid_to_submesh_material);
	glCreateBuffers(1, &glinstance_to_instance);
}

struct ObjGpu
{
	uint hlobj_idx;			// index into Render_Pass.hl_objs
	uint mylod;				// this objs lod
	uint batch_idx;			// this objs batch
	uint multibatch_idx;	// this objs multibatch
	uint instance_index;	// index into Render_Object
};
// uint MultidrawCounts[] = {0, 0, 0, 0}
// uint MultidrawOffsets[] = {0,3,5,6}

struct PerObjResult
{
	int8_t lod_choosen = -1;
};


glm::vec4 normalize_plane(glm::vec4 p)
{
	return p / glm::length(glm::vec3(p));
}

// Use RAII here, this is allocated on the heap post OpenGL initialization
class GpuDataForGbufferCulling
{
public:
	GpuDataForGbufferCulling();
	~GpuDataForGbufferCulling();


	bufferhandle out_command_buffer{};
	bufferhandle in_command_buffer{};
	bufferhandle mdi_counts{};
	bufferhandle mdi_offsets{};
	bufferhandle occluded_list{};
	bufferhandle hlobjs{};
	bufferhandle instances{};
	bufferhandle meshlet_cull_args{};
	bufferhandle tri_cull_args{};
};

class RenderListBuilder
{
public:
	static void build_standard_cpu(
		Render_Lists& list,
		Render_Pass& src,
		Free_List<ROP_Internal>& proxy_list
	)
	{
		// first build the lists
		list.build_from(src, proxy_list);

		// Frustum cull and choose LOD
		glm::mat4 projectionT = glm::transpose(draw.vs.proj);
		glm::vec4 frustumX = normalize_plane(projectionT[3] + projectionT[0]); // x + w < 0
		glm::vec4 frustumY = normalize_plane(projectionT[3] + projectionT[1]); // y + w < 0
		glm::vec4 cullfrustum = glm::vec4(
			frustumX.x,
			frustumX.z,
			frustumY.y,
			frustumY.z
		);

		auto& arena = draw.get_arena();
		const int count = src.high_level_objects_in_pass.size();
		const auto marker = arena.get_bottom_marker();
		PerObjResult* results = (PerObjResult*)arena.alloc_bottom(sizeof(PerObjResult) * count);
		const float near = draw.vs.near;
		const float far = draw.vs.far;
		const float inv_two_times_tanfov = 1.0 / (tan(draw.get_current_frame_vs().fov * 0.5));
		const glm::mat4 view = draw.vs.view;
		for (int hlIndex = 0; hlIndex < count; hlIndex++) {
			// this can all be optimized later, cache center+radius in the Render_Object when it changes
			auto& proxy = proxy_list.get(src.high_level_objects_in_pass[hlIndex].id).proxy;
			const Model* m = proxy.model;
			glm::vec4 boundingsphere = m->get_bounding_sphere();
			vec3 center = glm::vec3(boundingsphere);
			center = (view * proxy.transform * vec4(center, 1.0));
			float radius = boundingsphere.w;	// fixme scale not applied to radius

			bool visible = true;
			visible = visible && center.z * cullfrustum[1] - abs(center.x) * cullfrustum[0] > -radius;
			visible = visible && center.z * cullfrustum[3] - abs(center.y) * cullfrustum[2] > -radius;
			visible = visible && center.z - radius < -near && center.z + radius > -far;

			results[hlIndex].lod_choosen = visible ? 0 : -1;
			if (!visible)
				continue;

			float percentage = inv_two_times_tanfov / -center.z;
			percentage *= radius;

			for (int i = m->get_num_lods() - 1; i > 0; i--) {
				if (percentage <= m->get_lod(i).end_percentage) {
					results[hlIndex].lod_choosen = i;
					break;
				}
			}
		}

		const int objCount = src.objects.size();
		uint32_t* glinstance_to_instance = (uint32_t*)arena.alloc_bottom(sizeof(uint32_t) * objCount);

		for (int objIndex = 0; objIndex < objCount; objIndex++) {
			auto& obj = src.objects[objIndex];
			bool visible = results[obj.hl_obj_index].lod_choosen == obj.lod_index;
			if (!visible) 
				continue; 
			uint32_t precount = list.commands[obj.batch_idx].primCount++;	// increment count
			uint32_t ofs = list.commands[obj.batch_idx].baseInstance;

			// set the pointer to the Render_Object strucutre that will be found on the gpu
			glinstance_to_instance[ofs+precount] = proxy_list.handle_to_obj[obj.render_obj.id];
		}

		glNamedBufferData(list.glinstance_to_instance, sizeof(uint32_t) * objCount, nullptr, GL_DYNAMIC_DRAW);
		glNamedBufferSubData(list.glinstance_to_instance, 0, sizeof(uint32_t) * objCount, glinstance_to_instance);


		arena.free_bottom_to_marker(marker);

	}


	static void prep_for_gpu()
	{

	}
};



extern bool use_32_bit_indicies;
void Render_Lists::build_from(Render_Pass& src, Free_List<ROP_Internal>& proxy_list)
{
	// This function essentially just loops over all batches and creates gpu commands for them
	// its O(n) to the number of batches, not n^2 which it kind of looks like it is

	commands.clear();
	command_count.clear();

	//static std::vector<uint32_t> instance_to_instance;
	static std::vector<uint32_t> draw_to_material;
	draw_to_material.clear();
	//instance_to_instance.clear();

	int base_instance = 0;

	for (int i = 0; i < src.batches.size(); i++) {
		Multidraw_Batch& mdb = src.batches[i];


		for (int j = 0; j < mdb.count; j++) {
			Mesh_Batch& meshb = src.mesh_batches[mdb.first + j];
			auto& obj = src.objects[meshb.first];
			Render_Object& proxy = proxy_list.get(obj.render_obj.id).proxy;
		

			auto& part = proxy.model->get_part(obj.submesh_index);// mesh.parts[obj.submesh_index];
			gpu::DrawElementsIndirectCommand cmd;

			cmd.baseVertex = part.base_vertex + proxy.model->get_merged_vertex_ofs();
			cmd.count = part.element_count;
			cmd.firstIndex = part.element_offset + proxy.model->get_merged_index_ptr();
			cmd.firstIndex /= (use_32_bit_indicies) ? 4 : 2;

			// Important! Set primCount to 0 because visible instances will increment this
			cmd.primCount = 0;// meshb.count;
			cmd.baseInstance = base_instance;

			commands.push_back(cmd);

			//for (int k = 0; k < meshb.count; k++) {
			//	instance_to_instance.push_back(proxy_list.handle_to_obj[src.objects[meshb.first + k].render_obj.id]);
			//}

			base_instance += meshb.count;// cmd.primCount;

			auto batch_material = meshb.material;
			draw_to_material.push_back(batch_material->gpu_buffer_offset);

			draw.stats.tris_drawn += meshb.count * cmd.count / 3;
		}

		command_count.push_back(mdb.count);
	}

	//glNamedBufferData(glinstance_to_instance, sizeof(uint32_t) * indirect_instance_buf_size, nullptr, GL_DYNAMIC_DRAW);
	//glNamedBufferSubData(glinstance_to_instance, 0, sizeof(uint32_t) * instance_to_instance.size(), instance_to_instance.data());

	glNamedBufferData(gldrawid_to_submesh_material, sizeof(uint32_t) * indirect_drawid_buf_size, nullptr, GL_DYNAMIC_DRAW);
	glNamedBufferSubData(gldrawid_to_submesh_material, 0, sizeof(uint32_t) * draw_to_material.size(), draw_to_material.data());
}


void Render_Scene::init()
{
	int obj_count = 20'000;
	int mat_count = 500;

	gbuffer_rlist.init(mat_count,obj_count);
	transparent_rlist.init(mat_count,obj_count);
	csm_shadow_rlist.init(mat_count,obj_count);

	glCreateBuffers(1, &gpu_render_instance_buffer);
	glCreateBuffers(1, &gpu_skinned_mats_buffer);
}

glm::vec4 to_vec4(Color32 color) {
	return glm::vec4(color.r, color.g, color.b, color.a) / 255.f;
}

#include <future>
#include <thread>



const MeshLod& get_lod_to_render(const Render_Object& object, float inv_two_times_tanfov, float& out_camera_dist)
{
	const auto& vs = draw.get_current_frame_vs();

	glm::vec3 to_origin = glm::vec3(object.transform[3]) - vs.origin;
	float distance_to_point = glm::dot(to_origin, vs.front);
	out_camera_dist = distance_to_point;
	float percentage = inv_two_times_tanfov / distance_to_point;
	percentage *= object.model->get_bounding_sphere().w;

	for (int i = object.model->get_num_lods() - 1; i > 0; i--) {
		if (percentage <= object.model->get_lod(i).end_percentage)
			return object.model->get_lod(i);
	}
	return object.model->get_lod(0);
}

// RenderObject new property: cache_static
//	this property is used for objects that arent changing their mesh etc. every frame

// So for every frame:
//		for objects that updated:
//			update object list
//		for materials that updated
//			update material list
// 
//		clear dynamic mesh pass sections
//		For all objects if not obj.cache_static
//			add every part in every lod to respective mesh passes (dynamic section)
//			sort+merge mesh dynamic passes
//		if cache invalidated
//			for all objs if obj.cache_static
//				add every part to static cache mesh passes
//			sort+merge mesh static passes
//
//		for all dynamic and static mesh passes, create render lists
//			cull object level and get results from array?
//			cull, cpu or gpu, remove the unused lods
//			add to final render lists
//
//		
//		


void Render_Scene::build_scene_data(bool build_for_editor)
{
	CPUFUNCTIONSTART;

	// upload materials, FIXME: cache this

	gbuffer_pass.clear();
	transparent_pass.clear();
	shadow_pass.clear();

	// add draw calls and sort them
	//gpu_objects.resize(proxy_list.objects.size());

	//skinned_matricies_vec.clear();
	{
		CPUSCOPESTART(traversal);

		const size_t num_ren_objs = proxy_list.objects.size();
		auto gpu_objects = (gpu::Object_Instance*)draw.get_arena().alloc_bottom(sizeof(gpu::Object_Instance) * num_ren_objs);
		const size_t max_skinned_matricies = 256 * 100;// budget for ~100 characters
		size_t current_skinned_matrix_index = 0;
		auto skinned_matricies = (glm::mat4*)draw.get_arena().alloc_bottom(sizeof(glm::mat4) * max_skinned_matricies);
		ASSERT(gpu_objects && skinned_matricies);

		const float inv_two_times_tanfov = 1.0 / ( tan(draw.get_current_frame_vs().fov*0.5));


		for (int i = 0; i < proxy_list.objects.size(); i++) {
			auto& obj = proxy_list.objects[i];
			handle<Render_Object> objhandle{obj.handle};
			auto& proxy = obj.type_.proxy;
			if (proxy.visible && proxy.model) {

				auto& vs = draw.get_current_frame_vs();
				glm::vec3 to_origin = glm::vec3(proxy.transform[3]) - vs.origin;
				float CAM_DIST = glm::dot(to_origin, vs.front);
				float far = draw.get_current_frame_vs().far;
				CAM_DIST = 2.0 * (CAM_DIST / (far + CAM_DIST));
				CAM_DIST = glm::max(CAM_DIST, 0.f);
				uint32_t quantized_CAM_DIST = CAM_DIST * (1 << 15);
				if (quantized_CAM_DIST >= (1 << 15)) quantized_CAM_DIST = (1 << 15) - 1;

				auto model = proxy.model;
				for (int iLOD = 0; iLOD < model->get_num_lods(); iLOD++) {
					auto& lod = model->get_lod(iLOD);

					const int pstart = lod.part_ofs;
					const int pend = pstart + lod.part_count;

					for (int j = pstart; j < pend; j++) {
						auto& part = proxy.model->get_part(j);

						const MaterialInstanceLocal* mat = (MaterialInstanceLocal*)proxy.model->get_material(part.material_idx);
						if (obj.type_.proxy.mat_override)
							mat = (MaterialInstanceLocal*)obj.type_.proxy.mat_override;
						const MasterMaterial* mm = mat->get_master_material();
						
						if (mm->is_translucent()) {
							transparent_pass.add_object(proxy, objhandle, mat, quantized_CAM_DIST, j, iLOD, 0, build_for_editor);
						}
						else {
							shadow_pass.add_object(proxy, objhandle, mat, 0, j, iLOD, 0, build_for_editor);
							gbuffer_pass.add_object(proxy, objhandle, mat, 0, j, iLOD, 0, build_for_editor);
						}
					}


				}

				if (proxy.animator) {

					if (g_debug_skeletons.get_bool()) {
						draw_skeleton(proxy.animator, 0.05, proxy.transform);
					}


					//gpu_objects[i].anim_mat_offset = skinned_matricies_vec.size();
					gpu_objects[i].anim_mat_offset = current_skinned_matrix_index;
					auto& mats = proxy.animator->get_matrix_palette();
					const uint32_t num_bones = proxy.animator->num_bones();
					ASSERT(num_bones + current_skinned_matrix_index < max_skinned_matricies);
					std::memcpy(skinned_matricies + current_skinned_matrix_index, mats.data(), sizeof(glm::mat4) * num_bones);
					current_skinned_matrix_index += num_bones;
					//for (int i = 0; i < proxy.animator->num_bones(); i++) {
					//	skinned_matricies_vec.push_back(mats[i]);
					//}

				}
				else
					gpu_objects[i].anim_mat_offset = 0;

				if (proxy.viewmodel_layer) {
					gpu_objects[i].model = glm::inverse(draw.vs.view) * proxy.transform;
					gpu_objects[i].invmodel = glm::inverse(gpu_objects[i].model);
				}
				else {
					gpu_objects[i].model = proxy.transform;
					gpu_objects[i].invmodel = obj.type_.inv_transform;
				}
				gpu_objects[i].colorval = to_vec4(proxy.param1);
				gpu_objects[i].opposite_dither = (uint32_t)proxy.opposite_dither;
				gpu_objects[i].colorval2 = proxy.param2.to_uint();
			}
		}
		glNamedBufferData(gpu_render_instance_buffer, sizeof(gpu::Object_Instance) * num_ren_objs, gpu_objects, GL_DYNAMIC_DRAW);
		glNamedBufferData(gpu_skinned_mats_buffer, sizeof(glm::mat4) * current_skinned_matrix_index,skinned_matricies, GL_DYNAMIC_DRAW);
		

		draw.get_arena().free_bottom();
	}

	{
		CPUSCOPESTART(make_batches);

		gbuffer_pass.make_batches(*this);
		shadow_pass.make_batches(*this);
		transparent_pass.make_batches(*this);
	}
	{
		CPUSCOPESTART(make_render_lists);
		
		// cull + build draw calls

		RenderListBuilder::build_standard_cpu(
			gbuffer_rlist,
			gbuffer_pass,
			proxy_list
		);
		RenderListBuilder::build_standard_cpu(
			csm_shadow_rlist,
			shadow_pass,
			proxy_list
		);
		RenderListBuilder::build_standard_cpu(
			transparent_rlist,
			transparent_pass,
			proxy_list
		);
	}
}

// culling step:
// cpu: loop through submesh instance buffer, get the instance sphere bounds cull and output to draw call index
//		compact


// drawing structure

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
	MaterialInstance* material;
	int vert_fmt;
	int start;
	int end;
};

struct High_Level_Render_Object
{
	// Model* m
	MaterialInstance* material;
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



enum mdimodes
{
	NAIVE,
	NAIVE_MESHLETS,
	MDI_NO_MESHLETS,
	MDI_MESHLETS,
	MDI_MESHLETS_W_CULLING,
	MDI_MESHLETS_CULLING_DYNAMIC_INDEX,
	NUM_MDI_MODES,
};
const char* mdiitems[] = {
	"NAIVE",
	"NAIVE_MESHLETS",
	"MDI_NO_MESHLETS",
	"MDI_MESHLETS",
	"MDI_MESHLETS_W_CULLING",
	"MDI_MESHLETS_CULLING_DYNAMIC_INDEX"
};

bool use_simple_sphere = false;
int num_batches_to_render = 1;
int num_prims_per_batch = 1;
bool use_persistent_mapped = true;
static const int MAX_OBJECTS = 1'000;
bool mdi_naive = false;
bool use_storage = false;
bool wireframe_mdi = false;
mdimodes mdi_modes = MDI_MESHLETS_W_CULLING;
void mdi_test_imgui()
{
	ImGui::DragInt("batches", &num_batches_to_render, 1.f,0, 1000);
	ImGui::DragInt("prims", &num_prims_per_batch, 1.f, 0, 10000);
	ImGui::Checkbox("use_persistent_mapped", &use_persistent_mapped);
	ImGui::Checkbox("storage", &use_storage);
	ImGui::Checkbox("naive", &mdi_naive);
	ImGui::Checkbox("simple_sphere", &use_simple_sphere);
	ImGui::Checkbox("wireframe", &wireframe_mdi);
	ImGui::ListBox("Mode", (int*)&mdi_modes, mdiitems, NUM_MDI_MODES);

	if (num_batches_to_render * num_prims_per_batch > MAX_OBJECTS) {
		num_prims_per_batch = MAX_OBJECTS /num_batches_to_render;
	}
}
#include "Meshlet.h"

#if 0
void create_full_mdi_buffers(Chunked_Model* mod, 
	uint32_t& chunk_buffer,
	uint32_t& drawid_to_instance_buffer,
	uint32_t& compute_indirect_buffer,
	uint32_t& draw_elements_indirect_buffer,
	uint32_t& prefix_sum_buffer,
	uint32_t& draw_count_buffer
	)
{
	glCreateBuffers(1, &chunk_buffer);
	glCreateBuffers(1, &drawid_to_instance_buffer);
	glCreateBuffers(1, &compute_indirect_buffer);
	glCreateBuffers(1, &draw_elements_indirect_buffer);
	glCreateBuffers(1, &prefix_sum_buffer);

	glNamedBufferStorage(drawid_to_instance_buffer, MAX_OBJECTS * sizeof(int) * mod->chunks.size(), nullptr, GL_DYNAMIC_STORAGE_BIT);
	glNamedBufferStorage(compute_indirect_buffer, sizeof(gpu::DispatchIndirectCommand), nullptr, GL_DYNAMIC_STORAGE_BIT);

	size_t dei_size = sizeof(gpu::DrawElementsIndirectCommand) * MAX_OBJECTS * mod->chunks.size() + sizeof(glm::ivec4);
	glNamedBufferStorage(draw_elements_indirect_buffer, dei_size, nullptr, GL_DYNAMIC_STORAGE_BIT);
	glNamedBufferStorage(prefix_sum_buffer, sizeof(uvec4) * (MAX_OBJECTS + 1), nullptr, GL_DYNAMIC_STORAGE_BIT);

	vector<gpu::Chunk> chunks;
	for (int i = 0; i < mod->chunks.size(); i++) {
		gpu::Chunk c;
		Chunk& c_s = mod->chunks[i];
		c.bounding_sphere = c_s.bounding_sphere;
		c.cone_apex = c_s.cone_apex;
		c.cone_axis_cutoff = c_s.cone_axis_cutoff;
		c.index_count = c_s.index_count;
		c.index_offset = c_s.index_offset / sizeof(int);
		chunks.push_back(c);
	}

	glNamedBufferStorage(chunk_buffer, sizeof(gpu::Chunk) * mod->chunks.size(), nullptr, GL_DYNAMIC_STORAGE_BIT);
	glNamedBufferSubData(chunk_buffer, 0, sizeof(gpu::Chunk) * chunks.size(), chunks.data());

	glCreateBuffers(1, &draw_count_buffer);
	glNamedBufferStorage(draw_count_buffer, sizeof(glm::uvec4), nullptr, GL_DYNAMIC_STORAGE_BIT);
}
#endif
static glm::vec4 bounds_to_sphere(Bounds b)
{
	glm::vec3 center = b.get_center();
	glm::vec3 mindiff = center - b.bmin;
	glm::vec3 maxdiff = b.bmax - center;
	glm::vec3 diff = glm::max(mindiff, maxdiff);
	float radius = diff.x;
	if (diff.y > radius)radius = diff.y;
	if (diff.z > radius)radius = diff.z;
	return glm::vec4(center, radius);
}
#if 0
void multidraw_testing()
{
	GPUFUNCTIONSTART;

	static bool has_initialized = false;

	static Model* spherelod0;
	static Model* spherelod1;
	static Persistently_Mapped_Buffer mdi_buffer_pm;
	static Persistently_Mapped_Buffer mdi_buffer2_pm;
	static Persistently_Mapped_Buffer mdi_command_buf_pm;
	static Persistently_Mapped_Buffer integer_to_obj;

	static uint32_t chunk_buffer;
	static uint32_t drawid_to_instance_buffer;
	static uint32_t compute_indirect_buffer;
	static uint32_t draw_elements_indirect_buffer;
	static uint32_t prefix_sum_buffer;
	static uint32_t draw_count_buffer;

	const static int MAX_DRAW_CALLS = MAX_OBJECTS;

	static vector<glm::mat4> matricies;

	static Chunked_Model* meshlet_model;
	if (!has_initialized) {
		meshlet_model = get_chunked_mod("sphere_lod1.glb");

		//create_full_mdi_buffers(meshlet_model,
		//	chunk_buffer,
		//	drawid_to_instance_buffer,
		//	compute_indirect_buffer,
		//	draw_elements_indirect_buffer,
		//	prefix_sum_buffer,
		//	draw_count_buffer);

		for (int y = 0; y < 5; y++) {
			for (int z = 0; z < 5; z++) {
				for (int x = 0; x < 5; x++) {
					matricies.push_back(glm::scale(glm::translate(glm::mat4(1), glm::vec3(x, y, z)*0.9f),glm::vec3(0.2)));
					
					
					auto handle = draw.scene.register_renderable();
					Render_Object rop;
					rop.mesh = &meshlet_model->model->mesh;
					rop.mats = &meshlet_model->model->mats;
					rop.transform = matricies.back();
					rop.visible = true;
					
					draw.scene.update(handle, rop);
				}
			}
		}
		has_initialized = true;
		return;


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

			mdi_command_buf_pm.init(MAX_DRAW_CALLS * sizeof(gpu::DrawElementsIndirectCommand));
		}

		integer_to_obj.init(MAX_DRAW_CALLS * sizeof(int) * 100);

		spherelod0 = mods.find_or_load("monkey.glb");
		spherelod1 = mods.find_or_load("spherelod1.glb");
		Debug_Interface::get()->add_hook("mdi testing", mdi_test_imgui);
		has_initialized = true;
	}

	return;


	Model* sphere = meshlet_model->model;;// (use_simple_sphere) ? spherelod1 : spherelod0;

	GLenum index_type = (use_32_bit_indicies) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
	bool meshlet = true;
	if (wireframe_mdi)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	if (mdi_modes == NAIVE) {
		naiveshader.use();
		mdi_buffer_pm.bind_buffer(GL_SHADER_STORAGE_BUFFER, 3);
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
	else if (mdi_modes == NAIVE_MESHLETS) {
		naiveshader.use();
		mdi_buffer_pm.bind_buffer(GL_SHADER_STORAGE_BUFFER, 3);
		glBindVertexArray(meshlet_model->vao);
		Submesh& submesh = meshlet_model->model->mesh.parts[0];
		Mesh& orgmesh = meshlet_model->model->mesh;
		int num_objs = num_batches_to_render * num_prims_per_batch;
		for (int j = 0; j < num_objs; j++) {
			naiveshader.set_mat4("model", matricies[j]);
			for (int i = 0; i < meshlet_model->chunks.size(); i++) {
				naiveshader.set_int("integer", i);;
				Chunk& chunk = meshlet_model->chunks[i];
				int base_vertex = submesh.base_vertex + orgmesh.merged_vert_offset;
				int count = chunk.index_count;
				int first_index = chunk.index_offset;
				glDrawElementsBaseVertex(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)first_index, base_vertex);
			}
		}
	}
	else if (mdi_modes == MDI_NO_MESHLETS) {
		gpu::DrawElementsIndirectCommand* cmds =  (gpu::DrawElementsIndirectCommand*)mdi_command_buf_pm.wait_and_get_write_ptr();
		int num_commands = num_batches_to_render;
		for (int i = 0; i < num_commands; i++) {
			gpu::DrawElementsIndirectCommand& cmd = cmds[i];
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
		draw.set_shader(draw.prog.mdi_testing);

		glBindVertexArray(sphere->mesh.vao);
		
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		glMultiDrawElementsIndirect(GL_TRIANGLES, 
			index_type, 
			(void*)cmds,
			num_commands, 
			sizeof(gpu::DrawElementsIndirectCommand));
	}
	else if (mdi_modes == MDI_MESHLETS) {

		int num_objects = num_batches_to_render*num_prims_per_batch;
		
		int* indirects = (int*)integer_to_obj.wait_and_get_write_ptr();
		gpu::DrawElementsIndirectCommand* cmds =  (gpu::DrawElementsIndirectCommand*)mdi_command_buf_pm.wait_and_get_write_ptr();
		int num_commands = meshlet_model->chunks.size();
		Submesh& submesh = meshlet_model->model->mesh.parts[0];
		Mesh& orgmesh = meshlet_model->model->mesh;
		for (int j = 0; j < num_commands; j++) {
			Chunk& chunk = meshlet_model->chunks[j];
			int base_vertex = submesh.base_vertex + orgmesh.merged_vert_offset;
			int count = chunk.index_count;
			int first_index = chunk.index_offset;
			
			gpu::DrawElementsIndirectCommand& cmd = cmds[j];
			cmd.baseInstance = 0;
			cmd.primCount = num_objects;
			cmd.count = count;
			cmd.firstIndex = first_index/4;
			cmd.baseVertex = base_vertex;
		}
		mdi_command_buf_pm.lock_current_range();

		naiveshader2.use();
		glBindVertexArray(meshlet_model->vao);
		mdi_buffer_pm.bind_buffer(GL_SHADER_STORAGE_BUFFER, 3);
		mdi_buffer2_pm.bind_buffer(GL_SHADER_STORAGE_BUFFER, 2);
		integer_to_obj.bind_buffer(GL_SHADER_STORAGE_BUFFER, 4);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mdi_command_buf_pm.get_handle());
		glMultiDrawElementsIndirect(GL_TRIANGLES, 
			index_type, 
			(void*)mdi_command_buf_pm.get_offset(),
			num_commands, 
			sizeof(gpu::DrawElementsIndirectCommand));
	}
	else if (mdi_modes == MDI_MESHLETS_W_CULLING) {
		const int OBJECTS_TO_RENDER = num_prims_per_batch * num_batches_to_render;

		// from vkguide
		glm::mat4 projectionT = glm::transpose(draw.vs.proj);
		glm::vec4 frustumX = normalize_plane(projectionT[3] + projectionT[0]); // x + w < 0
		glm::vec4 frustumY = normalize_plane(projectionT[3] + projectionT[1]); // y + w < 0
		{
			GPUSCOPESTART("zero_buf");

			glMemoryBarrier(GL_ALL_BARRIER_BITS);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefix_sum_buffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, compute_indirect_buffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, draw_elements_indirect_buffer);
			mdi_buffer2_pm.bind_buffer(GL_SHADER_STORAGE_BUFFER, 9); // "instances" just matricies for now
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, chunk_buffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, drawid_to_instance_buffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, draw_count_buffer);

			mdi_meshlet_zero_bufs.use();
			glDispatchCompute(100, 1, 1);
		}
		{
			GPUSCOPESTART("pre inst");
			glMemoryBarrier(GL_ALL_BARRIER_BITS);


			meshlet_reset_pre_inst.use();	// reset the prefix sum buffer
			glDispatchCompute(1, 1, 1);
		}
		glm::vec4 cullfrustum = glm::vec4(
			frustumX.x,
			frustumX.z,
			frustumY.y,
			frustumY.z
		);
		{
			GPUSCOPESTART("cull inst");
			// barrier for reset
			glMemoryBarrier(GL_ALL_BARRIER_BITS);

			// cull instances, append meshlets to prefix sum
			meshlet_inst_cull.use();
			meshlet_inst_cull.set_vec4("frustum_plane", cullfrustum);
			glm::vec4 meshbounds = bounds_to_sphere(meshlet_model->model->mesh.aabb);
			meshlet_inst_cull.set_vec4("meshbounds", meshbounds);

			meshlet_inst_cull.set_int("num_instances", OBJECTS_TO_RENDER);
			meshlet_inst_cull.set_int("meshlets_in_mesh", meshlet_model->chunks.size());
			{
				Submesh& submesh = meshlet_model->model->mesh.parts[0];
				Mesh& orgmesh = meshlet_model->model->mesh;
				int base_vertex = submesh.base_vertex + orgmesh.merged_vert_offset;
				meshlet_inst_cull.set_int("mesh_basevertex", base_vertex);
			}

			int groupsx = (OBJECTS_TO_RENDER == 0) ? 1 : (OBJECTS_TO_RENDER - 1) / 64 + 1;
			glDispatchCompute(groupsx, 1, 1);
		}
		{
			GPUSCOPESTART("meshlet cull");

			glMemoryBarrier(GL_ALL_BARRIER_BITS);

			meshlet_reset_post_inst.use();
			glDispatchCompute(1, 1, 1);

			glMemoryBarrier(GL_ALL_BARRIER_BITS);

			meshlet_meshlet_cull.use();

			meshlet_meshlet_cull.set_vec4("frustum_plane", cullfrustum);

			meshlet_meshlet_cull.set_int("num_instances", OBJECTS_TO_RENDER);
			meshlet_meshlet_cull.set_int("meshlets_in_mesh", meshlet_model->chunks.size());
			{
				Submesh& submesh = meshlet_model->model->mesh.parts[0];
				Mesh& orgmesh = meshlet_model->model->mesh;
				int base_vertex = submesh.base_vertex + orgmesh.merged_vert_offset;
				meshlet_meshlet_cull.set_int("mesh_basevertex", base_vertex);
			}
			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, compute_indirect_buffer);
			glDispatchComputeIndirect(0);
		}

		//{
		//	GPUSCOPESTART("compact");
		//	glMemoryBarrier(GL_ALL_BARRIER_BITS);
		//
		//	meshlet_compact.use();
		//	int draw_call_count = OBJECTS_TO_RENDER * meshlet_model->chunks.size();
		//	glDispatchCompute(draw_call_count / 256 + 1, 1, 1);
		//}

		{
			GPUSCOPESTART("drawit");
			glMemoryBarrier(GL_ALL_BARRIER_BITS);

			// now draw the shit
			glBindVertexArray(meshlet_model->vao);
			mdi_meshlet_cull_shader.use();
			mdi_buffer2_pm.bind_buffer(GL_SHADER_STORAGE_BUFFER, 2);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, drawid_to_instance_buffer);

			//glCullFace(GL_FRONT);

			int draw_call_count = OBJECTS_TO_RENDER * meshlet_model->chunks.size();
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_elements_indirect_buffer);
			glBindBuffer(GL_PARAMETER_BUFFER, draw_elements_indirect_buffer);
			glMultiDrawElementsIndirectCount(
				GL_TRIANGLES,
				GL_UNSIGNED_INT,
				(void*)sizeof(glm::ivec4),	// offset to draw calls
				0,							// offset to draw count in GL_PARAMETER_BUFFER
				draw_call_count,			// max draw calls possible
				sizeof(gpu::DrawElementsIndirectCommand)
			);
		}


	}
	else if (mdi_modes == MDI_MESHLETS_CULLING_DYNAMIC_INDEX) {

	}

	else {

		
	}
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	draw.stats.tris_drawn = sphere->mesh.parts[0].element_count * num_batches_to_render * num_prims_per_batch / 3;
}
#endif
struct Debug_Shape
{
	enum type {
		sphere,
		line,
		box
	}type;
	glm::vec3 pos;
	glm::vec3 size;
	Color32 color;
	float lifetime = 0.0;
};

struct Debug_Shape_Ctx
{
	vector<Debug_Shape> shapes;
	vector<Debug_Shape> one_frame_fixedupdate;
	void add(Debug_Shape shape, bool fixedupdate) {
		if (shape.lifetime <= 0.f && fixedupdate)
			one_frame_fixedupdate.push_back(shape);
		else
			shapes.push_back(shape);
	}
};
static Debug_Shape_Ctx debug_shapes;


void Debug::add_line(glm::vec3 f, glm::vec3 to, Color32 color, float duration, bool fixedupdate)
{
	Debug_Shape shape;
	shape.type = Debug_Shape::line;
	shape.pos = f;
	shape.size = to;
	shape.color = color;
	shape.lifetime = duration;
	debug_shapes.add(shape,fixedupdate);
}
void Debug::add_box(glm::vec3 c, glm::vec3 size, Color32 color, float duration, bool fixedupdate)
{
	Debug_Shape shape;
	shape.type = Debug_Shape::box;
	shape.pos = c;
	shape.size = size;
	shape.color = color;
	shape.lifetime = duration;
	debug_shapes.add(shape,fixedupdate);
}
void Debug::add_sphere(glm::vec3 c, float radius, Color32 color, float duration, bool fixedupdate)
{
	Debug_Shape shape;
	shape.type = Debug_Shape::sphere;
	shape.pos = c;
	shape.size = vec3(radius);
	shape.color = color;
	shape.lifetime = duration;
	debug_shapes.add(shape,fixedupdate);
}

void Debug::on_fixed_update_start()
{
	debug_shapes.one_frame_fixedupdate.clear();
}

void draw_debug_shapes(float dt)
{
	MeshBuilder builder;
	builder.Begin();

	vector<Debug_Shape>* shapearrays[2] = { &debug_shapes.one_frame_fixedupdate,&debug_shapes.shapes };
	for (int i = 0; i < 2; i++) {
		vector<Debug_Shape>& shapes = *shapearrays[i];
		for (int j = 0; j < shapes.size(); j++) {
			switch (shapes[j].type)
			{
			case Debug_Shape::line:
				builder.PushLine(shapes[j].pos, shapes[j].size, shapes[j].color);
				break;
			case Debug_Shape::box:
				builder.PushLineBox(shapes[j].pos - shapes[j].size * 0.5f, shapes[j].pos + shapes[j].size * 0.5f, shapes[j].color);
				break;
			case Debug_Shape::sphere:
				builder.AddSphere(shapes[j].pos, shapes[j].size.x, 8, 6, shapes[j].color);
				break;
			}
		}
	}
	builder.End();
	glDisable(GL_DEPTH_TEST);
	builder.Draw(GL_LINES);
	glEnable(GL_DEPTH_TEST);
	glCheckError();
	vector<Debug_Shape>& shapes = debug_shapes.shapes;
	for (int i = 0; i < shapes.size(); i++) {
		shapes[i].lifetime -= dt;
		if (shapes[i].lifetime <= 0.f) {
			shapes.erase(shapes.begin() + i);
			i--;
		}
	}
	builder.Free();
}

extern ConfigVar g_draw_grid;
extern ConfigVar g_grid_size;

void draw_debug_grid()
{
	static MeshBuilder mb;
	static bool init = true;

	if (init) {
		mb.Begin();
		for (int x = 0; x < 11; x++) {
			mb.PushLine(glm::vec3(-5, 0, x - 5), glm::vec3(5, 0, x - 5), COLOR_WHITE);
			mb.PushLine(glm::vec3(x - 5, 0, -5), glm::vec3(x - 5, 0, 5), COLOR_WHITE);
		}
		mb.End();
		init = false;
	}
	glEnable(GL_DEPTH_TEST);
	mb.Draw(GL_LINES);
}

// ORDER:
// pre draw:
//		setup framebuffers
//		setup view stuff
//		setup material buffer
//		setup scene buffers
// main draw:
//		kick off shadow map drawing
//			directional cascades + any point/spot lights
// 
//		if GPU culling:
//			dispatch GPU culling compute shader
//			gbuffer pass 1
//				unoccluded opaques drawn to buffer
//			rebuild HZB
//			dispatch GPU culing compute shader for occluded objs+
//			gbuffer pass 2
//				unoccluded opaques draw to buffer
//			build HZB for next frame
//		else:
//			gbuffer pass
//				draw all objects
// 
//		editor outline pass:
//			for selected objects, draw to outline buffer
//		custom depth pass:
//			for all custom depth objects: draw to custom depth buffer with stencil etc.	
// 
//		screenspace passes using gbuffers:
//			ssao pass (use depth buffer)
//			ssr pass with gbuffer
//			screen space shadows
// 	
//		decal pass
//			for all decals: draw OOB and modify gbuffers
//		lighting passes
//			for all lights: accumulate lighting + shadowing
//		ambient light pass
//			add ambient light to scene (use probes in future)
//		reflection pass
//			sample scene cubemaps for each pixel and accumulate
//		transcluent pass
//			all transcluents get rendered to lighting buffer
//				optional forward lighting for N relevant lights (Sun + nearby point lights?)
// 
// 
// post draw:
//		combine ss shadows to color buffer
//		combine SSR to color buffer
//		combine SSAO on to color buffer
//		bloom pass on color buffer	
//		post processing FXs
//		composite post process and bloom


// main function for lighting the gbuffer
// directional lights
// point+spotlights

void Renderer::accumulate_gbuffer_lighting()
{
	GPUSCOPESTART("accumulate_gbuffer_lighting");



	
	Model* LIGHT_CONE = mods.get_light_cone();
	Model* LIGHT_SPHERE = mods.get_light_sphere();
	Model* LIGHT_DOME = mods.get_light_dome();
	{
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, active_constants_ubo);

		bind_vao(mods.get_vao(true/* animated */));

		// bind the forward_render framebuffer
		// outputs to the scene_color texture
		glBindFramebuffer(GL_FRAMEBUFFER, fbo.forward_render);
		glViewport(0, 0, vs.width, vs.height);

		// disable depth writes
		glDepthMask(GL_FALSE);

		glEnable(GL_BLEND);	// enable additive blending
		glBlendFunc(GL_ONE, GL_ONE);

		set_shader(prog.light_accumulation);

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);

		bind_texture(0, tex.scene_gbuffer0);
		bind_texture(1, tex.scene_gbuffer1);
		bind_texture(2, tex.scene_gbuffer2);
		bind_texture(3, tex.scene_depth);

		for (auto& light_pair : scene.light_list.objects) {
			auto& light = light_pair.type_.light;

			glm::mat4 ModelTransform = glm::translate(glm::mat4(1.f), light.position);
			const float scale = light.radius;
			ModelTransform = glm::scale(ModelTransform, glm::vec3(scale));

			// Copied code from execute_render_lists
			auto& part = LIGHT_SPHERE->get_part(0);
			const GLenum index_type = (mods.get_index_type_size() == 4) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
			gpu::DrawElementsIndirectCommand cmd;
			cmd.baseVertex = part.base_vertex + LIGHT_SPHERE->get_merged_vertex_ofs();
			cmd.count = part.element_count;
			cmd.firstIndex = part.element_offset + LIGHT_SPHERE->get_merged_index_ptr();
			cmd.firstIndex /= (use_32_bit_indicies) ? 4 : 2;
			cmd.primCount = 1;
			cmd.baseInstance = 0;

			shader().set_mat4("Model", ModelTransform);
			shader().set_vec3("position", light.position);
			shader().set_float("radius", light.radius);
			shader().set_bool("is_spot_light", light.is_spotlight);
			shader().set_float("spot_inner", cos(glm::radians(light.conemin)));
			shader().set_float("spot_angle", cos(glm::radians(light.conemax)));
			shader().set_vec3("spot_normal",light.normal);

			glMultiDrawElementsIndirect(
				GL_TRIANGLES,
				index_type,
				(void*)&cmd,
				1,
				sizeof(gpu::DrawElementsIndirectCommand)
			);
		}

		// undo state changes
		glDepthMask(GL_TRUE);
		glDisable(GL_STENCIL_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}


	// fullscreen pass for directional light(s)
	RSunInternal* sun_internal = scene.get_main_directional_light();
	if(sun_internal)
	{
		// bind the forward_render framebuffer
		// outputs to the scene_color texture
		glBindFramebuffer(GL_FRAMEBUFFER, fbo.forward_render);
		glViewport(0, 0, vs.width, vs.height);

		// disable depth writes
		glDepthMask(GL_FALSE);

		glEnable(GL_BLEND);	// enable additive blending
		glBlendFunc(GL_ONE, GL_ONE);

		glDisable(GL_DEPTH_TEST);

		set_shader(prog.sunlight_accumulation);

		bind_texture(0, tex.scene_gbuffer0);
		bind_texture(1, tex.scene_gbuffer1);
		bind_texture(2, tex.scene_gbuffer2);
		bind_texture(3, tex.scene_depth);
		bind_texture(4, draw.shadowmap.texture.shadow_array);
		glBindBufferBase(GL_UNIFORM_BUFFER, 8, draw.shadowmap.ubo.info);

		shader().set_vec3("uSunDirection", sun_internal->sun.direction);
		shader().set_vec3("uSunColor", sun_internal->sun.color);
		
		// fullscreen shader, no vao used
		glBindVertexArray(vao.default_);
		// to prevent crashes??
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindVertexBuffer(0, buf.default_vb, 0, 0);
		glBindVertexBuffer(1, buf.default_vb, 0, 0);
		glBindVertexBuffer(2, buf.default_vb, 0, 0);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glDepthMask(GL_TRUE);
		glDisable(GL_STENCIL_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// indirect lighting pass (full screen, use constant ambient for now), multiple with SSAO

	// reflection pass (use static for now)
}


void Renderer::scene_draw(SceneDrawParamsEx params, View_Setup view, UIControl* gui, IEditorTool* tool)
{
	GPUFUNCTIONSTART;

	mem_arena.free_bottom();
	stats = Render_Stats();
	state_machine.invalidate_all();

	const bool needs_composite = !params.output_to_screen;

	if (cur_w != view.width || cur_h != view.height)
		InitFramebuffers(true, view.width, view.height);
	lastframe_vs = current_frame_main_view;

	current_frame_main_view = view;

	if (enable_vsync.get_bool())
		SDL_GL_SetSwapInterval(1);
	else
		SDL_GL_SetSwapInterval(0);

	// Update any gpu materials that became invalidated or got newly allocated
	matman.pre_render_update();

	if (!params.draw_world && (!params.draw_ui || !gui))
		return;
	else if (gui && !params.draw_world && params.draw_ui) {
		// just paint ui and then return
		gui->ui_paint();
		return;
	}
	vs = current_frame_main_view;
	upload_ubo_view_constants(ubo.current_frame);
	active_constants_ubo = ubo.current_frame;
	scene.build_scene_data(params.is_editor);

	shadowmap.update();

	//volfog.compute();
	const bool is_wireframe_mode = r_debug_mode.get_integer() == gpu::DEBUG_WIREFRAME;
	if(is_wireframe_mode)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// depth prepass
	//if (!is_wireframe_mode)
	//{
	//	GPUSCOPESTART("Depth prepass");
	//	Render_Level_Params params;
	//	params.output_framebuffer = fbo.scene;
	//	params.view = current_frame_main_view;
	//	params.pass = Render_Level_Params::DEPTH;
	//	params.upload_constants = false;
	//	params.provied_constant_buffer = ubo.current_frame;
	//	params.draw_viewmodel = true;
	//	render_level_to_target(params);
	//}

	// render ssao using prepass buffer
	if (enable_ssao.get_bool())
		ssao.render();

	// planar reflection render
	//{
	//	GPUSCOPESTART("Planar reflection");
	//	planar_reflection_pass();
	//}

	// main level render
	{
		GPUSCOPESTART("GBUFFER PASS");
		Render_Level_Params params(
			current_frame_main_view,
			&scene.gbuffer_rlist,
			&scene.gbuffer_pass,
			fbo.gbuffer,
			true,	/* clear framebuffer */
			Render_Level_Params::OPAQUE
		);
		
		params.upload_constants = true;
		params.provied_constant_buffer = ubo.current_frame;
		params.draw_viewmodel = true;

		render_level_to_target(params);
	}

	if(r_debug_mode.get_integer() == 0)
		accumulate_gbuffer_lighting();

	{
		GPUSCOPESTART("TRANSPARENTS");
		Render_Level_Params params(
			current_frame_main_view,
			&scene.transparent_rlist,
			&scene.transparent_pass,
			fbo.forward_render,
			false,	/* dont clear framebuffer */
			Render_Level_Params::TRANSLUCENT
		);
		
		params.upload_constants = true;
		params.provied_constant_buffer = ubo.current_frame;
		params.draw_viewmodel = true;
		glDepthMask(GL_FALSE);
		glDepthFunc(GL_LEQUAL);
		render_level_to_target(params);
		glDepthMask(GL_TRUE);
	}

	set_blend_state(blend_state::OPAQUE);

	if (is_wireframe_mode)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.forward_render);

	set_shader(prog.simple);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.f));

	draw_debug_shapes(params.dt);

	// hook in physics debugging, function determines if its drawing or not
	g_physics->debug_draw_shapes();

	if (tool)
		tool->overlay_draw();

	if (g_draw_grid.get_bool())
		draw_debug_grid();
	
	// Bloom update
	render_bloom_chain();

	int x = vs.width;
	int y = vs.height;


	uint32_t framebuffer_to_output = (needs_composite) ? fbo.composite : 0;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_to_output);
	glViewport(0, 0, cur_w, cur_h);

	set_shader(prog.combine);
	uint32_t bloom_tex = tex.bloom_chain[0];
	if (!enable_bloom.get_bool()) 
		bloom_tex = black_texture.gl_id;
	bind_texture(0, tex.scene_color);
	bind_texture(1, bloom_tex);
	bind_texture(2, lens_dirt->gl_id);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	set_shader(prog.simple);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.f));

	if (gui && params.draw_ui)
		gui->ui_paint();


	debug_tex_out.draw_out();

	//cubemap_positions_debug();

	glClear(GL_DEPTH_BUFFER_BIT);

	// FIXME: ubo view constant buffer might be wrong since its changed around a lot (bad design)
}

void Renderer::cubemap_positions_debug()
{
	set_shader(prog.simple);
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

Shader Renderer::shader()
{
	if (state_machine.active_program == -1) return Shader();
	return prog_man.get_obj(state_machine.active_program);
}

void Renderer::DrawEntBlobShadows()
{
	return;

	shadowverts.Begin();

	
	glCheckError();

	shadowverts.End();
	glCheckError();

	//set_shader(prog.particle_basic);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.0));
	shader().set_vec4("tint_color", vec4(0, 0, 0, 1));
	glCheckError();

	//bind_texture(0, eng->media.blob_shadow->gl_id);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	shadowverts.Draw(GL_TRIANGLES);

	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);

	glCheckError();

}


#undef SET_OR_USE_FALLBACK
#define SET_OR_USE_FALLBACK(texture, where, fallback) \
if(gs->images[(int)texture]) bind_texture(where, gs->images[(int)texture]->gl_id); \
else bind_texture(where, fallback.gl_id);

void Renderer::planar_reflection_pass()
{
	ASSERT(0);
#if 0
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
	if (use_halfres_reflections.get_bool()) {
		setup.width /= 2;
		setup.height /= 2;
	}
	
	Render_Level_Params params;
	params.view = setup;
	params.has_clip_plane = true;
	params.custom_clip_plane = vec4(plane_n, plane_d);
	params.pass = params.OPAQUE;
	params.clear_framebuffer = true;
	//params.output_framebuffer = fbo.reflected_scene;
	params.draw_viewmodel = false;
	params.upload_constants = true;
	params.is_water_reflection_pass = true;

	render_level_to_target(params);
#endif
}

#if 0
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
#endif
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

		Render_Level_Params params(
			cubemap_view,
			&scene.gbuffer_rlist,
			&scene.gbuffer_pass,
			fbo,
			true,
			Render_Level_Params::OPAQUE
			);
	
		params.provied_constant_buffer = 0;
	
		params.upload_constants = true;
		params.is_probe_render = true;

		render_level_to_target(params);
		glCheckError();

	}
}


RSunInternal* Render_Scene::get_main_directional_light()
{
	if (!suns.empty())
		return &suns.at(suns.size() - 1);
	return nullptr;
}
void Renderer::on_level_end()
{


}

void Renderer::on_level_start()
{
	scene.cubemaps.clear();

	glDeleteBuffers(1, &scene.cubemap_ssbo);
	glDeleteBuffers(1, &scene.light_ssbo);
	glDeleteTextures(1, &scene.levelcubemapirradiance_array);
	glDeleteTextures(1, &scene.levelcubemapspecular_array);
	glDeleteTextures(1, &scene.skybox);
	scene.cubemap_ssbo = 0;
	scene.levelcubemapirradiance_array = scene.levelcubemapspecular_array = 0;


	// Render cubemaps
	scene.cubemaps.push_back({});		// skybox probe
	scene.cubemaps[0].found_probe_flag = true;
#if 0
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
#endif
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
	size_t size = glm::min((int)scene.cubemaps.size(), 128);
	glNamedBufferData(scene.cubemap_ssbo, (sizeof Cubemap_Ssbo_Struct)* size, probes, GL_STATIC_DRAW);
}

void Render_Scene::update_obj(handle<Render_Object> handle, const Render_Object& proxy)
{
	ROP_Internal& in = proxy_list.get(handle.id);
	in.proxy = proxy;
	if (!proxy.viewmodel_layer) 
		in.inv_transform = glm::inverse(proxy.transform);
	if (proxy.model)
		in.bounding_sphere_and_radius = proxy.transform * proxy.model->get_bounding_sphere();
}


void DepthPyramid::init()
{
	depth_pyramid = g_imgs.install_system_texture("_depth_pyramid");
	assert(depth_pyramid);
	draw.on_viewport_size_changed.add(this, &DepthPyramid::on_viewport_size_changed);
	draw.prog.cCreateDepthPyramid = draw.prog_man.create_compute("misc/depth_pyramid.txt");
}
void DepthPyramid::free()
{

}
void DepthPyramid::dispatch_depth_pyramid_creation()
{

}
void DepthPyramid::on_viewport_size_changed(int x, int y)
{
	texhandle handle = depth_pyramid->gl_id;

	glDeleteTextures(1, &handle);

	// allocate a depth pyramid
	const uint width = x / 2;
	const uint height = y / 2;
	const uint32_t mip_count = get_mip_map_count(width, height);
	glCreateTextures(GL_TEXTURE_2D, 1, &handle);
	glTextureStorage2D(handle, mip_count, GL_DEPTH_COMPONENT24, width, height);
	glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// 2x2 min filter, special extension but has pretty wide support for any newish hardware
	glTextureParameteri(handle, GL_TEXTURE_REDUCTION_MODE_ARB, GL_MIN);
	// update virtual texture
	depth_pyramid->update_specs(handle, width, height, 3, Texture_Format::TEXFMT_RGB8);
	depth_pyramid->type = Texture_Type::TEXTYPE_2D;
}

void DebuggingTextureOutput::draw_out()
{
	if (!output_tex)
		return;
	if (output_tex->gl_id == 0) {
		sys_print("!!! DebuggingTextureOutput has invalid texture\n");
		output_tex = nullptr;
		return;
	}
	if (output_tex->type == Texture_Type::TEXTYPE_2D)
		draw.set_shader(draw.prog.tex_debug_2d);
	else if (output_tex->type == Texture_Type::TEXTYPE_2D_ARRAY)
		draw.set_shader(draw.prog.tex_debug_2d_array);
	else {
		sys_print("!!! can only debug 2d and 2d array textures\n");
		output_tex = nullptr;
		return;
	}

	const int w = output_tex->width;
	const int h = output_tex->height;

	const float cur_w = draw.get_current_frame_vs().width;
	const float cur_h = draw.get_current_frame_vs().height;

	draw.shader().set_mat4("Model", mat4(1));
	glm::mat4 proj = glm::ortho(0.f, cur_w, cur_h, 0.f);
	draw.shader().set_mat4("ViewProj", proj);

	draw.shader().set_float("alpha", alpha);
	draw.shader().set_float("mip_slice", -1);

	draw.bind_texture(0, output_tex->gl_id);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glm::vec2 upper_left = glm::vec2(0, 1);
	glm::vec2 size = glm::vec2(1, -1);


	MeshBuilder mb;
	mb.Begin();
	mb.Push2dQuad(glm::vec2(0, 0), glm::vec2(w * scale, h * scale), upper_left, size, {});
	mb.End();
	mb.Draw(MeshBuilder::TRIANGLES);

	glDisable(GL_BLEND);

	mb.Free();

}

static float linearize_depth(float d, float zNear, float zFar)
{
	float z_n = 2.0 * d - 1.0;
	return 2.0 * zNear * zFar / (zFar + zNear - z_n * (zFar - zNear));
}

float Renderer::get_scene_depth_for_editor(int x, int y)
{
	// super slow garbage functions obviously

	if (x < 0 || y < 0 || x >= cur_w || y >= cur_h) {
		sys_print("!!! invalid mouse coords for mouse_pick_scene\n");
		return { -1 };
	}

	glFlush();
	glFinish();

	const size_t size = cur_h * cur_w;
	float* buffer_pixels = new float[size];

	glGetTextureImage(tex.scene_depth, 0, GL_DEPTH_COMPONENT, GL_FLOAT, size*sizeof(float), buffer_pixels);

	y = cur_h - y - 1;

	const size_t ofs = cur_w * y + x;
	const float depth = buffer_pixels[ofs];
	delete[] buffer_pixels;

	return linearize_depth(depth, vs.near, vs.far);
}

handle<Render_Object> Renderer::mouse_pick_scene_for_editor(int x, int y)
{
	// super slow garbage functions obviously

	if (x < 0 || y < 0 || x >= cur_w || y >= cur_h) {
		sys_print("!!! invalid mouse coords for mouse_pick_scene\n");
		return { -1 };
	}

	glFlush();
	glFinish();

	const size_t size = cur_h * cur_w * 4;
	uint8_t* buffer_pixels = new uint8_t[size];

	glGetTextureImage(tex.editor_id_buffer,0, GL_RGBA,GL_UNSIGNED_BYTE, size, buffer_pixels);

	y = cur_h - y - 1;

	const size_t ofs = cur_w * y * 4 + x * 4;
	uint8_t* ptr = &buffer_pixels[ofs];
	uint32_t id = uint32_t(ptr[0]) | uint32_t(ptr[1]) << 8 | uint32_t(ptr[1]) << 16 | uint32_t(ptr[3]) << 24;
	delete[] buffer_pixels;

	if (id == 0xff000000) {
		sys_print("NONE\n");
		return { -1 };
	}

	uint32_t realid = id - 1;	// allow for nullptr

	if (realid >= scene.proxy_list.objects.size()) {
		sys_print("!!! invalid editorid\n");
		return { -1 };
	}
	int handle_out = scene.proxy_list.objects.at(realid).handle;

	sys_print("MODEL: %s\n", scene.proxy_list.objects.at(realid).type_.proxy.model->get_name().c_str());

	return { handle_out };
}