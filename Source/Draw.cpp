#include "DrawLocal.h"

#include "Framework/Util.h"
#include "glad/glad.h"
#include "Texture.h"
#include "Game_Engine.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "Entity.h"

#ifdef EDITDOC
#include "EditorDocPublic.h"
#endif

//#pragma optimize("", off)

Renderer draw;
RendererPublic* idraw = &draw;

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

extern Auto_Config_Var g_debug_skeletons;


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
	bool invalid = state_machine.is_bit_invalid(Opengl_State_Machine::TEXTURE0_BIT + bind);
	if (invalid || state_machine.textures_bound[bind] != id) {
		state_machine.set_bit_valid(Opengl_State_Machine::TEXTURE0_BIT + bind);
		glBindTextureUnit(bind, id);
		state_machine.textures_bound[bind] = id;
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

	draw.bind_texture(SHADOWMAP_LOC, draw.shadowmap.texture.shadow_array);

	glCheckError();

	//shader().set_vec4("aoproxy_sphere", vec4(eng->local_player().position + glm::vec3(0,aosphere.y,0), aosphere.x));
	//shader().set_float("aoproxy_scale_factor", aosphere.z);

	uint32_t ssao_tex = draw.ssao.texture.result;
	if (params.is_probe_render || params.is_water_reflection_pass) ssao_tex = draw.white_texture.gl_id;

	draw.bind_texture(SSAO_TEX_LOC, ssao_tex);
	glCheckError();

	draw.bind_texture(CAUSTICS_LOC, draw.casutics->gl_id);

	glCheckError();


	if (eng->level && eng->level->lightmap)
		draw.bind_texture(LIGHTMAP_LOC, eng->level->lightmap->gl_id);
	else
		draw.bind_texture(LIGHTMAP_LOC, draw.white_texture.gl_id);

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
	else {
		if (def.geo)
			def.compile_failed = !Shader::compile(def.shader_obj, def.vert, def.frag, def.geo, def.defines);
		else
			def.compile_failed = Shader::compile(&def.shader_obj, def.vert, def.frag, def.defines) != ShaderResult::SHADER_SUCCESS;
	}
}

Material_Shader_Table::Material_Shader_Table() : shader_hash_map(128)
{
	ASSERT(((shader_hash_map.size() & (shader_hash_map.size() - 1)) == 0) && "must be power of 2");
}

program_handle Material_Shader_Table::lookup(shader_key key)
{
	uint32_t key32 = key.as_uint32();
	uint32_t hash = std::hash<uint32_t>()(key32);
	uint32_t size = shader_hash_map.size();
	for (uint32_t i = 0; i < size; i++) {
		uint32_t index = (hash + i) & (size - 1);
		if (shader_hash_map[index].handle == -1) return -1;
		if (shader_hash_map[index].key.as_uint32() == key32)
			return shader_hash_map[index].handle;
		//printf("material table collision\n");
	}
	return -1;
}
void Material_Shader_Table::insert(shader_key key, program_handle handle)
{
	uint32_t key32 = key.as_uint32();
	uint32_t hash = std::hash<uint32_t>()(key32);
	uint32_t size = shader_hash_map.size();
	hash %= size;
	for (uint32_t i = 0; i < size; i++) {
		uint32_t index = (hash + i) % size;
		if (shader_hash_map[index].handle == -1) {
			shader_hash_map[index].key = key;
			shader_hash_map[index].handle = handle;
			return;
		}
	}
	ASSERT(!"material table overflow\n");
}


