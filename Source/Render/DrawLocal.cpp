#include "DrawLocal.h"
#include "Framework/Util.h"
#include "glad/glad.h"
#include "Render/Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include <SDL2/SDL.h>

#include "UI/GUISystemPublic.h"	// for GuiSystemPublic::paint
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h"	// FIXME
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "tracy/public/tracy/Tracy.hpp"
#include "tracy/public/tracy/TracyOpenGL.hpp"
#include "Framework/ArenaAllocator.h"
#include "IGraphsDevice.h"

const GLenum MODEL_INDEX_TYPE_GL = GL_UNSIGNED_SHORT;

//#pragma optimize("", off)

extern ConfigVar g_window_w;
extern ConfigVar g_window_h;
extern ConfigVar g_window_fullscreen;

Renderer draw;
RendererPublic* idraw = &draw;

// HOLY CONFIG VARS
ConfigVar enable_vsync("r.enable_vsync","1",CVAR_BOOL,"enable/disable vsync");
ConfigVar shadow_quality_setting("r.shadow_setting","0",CVAR_INTEGER,"csm shadow quality",0,3);
ConfigVar enable_bloom("r.bloom","1",CVAR_BOOL,"enable/disable bloom");
ConfigVar enable_volumetric_fog("r.vol_fog","0",CVAR_BOOL,"enable/disable volumetric fog");
ConfigVar enable_ssao("r.ssao","1",CVAR_BOOL,"enable/disable screen space ambient occlusion");
ConfigVar use_halfres_reflections("r.halfres_reflections","1",CVAR_BOOL,"");
ConfigVar dont_use_mdi("r.dont_use_mdi", "0", CVAR_BOOL|CVAR_DEV,"disable multidrawindirect and use drawelements instead");
// 12mb arena
ConfigVar renderer_memory_arena_size("r.mem_arena_size", "12000000", CVAR_INTEGER | CVAR_UNBOUNDED, "size of the renderers memory arena in bytes");

ConfigVar r_taa_enabled("r.taa", "1", CVAR_BOOL,"enable temporal anti aliasing");

static const int MAX_TAA_SAMPLES = 16;
ConfigVar r_taa_samples("r.taa_samples", "4", CVAR_INTEGER, "", 2, MAX_TAA_SAMPLES);
ConfigVar r_taa_32f("r.taa_32f", "0", CVAR_BOOL, "use 32 bit scene motion buffer instead of 16 bit");

// basically:
// diffuse_ao = pow(ao, ssao.intensity)
// specular_ao = pow(diffuse_ao,r_specular_ao_intensity)

