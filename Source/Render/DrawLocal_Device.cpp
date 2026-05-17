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
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "tracy/public/tracy/Tracy.hpp"
#include <tracy/public/tracy/TracyOpenGL.hpp>
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"
#include <algorithm>

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
static const int DEBUG_OUTLINED = 100; // uses objID

void Renderer::InitGlState() {
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

void Renderer::bind_texture(int bind, int id) {
	device.bind_texture(bind, id);
}

static int combine_flags_type(int flags, int type, int flag_bits) {
	return flags + (type >> flag_bits);
}

program_handle Program_Manager::create_single_file(const std::string& shared_file, bool is_tesseltion,
												   const std::string& defines) {
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
program_handle Program_Manager::create_raster(const std::string& vert, const std::string& frag,
											  const std::string& defines) {
	program_def def;
	def.vert = vert;
	def.frag = frag;
	def.defines = defines;
	def.is_compute = false;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
program_handle Program_Manager::create_raster_geo(const std::string& vert, const std::string& frag,
												  const std::string& geo, const std::string& defines) {
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
program_handle Program_Manager::create_compute(const std::string& compute, const std::string& defines) {
	program_def def;
	def.vert = compute;
	def.defines = defines;
	def.is_compute = true;
	programs.push_back(def);
	recompile(programs.back());
	return programs.size() - 1;
}
void Program_Manager::recompile_all() {
	for (int i = 0; i < programs.size(); i++)
		recompile(programs[i]);
}
#include "Framework/StringUtils.h"
#include "Framework/BinaryReadWrite.h"
//
string compute_hash_for_program_def(Program_Manager::program_def& def) {
	string inp = def.vert + def.frag + def.geo + def.defines;
	return StringUtils::alphanumeric_hash(inp);
}

void Program_Manager::recompile(program_def& def) {
	double start = GetTime();
	recompile_do(def);
	float time = GetTime() - start;
	if (log_shader_compiles.get_bool())
		sys_print(Debug, "Program_Manager::recompile: compiled/loaded %s in %f\n", def.vert.c_str(), time);
}

void Program_Manager::recompile_shared(program_def& def) {
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
			} else {
				return; // done
			}
		}
	}
	binFile.reset();

	// fail path
	def.compile_failed =
		Shader::compile_vert_frag_single_file(&def.shader_obj, def.vert, def.defines) != ShaderResult::SHADER_SUCCESS;

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
		} else {
			sys_print(Error, "Program_Manager::recompile: couldnt open file to write program binary: %s\n",
					  hashed_path.data());
		}
	}
}

void Program_Manager::recompile_normal(program_def& def) {
	string hashed_path = compute_hash_for_program_def(def) + ".bin";
	auto binFile = FileSys::open_read(hashed_path.c_str(), FileSys::SHADER_CACHE);
	auto shaderFile = FileSys::open_read_engine(("Shaders\\" + def.vert).c_str());
	auto shaderFileF = FileSys::open_read_engine(("Shaders\\" + def.frag).c_str());

	if (shaderFile && binFile) {
		if (shaderFile->get_timestamp() <= binFile->get_timestamp() &&
			shaderFileF->get_timestamp() <= binFile->get_timestamp()) {
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
			} else {
				return; // done
			}
		}
	}
	binFile.reset();

	// fail path
	if (!def.geo.empty())
		def.compile_failed = !Shader::compile(def.shader_obj, def.vert, def.frag, def.geo, def.defines);
	else
		def.compile_failed =
			Shader::compile(&def.shader_obj, def.vert, def.frag, def.defines) != ShaderResult::SHADER_SUCCESS;

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
		} else {
			sys_print(Error, "Program_Manager::recompile: couldnt open file to write program binary: %s\n",
					  hashed_path.data());
		}
	}
}

void Program_Manager::recompile_do(program_def& def) {
	// look in shader cache, only for "shared shaders" now, these are the main materials so whatev
	if (def.is_shared() && !def.is_tesselation) {
		// if (!def.program) {
		//	CreateProgramArgs args;
		//	args.file_name = def.vert;
		//	args.defines = def.defines;
		//	def.program = gfx().create_program(args);
		//	def.shader_obj.ID = def.program->get_internal_handle();
		//	def.compile_failed = false;
		//}

		recompile_shared(def);
		return;
	}

	// Free the prior program (if any) before installing the new IGraphicsShader.
	// Ownership lives in exactly one place: gfx_shader if non-null, else the
	// raw shader_obj.ID (binary-cache path, 1.7d).
	auto release_prior_program = [&]() {
		if (def.gfx_shader) {
			safe_release(def.gfx_shader);
		} else if (def.shader_obj.ID != 0) {
			glDeleteProgram(def.shader_obj.ID);
		}
		def.shader_obj.ID = 0;
	};

	if (def.is_compute) {
		release_prior_program();
		def.gfx_shader = gfx().create_shader_compute(def.vert, def.defines);
		def.compile_failed = (def.gfx_shader == nullptr);
		def.shader_obj.ID = def.gfx_shader ? def.gfx_shader->get_internal_handle() : 0;
	} else if (def.is_shared()) {
		assert(def.is_tesselation);
		release_prior_program();
		def.gfx_shader = gfx().create_shader_single_file_tess(def.vert, def.defines);
		def.compile_failed = (def.gfx_shader == nullptr);
		def.shader_obj.ID = def.gfx_shader ? def.gfx_shader->get_internal_handle() : 0;
	} else {
		recompile_normal(def);

		// if (!def.geo.empty())
		//	def.compile_failed = !Shader::compile(def.shader_obj, def.vert, def.frag, def.geo, def.defines);
		// else
		//	def.compile_failed = Shader::compile(&def.shader_obj, def.vert, def.frag, def.defines) !=
		// ShaderResult::SHADER_SUCCESS;
	}
}