shader_key get_real_shader_key_from_shader_type(shader_key key)
{
	material_type type = (material_type)key.shader_type;
	if (key.depth_only) {
		key.normal_mapped = 0;
		key.vertex_colors = 0;
	
		// FIXME dithering

		// windsway has vertex shader modifications
		if(type !=material_type::WINDSWAY)
			key.shader_type = (int)material_type::DEFAULT;
	}
	else {
		if (type == material_type::WINDSWAY) {
			key.animated = 0;
			key.vertex_colors = 0;
		}
		else if (type == material_type::TWOWAYBLEND)
			key.vertex_colors = 1;
		else if (type == material_type::WATER)
			key.normal_mapped = 1;
		else if (type == material_type::UNLIT) {
			key.normal_mapped = 0;
			key.alpha_tested = 0;
		}
	}
	return key;
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

program_handle compile_mat_shader(shader_key key)
{
	std::string params;
	const char* vert_shader = "";
	const char* frag_shader = "";

	material_type type = (material_type)key.shader_type;
	switch (type) {
	case material_type::DEFAULT:
	case material_type::WINDSWAY:
	case material_type::TWOWAYBLEND:
	case material_type::UNLIT:
		vert_shader = "AnimBasicV.txt";
		if (key.depth_only) 
			frag_shader = "DepthF.txt";
		else 
			frag_shader = "PbrBasicF.txt";
		break;
	case material_type::WATER:
		vert_shader = "AnimBasicV.txt";
		frag_shader = "WaterF.txt";
		break;
	default:
		ASSERT(!"material type not defined\n");
		break;
	}

	if (key.alpha_tested) params += "ALPHATEST,";
	if (key.animated) params += "ANIMATED,";
	if (key.normal_mapped) params += "NORMALMAPPED,";
	if (key.vertex_colors) params += "VERTEX_COLOR,";
	if (key.dither) params += "DITHER,";
	if (type == material_type::WINDSWAY) params += "WIND,";
	if (type == material_type::UNLIT) params += "UNLIT,";
	if (!params.empty())params.pop_back();

	printf("INFO: compiling shader: %s %s (%s)\n", vert_shader, frag_shader, params.c_str());

	program_handle handle = draw.prog_man.create_raster(vert_shader, frag_shader, params);
	ASSERT(handle != -1);
	draw.set_shader(handle);
	draw.set_shader_sampler_locations();
	draw.set_shader(-1);

	draw.mat_table.insert(key, handle);
	return handle;
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
	Shader::compile(&naiveshader, "SimpleMeshV.txt", "UnlitF.txt", "NAIVE");
	Shader::compile(&naiveshader2, "SimpleMeshV.txt", "UnlitF.txt", "NAIVE2");

	Shader::compile(&mdi_meshlet_cull_shader, "SimpleMeshV.txt", "UnlitF.txt", "MDICULL");


	Shader::compute_compile(&meshlet_inst_cull, "Meshlets/meshlets.txt", "INSTANCE_CULLING");
	Shader::compute_compile(&meshlet_meshlet_cull, "Meshlets/meshlets.txt", "MESHLET_CULLING");
	Shader::compute_compile(&meshlet_reset_pre_inst, "Meshlets/reset.txt", "RESET_PRE_INSTANCES");
	Shader::compute_compile(&meshlet_reset_post_inst, "Meshlets/reset.txt", "RESET_POST_INSTANCES");
	Shader::compute_compile(&mdi_meshlet_zero_bufs, "Meshlets/zerobuf.txt");
	Shader::compute_compile(&meshlet_compact, "Meshlets/compact.txt");


	prog.simple = prog_man.create_raster("MbSimpleV.txt", "MbSimpleF.txt");
	prog.textured = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt");
	prog.textured3d = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE3D");
	prog.texturedarray = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTUREARRAY");
	prog.skybox = prog_man.create_raster("MbSimpleV.txt", "SkyboxF.txt", "SKYBOX");
	prog.particle_basic = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "PARTICLE_SHADER");

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

	for (int i = 0; i < mat_table.shader_hash_map.size(); i++) {
		if (mat_table.shader_hash_map[i].handle != -1) {
			set_shader(mat_table.shader_hash_map[i].handle);
			set_shader_sampler_locations();
		}
	}
}


void Renderer::upload_ubo_view_constants(uint32_t ubo, glm::vec4 custom_clip_plane)
{
	gpu::Ubo_View_Constants_Struct constants;
	constants.view = vs.view;
	constants.viewproj = vs.viewproj;
	constants.invview = glm::inverse(vs.view);
	constants.invproj = glm::inverse(vs.proj);
	constants.viewpos_time = glm::vec4(vs.origin, eng->time);
	constants.viewfront = glm::vec4(vs.front, 0.0);
	constants.viewport_size = glm::vec4(vs.width, vs.height, 0, 0);

	constants.near = vs.near;
	constants.far = vs.far;
	constants.shadowmap_epsilon = shadowmap.tweak.epsilon;

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

	constants.numcubemaps = scene.cubemaps.size();
	constants.numlights = scene.lights.size();
	if (using_skybox_for_specular)
		constants.forcecubemap = 0.0;
	else
		constants.forcecubemap = -1.0;

	constants.custom_clip_plane = custom_clip_plane;

	glNamedBufferData(ubo, sizeof gpu::Ubo_View_Constants_Struct, &constants, GL_DYNAMIC_DRAW);
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
	ImGui::Text("Vao binds: %d", draw.stats.vaos_bound);
	ImGui::Text("Blend changes: %d", draw.stats.blend_changes);

	ImGui::Text("opaque batches: %d", (int)draw.scene.opaque.batches.size());
	ImGui::Text("depth batches: %d", (int)draw.scene.depth.batches.size());
	ImGui::Text("transparent batches: %d", (int)draw.scene.transparents.batches.size());

	ImGui::Text("total objects: %d", (int)draw.scene.proxy_list.objects.size());
	ImGui::Text("opaque mesh batches: %d", (int)draw.scene.opaque.mesh_batches.size());
}