ConfigVar r_specular_ao_intensity("r.specular_ao_intensity", "2", CVAR_FLOAT | CVAR_UNBOUNDED, "");
ConfigVar r_debug_skip_build_scene_data("r.debug_skip_build_scene_data", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_skip_gbuffer("r_skip_gbuffer", "0", CVAR_BOOL, "");


ConfigVar force_render_cubemaps("r.force_cubemap_render", "0", CVAR_BOOL | CVAR_DEV, "force cubemaps to re-render, treated like a flag and not a setting");

ConfigVar r_drawterrain("r.drawterrain", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of terrain");
ConfigVar r_force_hide_ui("r.force_hide_ui", "0", CVAR_BOOL, "disable ui drawing");
ConfigVar test_thumbnail_model("test_thumbnail_model", "", CVAR_DEV, "");

ConfigVar r_drawdecals("r.drawdecals", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of decals");

ConfigVar thumbnail_fov("thumbnail_fov", "60", CVAR_FLOAT | CVAR_UNBOUNDED, "");

ConfigVar debug_sun_shadow("r.debug_csm", "0", CVAR_BOOL | CVAR_DEV, "debug csm shadow rendering");
ConfigVar debug_specular_reflection("r.debug_specular", "0", CVAR_BOOL | CVAR_DEV, "debug specular lighting");
ConfigVar r_no_indirect("r.no_indirect", "0", CVAR_BOOL, "");

ConfigVar r_no_meshbuilders("r_no_meshbuilders", "0", CVAR_BOOL | CVAR_DEV, "");
// 128 bones * 100 characters * 2 (double bffer) =  
ConfigVar r_skinned_mats_bone_buffer_size("r.skinned_mats_bone_buffer_size", "25600", CVAR_INTEGER | CVAR_UNBOUNDED | CVAR_READONLY, "");

ConfigVar r_better_depth_batching("r.better_depth_batching", "1", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_no_batching("r.no_batching", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar r_ignore_depth_shader("r_ignore_depth_shader", "0", CVAR_BOOL | CVAR_DEV, "");

ConfigVar enable_gl_debug_output("enable_gl_debug_output", "1", CVAR_BOOL, "");
ConfigVar r_taa_jitter_test("r.taa_jitter_test", "0", CVAR_INTEGER, "", 0, 4);
ConfigVar r_normal_shaded_debug("r.normal_shaded_debug", "1", CVAR_BOOL, "");
ConfigVar log_shader_compiles("log_shader_compiles", "1", CVAR_BOOL, "");

ConfigVar r_debug_mode("r.debug_mode", "0", CVAR_INTEGER | CVAR_DEV, "render debug mode, see Draw.cpp for DEBUG_x values, 0 to disable", 0, 200);

ConfigVar r_drawfog("r.drawfog", "1", CVAR_BOOL | CVAR_DEV, "enable/disable drawing of fog");

RenderWindowBackend* RenderWindowBackend::inst = nullptr;
class RenderWindowBackendLocal : public RenderWindowBackend
{
public:
	int id_counter = 1;

	std::vector<UIDrawCmd> drawCmds;

	handle<RenderWindow> register_window() {
		return { 1 };
	}
	void update_window(handle<RenderWindow> handle, RenderWindow& data) final {
		assert(handle.id == 1);
		//return;
		drawCmds = data.get_draw_cmds();
		mb_draw_data.init_from(data.meshbuilder);
		this->view_proj = data.view_mat;
	}
	virtual void remove_window(handle<RenderWindow> handle) final {
		assert(handle.id == 1);
	}

	void render() {
		//return;
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, draw.ubo.current_frame);
		auto& device = draw.get_device();
		for (int i = 0; i < drawCmds.size(); i++) {
			const auto& dcmd = drawCmds.at(i);
			if (dcmd.type == UIDrawCmd::Type::SetScissor) {
				if (dcmd.sc.enable) {
					glEnable(GL_SCISSOR_TEST);
					auto& r = dcmd.sc.rect;
					glScissor(r.x, r.y, r.w, r.h);
				}
				else {
					glDisable(GL_SCISSOR_TEST);
				}
				continue;
			}
			const UIDrawCall& dc = dcmd.dc;

			if (!dc.mat)
				continue;

			assert(dc.mat->get_master_material()->usage == MaterialUsage::UI);

			const GLenum mode = GL_TRIANGLES;

			RenderPipelineState pipe;
			pipe.backface_culling = true;
			pipe.blend = dc.mat->get_master_material()->blend;
			pipe.cull_front_face = false;
			pipe.depth_testing = false;
			pipe.depth_writes = false;
			pipe.program = matman.get_mat_shader(nullptr,dc.mat,0);
			pipe.vao = mb_draw_data.VAO;
			
			device.set_pipeline(pipe);

			draw.shader().set_mat4("UIViewProj",view_proj);

			auto& texs = dc.mat->impl->get_textures();
			for (int i = 0; i < texs.size(); i++)
				device.bind_texture(i, texs[i]->gl_id);
			if (dc.texOverride != nullptr)
				device.bind_texture(0, dc.texOverride->gl_id);

			glDrawElementsBaseVertex(mode, dc.index_count, GL_UNSIGNED_INT, (void*)(dc.index_start * sizeof(int)), 0);
			
			draw.stats.total_draw_calls++;
		}

		glDisable(GL_SCISSOR_TEST);

		device.reset_states();
	}
private:
	glm::mat4 view_proj{};
	MeshBuilderDD mb_draw_data;
};



class TaaManager
{
public:

	TaaManager() {
		generateHaltonSequence(MAX_TAA_SAMPLES, jitters);
	}
	

	void start_frame() {
		index = (index + 1) % r_taa_samples.get_integer();
	}
	glm::vec2 get_last_frame_jitter(int w, int h) const {
		int previndex = index - 1;
		if (previndex < 0) 
			previndex = r_taa_samples.get_integer() - 1;
		return calc_jitter(previndex, w, h);
	}
	glm::vec2 calc_frame_jitter(int width, int height) const {
		return calc_jitter(index, width, height);
	}
	glm::mat4 add_jitter_to_projection(const glm::mat4& inproj, glm::vec2 jitter) const {
		glm::mat4 matrix = inproj;
		matrix[2][0] += jitter.x;
		matrix[2][1] += jitter.y;

		return matrix;
	}

private:
	glm::vec2 calc_jitter(int the_index, int width, int height) const {
		auto jit = jitters[the_index];	// [0,1]
		jit = jit - glm::vec2(0.5);	//[-1/2,1/2]
		return glm::vec2(jit.x / width, jit.y / height);
	}

	static float radicalInverse(int base, int index) {
		float result = 0.0;
		float fraction = 1.0 / base;
		while (index > 0) {
			result += (index % base) * fraction;
			index /= base;
			fraction /= base;
		}
		return result;
	}
	static void generateHaltonSequence(int numPoints, glm::vec2* sequence) {
		const int primes[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47 };
		const int dimension = 2;
		for (int d = 0; d < dimension; ++d) {
			int base = primes[d];
			for (int i = 0; i < numPoints; ++i) {
				sequence[i][d] = radicalInverse(base, i + 1);
			}
		}
	}

	glm::vec2 jitters[MAX_TAA_SAMPLES];
	int index = 0;
};
static TaaManager r_taa_manager;



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
const uint DEBUG_LIGHTMAP_UV = 10;
*/
//
// special:
static const int DEBUG_OUTLINED = 100;//uses objID

void Renderer::InitGlState()
{
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glClearColor(0.5f, 0.3f, 0.2f, 1.f);
	glDepthFunc(GL_LEQUAL);

	// Fix opengl's clip space
	// now outputs from 0,1 instead of -1,1
	glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	// init clear depth to 0
	// for reverse Z, it clears to 1, that gets set in the scene drawer
	glClearDepth(0.0);
}



void Renderer::bind_texture(int bind, int id)
{
	device.bind_texture(bind, id);
}

static int combine_flags_type(int flags, int type, int flag_bits)
{
	return flags + (type >> flag_bits);
}

program_handle Program_Manager::create_single_file(const std::string& shared_file, bool is_tesseltion, const std::string& defines)
{
	program_def def;
	def.vert = shared_file;
	def.frag = "";
	def.defines = defines;
	def.is_compute = false;
	def.is_tesselation = is_tesseltion;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
program_handle Program_Manager::create_raster(const std::string& vert, const std::string& frag, const std::string& defines)
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
program_handle Program_Manager::create_raster_geo(const std::string& vert, const std::string& frag, const std::string& geo, const std::string& defines)
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
program_handle Program_Manager::create_compute(const std::string& compute, const std::string& defines)
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
#include "Framework/StringUtils.h"
#include "Framework/BinaryReadWrite.h"
//
string compute_hash_for_program_def(Program_Manager::program_def& def)
{
	string inp = def.vert + def.frag+def.geo+def.defines;
	return StringUtils::alphanumeric_hash(inp);
}



void Program_Manager::recompile(program_def& def) {
	double start = GetTime();
	recompile_do(def);
	float time = GetTime() - start;
	if(log_shader_compiles.get_bool())
		sys_print(Debug, "Program_Manager::recompile: compiled/loaded %s in %f\n", def.vert.c_str(), time);
}
void Program_Manager::recompile_shared(program_def& def)
{
	string hashed_path = compute_hash_for_program_def(def) + ".bin";
	auto binFile = FileSys::open_read(hashed_path.c_str(), FileSys::SHADER_CACHE);
	auto shaderFile = FileSys::open_read_engine(def.vert.c_str());
	if (shaderFile && binFile) {
		if (shaderFile->get_timestamp() <= binFile->get_timestamp()) {
			if (log_shader_compiles.get_bool())
				sys_print(Debug, "Program_Manager::recompile: loading cached binary: %s\n", hashed_path.data());

			// load cached binary
			BinaryReader reader(binFile.get());
			auto sourceType = reader.read_int32();
			auto len = reader.read_int32();
			vector<uint8_t> bytes(len, 0);
			reader.read_bytes_ptr(bytes.data(), bytes.size());

			if (def.shader_obj.ID != 0) {
				glDeleteProgram(def.shader_obj.ID);
			}
			def.shader_obj.ID = glCreateProgram();
			glProgramBinary(def.shader_obj.ID, sourceType, bytes.data(), bytes.size());
			glValidateProgram(def.shader_obj.ID);

			GLint success = 0;
			glGetProgramiv(def.shader_obj.ID, GL_LINK_STATUS, &success);
			if (success == GL_FALSE) {
				GLint logLength = 0;
				glGetProgramiv(def.shader_obj.ID, GL_INFO_LOG_LENGTH, &logLength);
				std::vector<GLchar> log(logLength);
				glGetProgramInfoLog(def.shader_obj.ID, logLength, nullptr, log.data());
				sys_print(Error, "Program_Manager::recompile: loading binary failed: %s\n", log.data());
			}
			else {
				return;	// done
			}
		}
	}
	binFile.reset();

	// fail path
	def.compile_failed = Shader::compile_vert_frag_single_file(&def.shader_obj, def.vert, def.defines) != ShaderResult::SHADER_SUCCESS;

	if (!def.compile_failed) {
		const auto program = def.shader_obj.ID;
		GLint length = 0;
		glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &length);
		if (log_shader_compiles.get_bool())
			sys_print(Debug, "Program_Manager::recompile: saving cached binary: %s\n", hashed_path.data());
		vector<uint8_t> bytes(length, 0);
		GLenum outType = 0;
		glGetProgramBinary(def.shader_obj.ID, bytes.size(), nullptr, &outType, bytes.data());
		FileWriter writer(bytes.size() + 8);
		writer.write_int32(outType);
		writer.write_int32(bytes.size());
		writer.write_bytes_ptr(bytes.data(), bytes.size());
		auto outFile = FileSys::open_write(hashed_path.c_str(), FileSys::SHADER_CACHE);
		if (outFile) {
			outFile->write(writer.get_buffer(), writer.get_size());
		}
		else {
			sys_print(Error, "Program_Manager::recompile: couldnt open file to write program binary: %s\n", hashed_path.data());
		}
	}
}

void Program_Manager::recompile_do(program_def& def)
{
	// look in shader cache, only for "shared shaders" now, these are the main materials so whatev
	if(def.is_shared() && !def.is_tesselation)
	{
		//if (!def.program) {
		//	CreateProgramArgs args;
		//	args.file_name = def.vert;
		//	args.defines = def.defines;
		//	def.program = IGraphicsDevice::inst->create_program(args);
		//	def.shader_obj.ID = def.program->get_internal_handle();
		//	def.compile_failed = false;
		//}


		recompile_shared(def);
		return;
	}

	if (def.is_compute) {
		def.compile_failed = Shader::compute_compile(&def.shader_obj, def.vert, def.defines) 
			!= ShaderResult::SHADER_SUCCESS;
	}
	else if (def.is_shared()) {
		assert(def.is_tesselation);
		def.compile_failed = Shader::compile_vert_frag_tess_single_file(&def.shader_obj, def.vert, def.defines) != ShaderResult::SHADER_SUCCESS;
	}
	else {
		if (!def.geo.empty())
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
	device.set_vao(vao);
}

void Renderer::set_blend_state(blend_state blend)
{
	device.set_blend_state(blend);
}
void Renderer::set_show_backfaces(bool show_backfaces)
{
	device.set_show_backfaces(show_backfaces);
}

void Renderer::set_shader(program_handle handle)
{
	device.set_shader(handle);
}


void OpenglRenderDevice::bind_texture(int bind, int id)
{
	ASSERT(bind >= 0 && bind < MAX_SAMPLER_BINDINGS);
	bool invalid = is_bit_invalid(TEXTURE0_BIT + bind);
	if (invalid || textures_bound[bind] != id) {
		set_bit_valid(TEXTURE0_BIT + bind);
		glBindTextureUnit(bind, id);
		textures_bound[bind] = id;
		activeStats.texture_binds++;
	}
}

void OpenglRenderDevice::set_vao(vertexarrayhandle vao)
{
	bool invalid = is_bit_invalid(VAO_BIT);
	if (invalid || vao != current_vao) {
		set_bit_valid(VAO_BIT);
		current_vao = vao;
		glBindVertexArray(vao);
		activeStats.vertex_array_changes++;
	}
}

void OpenglRenderDevice::set_blend_state(blend_state blend)
{
	bool invalid = is_bit_invalid(BLENDING_BIT);
	if (invalid || blend != blending) {
		if (blend == blend_state::OPAQUE)
			glDisable(GL_BLEND);
		else if (blend == blend_state::ADD) {
			if (invalid || blending == blend_state::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
		}
		else if (blend == blend_state::BLEND) {
			if (invalid || blending == blend_state::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else if (blend == blend_state::MULT) {
			if (invalid || blending == blend_state::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_DST_COLOR, GL_ZERO);
		}
		else if (blend == blend_state::SCREEN) {
			if (invalid || blending == blend_state::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		}
		else if (blend == blend_state::PREMULT_BLEND) {
			if (invalid || blending == blend_state::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}
		blending = blend;
		set_bit_valid(BLENDING_BIT);
		activeStats.blend_changes++;
	}
}
void OpenglRenderDevice::set_show_backfaces(bool show_backfaces)
{
	bool invalid = is_bit_invalid(BACKFACE_BIT);
	if (invalid || show_backfaces != this->show_backface) {
		if (show_backfaces)
			glDisable(GL_CULL_FACE);
		else
			glEnable(GL_CULL_FACE);
		set_bit_valid(BACKFACE_BIT);
		this->show_backface = show_backfaces;
	}
}
void OpenglRenderDevice::set_depth_test_enabled(bool enabled)
{
	bool invalid = is_bit_invalid(DEPTHTEST_BIT);
	if (invalid || enabled != this->depth_test_enabled) {
		if (enabled)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		set_bit_valid(DEPTHTEST_BIT);
		this->depth_test_enabled = enabled;
	}
}
void OpenglRenderDevice::set_depth_write_enabled(bool enabled)
{
	bool invalid = is_bit_invalid(DEPTHWRITE_BIT);
	if (invalid || enabled != this->depth_write_enabled) {
		if (enabled)
			glDepthMask(GL_TRUE);
		else
			glDepthMask(GL_FALSE);
		set_bit_valid(DEPTHWRITE_BIT);
		this->depth_write_enabled = enabled;
	}
}
void OpenglRenderDevice::set_cull_front_face(bool enabled)
{
	bool invalid = is_bit_invalid(CULLFRONTFACE_BIT);
	if (invalid || enabled != this->cullfrontface) {
		if (enabled)
			glCullFace(GL_FRONT);
		else
			glCullFace(GL_BACK);
		set_bit_valid(CULLFRONTFACE_BIT);
		this->cullfrontface = enabled;
	}
}


void OpenglRenderDevice::set_shader(program_handle handle)
{
	if (handle == -1) {
		active_program = handle;
		glUseProgram(0);
	}
	bool invalid = is_bit_invalid(PROGRAM_BIT);
	if (invalid || handle != active_program) {
		set_bit_valid(PROGRAM_BIT);
		active_program = handle;
		prog_man.get_obj(handle).use();
		activeStats.program_changes++;
	}
}
GpuRenderPassScope OpenglRenderDevice::start_render_pass(const RenderPassSetup& setup)
{
	glBindFramebuffer(GL_FRAMEBUFFER, setup.framebuffer);
	glViewport(setup.x, setup.y, setup.w, setup.h);
	if (setup.clear_depth || setup.clear_color) {
		glClearDepth(setup.clear_depth_value);
		glClearColor(0, 0.5, 0, 1);
		GLbitfield mask{};
		if (setup.clear_depth)
			mask |= GL_DEPTH_BUFFER_BIT;
		if (setup.clear_color)
			mask |= GL_COLOR_BUFFER_BIT;

		set_depth_write_enabled(true);	// ugh: glDepthMask applies to glClear also

		glClear(mask);
		activeStats.framebuffer_clears++;
	}
	activeStats.framebuffer_changes++;

	return GpuRenderPassScope(setup);
}
void OpenglRenderDevice::clear_framebuffer(bool clear_depth, bool clear_color, float depth_value)
{
	if (clear_depth || clear_color) {
		glClearDepth(depth_value);
		glClearColor(0, 0, 0, 1);
		GLbitfield mask{};
		if (clear_depth)
			mask |= GL_DEPTH_BUFFER_BIT;
		if (clear_color)
			mask |= GL_COLOR_BUFFER_BIT;

		set_depth_write_enabled(true);	// ugh: glDepthMask applies to glClear also

		glClear(mask);
		activeStats.framebuffer_clears++;
	}
}
void OpenglRenderDevice::set_depth_less_than(bool less_than)
{
	bool invalid = is_bit_invalid(DEPTHLESS_THAN_BIT);
	if (invalid || less_than != this->depth_less_than_enabled) {
		if (less_than)
			glDepthFunc(GL_LEQUAL);
		else
			glDepthFunc(GL_GEQUAL);
		set_bit_valid(DEPTHLESS_THAN_BIT);
		this->depth_less_than_enabled = less_than;
	}
}

void OpenglRenderDevice::set_pipeline(const RenderPipelineState& s)
{
	set_shader(s.program);
	set_blend_state(s.blend);
	set_vao(s.vao);
	set_cull_front_face(s.cull_front_face);
	set_depth_test_enabled(s.depth_testing);
	set_show_backfaces(!s.backface_culling);
	set_depth_write_enabled(s.depth_writes);
	set_depth_less_than(s.depth_less_than);
}

void Renderer::create_shaders()
{
	ssao.reload_shaders();
	
	auto& prog_man = get_prog_man();

	prog.simple = prog_man.create_raster("MbSimpleV.txt", "MbSimpleF.txt");
	prog.simple_solid_color = prog_man.create_raster("MbSimpleV.txt", "MbSimpleF.txt", "USE_SOLID_COLOR");


	prog.tex_debug_2d = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_2D_VERSION");
	prog.tex_debug_2d_array = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_2D_ARRAY_VERSION");
	prog.tex_debug_cubemap = prog_man.create_raster("MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_CUBEMAP_VERSION");
	// Bloom shaders
	prog.bloom_downsample = prog_man.create_raster("fullscreenquad.txt", "BloomDownsampleF.txt");
	prog.bloom_upsample = prog_man.create_raster("fullscreenquad.txt", "BloomUpsampleF.txt");
	prog.combine = prog_man.create_raster("fullscreenquad.txt", "CombineF.txt");
	prog.taa_resolve = prog_man.create_raster("fullscreenquad.txt", "TaaResolveF.txt");


	prog.mdi_testing = prog_man.create_raster("SimpleMeshV.txt", "UnlitF.txt", "MDI");

	prog.light_accumulation = prog_man.create_raster("LightAccumulationV.txt", "LightAccumulationF.txt");
	prog.light_accumulation_shadowed = prog_man.create_raster("LightAccumulationV.txt", "LightAccumulationF.txt","SHADOWED");
	prog.light_accumulation_shadow_cookie = prog_man.create_raster("LightAccumulationV.txt", "LightAccumulationF.txt", "SHADOWED,COOKIE");

	prog.sunlight_accumulation = prog_man.create_raster("fullscreenquad.txt", "SunLightAccumulationF.txt");
	prog.sunlight_accumulation_debug = prog_man.create_raster("fullscreenquad.txt", "SunLightAccumulationF.txt","DEBUG");

	prog.ambient_accumulation = prog_man.create_raster("fullscreenquad.txt", "AmbientLightingF.txt");
	prog.reflection_accumulation = prog_man.create_raster("fullscreenquad.txt", "SampleCubemapsF.txt");

	prog.height_fog = prog_man.create_raster("fullscreenquad.txt", "HeightFogF.txt");
	//prog_man.create_single_file()
	// volumetric fog shaders
	Shader::compute_compile(&volfog.prog.lightcalc, "VfogScatteringC.txt");
	Shader::compute_compile(&volfog.prog.raymarch, "VfogRaymarchC.txt");
	Shader::compute_compile(&volfog.prog.reproject, "VfogScatteringC.txt", "REPROJECTION");
	volfog.prog.lightcalc.use();
	volfog.prog.lightcalc.set_int("previous_volume", 0);
	volfog.prog.lightcalc.set_int("perlin_noise", 1);

	glUseProgram(0);
}

void Renderer::reload_shaders()
{
	assert(0);
	on_reload_shaders.invoke();

	ssao.reload_shaders();
	//prog_man.recompile_all();

}


void Renderer::upload_ubo_view_constants(const View_Setup& view_to_use, bufferhandle ubo, bool wireframe_secondpass)
{
	gpu::Ubo_View_Constants_Struct constants;
	auto& vs = view_to_use;
	constants.view = vs.view;
	constants.viewproj = vs.viewproj;
	constants.invview = glm::inverse(vs.view);
	constants.invproj = glm::inverse(vs.proj);
	constants.inv_viewproj = glm::inverse(vs.viewproj);
	constants.viewpos_time = glm::vec4(vs.origin, TimeSinceStart());
	constants.viewfront = glm::vec4(vs.front, 0.0);
	constants.viewport_size = glm::vec4(vs.width, vs.height, 0, 0);
	constants.prev_viewproj = last_frame_main_view.viewproj;
	constants.near = vs.near;
	constants.far = vs.far;
	constants.shadowmap_epsilon = shadowmap.tweak.epsilon;
	constants.inv_scale_by_proj_distance = 1.0 / (2.0 * tan(vs.fov * 0.5));

	constants.fogcolor = vec4(vec3(0.7), 1);
	constants.fogparams = vec4(10, 30, 0, 0);

	constants.numcubemaps = 0;

	constants.forcecubemap = -1.0;

	auto cur_jit = r_taa_manager.calc_frame_jitter(cur_w, cur_h);
	auto prev_jit = r_taa_manager.get_last_frame_jitter(cur_w, cur_h);
	if (r_taa_jitter_test.get_integer() == 1) {
		cur_jit *= -1;
	}
	else if (r_taa_jitter_test.get_integer() == 2) {
		cur_jit *= -1;
		prev_jit *= -1;
	}
	else if (r_taa_jitter_test.get_integer() == 3) {
		prev_jit *= -1;
	}

	constants.current_and_prev_jitter = glm::vec4(cur_jit.x, cur_jit.y, prev_jit.x, prev_jit.y);

	constants.debug_options = r_debug_mode.get_integer();
	if (r_debug_mode.get_integer() == DEBUG_OUTLINED)
		constants.debug_options = gpu::DEBUG_OBJID;

	constants.flags = 0;
	if (wireframe_secondpass)
		constants.flags |= (1 << 0);
	if (r_normal_shaded_debug.get_bool()) {
		constants.flags |= (1 << 1);
	}


	glNamedBufferData(ubo, sizeof(gpu::Ubo_View_Constants_Struct), &constants, GL_DYNAMIC_DRAW);
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
		return "";
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
		return "";
	}();

	auto const severity_str = [severity]() {
		switch (severity) {
		case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
		case GL_DEBUG_SEVERITY_LOW: return "LOW";
		case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
		case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
		}
		return "";
	}();

	sys_print(Error, "%s, %s, %s, %d: %s\n", src_str, type_str, severity_str, id, message);
}

void imgui_stat_hook()
{
	auto& stats = draw.stats;
	ImGui::Text("Draw calls: %d", stats.total_draw_calls);
	ImGui::Text("Total tris: %d", stats.tris_drawn);
	ImGui::Text("Texture binds: %d", stats.texture_binds);
	ImGui::Text("Shader binds: %d", stats.program_changes);
	ImGui::Text("Vao binds: %d", stats.vertex_array_changes);
	ImGui::Text("Blend changes: %d", stats.blend_changes);
	ImGui::Separator();
	ImGui::Text("shadow objs: %d", stats.shadow_objs);
	ImGui::Text("shadow lights: %d", stats.shadow_lights);
	ImGui::Separator();
	auto& scene = draw.scene;
	ImGui::Text("depth batches: %d", (int)scene.shadow_pass.batches.size());
	ImGui::Text("depth mesh batches: %d", (int)scene.shadow_pass.mesh_batches.size());
	ImGui::Text("transparent batches: %d", (int)scene.transparent_pass.batches.size());
	ImGui::Text("opaque batches: %d", (int)scene.gbuffer_pass.batches.size());
	ImGui::Text("opaque mesh batches: %d", (int)scene.gbuffer_pass.mesh_batches.size());
	ImGui::Separator();
	ImGui::Text("total objects: %d", (int)scene.proxy_list.objects.size());
	ImGui::Text("total lights: %d", (int)scene.light_list.objects.size());
	ImGui::Text("total decals: %d", (int)scene.decal_list.objects.size());
	ImGui::Text("total meshbuilders: %d", (int)scene.meshbuilder_objs.objects.size());

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

	sys_print(Debug,"==== Extension support ====\n");
	sys_print(Debug,"-GL_ARB_bindless_texture: %s\n", (supports_bindless) ? "yes" : "no");
	sys_print(Debug,"-GL_ARB_sparse_texture: %s\n", (supports_sprase_tex) ? "yes" : "no");
	sys_print(Debug,"-GL_ARB_texture_filter_minmax: %s\n", (supports_filter_minmax) ? "yes" : "no");
	sys_print(Debug,"-GL_EXT_texture_compression_s3tc: %s\n", (supports_compression) ? "yes" : "no");
	sys_print(Debug,"-GL_NV_shader_atomic_int64: %s\n", (supports_atomic64) ? "yes" : "no");
	sys_print(Debug,"-GL_ARB_gpu_shader_int64: %s\n", (supports_int64) ? "yes" : "no");

	if (!supports_compression) {
		Fatalf("Opengl driver needs GL_EXT_texture_compression_s3tc\n");
	}

	GLint binary_formats;
	glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &binary_formats);
	if (binary_formats == 0) {
		Fatalf("Opengl driver must support program binary. (GL_NUM_PROGRAM_BINARY_FORMATS>0)\n");
	}

	sys_print(Debug,"==== GL Hardware Values ====\n");
	int max_buffer_bindings = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_buffer_bindings);
	sys_print(Debug,"-GL_MAX_UNIFORM_BUFFER_BINDINGS: %d\n", max_buffer_bindings);
	int max_texture_units = 0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
	sys_print(Debug,"-GL_MAX_TEXTURE_IMAGE_UNITS: %d\n", max_texture_units);
	sys_print(Debug, "-GL_NUM_PROGRAM_BINARY_FORMATS: %d\n", binary_formats);

	sys_print(Debug,"\n");
}

void Renderer::create_default_textures()
{
	const uint8_t wdata[] = { 0xff,0xff,0xff };
	const uint8_t bdata[] = { 0x0,0x0,0x0 };
	const uint8_t normaldata[] = { 128,128,255,255 };

	auto create_defeault = [](texhandle* handle, const uint8_t* data) -> void{
		glCreateTextures(GL_TEXTURE_2D, 1, handle);
		glTextureStorage2D(*handle, 1, GL_RGB8, 1, 1);
		glTextureSubImage2D(*handle, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, data);
		glTextureParameteri(*handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(*handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateTextureMipmap(*handle);
	};
	create_defeault(&white_texture.gl_id, wdata);
	create_defeault(&black_texture.gl_id, bdata);
	create_defeault(&flat_normal_texture.gl_id, normaldata);

	auto white_tex = Texture::install_system("_white");
	auto black_tex = Texture::install_system("_black");
#ifdef EDITOR_BUILD
	white_tex->hasSimplifiedColor = true;
	white_tex->simplifiedColor = COLOR_WHITE;
	black_tex->hasSimplifiedColor = true;
	black_tex->simplifiedColor = COLOR_BLACK;
#endif
	auto flat_normal = Texture::install_system("_flat_normal");

	white_tex->update_specs(white_texture.gl_id, 1, 1, 3, {});
	black_tex->update_specs(black_texture.gl_id, 1, 1, 3, {});
	flat_normal->update_specs(flat_normal_texture.gl_id, 1, 1, 3, {});

	// create the "virtual texture system" handles so materials/debuging can reference these like a normal texture
	tex.bloom_vts_handle = Texture::install_system("_bloom_result");
	tex.scene_color_vts_handle = Texture::install_system("_scene_color");
	tex.scene_depth_vts_handle = Texture::install_system("_scene_depth");
	tex.gbuffer0_vts_handle = Texture::install_system("_gbuffer0");
	tex.gbuffer1_vts_handle = Texture::install_system("_gbuffer1");
	tex.gbuffer2_vts_handle = Texture::install_system("_gbuffer2");
	tex.editorid_vts_handle = Texture::install_system("_editorid");
	tex.editorSel_vts_handle = Texture::install_system("_editorSelDepth");
	tex.postProcessInput_vts_handle = Texture::install_system("_PostProcessInput");
	tex.scene_motion_vts_handle = Texture::install_system("_scene_motion");
}

void Renderer::init()
{
	sys_print(Info, "--------- Initializing Renderer ---------\n");

	IGraphicsDevice::inst = IGraphicsDevice::create_opengl_device();


	// Check hardware settings like extension availibility
	check_hardware_options();

	// Enable debug output on debug builds
	if (enable_gl_debug_output.get_bool()) {
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(debug_message_callback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	}

	InitGlState();
	windowDrawer = new RenderWindowBackendLocal();
	RenderWindowBackend::inst = windowDrawer;

	mem_arena.init("RenderTemp", renderer_memory_arena_size.get_integer());
	// Init scene draw buffers
	scene.init();
	create_shaders();
	create_default_textures();
	glCreateBuffers(1, &ubo.current_frame);
	InitFramebuffers(true, g_window_w.get_integer(), g_window_h.get_integer());
	EnviornmentMapHelper::get().init();
	volfog.init();
	shadowmap.init();
	ssao.init();
	lens_dirt = g_assets.find_global_sync<Texture>("eng/lens_dirt_fine.png").get();


	glGenVertexArrays(1, &vao.default_);
	glCreateBuffers(1, &buf.default_vb);
	glNamedBufferStorage(buf.default_vb, 12 * 3, nullptr, 0);
	glBindVertexArray(vao.default_);
	glBindBuffer(GL_ARRAY_BUFFER, buf.default_vb);
	glBindVertexArray(0);



	on_level_start();
	Debug_Interface::get()->add_hook("Render stats", imgui_stat_hook);
	auto brdf_lut = Texture::install_system("_brdf_lut");
	brdf_lut->gl_id = EnviornmentMapHelper::get().integrator.lut_id;
	brdf_lut->width = EnviornmentMapHelper::BRDF_PREINTEGRATE_LUT_SIZE;
	brdf_lut->height = EnviornmentMapHelper::BRDF_PREINTEGRATE_LUT_SIZE;
	brdf_lut->type = Texture_Type::TEXTYPE_2D;
	consoleCommands = ConsoleCmdGroup::create("");
	consoleCommands->add("cot", [this](const Cmd_Args& args) { debug_tex_out.output_tex = nullptr; });
	consoleCommands->add("ot", [this](const Cmd_Args& args) { 
		static const char* usage_str = "Usage: ot <scale:float> <alpha:float> <mip/slice:float> <texture_name>\n";
		if (args.size() != 2) {
			sys_print(Info, usage_str);
			return;
		}
		const char* texture_name = args.at(1);

		debug_tex_out.output_tex = g_assets.find_sync<Texture>(texture_name).get();
		debug_tex_out.scale = 1.f;
		debug_tex_out.alpha = 1.f;
		debug_tex_out.mip = 1.f;


		if (!debug_tex_out.output_tex) {
			sys_print(Error, "output_texture: couldn't find texture %s\n", texture_name);
		}	
		});
	consoleCommands->add("test_mode", [this](const Cmd_Args& args) {
		if (args.size() != 2)return;
		int i = atoi(args.at(1));
		if (i == 0) {
			dont_use_mdi.set_bool(false);
			r_no_batching.set_bool(false);
			r_better_depth_batching.set_bool(true);
			r_debug_skip_build_scene_data.set_bool(false);
		}
		if (i == 1) {
			dont_use_mdi.set_bool(true);
			r_no_batching.set_bool(true);
			r_better_depth_batching.set_bool(true);
			r_debug_skip_build_scene_data.set_bool(false);
		}
		if (i == 2) {
			dont_use_mdi.set_bool(true);
			r_no_batching.set_bool(true);
			r_better_depth_batching.set_bool(true);
			r_debug_skip_build_scene_data.set_bool(true);
		}
		
		});
	consoleCommands->add("otex", [this](const Cmd_Args& args){
			static const char* usage_str = "Usage: otex <scale:float> <alpha:float> <mip/slice:float> <texture_name>\n";
			if (args.size() != 5) {
				sys_print(Info, usage_str);
				return;
			}
			float scale = atof(args.at(1));
			float alpha = atof(args.at(2));
			float mip = atof(args.at(3));
			const char* texture_name = args.at(4);
			debug_tex_out.output_tex = g_assets.find_sync<Texture>(texture_name).get();
			debug_tex_out.scale = scale;
			debug_tex_out.alpha = alpha;
			debug_tex_out.mip = mip;
			if (!debug_tex_out.output_tex) {
				sys_print(Error, "output_texture: couldn't find texture %s\n", texture_name);
			}
		});

	spotShadows = std::make_unique<ShadowMapManager>();

#ifdef EDITOR_BUILD
	thumbnailRenderer = std::make_unique<ThumbnailRenderer>(64);
#endif
}



void Renderer::InitFramebuffers(bool create_composite_texture, int s_w, int s_h)
{
	refresh_render_targets_next_frame = false;
	disable_taa_this_frame = true;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	auto create_and_delete_fb = [](uint32_t & framebuffer) {
		glDeleteFramebuffers(1, &framebuffer);
		glCreateFramebuffers(1, &framebuffer);
	};

	auto delete_and_create_texture = [&](IGraphicsTexture*& ptr, GraphicsTextureFormat format) {
		if (ptr) {
			ptr->release();
		}
		CreateTextureArgs args;
		args.format = format;
		args.num_mip_maps = 1;
		args.width = s_w;
		args.height = s_h;
		args.sampler_type = GraphicsSamplerType::NearestClamped;
		ptr = IGraphicsDevice::inst->create_texture(args);
	};

	using gtf = GraphicsTextureFormat;
	delete_and_create_texture(tex.scene_color, gtf::rgb16f);

	// Main accumulation buffer, 16 bit color
	//create_and_delete_texture(tex.scene_color);
	//glTextureStorage2D(tex.scene_color, 1, GL_RGB16F, s_w, s_h);
	//set_default_parameters(tex.scene_color);

	// last frame, for TAA
	delete_and_create_texture(tex.last_scene_color, gtf::rgb16f);
	//create_and_delete_texture(tex.last_scene_color);
	//glTextureStorage2D(tex.last_scene_color, 1, GL_RGB16F, s_w, s_h);
	//set_default_parameters(tex.last_scene_color);

	// Main scene depth
	delete_and_create_texture(tex.scene_depth, gtf::depth32f);
	//create_and_delete_texture(tex.scene_depth);
	//glTextureStorage2D(tex.scene_depth, 1, GL_DEPTH_COMPONENT32F, s_w, s_h);
	//set_default_parameters(tex.scene_depth);

	// for mouse picking
	delete_and_create_texture(tex.editor_id_buffer, gtf::rgba8);

	//create_and_delete_texture(tex.editor_id_buffer);
	//glTextureStorage2D(tex.editor_id_buffer, 1, GL_RGBA8, s_w, s_h);
	//set_default_parameters(tex.editor_id_buffer);

	// Create forward render framebuffer
	// Transparents and other immediate stuff get rendered to this
	create_and_delete_fb(fbo.forward_render);
	glNamedFramebufferTexture(fbo.forward_render, GL_COLOR_ATTACHMENT0, tex.scene_color->get_internal_handle(), 0);
	glNamedFramebufferTexture(fbo.forward_render, GL_DEPTH_ATTACHMENT, tex.scene_depth->get_internal_handle(), 0);
	//glNamedFramebufferTexture(fbo.forward_render, GL_COLOR_ATTACHMENT4, tex.editor_id_buffer, 0);

	unsigned int attachments[5] = { GL_COLOR_ATTACHMENT0,0,0,0, 0 };
	glNamedFramebufferDrawBuffers(fbo.forward_render, 5, attachments);

	// Editor selection
	delete_and_create_texture(tex.editor_selection_depth_buffer, gtf::depth32f);
	//create_and_delete_texture(tex.editor_selection_depth_buffer);
	//glTextureStorage2D(tex.editor_selection_depth_buffer, 1, GL_DEPTH_COMPONENT32F, s_w, s_h);
	//set_default_parameters(tex.editor_selection_depth_buffer);

	create_and_delete_fb(fbo.editorSelectionDepth);
	glNamedFramebufferTexture(fbo.editorSelectionDepth, GL_DEPTH_ATTACHMENT, tex.editor_selection_depth_buffer->get_internal_handle(), 0);

	
	// Gbuffer textures
	// See the comment above these var's decleration in DrawLocal.h for details
	delete_and_create_texture(tex.scene_gbuffer0, gtf::rgb16f);
	//create_and_delete_texture(tex.scene_gbuffer0);
	//glTextureStorage2D(tex.scene_gbuffer0, 1, GL_RGB16F, s_w, s_h);
	//set_default_parameters(tex.scene_gbuffer0);

	
	delete_and_create_texture(tex.scene_gbuffer1, gtf::rgba8);

	//create_and_delete_texture(tex.scene_gbuffer1);
//	glTextureStorage2D(tex.scene_gbuffer1, 1, GL_RGBA8, s_w, s_h);
	//set_default_parameters(tex.scene_gbuffer1);

	delete_and_create_texture(tex.scene_gbuffer2, gtf::rgba8);

//	create_and_delete_texture(tex.scene_gbuffer2);
//	glTextureStorage2D(tex.scene_gbuffer2, 1, GL_RGBA8, s_w, s_h);
	//set_default_parameters(tex.scene_gbuffer2);


	//const GLenum scene_motion_format = (r_taa_32f.get_bool()) ? GL_RG32F : GL_RG16F;
	const gtf scene_motion_format = (r_taa_32f.get_bool()) ? gtf::rg32f : gtf::rg16f;


	delete_and_create_texture(tex.scene_motion, scene_motion_format);
	//create_and_delete_texture(tex.scene_motion);
	//glTextureStorage2D(tex.scene_motion, 1, scene_motion_format, s_w, s_h);
	//set_default_parameters(tex.scene_motion);

	delete_and_create_texture(tex.last_scene_motion, scene_motion_format);
	//create_and_delete_texture(tex.last_scene_motion);
	//glTextureStorage2D(tex.last_scene_motion, 1, scene_motion_format, s_w, s_h);
	//set_default_parameters(tex.last_scene_motion);


	// Create Gbuffer
	// outputs to 4 render targets: gbuffer 0,1,2 and scene_color for emissives
	create_and_delete_fb(fbo.gbuffer);
	glNamedFramebufferTexture(fbo.gbuffer, GL_DEPTH_ATTACHMENT, tex.scene_depth->get_internal_handle(), 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT0, tex.scene_gbuffer0->get_internal_handle(), 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT1, tex.scene_gbuffer1->get_internal_handle(), 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT2, tex.scene_gbuffer2->get_internal_handle(), 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT3, tex.scene_color->get_internal_handle(), 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT4, tex.editor_id_buffer->get_internal_handle(), 0);
	glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT5, tex.scene_motion->get_internal_handle(), 0);



	const uint32_t gbuffer_attach_count = 6;
	unsigned int gbuffer_attachments[gbuffer_attach_count] = { 
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2,
		GL_COLOR_ATTACHMENT3,
		GL_COLOR_ATTACHMENT4,
		GL_COLOR_ATTACHMENT5,
	};
	glNamedFramebufferDrawBuffers(fbo.gbuffer, gbuffer_attach_count, gbuffer_attachments);

	// Composite textures
	create_and_delete_fb(fbo.composite);
	delete_and_create_texture(tex.output_composite, gtf::rgb8);
	delete_and_create_texture(tex.output_composite_2, gtf::rgb8);


	//create_and_delete_texture(tex.output_composite);
	//create_and_delete_texture(tex.output_composite_2);
	//glTextureStorage2D(tex.output_composite, 1, GL_RGB8, s_w, s_h);
	//glTextureStorage2D(tex.output_composite_2, 1, GL_RGB8, s_w, s_h);
	//set_default_parameters(tex.output_composite);
	//set_default_parameters(tex.output_composite_2);
	glNamedFramebufferTexture(fbo.composite, GL_COLOR_ATTACHMENT0, tex.output_composite->get_internal_handle(), 0);


	// write to scene gbuffer0 for taa resolve
	create_and_delete_fb(fbo.taa_resolve);
	glNamedFramebufferTexture(fbo.taa_resolve, GL_COLOR_ATTACHMENT0, tex.scene_gbuffer0->get_internal_handle(), 0);
	create_and_delete_fb(fbo.taa_blit);

	cur_w = s_w;
	cur_h = s_h;

	// Update vts handles
	tex.scene_color_vts_handle->update_specs_ptr(tex.scene_color, s_w, s_h, 4, {});
	tex.scene_depth_vts_handle->update_specs_ptr(tex.scene_depth, s_w, s_h, 4, {});
	tex.gbuffer0_vts_handle->update_specs_ptr(tex.scene_gbuffer0, s_w, s_h, 3, {});
	tex.gbuffer1_vts_handle->update_specs_ptr(tex.scene_gbuffer1, s_w, s_h, 3, {});
	tex.gbuffer2_vts_handle->update_specs_ptr(tex.scene_gbuffer2, s_w, s_h, 3, {});
	tex.editorid_vts_handle->update_specs_ptr(tex.editor_id_buffer, s_w, s_h, 4, {});
	tex.editorSel_vts_handle->update_specs_ptr(tex.editor_selection_depth_buffer, s_w, s_h, 4, {});
	tex.scene_motion_vts_handle->update_specs_ptr(tex.scene_motion, s_w, s_h, 2, {});

	// Also update bloom buffers (this can be elsewhere)
	init_bloom_buffers();

	// alert any observers that they need to update their buffer sizes (like SSAO, etc.)
	on_viewport_size_changed.invoke(cur_w, cur_h);
}

void Renderer::init_bloom_buffers()
{
	glDeleteFramebuffers(1, &fbo.bloom);
	//if(tex.number_bloom_mips>0)
	//	glDeleteTextures(tex.number_bloom_mips, tex.bloom_chain);
	glCreateFramebuffers(1, &fbo.bloom);
	
	int x = cur_w / 2;
	int y = cur_h / 2;
	tex.number_bloom_mips = glm::min((int)MAX_BLOOM_MIPS, Texture::get_mip_map_count(x, y));
	//glCreateTextures(GL_TEXTURE_2D, tex.number_bloom_mips, tex.bloom_chain);

	float fx = x;
	float fy = y;
	for (int i = 0; i < tex.number_bloom_mips; i++) {
		auto& bc = tex.bloom_chain[i];

		CreateTextureArgs args;
		args.width = x;
		args.height = y;
		args.format = GraphicsTextureFormat::r11f_g11f_b10f;
		args.num_mip_maps = 1;
		args.sampler_type = GraphicsSamplerType::LinearClamped;
		if (bc.texture)
			bc.texture->release();
		bc.texture = IGraphicsDevice::inst->create_texture(args);

		bc.isize = { x,y };
		bc.fsize = { fx,fy };
		//glTextureStorage2D(tex.bloom_chain[i], 1, GL_R11F_G11F_B10F, x, y);
		//glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		//glTextureParameteri(tex.bloom_chain[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		x /= 2;
		y /= 2;
		fx *= 0.5;
		fy *= 0.5;
	}

	tex.bloom_vts_handle->update_specs_ptr(tex.bloom_chain[0].texture, cur_w / 2, cur_h / 2, 3, {});

}

void Renderer::render_bloom_chain(texhandle scene_color_handle)
{
	ZoneScoped;
	GPUFUNCTIONSTART;

	if (!enable_bloom.get_bool())
		return;

	device.reset_states();

	RenderPassSetup setup("bloompass", fbo.bloom, false, false, 0, 0, cur_w, cur_h);
	auto scope = device.start_render_pass(setup);

	///IGraphicsDevice* device = IGraphicsDevice::inst;

	{
		RenderPipelineState state;
		state.vao = get_empty_vao();
		state.program = prog.bloom_downsample;
		device.set_pipeline(state);


		//*set_shader(prog.bloom_downsample);
		float src_x = cur_w;
		float src_y = cur_h;


		glBindTextureUnit(0, scene_color_handle);
		glClearColor(0, 0, 0, 1);
		for (int i = 0; i < tex.number_bloom_mips; i++)
		{
			auto& bc = tex.bloom_chain[i];

			glNamedFramebufferTexture(fbo.bloom, GL_COLOR_ATTACHMENT0,bc.texture->get_internal_handle(), 0);

			shader().set_vec2("srcResolution", vec2(src_x, src_y));
			shader().set_int("mipLevel", i);
			src_x = bc.fsize.x;
			src_y = bc.fsize.y;

			device.set_viewport(0, 0, src_x, src_y);
			device.clear_framebuffer(false, true/* clear color*/);


			glDrawArrays(GL_TRIANGLES, 0, 3);

			glBindTextureUnit(0, bc.texture->get_internal_handle());
		}
	}

	{
		RenderPipelineState state;
		state.vao = get_empty_vao();
		state.program = prog.bloom_upsample;
		state.blend = blend_state::ADD;
		device.set_pipeline(state);


		for (int i = tex.number_bloom_mips - 1; i > 0; i--)
		{
			auto& bc = tex.bloom_chain[i-1];

			glNamedFramebufferTexture(fbo.bloom, GL_COLOR_ATTACHMENT0, bc.texture->get_internal_handle(), 0);

			vec2 destsize =  bc.fsize;
			device.set_viewport(0, 0, destsize.x, destsize.y);

			glBindTextureUnit(0, bc.texture->get_internal_handle());
			shader().set_float("filterRadius", 0.0001f);

			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	}

	device.reset_states();
}

inline void setup_batch(Render_Lists& list,
	Render_Pass& pass,
	bool depth_test_enabled,
	bool force_show_backfaces,
	bool depth_less_than_op, const int i, const int offset) 
{
	const auto& batch = pass.batches[i];
	const auto& mesh_batch = pass.mesh_batches[batch.first];

	const MaterialInstance* mat = (MaterialInstance*)mesh_batch.material;
	const draw_call_key batch_key = pass.objects[mesh_batch.first].sort_key;
	const program_handle program = (program_handle)batch_key.shader;
	const blend_state blend = (blend_state)batch_key.blending;
	const bool show_backface = batch_key.backface;
	const uint32_t layer = batch_key.layer;
	const VaoType vaoType = (VaoType)batch_key.vao;
	IGraphicsVertexInput* vao_ptr = g_modelMgr.get_vao_ptr(vaoType);

	RenderPipelineState state;
	state.program = program;
	state.vao = vao_ptr->get_internal_handle();
	state.backface_culling = !show_backface && !force_show_backfaces;
	state.blend = blend;
	state.depth_testing = depth_test_enabled;
	//state.depth_writes = depth_write_enabled;
	state.depth_writes = !mat->get_master_material()->is_translucent();
	state.depth_less_than = depth_less_than_op;
	draw.get_device().set_pipeline(state);


	draw.shader().set_int("indirect_material_offset", offset);

	auto& textures = mat->impl->get_textures();

	for (int i = 0; i < textures.size(); i++) {
		Texture* t = textures[i];
		uint32_t id = t->gl_id;
		if (t->gpu_ptr) {
			id = t->gpu_ptr->get_internal_handle();
		}
		draw.bind_texture(i, id);
	}
}
inline void setup_execute_render_lists(Render_Lists& list,Render_Pass& pass) {
	auto& scene = draw.scene;
	
	IGraphicsBuffer* material_buffer = matman.get_gpu_material_buffer();

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, scene.gpu_render_instance_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, scene.gpu_skinned_mats_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, material_buffer->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, list.glinstance_to_instance);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, list.gldrawid_to_submesh_material);
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);


	if (scene.has_lightmap && scene.lightmapObj.lightmap_texture) {
		auto texture = scene.lightmapObj.lightmap_texture;
		draw.bind_texture(20/* FIXME, defined to be bound at spot 20,*/, texture->gl_id);
	}
	else {
		draw.bind_texture(20/* FIXME, defined to be bound at spot 20,*/, draw.black_texture.gl_id);
	}

}

void Renderer::execute_render_lists(
	Render_Lists& list, 
	Render_Pass& pass, 
	bool depth_test_enabled,
	bool force_show_backfaces,
	bool depth_less_than_op)
{
	setup_execute_render_lists(list, pass);
	int offset = 0;
	for (int i = 0; i < pass.batches.size(); i++) {
		setup_batch(list, pass, depth_test_enabled, force_show_backfaces, depth_less_than_op, i, offset);
		const int count = list.command_count[i];

		const GLenum index_type = MODEL_INDEX_TYPE_GL;

		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			index_type,
			(void*)(list.commands.data() + offset),
			count,
			sizeof(gpu::DrawElementsIndirectCommand)
		);

		offset += count;

		stats.total_draw_calls++;
	}
}

void Renderer::render_lists_old_way(Render_Lists& list, Render_Pass& pass,
	bool depth_test_enabled,
	bool force_show_backfaces,
	bool depth_less_than_op)
{
	setup_execute_render_lists(list, pass);
	int offset = 0;
	for (int i = 0; i < pass.batches.size(); i++) {
		setup_batch(list, pass, depth_test_enabled, force_show_backfaces, depth_less_than_op, i, offset);

		const int count = list.command_count[i];
		const auto& batch = pass.batches[i];
		const GLenum index_type = MODEL_INDEX_TYPE_GL;
		for (int dc = 0; dc < batch.count; dc++) {
			auto& cmd = list.commands[offset + dc];

			#pragma warning(disable : 4312)	// (void*) casting
			glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES,
				cmd.count,
				index_type,
				(void*)(cmd.firstIndex * MODEL_BUFFER_INDEX_TYPE_SIZE),
				cmd.primCount,
				cmd.baseVertex,
				cmd.baseInstance);
			#pragma warning(default : 4312)

			stats.total_draw_calls++;
		}

		offset += count;
	}
}

void Renderer::render_level_to_target(const Render_Level_Params& params)
{
	ZoneScoped;


	device.reset_states();

	bufferhandle what_ubo = params.provied_constant_buffer;
	{
		bool upload = params.upload_constants;
		if (params.provied_constant_buffer == 0) {
			what_ubo = ubo.current_frame;
			upload = true;
		}
		if (upload)
			upload_ubo_view_constants(params.view, what_ubo, params.wireframe_secondpass);
	}

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, what_ubo);
	

	//*glBindFramebuffer(GL_FRAMEBUFFER, params.output_framebuffer);
	//*glViewport(0, 0, vs.width, vs.height);
	//*if (params.clear_framebuffer) {
	//*	glClearColor(0.f, 0.0f, 0.f, 1.f);
	//*
	//*	if (params.pass != Render_Level_Params::SHADOWMAP) {
	//*		// set clear depth to 0 
	//*		// reversed Z has 1.0 being closest to camera and 0 being furthest
	//*		glClearDepth(0.0);
	//*	}
	//*
	//*	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	//*
	//*}

	if (params.pass == Render_Level_Params::SHADOWMAP ) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(params.offset_poly_units, 4/* this does nothing?*/);
		//*glCullFace(GL_FRONT);
		//*glDisable(GL_CULL_FACE);
	}

	if (params.pass == Render_Level_Params::FORWARD_PASS) {
		// fixme, for lit transparents
		const Texture* reflectionProbeTex = scene.get_reflection_probe_for_render(params.view.origin);
		if(reflectionProbeTex)
			bind_texture(19, reflectionProbeTex->gl_id);
		else {
			// uh...
			bind_texture(19, black_texture.gl_id);//expects a cubemap...
		}
		bind_texture(18, EnviornmentMapHelper::get().integrator.get_texture());
	}


	{
		// shadows map dont have reversed Z, just standard 0,1 depth
		//*if (params.pass != Render_Level_Params::SHADOWMAP)
		//*	glDepthFunc(GL_GREATER);

		const bool force_backface_state = params.pass == Render_Level_Params::SHADOWMAP || r_debug_mode.get_integer()!=0;

		const bool depth_less_than = params.wants_non_reverse_z;// params.pass == Render_Level_Params::SHADOWMAP;	// else, GL_GREATER
		const bool depth_testing = true;
		//const bool depth_writes = params.pass != Render_Level_Params::TRANSLUCENT;
		if (dont_use_mdi.get_bool()) {
			render_lists_old_way(*params.rl, *params.rp,
				depth_testing,
				force_backface_state,
				depth_less_than);
		}
		else {
			execute_render_lists(*params.rl, *params.rp,
				depth_testing,
				force_backface_state,
				depth_less_than);
		}

	}

	//glClearDepth(1.0);
	//glDepthFunc(GL_LESS);
	glDisable(GL_POLYGON_OFFSET_FILL);
	//glCullFace(GL_BACK);
	//glEnable(GL_CULL_FACE);

	device.reset_states();
}

void Renderer::render_particles()
{
	device.reset_states();
	auto& pobjs = scene.particle_objs.objects;

	for (auto& p_ : pobjs) {
		auto& p = p_.type_;
		const MaterialInstance* mat = p.obj.material;
		if (!mat)
			continue;

		RenderPipelineState state;
		state.program = matman.get_mat_shader(nullptr, mat,0);
		state.vao = p.dd.VAO;// meshbuilder->VAO;
		state.backface_culling = mat->get_master_material()->backface;
		state.blend = mat->get_master_material()->blend;
		state.depth_testing = true;
		state.depth_writes = state.blend == blend_state::OPAQUE;
		state.depth_less_than = false;
		device.set_pipeline(state);

		shader().set_uint("FS_IN_Matid", mat->impl->gpu_buffer_offset);
		shader().set_mat4("Model", p.obj.transform);
		shader().set_mat4("ViewProj", current_frame_view.viewproj);

		auto& textures = mat->impl->get_textures();
		for (int i = 0; i < textures.size(); i++) {
			auto tex = textures[i];
			tex = tex ? tex : &white_texture;
			bind_texture(i, tex->gl_id);
		}

		glDrawElements(GL_TRIANGLES, p.dd.num_indicies, GL_UNSIGNED_INT, (void*)0);
	}
}


#include <algorithm>



Render_Pass::Render_Pass(pass_type type) : type(type) {}

draw_call_key Render_Pass::create_sort_key_from_obj(
	const Render_Object& proxy, 
	const MaterialInstance* material,
	uint32_t camera_dist, 
	int submesh, 
	int layer,
	bool is_editor_mode
)
{
	draw_call_key key;

#ifdef _DEBUG
	const bool is_depth = !r_ignore_depth_shader.get_bool() && (type == pass_type::DEPTH);
#else
	const bool is_depth = type == pass_type::DEPTH;
#endif
	
	int flags = 0;
	// do some if/else here to cut back on permutation insanity. depth only doesnt care about lightmap,taa,editor_id, or debug
	if (proxy.animator_bone_ofs != -1 && proxy.model && proxy.model->has_bones())
		flags |= MSF_ANIMATED;
	if (is_depth) {
		flags |= MSF_DEPTH_ONLY;
	}
	else if (forced_forward) {
		flags |= MSF_IS_FORCED_FORWARD;
	}
	else {
		if (proxy.lightmapped)
			flags |= MSF_LIGHTMAPPED;
		if (is_editor_mode)
			flags |= MSF_EDITOR_ID;
		if (!r_taa_enabled.get_bool())
			flags |= MSF_NO_TAA;
		if (r_debug_mode.get_integer() != 0)
			flags |= MSF_DEBUG;
	}

	key.shader = matman.get_mat_shader(
		proxy.model, material, 
		flags
	);
	const MasterMaterialImpl* mm = material->get_master_material();

	key.blending = (uint64_t)mm->blend;
	key.backface = mm->backface;
	key.texture = material->impl->texture_id_hash;

	VaoType theVaoType = VaoType::Animated;
	if (proxy.lightmapped)
		theVaoType = VaoType::Lightmapped;

	key.vao = (int)theVaoType;
	key.mesh = proxy.model->get_uid();
	key.layer = layer;
	key.distance = camera_dist;

	return key;
}


void Render_Pass::add_object(
	const Render_Object& proxy, 
	handle<Render_Object> handle,
	const MaterialInstance* material,
	uint32_t camera_dist,
	int submesh,
	int lod,
	int layer,
	bool is_editor_mode) {
	ASSERT(handle.is_valid() && "null handle");
	ASSERT(material && "null material");
	Pass_Object obj;
	obj.sort_key = create_sort_key_from_obj(proxy, material,camera_dist, submesh, layer, is_editor_mode);
	obj.render_obj = handle;
	obj.submesh_index = submesh;
	obj.material = material;
	obj.lod_index = lod;
	
	// ensure this material maps to a gpu material
	if(material->impl->gpu_buffer_offset != MaterialImpl::INVALID_MAPPING)
		objects.push_back(obj);
}

void Render_Pass::add_static_object(
	const Render_Object& proxy,
	handle<Render_Object> handle,
	const MaterialInstance* material,
	uint32_t camera_dist,
	int submesh,
	int lod,
	int layer, bool is_editor_mode)
{
	ASSERT(handle.is_valid() && "null handle");
	ASSERT(material && "null material");
	Pass_Object obj;
	obj.sort_key = create_sort_key_from_obj(proxy, material, camera_dist, submesh, layer, is_editor_mode);
	obj.render_obj = handle;
	obj.submesh_index = submesh;
	obj.material = material;
	obj.lod_index = lod;
	if (material->impl->gpu_buffer_offset != MaterialImpl::INVALID_MAPPING)
		cached_static_objects.push_back(obj);
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
		if (a.sort_key.blending < b.sort_key.blending) return true;
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
		auto functor = [](int first, Pass_Object* po, const Render_Object* rop) -> Mesh_Batch
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

		const bool no_batching_dbg = r_no_batching.get_bool();

		// build mesh batches first
		Pass_Object* batch_obj = &objects.at(0);
		const Render_Object* batch_proxy = &scene.get(batch_obj->render_obj);
		Mesh_Batch batch = functor(0, batch_obj, batch_proxy);
		batch_obj->batch_idx = 0;

		for (int i = 1; i < objects.size(); i++) {
			Pass_Object* this_obj = &objects[i];
			const Render_Object* this_proxy = &scene.get(this_obj->render_obj);
			const bool sort_key_equal = this_obj->sort_key.as_uint64() == batch_obj->sort_key.as_uint64();
			const bool same_submesh = this_obj->submesh_index == batch_obj->submesh_index;
			const bool can_be_merged = !no_batching_dbg&&sort_key_equal && same_submesh && type != pass_type::TRANSPARENT;	// dont merge transparent meshes into instances
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

	const bool use_better_depth_batching = r_better_depth_batching.get_bool();

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

		if (type == pass_type::OPAQUE || type == pass_type::TRANSPARENT || !use_better_depth_batching) {
			if (same_vao && same_material && same_other_state && same_shader && same_layer)
				batch_this = true;	// can batch with different meshes
			else
				batch_this = false;

		}
		else {// pass==DEPTH
			// can batch across texture changes as long as its not alpha tested
			if (same_shader && same_vao && same_other_state && !this_batch->material->impl->get_master_impl()->is_alphatested())
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
	: gbuffer_pass(pass_type::OPAQUE),
	transparent_pass(pass_type::TRANSPARENT),
	//shadow_pass(pass_type::DEPTH),
	editor_sel_pass(pass_type::DEPTH),
	shadow_pass(pass_type::DEPTH)
{

}

void Render_Lists::init(uint32_t drawbufsz, uint32_t instbufsz)
{
	glCreateBuffers(1, &gldrawid_to_submesh_material);
	glCreateBuffers(1, &glinstance_to_instance);
}


static void build_standard_cpu(
	Render_Lists& list,
	Render_Pass& src,
	Free_List<ROP_Internal>& proxy_list
)
{
	// first build the lists
	list.build_from(src, proxy_list);

	auto& memArena = draw.get_arena();
	ArenaScope memScope(memArena);

	const int objCount = src.objects.size();
	uint32_t* glinstance_to_instance = memArena.alloc_bottom_type<uint32_t>(objCount);

	for (int objIndex = 0; objIndex < objCount; objIndex++) {
		auto& obj = src.objects[objIndex];

		uint32_t precount = list.commands[obj.batch_idx].primCount++;	// increment count
		uint32_t ofs = list.commands[obj.batch_idx].baseInstance;

		// set the pointer to the Render_Object strucutre that will be found on the gpu
		glinstance_to_instance[ofs + precount] = proxy_list.handle_to_obj[obj.render_obj.id];
	}

	glNamedBufferData(list.glinstance_to_instance, sizeof(uint32_t) * objCount, nullptr, GL_DYNAMIC_DRAW);
	glNamedBufferSubData(list.glinstance_to_instance, 0, sizeof(uint32_t) * objCount, glinstance_to_instance);
}

static void build_cascade_cpu(
	Render_Lists& shadowlist,
	Render_Pass& shadowpass,
	Free_List<ROP_Internal>& proxy_list,
	bool* visiblity
)
{
	// first build the lists
	shadowlist.build_from(shadowpass, proxy_list);

	auto& memArena = draw.get_arena();
	ArenaScope memScope(memArena);

	const int objCount = shadowpass.objects.size();
	uint32_t* glinstance_to_instance = memArena.alloc_bottom_type<uint32_t>(objCount);

	for (int objIndex = 0; objIndex < objCount; objIndex++) {
		auto& obj = shadowpass.objects[objIndex];
		int id = proxy_list.handle_to_obj[obj.render_obj.id];
		//ASSERT(visiblity[id]);
		if (!visiblity[id])
			continue;


		uint32_t precount = shadowlist.commands[obj.batch_idx].primCount++;	// increment count
		uint32_t ofs = shadowlist.commands[obj.batch_idx].baseInstance;

		// set the pointer to the Render_Object strucutre that will be found on the gpu
		glinstance_to_instance[ofs + precount] = id;
	}

	glNamedBufferData(shadowlist.glinstance_to_instance, sizeof(uint32_t) * objCount, nullptr, GL_DYNAMIC_DRAW);
	glNamedBufferSubData(shadowlist.glinstance_to_instance, 0, sizeof(uint32_t) * objCount, glinstance_to_instance);
}

void Render_Pass::merge_static_to_dynamic(bool* vis_array, int8_t* lod_array, Free_List<ROP_Internal>& objs)
{
	for (int i = 0; i < cached_static_objects.size(); i++) {
		auto& o = cached_static_objects[i];
		int idx = objs.handle_to_obj.at(o.render_obj.id);
		if (vis_array&&!vis_array[idx]) continue;
		if (lod_array[idx] != o.lod_index) continue;
		objects.push_back(o);
	}
}


void Render_Lists::build_from(Render_Pass& src, Free_List<ROP_Internal>& proxy_list)
{
	// This function essentially just loops over all batches and creates gpu commands for them
	// its O(n) to the number of batches, not n^2 which it kind of looks like it is

	commands.clear();
	command_count.clear();

	const int max_draw_to_materials = 20000;

	auto& memArena = draw.get_arena();
	ArenaScope scope(memArena);

	uint32_t* draw_to_material = memArena.alloc_bottom_type<uint32_t>(src.mesh_batches.size());
	int draw_to_material_index = 0;

	int base_instance = 0;
	int new_verts_drawn = 0;
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
			cmd.firstIndex /= MODEL_BUFFER_INDEX_TYPE_SIZE;

			// Important! Set primCount to 0 because visible instances will increment this
			cmd.primCount = 0;// meshb.count;
			cmd.baseInstance = base_instance;

			commands.push_back(cmd);

			base_instance += meshb.count;

			auto batch_material = meshb.material;

			assert(draw_to_material_index < src.mesh_batches.size());
			draw_to_material[draw_to_material_index++] = batch_material->impl->gpu_buffer_offset;

			new_verts_drawn += meshb.count * cmd.count;
		}

		command_count.push_back(mdb.count);
	}


	glNamedBufferData(gldrawid_to_submesh_material, sizeof(uint32_t) * draw_to_material_index, draw_to_material, GL_DYNAMIC_DRAW);

	draw.stats.tris_drawn += new_verts_drawn / 3;
}


void Render_Scene::init()
{
	gbuffer_rlist.init(0,0);
	transparent_rlist.init(0,0);
	//csm_shadow_rlist.init(0,0);
	editor_sel_rlist.init(0, 0);
	cascades_rlists.resize(4);
	for (auto& c : cascades_rlists)
		c.init(0, 0);
	spotLightShadowList.init(0, 0);


	glCreateBuffers(1, &gpu_render_instance_buffer);
	glCreateBuffers(1, &gpu_skinned_mats_buffer);

	gpu_skinned_mats_buffer_size = r_skinned_mats_bone_buffer_size.get_integer();
	glNamedBufferData(gpu_skinned_mats_buffer, gpu_skinned_mats_buffer_size * sizeof(glm::mat4), nullptr, GL_STATIC_DRAW);

}

glm::vec4 to_vec4(Color32 color) {
	return glm::vec4(color.r, color.g, color.b, color.a) / 255.f;
}

inline float get_screen_percentage_2(const glm::vec4& bounding_sphere, float inv_two_times_tanfov_2, float camera_dist_2)
{
	return (bounding_sphere.w * bounding_sphere.w) * inv_two_times_tanfov_2 / camera_dist_2;
}

inline const int get_lod_to_render(const Model* model, const float percentage)
{
	for (int i = model->get_num_lods() - 1; i > 0; i--) {
		if (percentage <= model->get_lod(i).end_percentage)
			return i;
	}
	return 0;
}

#include "Frustum.h"

Frustum build_frustom_for_ortho(const glm::mat4& ortho_viewproj)
{
	Frustum f;
	auto inv = glm::inverse(ortho_viewproj);
	const glm::vec3 front = -inv[2];
	const glm::vec3 side = inv[0];
	const glm::vec3 up = inv[1];
	glm::vec3 corners[8];
	for (int i = 0; i < 8; i++) {
		glm::vec3 v = glm::vec3(1, 1, 1);
		if (i % 2 == 1)v.x = -1;
		if (i % 4 >= 2) v.y = -1;
		if (i / 4 == 1) v.z = 0;
		corners[i] = inv * glm::vec4(v, 1.f);
	}
	glm::vec3 n = glm::normalize(corners[2] - corners[0]);
	f.top_plane = glm::vec4(n, -glm::dot(n, corners[0]));

	f.bot_plane = glm::vec4(-n, glm::dot(n, corners[2]));
	n = glm::normalize(corners[1] - corners[0]);
	f.right_plane = glm::vec4(n, -glm::dot(n, corners[0]));
	f.left_plane = glm::vec4(-n, glm::dot(n, corners[1]));
	return f;
}


void build_frustum_for_cascade(Frustum& f, int index)
{
	f = build_frustom_for_ortho(draw.shadowmap.matricies[index]);
}


void build_a_frustum_for_perspective(Frustum& f, const View_Setup& view, glm::vec3* arrow_origin)
{
	if (view.is_ortho) {
		f = build_frustom_for_ortho(view.viewproj);
		return;
	}

	const float fakeFar = 5.0;
	const float fakeNear = 0.0;
	const float aratio = view.width / (float)view.height;

	auto inv = glm::inverse(view.view);

	const glm::vec3 front = -inv[2];
	const glm::vec3 side = inv[0];
	const glm::vec3 up = inv[1];

	const float halfVSide = fakeFar * tanf(view.fov * .5f);
	const float halfHSide = halfVSide * aratio;

	glm::vec3 corners[4];
	corners[0] = front * fakeFar + halfHSide * side + halfVSide * up;
	corners[1] = front * fakeFar + halfHSide * side - halfVSide * up;
	corners[2] = front * fakeFar - halfHSide * side - halfVSide * up;
	corners[3] = front * fakeFar - halfHSide * side + halfVSide * up;

	if (arrow_origin) {
		arrow_origin[3] = front * fakeFar + halfVSide * up; // bottom
		arrow_origin[1] = front * fakeFar - halfVSide * up; // up
		arrow_origin[0] = front * fakeFar + halfHSide * side;	// right
		arrow_origin[2] = front * fakeFar - halfHSide * side;	// left
		for (int i = 0; i < 4; i++)
			arrow_origin[i] = arrow_origin[i] * 0.5f + view.origin;
	}

	glm::vec3 normals[4];
	normals[0] = -glm::normalize(glm::cross(corners[0], up));	//right
	normals[1] = -glm::normalize(glm::cross(corners[1], side));	// up
	normals[2] = glm::normalize(glm::cross(corners[2], up));	// left
	normals[3] = glm::normalize(glm::cross(corners[3], side)); // bottom

	f.top_plane = glm::vec4(normals[1], -glm::dot(normals[1], view.origin));
	f.bot_plane = glm::vec4(normals[3], -glm::dot(normals[3], view.origin));
	f.right_plane = glm::vec4(normals[0], -glm::dot(normals[0], view.origin));
	f.left_plane = glm::vec4(normals[2], -glm::dot(normals[2], view.origin));
}

static void cull_objects(Frustum& frustum, bool* visible_array, int visible_array_size,const Free_List<ROP_Internal>& objs_free_list)
{
	assert(visible_array_size == objs_free_list.objects.size());
	auto& objs = objs_free_list.objects;
	for (int i = 0; i < objs.size(); i++)
	{
		const auto& obj = objs[i].type_;
		const glm::vec3& center = glm::vec3(obj.bounding_sphere_and_radius);
		const float& radius = obj.bounding_sphere_and_radius.w;

		int res = 0;
		res += (glm::dot(glm::vec3(frustum.top_plane), center) + frustum.top_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.bot_plane), center) + frustum.bot_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.left_plane), center) + frustum.left_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.right_plane), center) + frustum.right_plane.w >= -radius) ? 1 : 0;

		visible_array[i] = res == 4;
	}
}
struct CullObjectsUser
{
	bool* visarray = nullptr;
	int count = 0;
	int index = 0;	// for shadows
};