void Renderer::bind_vao(uint32_t vao) {
	device.set_vao(vao);
}

void Renderer::set_blend_state(BlendState blend) {
	device.set_blend_state(blend);
}
void Renderer::set_show_backfaces(bool show_backfaces) {
	device.set_show_backfaces(show_backfaces);
}

void Renderer::set_shader(program_handle handle) {
	device.set_shader(handle);
}

void OpenglRenderDevice::bind_texture(int bind, int id) {
	ASSERT(bind >= 0 && bind < MAX_SAMPLER_BINDINGS);
	bool invalid = is_bit_invalid(TEXTURE0_BIT + bind);
	if (invalid || textures_bound[bind] != id) {
		set_bit_valid(TEXTURE0_BIT + bind);
		glBindTextureUnit(bind, id);
		textures_bound[bind] = id;
		activeStats.texture_binds++;
	}
}

void OpenglRenderDevice::set_vao(vertexarrayhandle vao) {
	bool invalid = is_bit_invalid(VAO_BIT);
	if (invalid || vao != current_vao) {
		set_bit_valid(VAO_BIT);
		current_vao = vao;
		glBindVertexArray(vao);
		activeStats.vertex_array_changes++;
	}
}

void OpenglRenderDevice::set_blend_state(BlendState blend) {
	bool invalid = is_bit_invalid(BLENDING_BIT);
	if (invalid || blend != blending) {
		if (blend == BlendState::OPAQUE)
			glDisable(GL_BLEND);
		else if (blend == BlendState::ADD) {
			if (invalid || blending == BlendState::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
		} else if (blend == BlendState::BLEND) {
			if (invalid || blending == BlendState::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		} else if (blend == BlendState::MULT) {
			if (invalid || blending == BlendState::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_DST_COLOR, GL_ZERO);
		} else if (blend == BlendState::SCREEN) {
			if (invalid || blending == BlendState::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		} else if (blend == BlendState::PREMULT_BLEND) {
			if (invalid || blending == BlendState::OPAQUE)
				glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}
		blending = blend;
		set_bit_valid(BLENDING_BIT);
		activeStats.blend_changes++;
	}
}
void OpenglRenderDevice::set_show_backfaces(bool show_backfaces) {
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
void OpenglRenderDevice::set_depth_test_enabled(bool enabled) {
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
void OpenglRenderDevice::set_depth_write_enabled(bool enabled) {
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
void OpenglRenderDevice::set_cull_front_face(bool enabled) {
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

void OpenglRenderDevice::set_shader(program_handle handle) {
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
GpuRenderPassScope OpenglRenderDevice::start_render_pass(const RenderPassSetup& setup) {
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

		set_depth_write_enabled(true); // ugh: glDepthMask applies to glClear also

		glClear(mask);
		activeStats.framebuffer_clears++;
	}
	activeStats.framebuffer_changes++;

	return GpuRenderPassScope(setup);
}
void OpenglRenderDevice::clear_framebuffer(bool clear_depth, bool clear_color, float depth_value) {
	if (clear_depth || clear_color) {
		glClearDepth(depth_value);
		glClearColor(0, 0, 0, 1);
		GLbitfield mask{};
		if (clear_depth)
			mask |= GL_DEPTH_BUFFER_BIT;
		if (clear_color)
			mask |= GL_COLOR_BUFFER_BIT;

		set_depth_write_enabled(true); // ugh: glDepthMask applies to glClear also

		glClear(mask);
		activeStats.framebuffer_clears++;
	}
}
void OpenglRenderDevice::set_depth_less_than(bool less_than) {
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

void OpenglRenderDevice::set_pipeline(const RenderPipelineState& s) {
	set_shader(s.program);
	set_blend_state(s.blend);
	set_vao(s.vao);
	set_cull_front_face(s.cull_front_face);
	set_depth_test_enabled(s.depth_testing);
	set_show_backfaces(!s.backface_culling);
	set_depth_write_enabled(s.depth_writes);
	set_depth_less_than(s.depth_less_than);
}