void Renderer::init()
{
	sys_print("--------- Initializing Renderer ---------\n");


	bool supports_compression = false;
	bool supports_sprase_tex = false;
	bool supports_bindless = false;
	bool supports_filter_minmax = false;
	bool supports_atomic64 = false;
	bool supports_int64 = false;



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
		else if (strcmp(ext, "GL_NV_shader_atomic_int64") == 0) supports_atomic64 = true;
		else if (strcmp(ext, "GL_ARB_gpu_shader_int64") == 0) supports_int64 = true;

	}

	printf("Extension support:\n");
	printf("-GL_ARB_bindless_texture: %s\n", (supports_bindless)?"yes":"no");
	printf("-GL_ARB_sparse_texture: %s\n", (supports_sprase_tex) ? "yes" : "no");
	printf("-GL_ARB_texture_filter_minmax: %s\n", (supports_filter_minmax) ? "yes" : "no");
	printf("-GL_EXT_texture_compression_s3tc: %s\n", (supports_compression) ? "yes" : "no");
	printf("-GL_NV_shader_atomic_int64: %s\n", (supports_atomic64) ? "yes" : "no");
	printf("-GL_ARB_gpu_shader_int64: %s\n", (supports_int64) ? "yes" : "no");


	if (!supports_compression) {
		Fatalf("Opengl driver needs GL_EXT_texture_compression_s3tc\n");
	}
	
	int max_buffer_bindings = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_buffer_bindings);
	printf("-GL_MAX_UNIFORM_BUFFER_BINDINGS: %d\n", max_buffer_bindings);

	InitGlState();

	scene.init();

	float start = GetTime();
	create_shaders();
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

	glCreateBuffers(1, &ubo.current_frame);

	perlin3d = generate_perlin_3d({ 16,16,16 }, 0x578437adU, 4, 2, 0.4, 2.0);

	fbo.scene = 0;
	fbo.reflected_scene = 0;
	tex.scene_color = tex.scene_depthstencil = 0;
	tex.reflected_color = tex.reflected_depth = 0;
	InitFramebuffers(true,eng->window_w.integer(), eng->window_h.integer());

	EnviornmentMapHelper::get().init();
	volfog.init();
	shadowmap.init();
	ssao.init();
	
	lens_dirt = mats.find_texture("lens_dirt.jpg");
	casutics = mats.find_texture("caustics.png");
	waternormal = mats.find_texture("waternormal.png");

	glGenVertexArrays(1, &vao.default_);
	glCreateBuffers(1, &buf.default_vb);
	glNamedBufferStorage(buf.default_vb, 12 * 3, nullptr, 0);

	Debug_Interface::get()->add_hook("Render stats", imgui_stat_hook);
}