void cull_objects_job(uintptr_t user)
{
	ZoneScopedN("ObjectCull");
	auto d = (CullObjectsUser*)user;
	Frustum frustum;
	build_a_frustum_for_perspective(frustum, draw.current_frame_view);
	auto& objs = draw.scene.proxy_list;
	cull_objects(frustum, d->visarray, d->count, objs);
}
void cull_shadow_objects_job(uintptr_t user)
{
	ZoneScopedN("ShadowObjectCull");
	auto d = (CullObjectsUser*)user;
	Frustum f;
	build_frustum_for_cascade(f, d->index);
	auto& objs = draw.scene.proxy_list;
	cull_objects(f, d->visarray, d->count, objs);
}

void cull_spot_shadow_objects_job(handle<Render_Light> lightId, bool* visArray, int visArraySize, bool& any_dynamic_found)
{
	ZoneScopedN("cull_spot_shadow_objects_job");
	auto& light = draw.scene.light_list.get(lightId.id);
	auto& p = light.light.position;
	auto& n = light.light.normal;

	Frustum frustum;
	View_Setup setup;
	glm::vec3 up = glm::vec3(0, 1, 0);
	if (glm::abs(glm::dot(up, n)) > 0.999) 
		up = glm::vec3(1, 0, 0);
	setup.view = glm::lookAt(p, p+n,up);
	setup.origin = p;
	setup.width = setup.height = 1;	//aratio=1
	setup.fov = glm::radians(light.light.conemax) * 2.0;
	build_a_frustum_for_perspective(frustum,setup,nullptr);

	glm::vec4 backplane = glm::vec4(-n, 0.0);
	backplane.w = glm::dot(n, p + n * light.light.radius);

	auto& objs = draw.scene.proxy_list.objects;

	assert(visArraySize == objs.size());
	int count = 0;
	for (int i = 0; i < objs.size(); i++) {
		const auto& obj = objs[i].type_;
		const glm::vec3& center = glm::vec3(obj.bounding_sphere_and_radius);
		const float& radius = obj.bounding_sphere_and_radius.w;

		int res = 0;
		res += (glm::dot(glm::vec3(frustum.top_plane), center) + frustum.top_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.bot_plane), center) + frustum.bot_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.left_plane), center) + frustum.left_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.right_plane), center) + frustum.right_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(backplane), center) + backplane.w >= -radius) ? 1 : 0;

		visArray[i] = res == 5;
		count += visArray[i];

		//visArray[i] = true;
	}
	draw.stats.shadow_objs += count;

	// fixme
	any_dynamic_found = true;
}



void calc_lod_job(uintptr_t user)
{
	ZoneScopedN("LodToRenderCalc");

	int8_t* lodarray = (int8_t*)user;

	const float inv_two_times_tanfov = 1.0 / (tan(draw.get_current_frame_vs().fov * 0.5));
	const float inv_two_times_tanfov_2 = inv_two_times_tanfov * inv_two_times_tanfov;
	auto& vs = draw.current_frame_view;
	auto& proxy_list = draw.scene.proxy_list;
	for (int i = 0; i < proxy_list.objects.size(); i++) {
		auto& obj = proxy_list.objects[i];
		auto& proxy = obj.type_.proxy;
		const glm::vec3 to_camera = glm::vec3(obj.type_.bounding_sphere_and_radius) - vs.origin;
		const float dist_to_camera_2 = glm::dot(to_camera, to_camera);
		const float percentage_2 = get_screen_percentage_2(obj.type_.bounding_sphere_and_radius, inv_two_times_tanfov_2, dist_to_camera_2);
		const bool casts_shadow = proxy.shadow_caster;//&& percentage_2 >= 0.001;
		if (!proxy.model)
			lodarray[i] = 0;
		else {
			lodarray[i] = (int8_t)get_lod_to_render(proxy.model, percentage_2);
		}
	}
}