void Renderer::InitFramebuffers(bool create_composite_texture, int s_w, int s_h)
{
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

	glDeleteFramebuffers(1, &fbo.composite);
	glDeleteTextures(1, &tex.output_composite);
	if (create_composite_texture) {
		glCreateTextures(GL_TEXTURE_2D, 1, &tex.output_composite);
		glTextureStorage2D(tex.output_composite, 1, GL_RGB8, s_w, s_h);
		glTextureParameteri(tex.output_composite, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(tex.output_composite, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(tex.output_composite, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(tex.output_composite, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glCreateFramebuffers(1, &fbo.composite);
		glNamedFramebufferTexture(fbo.composite, GL_COLOR_ATTACHMENT0, tex.output_composite, 0);
	}

	cur_w = s_w;
	cur_h = s_h;

	init_bloom_buffers();
}

void Renderer::init_bloom_buffers()
{
	glDeleteFramebuffers(1, &fbo.bloom);
	glDeleteTextures(BLOOM_MIPS, tex.bloom_chain);

	glCreateFramebuffers(1, &fbo.bloom);


	glCreateTextures(GL_TEXTURE_2D, BLOOM_MIPS, tex.bloom_chain);
	int x = cur_w / 2;
	int y = cur_h / 2;
	float fx = x;
	float fy = y;
	for (int i = 0; i < BLOOM_MIPS; i++) {
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
}


extern bool bloom_stop;

void Renderer::render_bloom_chain()
{
	GPUFUNCTIONSTART;

	glBindVertexArray(vao.default_);
	// to prevent crashes??
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexBuffer(0, buf.default_vb, 0, 0);
	glBindVertexBuffer(1, buf.default_vb, 0, 0);
	glBindVertexBuffer(2, buf.default_vb, 0, 0);


	if (!enable_bloom.integer())
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo.bloom);
	set_shader(prog.bloom_downsample);
	float src_x = cur_w;
	float src_y = cur_h;

	glBindTextureUnit(0, tex.scene_color);
	glClearColor(0, 0, 0, 1);
	for (int i = 0; i < BLOOM_MIPS; i++)
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

	if (bloom_stop) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	set_shader(prog.bloom_upsample);
	for (int i = BLOOM_MIPS - 1; i > 0; i--)
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
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, scene.gpu_render_material_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, list.glinstance_to_instance);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, list.gldrawid_to_submesh_material);

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	int offset = 0;
	for (int i = 0; i < pass.batches.size(); i++) {
		int count = list.command_count[i];

		const Material* mat = pass.mesh_batches[pass.batches[i].first].material;
		draw_call_key batch_key = pass.objects[pass.mesh_batches[pass.batches[i].first].first].sort_key;

		program_handle program = (program_handle)batch_key.shader;
		blend_state blend = (blend_state)batch_key.blending;
		bool backface = batch_key.backface;
		uint32_t layer = batch_key.layer;
		mesh_format format = (mesh_format)batch_key.vao;

		assert(program >= 0 && program < prog_man.programs.size());

		set_shader(program);

		if (mat->type == material_type::WINDSWAY)
			set_wind_constants();
		else if (mat->type == material_type::WATER)
			set_water_constants();

		bind_vao(mods.global_vertex_buffers[(int)format].main_vao);
		set_show_backfaces(backface);
		set_blend_state(blend);

		bool shader_doesnt_need_the_textures = mat->type == material_type::WATER || pass.type == pass_type::DEPTH;

		if (!shader_doesnt_need_the_textures) {

			if (mat->type == material_type::TWOWAYBLEND) {
				ASSERT(0);
			}
			else {
				SET_OR_USE_FALLBACK(material_texture::DIFFUSE, ALBEDO1_LOC, white_texture);
				SET_OR_USE_FALLBACK(material_texture::ROUGHNESS, ROUGH1_LOC, white_texture);
				SET_OR_USE_FALLBACK(material_texture::AO, AO1_LOC, white_texture);
				SET_OR_USE_FALLBACK(material_texture::METAL, METAL1_LOC, white_texture);
				SET_OR_USE_FALLBACK(material_texture::NORMAL, NORMAL1_LOC, flat_normal_texture);

			}
		}

		// alpha tested materials are a special case
		if (pass.type == pass_type::DEPTH && mat->is_alphatested()) {
			SET_OR_USE_FALLBACK(material_texture::DIFFUSE, ALBEDO1_LOC, white_texture);
		}

		shader().set_int("indirect_material_offset", offset);

		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			(void*)(list.commands.data() + offset),
			count,
			sizeof(gpu::DrawElementsIndirectCommand)
		);

		offset += count;

		stats.draw_calls++;
	}
}

void Renderer::render_level_to_target(Render_Level_Params params)
{
	vs = params.view;

	state_machine.invalidate_all();

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
		Render_Lists* lists = &scene.opaque_list;
		Render_Pass* pass = &scene.opaque;
		if (params.pass == params.SHADOWMAP || params.pass == params.DEPTH) {
			lists = &scene.vis_list;	// FIXME
			pass = &scene.depth;
		}
		else if (params.pass == params.TRANSLUCENT) {
			lists = &scene.transparents_list;
			pass = &scene.transparents;
		}

		execute_render_lists(*lists, *pass);
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


	set_shader(prog.textured3d);
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
		set_shader(prog.textured3d);
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


void draw_skeleton(const Animator* a,float line_len,const mat4& transform)
{
	auto& bones = a->get_global_bonemats();
	auto model = a->get_model();
	for (int index = 0; index < model->bones.size(); index++) {
		vec3 org = transform * bones[index][3];
		Color32 colors[] = { COLOR_RED,COLOR_GREEN,COLOR_BLUE };
		for (int i = 0; i < 3; i++) {
			vec3 dir = mat3(transform) * bones[index][i];
			dir = normalize(dir);
			Debug::add_line(org, org + dir * line_len, colors[i],-1.f,false);
		}

		if (model->bones[index].parent != -1) {
			vec3 parent_org = transform * bones[model->bones[index].parent][3];
			Debug::add_line(org, parent_org, COLOR_PINK,-1.f,false);
		}
	}
}

glm::mat4 Entity::get_world_transform()
{
	mat4 model;
	model = glm::translate(mat4(1), position);
	model = model * glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);
	model = glm::scale(model, vec3(1.f));

	return model;
}

#include "Player.h"
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

draw_call_key Render_Pass::create_sort_key_from_obj(const Render_Object& proxy, Material* material, uint32_t submesh, uint32_t layer)
{
	draw_call_key key;

	ASSERT(proxy.mats);
	key.shader = draw.get_mat_shader(proxy.animator!=nullptr, *proxy.mesh, *material, (type == pass_type::DEPTH), proxy.dither);
	key.blending = (uint64_t)material->blend;
	key.backface = material->backface;
	key.texture = material->material_id;
	key.vao = (uint64_t)proxy.mesh->format;
	key.mesh = proxy.mesh->id;
	key.layer = layer;

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
	Material* material,
	uint32_t submesh,
	uint32_t layer) {
	ASSERT(handle.is_valid() && "null handle");
	ASSERT(material && "null material");
	Pass_Object obj;
	obj.sort_key = create_sort_key_from_obj(proxy, material, submesh, layer);
	obj.render_obj = handle;
	obj.submesh_index = submesh;
	obj.material = material;
	ASSERT(material->gpu_material_mapping != Material::INVALID_MAPPING);
	objects.push_back(obj);
}
#include <iterator>
void Render_Pass::make_batches(Render_Scene& scene)
{
	//if (creations.empty() && deletions.empty())
	//	return;


	const auto& sort_functor = [](const Pass_Object& a, const Pass_Object& b)
	{ 
		auto a64 = a.sort_key.as_uint64();
		auto b64 = b.sort_key.as_uint64();
		return a64 < b64;
	};
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
		if (a.sort_key.layer < a.sort_key.layer) return true;
		else return false;
	};
	const auto& del_functor = [](const Pass_Object& a, const Pass_Object& b)
	{
		if (a.sort_key.as_uint64() < b.sort_key.as_uint64()) return true;
		else if (a.sort_key.as_uint64() == b.sort_key.as_uint64())
			return  a.render_obj.id < b.render_obj.id && a.submesh_index < b.submesh_index;
		else return false;
	};
	//if (!deletions.empty()) {
	//
	//	std::sort(deletions.begin(), deletions.end(), sort_functor);
	//
	//	std::vector<Pass_Object> dest;
	//	dest.reserve(sorted_list.size());
	//
	//	std::set_difference(sorted_list.begin(), sorted_list.end(), deletions.begin(), deletions.end(), std::back_inserter(dest), del_functor);
	//
	//	sorted_list = std::move(dest);
	//	deletions.clear();
	//}

	//if (!creations.empty()) {
	//	std::sort(creations.begin(), creations.end(), merge_functor);
	//	size_t start_index = sorted_list.size();
	//	sorted_list.reserve(sorted_list.size() + creations.size());
	//	for (auto p : creations)
	//		sorted_list.push_back(p);
	//	Pass_Object* start = sorted_list.data();
	//	Pass_Object* mid = sorted_list.data() + start_index;
	//	Pass_Object* end = sorted_list.data() + sorted_list.size();
	//
	//	std::inplace_merge(start, mid, end, merge_functor);
	//
	//	creations.clear();
	//}

	if (type == pass_type::TRANSPARENT)
		std::sort(objects.begin(), objects.end(), sort_functor_transparent);
	else
		std::sort(objects.begin(), objects.end(), merge_functor);

	batches.clear();
	mesh_batches.clear();

	if (objects.empty()) return;

	{
		const auto& functor = [](int first, Pass_Object* po, const Render_Object* rop) -> Mesh_Batch
		{
			Mesh_Batch batch;
			batch.first = first;
			batch.count = 1;
			auto& mats = rop->mats;
			int index = rop->mesh->parts.at(po->submesh_index).material_idx;
			batch.material = po->material;
			batch.shader_index = po->sort_key.shader;
			return batch;
		};

		// build mesh batches first
		Pass_Object* batch_obj = &objects[0];
		const Render_Object* batch_proxy = &scene.get(batch_obj->render_obj);
		Mesh_Batch batch = functor(0, batch_obj, batch_proxy);

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
		}
		mesh_batches.push_back(batch);
	}

	Multidraw_Batch batch;
	batch.first = 0;
	batch.count = 1;
	
	Mesh_Batch* mesh_batch = &mesh_batches[0];
	Pass_Object* batch_obj = &objects[mesh_batch->first];
	const Render_Object* batch_proxy = &scene.get(batch_obj->render_obj);
	bool is_at = mesh_batch->material->alpha_tested;
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

		if (type == pass_type::OPAQUE || type == pass_type::TRANSPARENT) {
			if (same_vao && same_material && same_other_state && same_shader && same_layer)
				batch_this = true;	// can batch with different meshes
			else
				batch_this = false;

		}
		else if (type == pass_type::DEPTH){
			// can batch across texture changes as long as its not alpha tested
			if (same_shader && same_vao && same_other_state && !this_batch->material->alpha_tested)
				batch_this = true;
			else
				batch_this = false;
		}

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
	: opaque(pass_type::OPAQUE),
	transparents(pass_type::TRANSPARENT),
	depth(pass_type::DEPTH)
{

}

void Render_Lists::init(uint32_t drawbufsz, uint32_t instbufsz)
{
	indirect_drawid_buf_size = drawbufsz;
	indirect_instance_buf_size = instbufsz;

	glCreateBuffers(1, &gldrawid_to_submesh_material);
	glCreateBuffers(1, &glinstance_to_instance);
}

extern bool use_32_bit_indicies;
void Render_Scene::build_render_list(Render_Lists& list, Render_Pass& src)
{
	list.commands.clear();
	list.command_count.clear();

	std::vector<uint32_t>& instance_to_instance = list.instance_to_instance;
	std::vector<uint32_t>& draw_to_material = list.draw_to_material;
	draw_to_material.clear();
	instance_to_instance.clear();

	int base_instance = 0;

	for (int i = 0; i < src.batches.size(); i++) {
		Multidraw_Batch& mdb = src.batches[i];


		for (int j = 0; j < mdb.count; j++) {
			Mesh_Batch& meshb = src.mesh_batches[mdb.first + j];
			auto& obj = src.objects[meshb.first];
			Render_Object& proxy = proxy_list.get(obj.render_obj.id).proxy;
			Mesh& mesh = *proxy.mesh;
			auto& part = mesh.parts[obj.submesh_index];
			gpu::DrawElementsIndirectCommand cmd;

			cmd.baseVertex = part.base_vertex + mesh.merged_vert_offset;
			cmd.count = part.element_count;
			cmd.firstIndex = part.element_offset + mesh.merged_index_pointer;
			cmd.firstIndex /= (use_32_bit_indicies) ? 4 : 2;
			cmd.primCount = meshb.count;
			cmd.baseInstance = base_instance;


			list.commands.push_back(cmd);

			for (int k = 0; k < meshb.count; k++) {
				instance_to_instance.push_back(proxy_list.handle_to_obj[src.objects[meshb.first + k].render_obj.id]);
			}

			base_instance += cmd.primCount;

			auto batch_material = meshb.material;
			draw_to_material.push_back(batch_material->gpu_material_mapping);

			draw.stats.tris_drawn += cmd.primCount * cmd.count / 3;
		}

		list.command_count.push_back(mdb.count);
	}

	glNamedBufferData(list.glinstance_to_instance, sizeof(uint32_t) * list.indirect_instance_buf_size, nullptr, GL_DYNAMIC_DRAW);
	glNamedBufferSubData(list.glinstance_to_instance, 0, sizeof(uint32_t) * instance_to_instance.size(), instance_to_instance.data());

	glNamedBufferData(list.gldrawid_to_submesh_material, sizeof(uint32_t) * list.indirect_drawid_buf_size, nullptr, GL_DYNAMIC_DRAW);
	glNamedBufferSubData(list.gldrawid_to_submesh_material, 0, sizeof(uint32_t) * draw_to_material.size(), draw_to_material.data());
}