void set_gpu_objects_data_job(uintptr_t p)
{
	const int current_bone_buffer_offset = draw.scene.get_front_bone_buffer_offset();
	const int prev_bone_buffer_offset = draw.scene.get_back_bone_buffer_offset();

	auto gpu_objects = (gpu::Object_Instance*)p;
	auto& proxy_list = draw.scene.proxy_list;
	ZoneScopedN("SetGpuObjectData");
	for (int i = 0; i < proxy_list.objects.size(); i++) {
		auto& obj = proxy_list.objects[i];
		auto& proxy = obj.type_.proxy;
		gpu_objects[i].anim_mat_offset = current_bone_buffer_offset + obj.type_.proxy.animator_bone_ofs;
		gpu_objects[i].model = proxy.transform;
		gpu_objects[i].prev_model = obj.type_.prev_transform;
		if (obj.type_.prev_bone_ofs == -1)
			gpu_objects[i].prev_anim_mat_offset = gpu_objects[i].anim_mat_offset;
		else
			gpu_objects[i].prev_anim_mat_offset = prev_bone_buffer_offset + obj.type_.prev_bone_ofs;
		gpu_objects[i].colorval = proxy.lightmap_coord;
		gpu_objects[i].flags = 0;

		bool needs_flat = !proxy.static_probe_lit&&!proxy.lightmapped;
		if (proxy.static_probe_lit) {
			int index = int(proxy.lightmap_coord.x)*6;
			if (index >= 0 && index < draw.scene.lightmapObj.staticAmbientCubeProbes.size()) {
				auto& probes = draw.scene.lightmapObj.staticAmbientCubeProbes;
				glm::vec3 a0 = probes.at(index);
				glm::vec3 a1 = probes.at(index+1);
				glm::vec3 a2 = probes.at(index+2);
				glm::vec3 a3 = probes.at(index+3);
				glm::vec3 a4 = probes.at(index+4);
				glm::vec3 a5 = probes.at(index+5);
				gpu_objects[i].ambientCube0 = glm::vec4(a0,0.0);
				gpu_objects[i].ambientCube1 = glm::vec4(a1, 0.0);
				gpu_objects[i].ambientCube2 = glm::vec4(a2, 0.0);
				gpu_objects[i].ambientCube3 = glm::vec4(a3, 0.0);
				gpu_objects[i].ambientCube4 = glm::vec4(a4, 0.0);
				gpu_objects[i].ambientCube5 = glm::vec4(a5, 0.0);
			}
			else
				needs_flat = true;
		}
		if (needs_flat) {
			glm::vec3 ambientCube[6];
			glm::vec3 pos = proxy.transform[3];
			if(proxy.model)
				pos = proxy.transform * glm::vec4( proxy.model->get_bounds().get_center(),1.0);
			draw.scene.evaluate_lighting_at_position(pos, ambientCube);

			gpu_objects[i].ambientCube0 = glm::vec4(ambientCube[1],0.f);
			gpu_objects[i].ambientCube1 = glm::vec4(ambientCube[0],0.f);
			gpu_objects[i].ambientCube2 = glm::vec4(ambientCube[3],0.f);
			gpu_objects[i].ambientCube3 = glm::vec4(ambientCube[2],0.f);
			gpu_objects[i].ambientCube4 = glm::vec4(ambientCube[5],0.f);
			gpu_objects[i].ambientCube5 = glm::vec4(ambientCube[4],0.f);
		}
	}
}


void make_batches_job(uintptr_t p)
{
	ZoneScopedN("make_batches_job");
	Render_Pass* pass = (Render_Pass*)p;
	pass->make_batches(draw.scene);
}

struct MakeShadowRenderListParam
{
	bool* visarray = nullptr;
	int index = 0;
};

void make_shadow_render_list_job(uintptr_t p)
{
	ZoneScopedN("make_shadow_render_list_job");

	auto param = (MakeShadowRenderListParam*)p;

	build_cascade_cpu(
		draw.scene.cascades_rlists[param->index],
		draw.scene.shadow_pass,
		draw.scene.proxy_list,
		param->visarray
	);
}

void merge_shadow_list_job(uintptr_t p)
{
	ZoneScopedN("merge_shadow_list_job");
	int8_t* lodarray = (int8_t*)p;
	draw.scene.shadow_pass.merge_static_to_dynamic(nullptr, lodarray, draw.scene.proxy_list);
}

#include "Framework/Jobs.h"
void Render_Scene::refresh_static_mesh_data(bool build_for_editor)
{
	// update static cache this frame
	gbuffer_pass.clear_static();
	transparent_pass.clear_static();
	editor_sel_pass.clear_static();
	shadow_pass.clear_static();

	for (int i = 0; i < proxy_list.objects.size(); i++) {
		auto& obj = proxy_list.objects[i];
		auto& proxy = obj.type_.proxy;
		obj.type_.is_static = false;	// FIXME
		if (!obj.type_.is_static || proxy.is_skybox)
			continue;
		handle<Render_Object> objhandle{ obj.handle };
		if (!proxy.visible || !proxy.model || !proxy.model->get_is_loaded())
			continue;
		const bool casts_shadow = proxy.shadow_caster;//&& percentage_2 >= 0.001;
		auto model = proxy.model;
		for (int LOD_index = 0; LOD_index < model->get_num_lods(); LOD_index++) {
			const auto& lod = model->get_lod(LOD_index);

			const int pstart = lod.part_ofs;
			const int pend = pstart + lod.part_count;

			for (int j = pstart; j < pend; j++) {
				auto& part = proxy.model->get_part(j);

				const MaterialInstance* mat = (MaterialInstance*)proxy.model->get_material(part.material_idx);
				
				if (obj.type_.proxy.mat_override)
					mat = (MaterialInstance*)obj.type_.proxy.mat_override;
				if (!mat || !mat->is_valid_to_use() || !mat->get_master_material()->is_compilied_shader_valid)
					mat = matman.get_fallback();
				const MasterMaterialImpl* mm = mat->get_master_material();

				if (mm->render_in_forward_pass()) {
					transparent_pass.add_static_object(proxy, objhandle, mat, 0/* fixme sorting distance */, j, LOD_index, 0, build_for_editor);
					if (!mm->is_translucent() && casts_shadow)
						shadow_pass.add_static_object(proxy, objhandle, mat, 0, j, LOD_index, 0, build_for_editor);
				}
				else {
					if (casts_shadow)
						shadow_pass.add_static_object(proxy, objhandle, mat, 0, j, LOD_index, 0, build_for_editor);
					gbuffer_pass.add_static_object(proxy, objhandle, mat, 0, j, LOD_index, 0, build_for_editor);
				}

#ifdef EDITOR_BUILD
				if (proxy.outline) {
					editor_sel_pass.add_static_object(proxy, objhandle, mat, 0, j, LOD_index, 0, build_for_editor);
				}
#endif
			}
		}
	}
}

void Render_Scene::build_scene_data(bool skybox_only, bool build_for_editor)
{
	ZoneScoped;
	if (r_debug_skip_build_scene_data.get_bool())
		return;
	if (static_cache_built_for_editor != build_for_editor)
		statics_meshes_are_dirty = true;
	if(static_cache_built_for_debug!=(r_debug_mode.get_integer() != 0))
		statics_meshes_are_dirty = true;

	auto& memArena = draw.get_arena();
	ArenaScope scope(memArena);

	// clear objects
	gbuffer_pass.clear();
	transparent_pass.clear();
	shadow_pass.clear();
	editor_sel_pass.clear();

	const int visible_count = proxy_list.objects.size();
	bool* cascade_vis[4] = { nullptr,nullptr,nullptr,nullptr };
	for(int i=0;i<4;i++)
		cascade_vis[i] = memArena.alloc_bottom_type<bool>(visible_count);
	
	bool* visible_array = memArena.alloc_bottom_type<bool>(visible_count);
	int8_t* lod_to_render_array = memArena.alloc_bottom_type<int8_t>(visible_count);

	{
		//CPUSCOPESTART(cpu_object_cull);
		ZoneScopedN("SetupThreaded");

		JobCounter* cullAndLodCounter{};

		const int NUM_FRUSTUM_JOBS = 5;
		JobDecl decls[NUM_FRUSTUM_JOBS];
		CullObjectsUser mainview;
		CullObjectsUser cascades[4];
		mainview.count = visible_count;
		mainview.visarray = visible_array;
		decls[0].func = cull_objects_job;
		decls[0].funcarg = uintptr_t(&mainview);
		for (int i = 0; i < 4; i++) {
			cascades[i].index = i;
			cascades[i].count = visible_count;
			cascades[i].visarray = cascade_vis[i];
			decls[i + 1].func = cull_shadow_objects_job;
			decls[i + 1].funcarg = uintptr_t(&cascades[i]);
		}

		JobSystem::inst->add_jobs(decls, NUM_FRUSTUM_JOBS, cullAndLodCounter);
		JobSystem::inst->add_job(calc_lod_job,uintptr_t(lod_to_render_array), cullAndLodCounter);
		
		// while waiting, can refresh static mesh data if needed
		if (statics_meshes_are_dirty) {
			//printf("reset static mesh (editor: %d)\n", (int)build_for_editor);
			refresh_static_mesh_data(build_for_editor);
			statics_meshes_are_dirty = false;
			static_cache_built_for_debug = r_debug_mode.get_integer() != 0;
			static_cache_built_for_editor = build_for_editor;

		}

		JobSystem::inst->wait_and_free_counter(cullAndLodCounter);
	}
	const size_t num_ren_objs = proxy_list.objects.size();
	gpu::Object_Instance* gpu_objects = memArena.alloc_bottom_type<gpu::Object_Instance>(num_ren_objs);
	ASSERT(gpu_objects);
	JobCounter* gpu_obj_set_cntr{};
	JobSystem::inst->add_job(set_gpu_objects_data_job, uintptr_t(gpu_objects), gpu_obj_set_cntr);

	auto add_objects_to_passes = [&]() {
		ZoneScopedN("Traversal");
		ZoneScopedN("LoopObjects");
		for (int i = 0; i < proxy_list.objects.size(); i++) {
			auto& obj = proxy_list.objects[i];
			handle<Render_Object> objhandle{ obj.handle };
			auto& proxy = obj.type_.proxy;

			if (!proxy.visible || !proxy.model || !proxy.model->get_is_loaded() || (proxy.model->get_num_lods() == 0))
				continue;

			if (!proxy.is_skybox && skybox_only)
				continue;
			if (obj.type_.is_static)	// only dynamic objects passthrough
				continue;

			const bool is_visible = visible_array[i];
			const bool casts_shadow = proxy.shadow_caster;//&& percentage_2 >= 0.001;

			if (!is_visible && !casts_shadow)
				continue;

			const int LOD_index = lod_to_render_array[i];

			auto model = proxy.model;
			const auto& lod = model->get_lod(LOD_index);

			const int pstart = lod.part_ofs;
			const int pend = pstart + lod.part_count;

			for (int j = pstart; j < pend; j++) {
				auto& part = proxy.model->get_part(j);

				const MaterialInstance* mat = proxy.model->get_material(part.material_idx);
				if (obj.type_.proxy.mat_override)
					mat = (MaterialInstance*)obj.type_.proxy.mat_override;
				if (!mat || !mat->is_valid_to_use() || !mat->get_master_material()->is_compilied_shader_valid)
					mat = matman.get_fallback();
				const MasterMaterialImpl* mm = mat->get_master_material();

				if (mm->render_in_forward_pass()) {
					if (is_visible)
						transparent_pass.add_object(proxy, objhandle, mat, 0/* fixme sorting distance */, j, LOD_index, 0, build_for_editor);
					if (!mm->is_translucent() && casts_shadow)
						shadow_pass.add_object(proxy, objhandle, mat, 0, j, LOD_index, 0, build_for_editor);
				}
				else {
					if (casts_shadow)
						shadow_pass.add_object(proxy, objhandle, mat, 0, j, LOD_index, 0, build_for_editor);
					if (is_visible)
						gbuffer_pass.add_object(proxy, objhandle, mat, 0, j, LOD_index, 0, build_for_editor);
				}

#ifdef EDITOR_BUILD
				if (obj.type_.proxy.outline && is_visible) {
					editor_sel_pass.add_object(proxy, objhandle, mat, 0, j, LOD_index, 0, build_for_editor);
				}
#endif
			}
		}
	};
	add_objects_to_passes();
		
	auto merge_static_and_dynamic = [&]() {
		ZoneScopedN("MergeStaticWithDynamic");
		if (!skybox_only) {
			draw.scene.shadow_pass.merge_static_to_dynamic(nullptr, lod_to_render_array, draw.scene.proxy_list);
			editor_sel_pass.merge_static_to_dynamic(visible_array, lod_to_render_array, proxy_list);
			gbuffer_pass.merge_static_to_dynamic(visible_array, lod_to_render_array, proxy_list);
			transparent_pass.merge_static_to_dynamic(visible_array, lod_to_render_array, proxy_list);
		}
	};
	merge_static_and_dynamic();

	auto make_batches_for_passes = [&]() {
		ZoneScopedN("MakeBatchesAndUploadGpuData");

		// start gbuffer pass, but do the rest on this thread
		//JobCounter* c{};
		//JobSystem::inst->add_job(make_batches_job,uintptr_t(&gbuffer_pass), c);
		//JobSystem::inst->add_job(make_batches_job, uintptr_t(&shadow_pass), c);
		gbuffer_pass.make_batches(*this);
		shadow_pass.make_batches(*this);
		transparent_pass.make_batches(*this);
		editor_sel_pass.make_batches(*this);

		JobSystem::inst->wait_and_free_counter(gpu_obj_set_cntr);
		{
			ZoneScopedN("UploadGpuData");
			glNamedBufferData(gpu_render_instance_buffer, sizeof(gpu::Object_Instance) * num_ren_objs, gpu_objects, GL_DYNAMIC_DRAW);
		}
	};
	make_batches_for_passes();


	auto make_render_lists = [&]() {
		ZoneScopedN("MakeRenderLists");

		auto make_shadow_render_lists = [&]() {
			JobDecl shadowlistdecl[4];
			MakeShadowRenderListParam params[4];
			for (int i = 0; i < 4; i++) {
				params[i].visarray = cascade_vis[i];
				params[i].index = i;
				shadowlistdecl[i].func = make_shadow_render_list_job;
				shadowlistdecl[i].funcarg = uintptr_t(&params[i]);
			}
			for (int i = 0; i < 4; i++) {
				shadowlistdecl[i].func(shadowlistdecl[i].funcarg);
			}
		};
		make_shadow_render_lists();

		auto update_spotlight_shadows = [&]() {
			std::vector<handle<Render_Light>> lightsToCalcShadow;
			draw.spotShadows->get_lights_to_render(lightsToCalcShadow);
			draw.stats.shadow_lights += lightsToCalcShadow.size();
			// tbh just do it here whatev
			if (!lightsToCalcShadow.empty()) {
				bool* spot_visible_array = memArena.alloc_bottom_type<bool>(visible_count);
				for (int i = 0; i < lightsToCalcShadow.size(); i++) {
					bool any_dynamic_found = false;
					cull_spot_shadow_objects_job(lightsToCalcShadow[i], spot_visible_array, visible_count, any_dynamic_found);
					build_cascade_cpu(spotLightShadowList, draw.scene.shadow_pass,
						draw.scene.proxy_list,
						spot_visible_array);
					draw.spotShadows->do_render(spotLightShadowList, lightsToCalcShadow[i], any_dynamic_found);
				}
			}
		};
		update_spotlight_shadows();


		build_standard_cpu(gbuffer_rlist, gbuffer_pass, proxy_list);
		build_standard_cpu(transparent_rlist, transparent_pass, proxy_list);
		build_standard_cpu(editor_sel_rlist, editor_sel_pass, proxy_list);

	};
	make_render_lists();


	memArena.free_bottom();

}

void Renderer::draw_meshbuilders()
{
	if (r_no_meshbuilders.get_bool())
		return;

	auto& mbFL = scene.meshbuilder_objs;
	auto& mbObjs = scene.meshbuilder_objs.objects;
	for (auto& mbPair : mbObjs)
	{
		auto& mb = mbPair.type_.obj;
		if (!mb.visible)
			continue;
		auto& dd = mbPair.type_.dd;
		if (dd.num_indicies == 0)	// this check ...
			continue;
		if (mb.use_background_color) {
			RenderPipelineState state;
			state.program = prog.simple_solid_color;
			state.depth_testing = mb.depth_tested;
			state.depth_writes = false;
			device.set_pipeline(state);


			shader().set_mat4("ViewProj", current_frame_view.viewproj);
			shader().set_mat4("Model", mb.transform);
			shader().set_vec4("solid_color", color32_to_vec4(mb.background_color));

			glLineWidth(3);
			dd.draw(MeshBuilderDD::LINES);
			glLineWidth(1);
		}

		RenderPipelineState state;
		state.program = prog.simple;
		state.depth_testing = mb.depth_tested;
		state.depth_writes = false;
		device.set_pipeline(state);


		shader().set_mat4("ViewProj", current_frame_view.viewproj);
		shader().set_mat4("Model", mb.transform);
		dd.draw(MeshBuilderDD::LINES);
	}
}

extern ConfigVar g_draw_grid;
extern ConfigVar g_grid_size;

static handle<MeshBuilder_Object> debug_grid_handle;

void update_debug_grid()
{
	static MeshBuilder mb;
	static bool has_init = false;
	if (!has_init) {
		mb.Begin();
		for (int x = 0; x < 11; x++) {
			Color32 colorx = COLOR_WHITE;
			Color32 colorz = COLOR_WHITE;
			if (x == 5) {
				colorx = COLOR_RED;
				colorz = COLOR_BLUE;
			}
			mb.PushLine(glm::vec3(-5, 0, x - 5), glm::vec3(5, 0, x - 5), colorx);
			mb.PushLine(glm::vec3(x - 5, 0, -5), glm::vec3(x - 5, 0, 5), colorz);
		}
		mb.End();
		debug_grid_handle = idraw->get_scene()->register_meshbuilder();
		has_init = true;
	}
	MeshBuilder_Object mbo;
	mbo.use_background_color = true;
	mbo.visible = g_draw_grid.get_bool();
	mbo.meshbuilder = &mb;
	idraw->get_scene()->update_meshbuilder(debug_grid_handle, mbo);
}

void Renderer::accumulate_gbuffer_lighting(bool is_cubemap_view)
{
	ZoneScoped;
	GPUSCOPESTART(accumulate_gbuffer_lighting);

	const auto& view_to_use = current_frame_view;

	RenderPassSetup setup("gbuffer-lighting", fbo.forward_render, false, false, 0, 0, view_to_use.width, view_to_use.height);
	auto scope = device.start_render_pass(setup);
	const bool wants_ssao = !is_cubemap_view && enable_ssao.get_bool();
	const texhandle ssao_tex = (wants_ssao) ? ssao.texture.result : white_texture.gl_id;	// skip ssao in cubemap view
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo.current_frame);

	device.reset_states();
	if(!r_no_indirect.get_bool())
	{
		RenderPipelineState state;
		state.vao = get_empty_vao();
		state.program = prog.ambient_accumulation;
		state.blend = blend_state::MULT;
		state.depth_testing = false;
		state.depth_writes = false;
		device.set_pipeline(state);

		bind_texture_ptr(0, tex.scene_gbuffer0);
		bind_texture_ptr(1, tex.scene_gbuffer1);
		bind_texture_ptr(2, tex.scene_gbuffer2);
		bind_texture_ptr(3, tex.scene_depth);
		bind_texture(4, ssao_tex);

		for (int i = 0; i < 6; i++)
			shader().set_vec3(string_format("AmbientCube[%d]", i), glm::vec3(0.f));
		// fullscreen shader, no vao used
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
	device.reset_states();



	Model* LIGHT_CONE = g_modelMgr.get_light_cone();
	Model* LIGHT_SPHERE = g_modelMgr.get_light_sphere();
	Model* LIGHT_DOME = g_modelMgr.get_light_dome();
	vertexarrayhandle vao = g_modelMgr.get_vao(VaoType::Animated);
	{

		RenderPipelineState state;
		state.vao = vao;
		state.depth_writes = false;
		state.depth_testing = false;
		state.program = prog.light_accumulation;
		state.backface_culling = true;
		state.cull_front_face = true;
		state.blend = blend_state::ADD;
		device.set_pipeline(state);


		bind_texture_ptr(0, tex.scene_gbuffer0);
		bind_texture_ptr(1, tex.scene_gbuffer1);
		bind_texture_ptr(2, tex.scene_gbuffer2);
		bind_texture_ptr(3, tex.scene_depth);

		// spot shadow atlas
		bind_texture(4, spotShadows->get_atlas().get_atlas_texture());


		for (auto& light_pair : scene.light_list.objects) {
			auto& light = light_pair.type_.light;

			glm::mat4 ModelTransform = glm::translate(glm::mat4(1.f), light.position);
			const float scale = light.radius;
			ModelTransform = glm::scale(ModelTransform, glm::vec3(scale));

			// Copied code from execute_render_lists
			auto& part = LIGHT_SPHERE->get_part(0);
			const GLenum index_type = MODEL_INDEX_TYPE_GL;
			gpu::DrawElementsIndirectCommand cmd;
			cmd.baseVertex = part.base_vertex + LIGHT_SPHERE->get_merged_vertex_ofs();
			cmd.count = part.element_count;
			cmd.firstIndex = part.element_offset + LIGHT_SPHERE->get_merged_index_ptr();
			cmd.firstIndex /= MODEL_BUFFER_INDEX_TYPE_SIZE;
			cmd.primCount = 1;
			cmd.baseInstance = 0;

			if (light.casts_shadow_mode != 0) {
				if(light.projected_texture)
					state.program = prog.light_accumulation_shadow_cookie;
				else
					state.program = prog.light_accumulation_shadowed;
			}
			else {
				state.program = prog.light_accumulation;
			}
			device.set_pipeline(state);


			shader().set_mat4("Model", ModelTransform);
			shader().set_vec3("position", light.position);
			shader().set_float("radius", light.radius);
			shader().set_bool("is_spot_light", light.is_spotlight);
			shader().set_float("spot_inner", cos(glm::radians(light.conemin)));
			shader().set_float("spot_angle", cos(glm::radians(light.conemax)));
			shader().set_vec3("spot_normal",light.normal);
			shader().set_vec3("color", light.color);
			shader().set_float("uEpsilon", shadowmap.tweak.epsilon * 0.01f);
			if (light.casts_shadow_mode != 0) {
				shader().set_mat4("shadow_view_proj", light_pair.type_.lightViewProj);
				Rect2d rect = spotShadows->get_atlas().get_atlas_rect(light_pair.type_.shadow_array_handle);
				glm::ivec2 atlas_size = spotShadows->get_atlas().get_size();
				//xy is scale, zw is offset
				glm::vec4 as_vec4 = glm::vec4(float(rect.w) / atlas_size.x, float(rect.h) / atlas_size.y,
					float(rect.x) / atlas_size.x, float(rect.y) / atlas_size.y);
				shader().set_vec4("shadow_atlas_offset", as_vec4);

				if (light.projected_texture)
					device.bind_texture(5, light.projected_texture->gl_id);
			}

			glMultiDrawElementsIndirect(
				GL_TRIANGLES,
				index_type,
				(void*)&cmd,
				1,
				sizeof(gpu::DrawElementsIndirectCommand)
			);
		}
	}


	// fullscreen pass for directional light(s)
	RSunInternal* sun_internal = scene.get_main_directional_light();
	if(sun_internal)
	{
		RenderPipelineState state;
		state.vao = get_empty_vao();
		state.program = prog.sunlight_accumulation;
		state.blend = blend_state::ADD;
		state.depth_testing = false;
		state.depth_writes = false;
		device.set_pipeline(state);

		bind_texture_ptr(0, tex.scene_gbuffer0);
		bind_texture_ptr(1, tex.scene_gbuffer1);
		bind_texture_ptr(2, tex.scene_gbuffer2);
		bind_texture_ptr(3, tex.scene_depth);
		bind_texture(4, draw.shadowmap.texture.shadow_array);
		glBindBufferBase(GL_UNIFORM_BUFFER, 8, draw.shadowmap.ubo.info);

		shader().set_vec3("uSunDirection", sun_internal->sun.direction);
		shader().set_vec3("uSunColor", sun_internal->sun.color);
		shader().set_float("uEpsilon", sun_internal->sun.epsilon);

		
		// fullscreen shader, no vao used
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}


	const Texture* reflectionProbeTex = scene.get_reflection_probe_for_render(view_to_use.origin);

	if (reflectionProbeTex&& !is_cubemap_view) {
		RenderPipelineState state;
		state.vao = get_empty_vao();
		state.program = prog.reflection_accumulation;
		state.blend = blend_state::ADD;
		state.depth_testing = false;
		state.depth_writes = false;
		device.set_pipeline(state);
		bind_texture(4, ssao_tex);
		bind_texture(5, reflectionProbeTex->gl_id);
		bind_texture(6, EnviornmentMapHelper::get().integrator.get_texture());
		shader().set_float("specular_ao_intensity", r_specular_ao_intensity.get_float());

		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
}
#ifdef EDITOR_BUILD
int write_png_wrapper(const char* filename, int w, int h, int comp, const void* data, int stride_in_bytes);

ThumbnailRenderer::ThumbnailRenderer(int size) : pass(pass_type::TRANSPARENT) {
	this->size = size;
	pass.forced_forward = true;
	list.init(0, 0);
	object = draw.scene.register_obj();
	Render_Object o;
	o.visible = false;
	draw.scene.update_obj(object, o);


	auto set_default_parameters = [](uint32_t handle) {
		glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	};

	auto create_and_delete_texture = [](uint32_t& texture) {
		glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	};

	auto create_and_delete_fb = [](uint32_t& framebuffer) {
		glDeleteFramebuffers(1, &framebuffer);
		glCreateFramebuffers(1, &framebuffer);
	};
	const int w = size;
	const int h = size;
	fbohandle fbo_handle = 0;
	texhandle color_tex = 0;
	texhandle depth_tex = 0;
	{
		glCreateFramebuffers(1, &fbo_handle);
		glCreateTextures(GL_TEXTURE_2D, 1, &color_tex);
		set_default_parameters(color_tex);
		glTextureStorage2D(color_tex, 1, GL_RGBA8, w, h);

		glCreateTextures(GL_TEXTURE_2D, 1, &depth_tex);
		set_default_parameters(depth_tex);
		glTextureStorage2D(depth_tex, 1, GL_DEPTH_COMPONENT32F, w, h);

		glNamedFramebufferTexture(fbo_handle, GL_COLOR_ATTACHMENT0, color_tex, 0);
		glNamedFramebufferTexture(fbo_handle, GL_DEPTH_ATTACHMENT, depth_tex, 0);
		unsigned int attachments[5] = { GL_COLOR_ATTACHMENT0,0,0,0, 0 };
		glNamedFramebufferDrawBuffers(fbo_handle, 5, attachments);
	}

	this->fbo = fbo_handle;
	this->color = color_tex;
	this->depth = depth_tex;

	vts_handle = Texture::install_system("_test_thumbnail");
	vts_handle->update_specs(color_tex, w, h, 4, {});
	vts_handle->type = Texture_Type::TEXTYPE_2D;
}


void ThumbnailRenderer::render(Model* model) {
	ASSERT(!eng->get_is_in_overlapped_period());
	if (!model || model->get_num_lods() == 0)
		return;
	pass.clear();
	auto& lod = model->get_lod(0);
	auto& scene = draw.scene;
	const int pstart = lod.part_ofs;
	const int pend = pstart + lod.part_count;
	auto& proxy = scene.proxy_list.get(object.id);
	proxy.proxy.model = model;
	for (int j = pstart; j < pend; j++) {
		auto& part = model->get_part(j);

		const MaterialInstance* mat = model->get_material(part.material_idx);
		if (!mat || !mat->is_valid_to_use() || !mat->get_master_material()->is_compilied_shader_valid)
			mat = matman.get_fallback();

		pass.add_object(proxy.proxy, object, mat, 0, j, 0, 0, false);
	}
	pass.make_batches(scene);
	build_standard_cpu(
		list,
		pass,
		scene.proxy_list
	);

	const int w = size;
	const int h = size;
	RenderPassSetup setup("thumbnail", this->fbo, true, true, 0, 0, w, h);
	auto scope = draw.get_device().start_render_pass(setup);
	auto sphere = model->get_bounding_sphere();
	const float fov_rad = glm::radians(thumbnail_fov.get_float());
	glm::vec3 center = glm::vec3(sphere);
	const float c_mult = 2.0 / fov_rad;
	glm::vec3 cam_pos = center + glm::normalize(glm::vec3(1, 1, 1)) * sphere.w * c_mult;
	View_Setup viewSetup = View_Setup(glm::lookAt(cam_pos, center, glm::vec3(0, 1, 0)), fov_rad, 0.01, 100.0, w, h);

	Render_Level_Params cmdparams(
		viewSetup,
		&list,
		&pass,
		Render_Level_Params::FORWARD_PASS
	);
	cmdparams.upload_constants = true;
	cmdparams.provied_constant_buffer = draw.ubo.current_frame;
	cmdparams.draw_viewmodel = true;
	draw.render_level_to_target(cmdparams);
}

void ThumbnailRenderer::output_to_path(std::string path) {
	const int w = size;
	const int h = size;
	std::vector<unsigned char> pixels(w * h * 4); // RGBA
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	int success = write_png_wrapper(path.c_str(), w, h, 4, pixels.data(), w * 4);
}
#endif

void Renderer::draw_height_fog()
{
	return;
	assert(0);
	if (!scene.has_fog)
		return;
	if (!r_drawfog.get_bool())
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo.forward_render);

	set_shader(prog.height_fog);

	shader().set_float("directionalExp", scene.fog.directional_exponent);
	shader().set_float("height_falloff", scene.fog.fog_height_falloff);
	shader().set_float("height_start", scene.fog.height);
	shader().set_float("density", scene.fog.fog_density);
	glm::vec3 color = glm::vec3(scene.fog.inscattering_color.r, scene.fog.inscattering_color.g, scene.fog.inscattering_color.b);
	color *= 1.0f / 255.f;
	shader().set_vec3("inscatteringColor", color);

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);

	// enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	bind_texture_ptr(3, tex.scene_depth);

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
	glDisable(GL_BLEND);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::deferred_decal_pass()
{
	GPUFUNCTIONSTART;

	if (!r_drawdecals.get_bool())
		return;
	const auto& view_to_use = current_frame_view;
	RenderPassSetup setup("decalgbuffer",fbo.gbuffer,false,false,0,0, view_to_use.width, view_to_use.height);
	auto scope = device.start_render_pass(setup);


	static Model* cube = find_global_asset_s<Model>("eng/cube.cmdl");	// cube model
	assert(cube->is_this_globally_referenced());
	// Copied code from execute_render_lists
	auto& part = cube->get_part(0);
	const GLenum index_type = MODEL_INDEX_TYPE_GL;
	gpu::DrawElementsIndirectCommand cmd;
	cmd.baseVertex = part.base_vertex + cube->get_merged_vertex_ofs();
	cmd.count = part.element_count;
	cmd.firstIndex = part.element_offset + cube->get_merged_index_ptr();
	cmd.firstIndex /= MODEL_BUFFER_INDEX_TYPE_SIZE;
	cmd.primCount = 1;
	cmd.baseInstance = 0;

	bind_texture_ptr(20/* FIXME, defined to be bound at spot 20, also in MasterDecalShader.txt*/, tex.scene_depth);

	vertexarrayhandle vao = g_modelMgr.get_vao(VaoType::Animated);

	for (int i = 0; i < scene.decal_list.objects.size(); i++) {
		auto& obj = scene.decal_list.objects[i].type_.decal;
		if (!obj.material)
			continue;
		MaterialInstance* l = (MaterialInstance*)obj.material;
		if (l->get_master_material()->usage != MaterialUsage::Decal)
			continue;

		program_handle program = matman.get_mat_shader(nullptr, l, 0);

		RenderPipelineState state;
		state.depth_testing = false;
		state.depth_writes = false;
		state.program = program;
		state.vao = vao;
		state.blend = l->get_master_material()->blend;
		device.set_pipeline(state);

		glm::mat4 ModelTransform = obj.transform;
		auto invTransform = glm::inverse(ModelTransform);

		shader().set_vec2("DecalTCScale", obj.uv_scale);
		shader().set_mat4("Model", ModelTransform);
		shader().set_mat4("DecalViewProj", invTransform);
		shader().set_mat4("InverseModel", invTransform);
		shader().set_uint("FS_IN_Matid", l->impl->gpu_buffer_offset);

		auto& texs = l->impl->get_textures();
		for (int j = 0; j < texs.size(); j++)
			bind_texture(j, texs[j]->gl_id);

		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			index_type,
			(void*)&cmd,
			1,
			sizeof(gpu::DrawElementsIndirectCommand)
		);
	}
}
void Renderer::sync_update()
{
	ZoneScoped;

	if (enable_vsync.was_changed()) {
		if (enable_vsync.get_bool())
			SDL_GL_SetSwapInterval(1);
		else
			SDL_GL_SetSwapInterval(0);
	}

	scene.execute_deferred_deletes();

	update_debug_grid();	// makes it visible/hidden

	for (auto& mbo_ : scene.meshbuilder_objs.objects) {
		auto& mbo = mbo_.type_;
		if (!mbo.obj.visible)
			continue;
		mbo.dd.init_from(*mbo.obj.meshbuilder);
	}
	for (auto& po_ : scene.particle_objs.objects) {
		auto& po = po_.type_;
		po.dd.init_from(*po.obj.meshbuilder);
	}

	// For TAA, double buffer bones

	scene.flip_bone_buffers();
	auto mgr = GameAnimationMgr::inst;
	assert(mgr);

	if (mgr->get_num_matricies_used() > scene.gpu_skinned_mats_buffer_size / 2)
		Fatalf("out of animated buffer memory\n");

	glNamedBufferSubData(
		scene.gpu_skinned_mats_buffer,
		scene.get_front_bone_buffer_offset() * sizeof(glm::mat4),
		sizeof(glm::mat4) * mgr->get_num_matricies_used(),
		mgr->get_bonemat_ptr(0)
	);
}