void Render_Scene::init()
{
	int obj_count = 20'000;
	int mat_count = 500;

	vis_list.init(mat_count,obj_count);
	opaque_list.init(mat_count,obj_count);
	transparents_list.init(mat_count,obj_count);
	shadow_lists.init(mat_count,obj_count);

	glCreateBuffers(1, &gpu_render_material_buffer);
	glCreateBuffers(1, &gpu_render_instance_buffer);
	glCreateBuffers(1, &gpu_skinned_mats_buffer);
}

void Render_Scene::upload_scene_materials()
{
	scene_mats_vec.clear();
	for (auto& mat : mats.materials) {
		//if (mat.second.texture_are_loading_in_memory) {	// this material is being used
			Material& m = mat.second;
			gpu::Material_Data gpumat;
			gpumat.diffuse_tint = glm::vec4(1.f);// m.diffuse_tint;
			gpumat.rough_mult = m.roughness_mult;
			gpumat.metal_mult = m.metalness_mult;
		//	gpumat.rough_remap_x = m.roughness_remap_range.x;
			//gpumat.rough_remap_y = m.roughness_remap_range.y;

			m.gpu_material_mapping = scene_mats_vec.size();

			scene_mats_vec.push_back(gpumat);
		//}
		//else {
		//	mat.second.gpu_material_mapping = -1;
		//}
	}

	glNamedBufferData(gpu_render_material_buffer, sizeof(gpu::Material_Data) * scene_mats_vec.size(), scene_mats_vec.data(), GL_DYNAMIC_DRAW);
}

glm::vec4 to_vec4(Color32 color) {
	return glm::vec4(color.r, color.g, color.b, color.a) / 255.f;
}

#include <future>
#include <thread>

void Render_Scene::build_scene_data()
{
	CPUFUNCTIONSTART;

	// upload materials, FIXME: cache this
	upload_scene_materials();

	transparents.clear();
	depth.clear();
	opaque.clear();

	// add draw calls and sort them
	gpu_objects.resize(proxy_list.objects.size());
	skinned_matricies_vec.clear();
	{
		CPUSCOPESTART(traversal);

		for (int i = 0; i < proxy_list.objects.size(); i++) {
			auto& obj = proxy_list.objects[i];
			handle<Render_Object> objhandle{obj.handle};
			auto& proxy = obj.type_.proxy;
			if (proxy.visible) {
				auto& mesh = *proxy.mesh;
				for (int j = 0; j < mesh.parts.size(); j++) {
					auto& part = mesh.parts[j];
					Material* mat = (*proxy.mats)[part.material_idx];
					if (mat->is_translucent())
						transparents.add_object(proxy, objhandle, mat, j, 0);
					else {
						depth.add_object(proxy, objhandle, mat, j, 0);
						opaque.add_object(proxy, objhandle, mat, j, 0);
					}
					if (proxy.color_overlay) {
						transparents.add_object(proxy, objhandle, mats.unlit, j, 1);
					}
				}

				if (proxy.animator) {

					if (g_debug_skeletons.integer()) {
						draw_skeleton(proxy.animator, 0.05, proxy.transform);
					}


					gpu_objects[i].anim_mat_offset = skinned_matricies_vec.size();
					auto& mats = proxy.animator->get_matrix_palette();

					auto model = proxy.animator->get_model();

					for (int i = 0; i < model->bones.size(); i++) {
						skinned_matricies_vec.push_back(mats[i]);
					}
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
			}
		}
		glNamedBufferData(gpu_render_instance_buffer, sizeof(gpu::Object_Instance) * gpu_objects.size(), gpu_objects.data(), GL_DYNAMIC_DRAW);
		glNamedBufferData(gpu_skinned_mats_buffer, sizeof(glm::mat4) * skinned_matricies_vec.size(), skinned_matricies_vec.data(), GL_DYNAMIC_DRAW);
	}

	{
		CPUSCOPESTART(make_batches);

		auto transtask = std::async(std::launch::async, [&]() {
			transparents.make_batches(*this);
			});
		auto opaquetask = std::async(std::launch::async, [&]() {
			opaque.make_batches(*this);
			});
		auto depthtask = std::async(std::launch::async, [&]() {
			depth.make_batches(*this);
			});

			//transparents.make_batches(*this);
			//opaque.make_batches(*this);
			//depth.make_batches(*this);
		transtask.wait();
		opaquetask.wait();
		depthtask.wait();
	}
	{
		CPUSCOPESTART(make_render_lists);
		// build draw calls
		build_render_list(vis_list, depth);
		build_render_list(shadow_lists, depth);
		build_render_list(opaque_list, opaque);
		build_render_list(transparents_list, transparents);
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
	Material* material;
	int vert_fmt;
	int start;
	int end;
};

struct High_Level_Render_Object
{
	Mesh* mesh;
	Material* material;
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

glm::vec4 normalize_plane(glm::vec4 p)
{
	return p / glm::length(glm::vec3(p));
}

glm::vec4 bounds_to_sphere(Bounds b)
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

void draw_debug_shapes()
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
		shapes[i].lifetime -= eng->frame_time;
		if (shapes[i].lifetime <= 0.f) {
			shapes.erase(shapes.begin() + i);
			i--;
		}
	}
	builder.Free();
}

extern Auto_Config_Var g_draw_grid;

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

void Renderer::scene_draw(View_Setup view, IEditorTool* tool)
{
	GPUFUNCTIONSTART;

	stats = Render_Stats();

	state_machine.invalidate_all();

	const bool needs_composite = eng->is_drawing_to_window_viewport();

	if (cur_w != view.width || cur_h != view.height)
		InitFramebuffers(true, view.width, view.height);
	lastframe_vs = current_frame_main_view;

	current_frame_main_view = view;
	
	if (enable_vsync.integer())
		SDL_GL_SetSwapInterval(1);
	else
		SDL_GL_SetSwapInterval(0);

	vs = current_frame_main_view;
	upload_ubo_view_constants(ubo.current_frame);
	active_constants_ubo = ubo.current_frame;

	//if (editor_mode)
	//	g_editor_doc->scene_draw_callback();

	scene.build_scene_data();

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
		params.clear_framebuffer = true;
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
		glDepthFunc(GL_LEQUAL);
		render_level_to_target(params);
		glDepthMask(GL_TRUE);
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, fbo.scene);
	//multidraw_testing();

	DrawEntBlobShadows();

	set_shader(prog.simple);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.f));

	draw_debug_shapes();

	if (tool)
		tool->overlay_draw();

	if (g_draw_grid.integer())
		draw_debug_grid();

	glCheckError();
	
	// Bloom update
	render_bloom_chain();

	int x = vs.width;
	int y = vs.height;


	uint32_t framebuffer_to_output = (needs_composite) ? fbo.composite : 0;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_to_output);
	glViewport(0, 0, cur_w, cur_h);

	set_shader(prog.combine);
	uint32_t bloom_tex = tex.bloom_chain[0];
	if (!enable_bloom.integer()) bloom_tex = black_texture.gl_id;
	bind_texture(0, tex.scene_color);
	bind_texture(1, bloom_tex);
	bind_texture(2, lens_dirt->gl_id);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	set_shader(prog.simple);
	shader().set_mat4("ViewProj", vs.viewproj);
	shader().set_mat4("Model", mat4(1.f));

	if (draw_collision_tris.integer())
		DrawCollisionWorld(eng->level);

	if(!tool)
		ui_render();


	//cubemap_positions_debug();

	glCheckError();
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

	for (auto ei = Ent_Iterator(); !ei.finished(); ei = ei.next())
	{

		RayHit rh;
		Ray r;
		r.pos = ei.get().position + glm::vec3(0, 0.1f, 0);
		r.dir = glm::vec3(0, -1, 0);
		rh = eng->phys.trace_ray(r, ei.get_index(), PF_WORLD);

		if (rh.dist < 0)
			continue;

		AddBlobShadow(rh.pos + vec3(0, 0.05, 0), rh.normal, CHAR_HITBOX_RADIUS * 4.5f);
	}
	glCheckError();

	shadowverts.End();
	glCheckError();

	set_shader(prog.particle_basic);
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