void Renderer::scene_draw(SceneDrawParamsEx params, View_Setup view)
{
	GPUFUNCTIONSTART;
	ZoneNamed(RendererSceneDraw,true);
	TracyGpuZone("scene_draw");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	r_taa_manager.start_frame();
	if (r_taa_32f.was_changed()) {
		refresh_render_targets_next_frame = true;
	}

	matman.pre_render_update();
	spotShadows->update();
	check_cubemaps_dirty();

	const bool temp_disable_taa = view.is_ortho;	// ortho view doesnt work with TAA

	if (temp_disable_taa) {
		disable_taa_this_frame = true;
	}

	// modify view_setup for TAA, fixme
	if(r_taa_enabled.get_bool() && !temp_disable_taa)
	{
		view.proj = r_taa_manager.add_jitter_to_projection(view.proj, r_taa_manager.calc_frame_jitter(view.width,view.height));
		view.viewproj = view.proj * view.view;
	}

	scene_draw_internal(params, view);
	last_frame_main_view = view;

	// swap last frame and current frame, fixme
	if (r_taa_enabled.get_bool() && !temp_disable_taa) {
		std::swap(tex.last_scene_color, tex.scene_color);
		std::swap(tex.last_scene_motion, tex.scene_motion);

		tex.scene_color_vts_handle->update_specs_ptr(tex.scene_color, cur_w, cur_h, 3, {});
		tex.scene_motion_vts_handle->update_specs_ptr(tex.scene_motion, cur_w, cur_h, 2, {});

		glNamedFramebufferTexture(fbo.forward_render, GL_COLOR_ATTACHMENT0, tex.scene_color->get_internal_handle(), 0);
		glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT3, tex.scene_color->get_internal_handle(), 0);
		glNamedFramebufferTexture(fbo.gbuffer, GL_COLOR_ATTACHMENT5, tex.scene_motion->get_internal_handle(), 0);
	}
}

void get_view_mat(int idx, glm::vec3 pos, glm::mat4& view, glm::vec3& front);