program_handle Renderer::get_mat_shader(bool has_animated_matricies, const Mesh& mesh, const Material& mat, bool depth_pass, bool dither)
{
	bool is_alpha_test = mat.alpha_tested;
	bool is_lightmapped = mesh.has_lightmap_coords();
	bool has_colors = mesh.has_colors();
	material_type shader_type = mat.type;
	bool is_normal_mapped = mesh.has_tangents();
	bool is_animated = mesh.has_bones() && has_animated_matricies;

	shader_key key;
	key.alpha_tested = is_alpha_test;
	key.vertex_colors = has_colors;
	key.shader_type = (uint32_t)shader_type;
	key.normal_mapped = is_normal_mapped;
	key.animated = is_animated;
	key.depth_only = depth_pass;
	key.dither = dither;
	
	key = get_real_shader_key_from_shader_type(key);
	program_handle handle = mat_table.lookup(key);
	if (handle != -1) return handle;
	return compile_mat_shader(key);	// dynamic compilation ...
}
#undef SET_OR_USE_FALLBACK
#define SET_OR_USE_FALLBACK(texture, where, fallback) \
if(gs->images[(int)texture]) bind_texture(where, gs->images[(int)texture]->gl_id); \
else bind_texture(where, fallback.gl_id);

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

handle<Render_Object> Renderer::register_obj()
{
	return scene.register_renderable();
}

void Renderer::update_obj(handle<Render_Object> handle, const Render_Object& proxy)
{
	scene.update(handle, proxy);
}

void Renderer::remove_obj(handle<Render_Object>& handle)
{
	scene.remove(handle);
	handle.id = -1;
}

void Renderer::on_level_end()
{


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

handle<Render_Object> Render_Scene::register_renderable() {
	handle<Render_Object> handle{ proxy_list.make_new() };
	return handle;
}

void Render_Scene::update(handle<Render_Object> handle, const Render_Object& proxy)
{
	ROP_Internal& in = proxy_list.get(handle.id);
	in.proxy = proxy;
	if (!proxy.viewmodel_layer) 
		in.inv_transform = glm::inverse(proxy.transform);
}

void Render_Scene::remove(handle<Render_Object> handle) {
	if (handle.id != -1) {
		proxy_list.free(handle.id);
	}
}