void Renderer::update_cubemap_specular_irradiance(glm::vec3 ambientCube[6], Texture* cubemap, glm::vec3 position, bool skybox_only)
{
	const int specular_cubemap_size = EnviornmentMapHelper::CUBEMAP_SIZE;
	const int num_mips = Texture::get_mip_map_count(specular_cubemap_size, specular_cubemap_size);
	assert(cubemap);
	//static Texture* somthing = nullptr;
	if (cubemap->gl_id == 0) {	// not created yet
		glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubemap->gl_id);
		glTextureStorage2D(cubemap->gl_id, num_mips, GL_RGB16F, specular_cubemap_size, specular_cubemap_size);	
		glTextureParameteri(cubemap->gl_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(cubemap->gl_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(cubemap->gl_id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTextureParameteri(cubemap->gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(cubemap->gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		cubemap->type = Texture_Type::TEXTYPE_CUBEMAP;
		cubemap->width = cubemap->height = specular_cubemap_size;

		auto set_default_parameters = [](uint32_t handle) {
			glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		};

		//auto somthing = Texture::install_system("_TEST");
		//somthing->update_specs(cubemap->gl_id, CUBEMAP_SIZE, CUBEMAP_SIZE, 3, {});
		//somthing->type = Texture_Type::TEXTYPE_2D;

		//glCreateTextures(GL_TEXTURE_2D, 1, &somthing->gl_id);
		//glTextureStorage2D(somthing->gl_id, 1, GL_RGB16F, 512, 512);
		//set_default_parameters(somthing->gl_id);
		//somthing->width = somthing->height = 512;
		//somthing->type = Texture_Type::TEXTYPE_2D;
	}
	
	auto& helper = EnviornmentMapHelper::get();

	fbohandle cubemap_fbo{};
	glCreateFramebuffers(1, &cubemap_fbo);
	unsigned int attachments[1] = { GL_COLOR_ATTACHMENT0 };
	glNamedFramebufferDrawBuffers(cubemap_fbo, 1, attachments);

	for (int i = 0; i < 6; i++) {
		glm::mat4 viewmat;
		glm::vec3 viewfront;
		get_view_mat(i, position, viewmat, viewfront);
		View_Setup cubemap_view(viewmat, glm::radians(90.f), 0.01, 100.f, specular_cubemap_size, specular_cubemap_size);

		SceneDrawParamsEx params(GetTime(),0.016f);
		params.draw_ui = false;
		params.draw_world = true;
		params.is_editor = false;
		params.output_to_screen = false;
		params.is_cubemap_view = true;
		params.skybox_only = skybox_only;

		scene_draw_internal(params, cubemap_view);

		glDepthMask(GL_TRUE);// need to set this for blit operation to work

		// set cubemap texture to a temp framebuffer
		glNamedFramebufferTextureLayer(cubemap_fbo, GL_COLOR_ATTACHMENT0, cubemap->gl_id, 0/* highest mip*/, i/* face index*/);
		//glNamedFramebufferTexture(cubemap_fbo, GL_COLOR_ATTACHMENT0, somthing->gl_id, 0);
		// blit output to framebuffer
		glBlitNamedFramebuffer(fbo.forward_render, cubemap_fbo,
			0, 0, specular_cubemap_size, specular_cubemap_size,
			0, 0, specular_cubemap_size, specular_cubemap_size,
			GL_COLOR_BUFFER_BIT,
			GL_NEAREST);
	}

	glDeleteFramebuffers(1, &cubemap_fbo);

	helper.compute_specular_new(cubemap);
	helper.compute_irradiance_new(cubemap, ambientCube);
}

void Renderer::check_cubemaps_dirty()
{
	GPUFUNCTIONSTART;

	bool had_changes = false;
	double start = GetTime();
	if (!scene.skylights.empty() && (scene.skylights[0].skylight.wants_update|| force_render_cubemaps.get_bool())) {
		sys_print(Debug,"check_cubemaps_dirty:rendering skylight cubemap\n");
		auto& skylight = scene.skylights[0];
		update_cubemap_specular_irradiance(skylight.ambientCube, (Texture*)skylight.skylight.generated_cube, glm::vec3(0.f), true);
		skylight.skylight.wants_update = false;

		auto up = colorvec_linear_to_srgb(glm::vec4(skylight.ambientCube[2],0.0));
		auto down = colorvec_linear_to_srgb(glm::vec4(skylight.ambientCube[3], 0.0));

		sys_print(Info, "skylight cubemap up/down irrad: (%f %f %f) (%f %f %f)\n", up.x, up.y, up.z, down.x, down.y, down.z);
		had_changes = true;
	}
	auto& vols = scene.reflection_volumes.objects;
	for (int i = 0; i < vols.size(); i++) {
		auto& vol = vols[i].type_;
		if (vol.wants_update||force_render_cubemaps.get_bool()) {
			sys_print(Debug, "check_cubemaps_dirty:rendering reflection vol cubemap\n");
			glm::vec3 dummy[6];
			update_cubemap_specular_irradiance(dummy, vol.generated_cube, vol.probe_position, false);
			vol.wants_update = false;
			had_changes = true;
		}
	}
	force_render_cubemaps.set_bool(false);

	if (had_changes) {
		double now = GetTime();
		sys_print(Debug, "Renderer::check_cubemaps_dirty: time %f\n", float(now - start));
	}
}
ConfigVar r_no_postprocess("r.skip_pp", "0", CVAR_BOOL | CVAR_DEV,"disable post processing");
ConfigVar r_devicecycle("r.devicecycle", "0", CVAR_INTEGER | CVAR_DEV, "", 0, 10);
ConfigVar r_taa_blend("r.taa_blend", "0.75", CVAR_FLOAT, "", 0, 1.0);
ConfigVar r_taa_flicker_remove("r.taa_flicker_remove", "1", CVAR_BOOL, "");
ConfigVar r_taa_reproject("r.taa_reproject", "0", CVAR_BOOL, "");
ConfigVar r_taa_dilate_velocity("r.taa_dilate_velocity", "1", CVAR_BOOL, "");
static float taa_doc_mult = 80.0;
static float taa_doc_vel_bias = 0.001;
static float taa_doc_bias = 0.2;
static float taa_doc_pow = 0.15;

void taa_menu()
{
	ImGui::DragFloat("taa_doc_mult", &taa_doc_mult, 0.1, 1, 100);
	ImGui::DragFloat("taa_doc_vel_bias", &taa_doc_vel_bias, 0.001, 0.0001, 0.01);
	ImGui::DragFloat("taa_doc_bias", &taa_doc_bias, 0.01, 0.001, 0.2);
	ImGui::DragFloat("taa_doc_pow", &taa_doc_pow, 0.01, 0, 1);
}
ADD_TO_DEBUG_MENU(taa_menu);
static float pp_contrast = 1.0;
static float pp_saturation = 1.0;
static float pp_exposure = 1.0;
static float pp_bloom_add = 0.05;
static glm::vec3 pp_color_tint = glm::vec3(1.f);
static int pp_tonemap_type = 0;




void post_process_menu()
{
	if (ImGui::InputInt("pp_tonemap_type", &pp_tonemap_type)) {
		pp_tonemap_type = glm::clamp(pp_tonemap_type, 0, 3);
	}
	ImGui::DragFloat("pp_contrast", &pp_contrast,0.01);
	ImGui::DragFloat("pp_saturation", &pp_saturation,0.01);
	ImGui::DragFloat("pp_exposure", &pp_exposure, 0.01);
	ImGui::DragFloat("pp_bloom_add", &pp_bloom_add, 0.0001);

}
ADD_TO_DEBUG_MENU(post_process_menu);


void Renderer::scene_draw_internal(SceneDrawParamsEx params, View_Setup view)
{
	TracyGpuZone("scene_draw_internal");
	ZoneScoped;

	current_time = GetTime();

	mem_arena.free_bottom();
	stats = Render_Stats();
	device.reset_states();

	if (view.width < 4 || view.height < 4) {
		sys_print(Error, "framebuffer too small for scene draw internal\n");
		return;
	}

	if (refresh_render_targets_next_frame || cur_w != view.width || cur_h != view.height)
		InitFramebuffers(true, view.width, view.height);

	current_frame_view = view;

	if (!params.draw_world&&!params.draw_ui)
		return;
	else if (!params.draw_world && params.draw_ui) {

		const auto& view_to_use = current_frame_view;
		assert(cur_w == view_to_use.width && cur_h == view_to_use.height);
		RenderPassSetup setup("composite", fbo.composite, true, true, 0, 0, view_to_use.width, view_to_use.height);
		auto scope = device.start_render_pass(setup);

		//draw_ui_local.render();

		windowDrawer->render();

		if (params.output_to_screen) {
			GPUSCOPESTART(Blit_composite_to_backbuffer);

			glBlitNamedFramebuffer(
				fbo.composite,
				0,	/* blit to backbuffer */
				0, 0, cur_w, cur_h,
				0, 0, cur_w, cur_h,
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST
			);
		}
		return;
	}
	upload_ubo_view_constants(current_frame_view, ubo.current_frame);
	scene.build_scene_data(params.skybox_only, params.is_editor);

	shadowmap.update();

	//volfog.compute();
	const bool is_wireframe_mode = r_debug_mode.get_integer() == gpu::DEBUG_WIREFRAME;

	// main level render


	auto gbuffer_pass = [&](bool is_wireframe = false, bool wireframe_secondpass = false) {
		if (r_skip_gbuffer.get_bool())
			return;


		const auto& view_to_use = current_frame_view;

		const bool clear_framebuffer = (!is_wireframe || !wireframe_secondpass);

		RenderPassSetup setup("gbuffer", fbo.gbuffer, clear_framebuffer,clear_framebuffer, 0, 0, view_to_use.width, view_to_use.height);
		auto scope = device.start_render_pass(setup);

		{
			GPUSCOPESTART(GbufferPass);

			Render_Level_Params cmdparams(
				view_to_use,
				&scene.gbuffer_rlist,
				&scene.gbuffer_pass,
				Render_Level_Params::OPAQUE
			);

			cmdparams.upload_constants = true;
			cmdparams.provied_constant_buffer = ubo.current_frame;
			cmdparams.draw_viewmodel = true;
			cmdparams.wireframe_secondpass = wireframe_secondpass;
			cmdparams.is_wireframe_pass = is_wireframe;

			render_level_to_target(cmdparams);
		}
	
	};

	if (is_wireframe_mode) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glLineWidth(3);
		gbuffer_pass(true,false);
		glLineWidth(1);
		gbuffer_pass(true,true);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	else
		gbuffer_pass();

	//device.reset_states();
	

	deferred_decal_pass();
		//device.reset_states();

	if (enable_ssao.get_bool()&&!params.is_cubemap_view)
		ssao.render();

	if(r_debug_mode.get_integer() == 0 && !params.skybox_only)
		accumulate_gbuffer_lighting(params.is_cubemap_view);


	//draw_height_fog();

	{
		GPUSCOPESTART(ForwardPass);

		const auto& view_to_use = current_frame_view;
		RenderPassSetup setup("transparents", fbo.forward_render, false, false, 0, 0, view_to_use.width, view_to_use.height);
		auto scope = device.start_render_pass(setup);

		Render_Level_Params params(
			view_to_use,
			&scene.transparent_rlist,
			&scene.transparent_pass,
			Render_Level_Params::FORWARD_PASS
		);
		
		params.upload_constants = true;
		params.provied_constant_buffer = ubo.current_frame;
		params.draw_viewmodel = true;

		render_level_to_target(params);

		render_particles();
	}


	// cubemap views end here
	// dont need to draw post processing or UI stuff
	if (params.is_cubemap_view)
		return;

	if(params.is_editor)
	{
		GPUSCOPESTART(EDITORSELECT_PASS);

		const auto& view_to_use = current_frame_view;
		RenderPassSetup setup("editor-id", fbo.editorSelectionDepth, false, true/* clear depth*/, 0, 0, view_to_use.width, view_to_use.height);
		auto scope = device.start_render_pass(setup);

		Render_Level_Params params(
			view_to_use,
			&scene.editor_sel_rlist,
			&scene.editor_sel_pass,
			Render_Level_Params::DEPTH
		);
		params.provied_constant_buffer = ubo.current_frame;
		render_level_to_target(params);
	}

	device.reset_states();
	
	// mesh builder stuff
	{
		const auto& view_to_use = current_frame_view;
		RenderPassSetup setup("meshbuilders", fbo.forward_render, false, false, 0, 0, view_to_use.width, view_to_use.height);
		auto scope = device.start_render_pass(setup);
		draw_meshbuilders();
	}

	auto taa_resolve_pass = [&]() -> texhandle {
		GPUSCOPESTART(TaaResolve);
		ZoneScopedN("TaaResolve");
		bool wants_disable = disable_taa_this_frame;
		disable_taa_this_frame = false;
		//if (wants_disable)
		//	sys_print(Debug, "disabled taa this frame\n");
		if (!r_taa_enabled.get_bool()||wants_disable) {
			return tex.scene_color->get_internal_handle();
		}

		glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo.current_frame);

		// write to tex.scene_gbuffer0
		RenderPassSetup setup("taa_resolve", fbo.taa_resolve, false, false, 0, 0, cur_w, cur_h);
		auto scope = device.start_render_pass(setup);
		RenderPipelineState state;
		state.program = prog.taa_resolve;
		state.vao = get_empty_vao();
		device.set_pipeline(state);
		shader().set_float("amt", r_taa_blend.get_float());
		shader().set_bool("remove_flicker", r_taa_flicker_remove.get_bool());
		shader().set_mat4("lastViewProj", last_frame_main_view.viewproj);
		shader().set_bool("use_reproject", r_taa_reproject.get_bool());
		shader().set_float("doc_mult", taa_doc_mult);
		shader().set_float("doc_vel_bias", taa_doc_vel_bias);
		shader().set_float("doc_bias", taa_doc_bias);
		shader().set_float("doc_pow", taa_doc_pow);
		shader().set_bool("dilate_velocity", r_taa_dilate_velocity.get_bool());

		bind_texture_ptr(0, tex.scene_color);
		bind_texture_ptr(1, tex.last_scene_color);
		bind_texture_ptr(2, tex.scene_depth);
		bind_texture_ptr(3, tex.scene_motion);
		bind_texture_ptr(4, tex.last_scene_motion);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glNamedFramebufferTexture(fbo.taa_blit, GL_COLOR_ATTACHMENT0, tex.scene_color->get_internal_handle(), 0);
		glBlitNamedFramebuffer(fbo.taa_resolve, fbo.taa_blit, 0, 0, cur_w, cur_h,
			0, 0, cur_w, cur_h, GL_COLOR_BUFFER_BIT,
			GL_NEAREST);

		return tex.scene_color->get_internal_handle();
	};
	const texhandle scene_color_handle = taa_resolve_pass();

	// last_scene
	// scene
	// gbuffer0 = taa_resolve(scene, last_scene)
	// last_scene = blit(gbuffer0)
	// scene_color_handle = gbuffer0
	// render_transparents(scene_color_handle)

	// Bloom update
	render_bloom_chain(scene_color_handle);

	{
		const auto& view_to_use = current_frame_view;
		assert(cur_w == view_to_use.width && cur_h == view_to_use.height);
		RenderPassSetup setup("composite", fbo.composite, true, false, 0, 0, view_to_use.width, view_to_use.height);
		auto scope = device.start_render_pass(setup);


		{
			RenderPipelineState state;
			state.program = prog.combine;
			state.vao = get_empty_vao();
			device.set_pipeline(state);

			uint32_t bloom_tex = tex.bloom_chain[0].texture->get_internal_handle();
			if (!enable_bloom.get_bool())
				bloom_tex = black_texture.gl_id;
			bind_texture(0, scene_color_handle);
			bind_texture(1, bloom_tex);
			bind_texture(2, lens_dirt->gl_id);

			shader().set_int("tonemap_type", pp_tonemap_type);
			shader().set_float("contrast_tweak", pp_contrast);
			shader().set_float("saturation_tweak", pp_saturation);
			shader().set_float("bloom_lerp", pp_bloom_add);
			shader().set_float("exposure", pp_exposure);





			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
		{
			std::vector<MaterialInstance*> postProcesses;
			if (r_debug_mode.get_integer() == DEBUG_OUTLINED) {
				auto mat = g_assets.find_global_sync<MaterialInstance>("eng/editorEdgeDetect.mm");
				if (mat.get() && mat->impl->gpu_buffer_offset != mat->impl->INVALID_MAPPING)
					postProcesses.push_back(mat.get());
			}
			if (params.is_editor) {
				postProcesses.push_back(matman.get_default_editor_sel_PP());
			}
			if (!r_no_postprocess.get_bool())
				do_post_process_stack(postProcesses);
		}

		{
			// UI
			if (params.draw_ui && !r_force_hide_ui.get_bool()) {
				windowDrawer->render();
			}

		}
		
		debug_tex_out.draw_out();
	}
	if (params.output_to_screen) {
		GPUSCOPESTART(Blit_composite_to_backbuffer);

		glBlitNamedFramebuffer(
			fbo.composite,
			0,	/* blit to backbuffer */
			0, 0, cur_w, cur_h,
			0, 0, cur_w, cur_h,
			GL_COLOR_BUFFER_BIT,
			GL_NEAREST
		);
	}

	glNamedFramebufferTexture(fbo.composite, GL_COLOR_ATTACHMENT0, tex.output_composite->get_internal_handle(), 0);
}



Shader Renderer::shader()
{
	return device.shader();
}


void Renderer::do_post_process_stack(const std::vector<MaterialInstance*>& postProcessMats)
{
	ZoneScoped;


	auto renderToTexture = tex.output_composite_2;
	auto renderFromTexture = tex.output_composite;
	tex.actual_output_composite = renderFromTexture;
	for (int i = 0; i < postProcessMats.size(); i++) {
		if (!postProcessMats[i])
			continue;
		glNamedFramebufferTexture(fbo.composite, GL_COLOR_ATTACHMENT0, renderToTexture->get_internal_handle(), 0);
		tex.postProcessInput_vts_handle->update_specs_ptr(renderFromTexture, cur_w, cur_h, 3, {});

		auto mat = postProcessMats[i];

		RenderPipelineState state;
		state.program = matman.get_mat_shader(nullptr, mat,0);
		state.blend = mat->get_master_material()->blend;
		state.depth_testing = state.depth_writes = false;
		state.vao = get_empty_vao();
		state.backface_culling = false;
		device.set_pipeline(state);

		auto& texs = mat->impl->get_textures();

		for (int i = 0; i < texs.size(); i++) {
			bind_texture(i, texs[i]->gl_id);
		}


		glDrawArrays(GL_TRIANGLES, 0, 3);


		tex.actual_output_composite = renderToTexture;

		auto temp = renderFromTexture;
		renderFromTexture = renderToTexture;
		renderToTexture = temp;
	}

	glNamedFramebufferTexture(fbo.composite, GL_COLOR_ATTACHMENT0, renderFromTexture->get_internal_handle(), 0);
}


static void get_view_mat(int idx, glm::vec3 pos, glm::mat4& view, glm::vec3& front)
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


RSunInternal* Render_Scene::get_main_directional_light() {
	if (!suns.empty())
		return &suns.at(suns.size() - 1);
	return nullptr;
}
Render_Scene::~Render_Scene() {}

void Renderer::on_level_end()
{
}
void Renderer::on_level_start()
{
	disable_taa_this_frame = true;
}

ConfigVar r_disable_animated_velocity_vector("r.disable_animated_velocity_vector", "0", CVAR_BOOL|CVAR_DEV, "");

void Render_Scene::update_obj(handle<Render_Object> handle, const Render_Object& proxy)
{
	ASSERT(!eng->get_is_in_overlapped_period());
	ROP_Internal& in = proxy_list.get(handle.id);
	if (proxy.is_skybox)
		in.is_static = false;

	// mark dirty if model or material changed
	if(in.is_static && (proxy.model!=in.proxy.model||proxy.mat_override!=in.proxy.mat_override))
		statics_meshes_are_dirty = true;

	in.prev_transform = in.proxy.transform;
	in.prev_bone_ofs = in.proxy.animator_bone_ofs;
	in.proxy = proxy;
	if (!in.has_init) {
		in.has_init = true;
		in.prev_transform = in.proxy.transform;
		in.prev_bone_ofs = -1;
	}
	//if (r_disable_animated_velocity_vector.get_bool())
	//	in.prev_bone_ofs = -1;

	if (proxy.model) {
		auto& sphere = proxy.model->get_bounding_sphere();
		auto center = proxy.transform * glm::vec4(glm::vec3(sphere),1.f);
		float max_scale = glm::max(glm::length(proxy.transform[0]), glm::max(glm::length(proxy.transform[1]), glm::length(proxy.transform[2])));
		float radius = sphere.w* max_scale;
		in.bounding_sphere_and_radius = glm::vec4(glm::vec3(center), radius);
	}
}
ConfigVar r_spot_near("r_spot_near", "0.1", CVAR_FLOAT, "", 0, 2);
void Render_Scene::update_light(handle<Render_Light> handle, const Render_Light& proxy) {
	ASSERT(!eng->get_is_in_overlapped_period());
	auto& l = light_list.get(handle.id);
	l.light = proxy;
	l.updated_this_frame = true;

	if (l.light.casts_shadow_mode != 0 && l.light.is_spotlight) {
		auto& p = l.light.position;
		auto& n = l.light.normal;
		glm::vec3 up = glm::vec3(0, 1, 0);
		if (glm::abs(glm::dot(up, n)) > 0.999)
			up = glm::vec3(1, 0, 0);
		auto viewMat = glm::lookAt(p, p + n, up);
		float fov = glm::radians(l.light.conemax) * 2.0;
		auto proj = glm::perspectiveRH_ZO(fov, 1.0f, l.light.radius, r_spot_near.get_float());
		//proj[2][2] *= -1.0f;	// reverse z // [1,0]
		//proj[3][2] *= -1.0f;

		l.lightViewProj = proj * viewMat;
	}

}

void Render_Scene::remove_light(handle<Render_Light>& handle) {
	if (eng->get_is_in_overlapped_period()) {
		add_to_queued_deletes(handle.id, RenderObjectTypes::Light);
		handle = { -1 };
		return;
	}
	if (!handle.is_valid())
		return;
	draw.spotShadows->on_remove_light(handle);
	light_list.free(handle.id);
	handle = { -1 };
}



void DebuggingTextureOutput::draw_out()
{
	if (!output_tex)
		return;
	if (output_tex->gl_id == 0) {
		sys_print(Error, "DebuggingTextureOutput has invalid texture\n");
		output_tex = nullptr;
		return;
	}

	auto& device = draw.get_device();

	RenderPipelineState state;
	state.vao = draw.get_empty_vao();
	state.blend = blend_state::BLEND;

	if (output_tex->type == Texture_Type::TEXTYPE_2D)
		state.program = (draw.prog.tex_debug_2d);
	else if (output_tex->type == Texture_Type::TEXTYPE_2D_ARRAY)
		state.program = (draw.prog.tex_debug_2d_array);
	else if (output_tex->type == Texture_Type::TEXTYPE_CUBEMAP)
		state.program = (draw.prog.tex_debug_cubemap);
	else {
		sys_print(Error, "can only debug 2d and 2d array textures\n");
		output_tex = nullptr;
		return;
	}

	const int w = output_tex->width;
	const int h = output_tex->height;

	const float cur_w = draw.get_current_frame_vs().width;
	const float cur_h = draw.get_current_frame_vs().height;

	device.set_pipeline(state);


	draw.shader().set_mat4("Model", mat4(1));
	glm::mat4 proj = glm::ortho(0.f, cur_w, cur_h, 0.f);
	draw.shader().set_mat4("ViewProj", proj);

	draw.shader().set_float("alpha", alpha);
	draw.shader().set_float("mip_slice", output_tex->type == Texture_Type::TEXTYPE_2D ?
		-1.f
		: mip);

	draw.bind_texture(0,  output_tex->gl_id);

	glm::vec2 upper_left = glm::vec2(0, 1);
	glm::vec2 size = glm::vec2(1, -1);

	MeshBuilderDD dd;
	MeshBuilder mb;
	mb.Begin();
	mb.Push2dQuad(glm::vec2(0, 0), glm::vec2(w * scale, h * scale), upper_left, size, {});
	mb.End();
	dd.init_from(mb);

	dd.draw(MeshBuilderDD::TRIANGLES);
	dd.free();

}
#ifdef EDITOR_BUILD
float Renderer::get_scene_depth_for_editor(int x, int y)
{
	ASSERT(!eng->get_is_in_overlapped_period());
	// super slow garbage functions obviously

	if (x < 0 || y < 0 || x >= cur_w || y >= cur_h) {
		sys_print(Error, "invalid mouse coords for mouse_pick_scene\n");
		return { -1 };
	}

	glFlush();
	glFinish();

	const size_t size = cur_h * cur_w;
	float* buffer_pixels = new float[size];

	glGetTextureImage(tex.scene_depth->get_internal_handle(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, size*sizeof(float), buffer_pixels);

	y = cur_h - y - 1;

	const size_t ofs = cur_w * y + x;
	const float depth = buffer_pixels[ofs];
	delete[] buffer_pixels;

	return -current_frame_view.near / depth;// linearize_depth(depth, vs.near, vs.far);
}

handle<Render_Object> Renderer::mouse_pick_scene_for_editor(int x, int y)
{
	auto handles = mouse_box_select_for_editor(x, y, 1, 1);
	if (handles.empty()) return { -1 };
	return handles.at(0);
}

std::vector<handle<Render_Object>> Renderer::mouse_box_select_for_editor(int x, int y, int w, int h)
{
	assert(!eng->get_is_in_overlapped_period());
	sys_print(Debug, "Renderer::mouse_box_select_for_editor\n");
	assert(w >= 0 && h >= 0);
	// super DUPER slow garbage functions obviously
	if (x < 0 || y < 0 || x >= cur_w || y >= cur_h ||x+w>=cur_w||y+h>=cur_h) {
		sys_print(Error, "Renderer::mouse_box_select_for_editor: invalid mouse coords\n");
		return {};
	}
	glFlush();
	glFinish();
	const int size = cur_h * cur_w * 4;
	std::vector<uint8_t> bufferPixels(size,0);
	glGetTextureImage(tex.editor_id_buffer->get_internal_handle(), 0, GL_RGBA, GL_UNSIGNED_BYTE, size, bufferPixels.data());
	y = cur_h - y - 1;
	std::unordered_set<int> found;
	const int skip_pixels = 4;	// check every 4 pixels
	for (int xCoordOfs = 0; xCoordOfs < w; xCoordOfs+= skip_pixels) {
		for (int yCoordOfs = 0; yCoordOfs < h; yCoordOfs+= skip_pixels) {
			const int xCoord = x + xCoordOfs;
			const int yCoord = y - yCoordOfs;
			assert(yCoord >= 0);
			const int ofs = cur_w * yCoord * 4 + xCoord * 4;
			assert(ofs >= 0 && ofs < (int)bufferPixels.size());
			uint8_t* ptr = &bufferPixels.at(ofs);
			uint32_t id = uint32_t(ptr[0]) | uint32_t(ptr[1]) << 8 | uint32_t(ptr[2]) << 16 | uint32_t(ptr[3]) << 24;
			if (id != 0xff000000) {
				uint32_t realid = id - 1;	// allow for nullptr
				if (realid < scene.proxy_list.objects.size()) {
					int handle_out = scene.proxy_list.objects.at(realid).handle;
					found.insert(handle_out);
				}
			}
		}
	}

	std::vector<handle<Render_Object>> outObjs;
	for (int f : found)
		outObjs.push_back(handle<Render_Object>{f});
	return outObjs;
}
#endif

bool CheckGlErrorInternal_(const char* file, int line)
{
	GLenum error_code = glGetError();
	bool has_error = 0;
	while (error_code != GL_NO_ERROR)
	{
		has_error = true;
		const char* error_name = "Unknown error";
		switch (error_code)
		{
		case GL_INVALID_ENUM:
			error_name = "GL_INVALID_ENUM"; break;
		case GL_INVALID_VALUE:
			error_name = "GL_INVALID_VALUE"; break;
		case GL_INVALID_OPERATION:
			error_name = "GL_INVALID_OPERATION"; break;
		case GL_STACK_OVERFLOW:
			error_name = "GL_STACK_OVERFLOW"; break;
		case GL_STACK_UNDERFLOW:
			error_name = "GL_STACK_UNDERFLOW"; break;
		case GL_OUT_OF_MEMORY:
			error_name = "GL_OUT_OF_MEMORY"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			error_name = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
		default:
			break;
		}
		sys_print(Error, "%s | %s (%d)\n", error_name, file, line);

		error_code = glGetError();
	}
	return has_error;
}