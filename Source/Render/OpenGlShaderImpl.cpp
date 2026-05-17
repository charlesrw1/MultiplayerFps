// OpenGLShaderImpl + GLSL source loader + GL program compile/link + program
// binary cache. Owns every gl* call that touches a shader/program in the
// engine. Phase 1.7d folded the binary cache (was in DrawLocal_Device.cpp)
// and the Shader::compile* helpers (was in Shader.cpp) into this TU and
// deleted the legacy Shader struct.

#include "OpenGlDeviceLocal.h"
#include "ShaderSourceLoader.h"
#include "glad/glad.h"

extern ConfigVar log_shader_compiles;

namespace {

// Same constant as in ShaderSourceLoader.cpp — used here for the binary-cache
// timestamp check (compares cached blob mtime against each engine-relative
// source file mtime). Kept local because the loader hides the prefix logic.
constexpr const char* OPENGL_SHADER_PATH = "Shaders\\";

bool make_shader(const char* source, GLenum type, uint32_t* gl_shader, char* error_buf, int error_buf_size) {
	int success = 0;
	*gl_shader = glCreateShader(type);
	glShaderSource(*gl_shader, 1, &source, NULL);
	glCompileShader(*gl_shader);

	glGetShaderiv(*gl_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(*gl_shader, error_buf_size, NULL, error_buf);
		return false;
	}
	return true;
}

// Link the previously-compiled shader stages into a program. Returns 0 on
// link failure (caller is responsible for deleting attached shader objects).
uint32_t link_program(std::initializer_list<uint32_t> shaders) {
	uint32_t program = glCreateProgram();
	for (uint32_t s : shaders)
		glAttachShader(program, s);
	glLinkProgram(program);

	int success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		char infolog[512];
		glGetProgramInfoLog(program, 512, NULL, infolog);
		sys_print(Error, "Error: shader program link failed: %s\n", infolog);
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

// Returns the GL program id (0 on failure). Source paths are relative to
// Shaders\\ (OPENGL_SHADER_PATH prefix added internally).
uint32_t compile_vert_frag(const std::string& vert_path, const std::string& frag_path,
						   const std::string& defines) {
	std::string defines_directive = format_shader_defines(defines);
	ShaderSource vertex_source = load_shader_source(vert_path, defines_directive);
	ShaderSource fragment_source = load_shader_source(frag_path, defines_directive);

	if (vertex_source.empty() || fragment_source.empty()) {
		sys_print(Error, "Parse fail normal %s %s\n", vert_path.c_str(), frag_path.c_str());
		return 0;
	}

	uint32_t vertex = 0, fragment = 0;
	char infolog[512] = {};
	if (!make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512)) {
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", vert_path.c_str(), infolog);
		vertex_source.print_error(infolog);
		return 0;
	}
	if (!make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512)) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", frag_path.c_str(), infolog);
		fragment_source.print_error(infolog);
		glDeleteShader(vertex);
		return 0;
	}
	uint32_t program = link_program({vertex, fragment});
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	return program;
}

uint32_t compile_vert_frag_geo(const std::string& vert_path, const std::string& frag_path,
							   const std::string& geo_path, const std::string& defines) {
	std::string defines_directive = format_shader_defines(defines);
	ShaderSource vertex_source = load_shader_source(vert_path, defines_directive);
	ShaderSource fragment_source = load_shader_source(frag_path, defines_directive);
	ShaderSource geometry_source = load_shader_source(geo_path, defines_directive);

	if (vertex_source.empty() || fragment_source.empty() || geometry_source.empty()) {
		sys_print(Error, "Parse fail normal w geo %s %s %s\n", vert_path.c_str(), frag_path.c_str(),
				  geo_path.c_str());
		return 0;
	}

	uint32_t vertex = 0, fragment = 0, geometry = 0;
	char infolog[512] = {};
	if (!make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512)) {
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", vert_path.c_str(), infolog);
		vertex_source.print_error(infolog);
		return 0;
	}
	if (!make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512)) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", frag_path.c_str(), infolog);
		fragment_source.print_error(infolog);
		glDeleteShader(vertex);
		return 0;
	}
	if (!make_shader(geometry_source.c_str(), GL_GEOMETRY_SHADER, &geometry, infolog, 512)) {
		sys_print(Error, "Error: geometry shader (%s) compiliation failed: %s\n", geo_path.c_str(), infolog);
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		return 0;
	}
	uint32_t program = link_program({vertex, fragment, geometry});
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	glDeleteShader(geometry);
	return program;
}

uint32_t compile_vert_frag_single_file(const std::string& shared_path, const std::string& defines) {
	std::string defines_directive = format_shader_defines(defines);
	ShaderSource vertex_source =
		load_shader_source(shared_path, defines_directive + "\n#define _VERTEX_SHADER\n#line 0\n", false);
	ShaderSource fragment_source =
		load_shader_source(shared_path, defines_directive + "\n#define _FRAGMENT_SHADER\n#line 0\n", false);

	if (vertex_source.empty() || fragment_source.empty()) {
		sys_print(Error, "Parse fail single file %s\n", shared_path.c_str());
		return 0;
	}

	uint32_t vertex = 0, fragment = 0;
	char infolog[512] = {};
	if (!make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512)) {
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		vertex_source.print_error(infolog);
		return 0;
	}
	if (!make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512)) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		fragment_source.print_error(infolog);
		glDeleteShader(vertex);
		return 0;
	}
	uint32_t program = link_program({vertex, fragment});
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	return program;
}

uint32_t compile_vert_frag_tess_single_file(const std::string& shared_path, const std::string& defines) {
	std::string defines_directive = format_shader_defines(defines);
	ShaderSource vertex_source =
		load_shader_source(shared_path, defines_directive + "\n#define _VERTEX_SHADER\n#line 0\n", false);
	ShaderSource fragment_source =
		load_shader_source(shared_path, defines_directive + "\n#define _FRAGMENT_SHADER\n#line 0\n", false);
	ShaderSource tess_eval_source =
		load_shader_source(shared_path, defines_directive + "\n#define _TESS_EVAL_SHADER\n#line 0\n", false);
	ShaderSource tess_ctrl_source =
		load_shader_source(shared_path, defines_directive + "\n#define _TESS_CTRL_SHADER\n#line 0\n", false);

	if (vertex_source.empty() || fragment_source.empty() || tess_eval_source.empty() || tess_ctrl_source.empty()) {
		sys_print(Error, "Parse fail single file with tess %s\n", shared_path.c_str());
		return 0;
	}

	uint32_t vertex = 0, fragment = 0, tess_control = 0, tess_eval = 0;
	char infolog[512] = {};
	if (!make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512)) {
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		return 0;
	}
	if (!make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512)) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		glDeleteShader(vertex);
		return 0;
	}
	if (!make_shader(tess_eval_source.c_str(), GL_TESS_EVALUATION_SHADER, &tess_eval, infolog, 512)) {
		sys_print(Error, "Error: tess eval shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		return 0;
	}
	if (!make_shader(tess_ctrl_source.c_str(), GL_TESS_CONTROL_SHADER, &tess_control, infolog, 512)) {
		sys_print(Error, "Error: tess ctrl shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		glDeleteShader(tess_eval);
		return 0;
	}
	uint32_t program = link_program({vertex, fragment, tess_control, tess_eval});
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	glDeleteShader(tess_eval);
	glDeleteShader(tess_control);
	return program;
}

uint32_t compile_compute(const std::string& compute_path, const std::string& defines) {
	std::string defines_directive = format_shader_defines(defines);
	ShaderSource compute_source = load_shader_source(compute_path, defines_directive);

	if (compute_source.empty()) {
		sys_print(Error, "Parse fail compute %s\n", compute_path.c_str());
		return 0;
	}

	uint32_t compute = 0;
	char infolog[512] = {};
	if (!make_shader(compute_source.c_str(), GL_COMPUTE_SHADER, &compute, infolog, 512)) {
		sys_print(Error, "Error: compute shader (%s) compiliation failed: %s\n", compute_path.c_str(), infolog);
		compute_source.print_error(infolog);
		return 0;
	}
	uint32_t program = link_program({compute});
	glDeleteShader(compute);
	return program;
}

// ---------------------------------------------------------------------------
// Program-binary cache. Hash key derived from the source paths + defines (the
// inputs are stable for a given pipeline variant); cache file lives in
// FileSys::SHADER_CACHE. Reload triggered when any source file is newer than
// the cache file. Phase 1.7d migrated this from Program_Manager.
// ---------------------------------------------------------------------------

std::string make_cache_filename(std::span<const std::string> inputs) {
	std::string concatenated;
	for (auto& s : inputs)
		concatenated += s;
	return StringUtils::alphanumeric_hash(concatenated) + ".bin";
}

// Engine-relative source paths (e.g. "Shaders\\ssao.txt" or a shared-file path
// passed through directly). Returns 0 if cache is stale or absent or the
// loaded binary fails to link.
uint32_t try_load_program_binary(const std::string& cache_filename,
								 std::span<const std::string> engine_relative_sources) {
	auto binFile = FileSys::open_read(cache_filename.c_str(), FileSys::SHADER_CACHE);
	if (!binFile)
		return 0;

	for (auto& src : engine_relative_sources) {
		auto srcFile = FileSys::open_read_engine(src.c_str());
		if (!srcFile || srcFile->get_timestamp() > binFile->get_timestamp())
			return 0;
	}

	if (log_shader_compiles.get_bool())
		sys_print(Debug, "shader-cache load: %s\n", cache_filename.c_str());

	BinaryReader reader(binFile.get());
	auto sourceType = reader.read_int32();
	auto len = reader.read_int32();
	std::vector<uint8_t> bytes(len, 0);
	reader.read_bytes_ptr(bytes.data(), bytes.size());

	uint32_t program = glCreateProgram();
	glProgramBinary(program, sourceType, bytes.data(), bytes.size());
	glValidateProgram(program);

	GLint success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == GL_FALSE) {
		GLint logLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		std::vector<GLchar> log(logLength);
		glGetProgramInfoLog(program, logLength, nullptr, log.data());
		sys_print(Error, "shader-cache load failed: %s\n", log.data());
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

void save_program_binary(uint32_t program, const std::string& cache_filename) {
	GLint length = 0;
	glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &length);
	if (length <= 0)
		return;
	if (log_shader_compiles.get_bool())
		sys_print(Debug, "shader-cache save: %s\n", cache_filename.c_str());
	std::vector<uint8_t> bytes(length, 0);
	GLenum outType = 0;
	glGetProgramBinary(program, bytes.size(), nullptr, &outType, bytes.data());
	FileWriter writer(bytes.size() + 8);
	writer.write_int32(outType);
	writer.write_int32(bytes.size());
	writer.write_bytes_ptr(bytes.data(), bytes.size());
	auto outFile = FileSys::open_write(cache_filename.c_str(), FileSys::SHADER_CACHE);
	if (outFile)
		outFile->write(writer.get_buffer(), writer.get_size());
	else
		sys_print(Error, "shader-cache write open failed: %s\n", cache_filename.c_str());
}

class OpenGLShaderImpl : public IGraphicsShader
{
public:
	uint32_t program_id = 0;

	explicit OpenGLShaderImpl(uint32_t id) : program_id(id) {}
	~OpenGLShaderImpl() override {
		if (program_id != 0)
			glDeleteProgram(program_id);
	}

	void release() override { delete this; }
	uint32_t get_internal_handle() override { return program_id; }

	// Returns true if `type` is a sampler / texture-only uniform (sampler2D,
	// samplerCube, samplerBuffer, isampler*, usampler*, sampler2DArray,
	// shadow-sampler variants, ...). Conservative whitelist of types this
	// engine actually uses in shaders; extend if a new family appears.
	static bool is_sampler_type(GLenum t) {
		switch (t) {
		case GL_SAMPLER_2D:
		case GL_SAMPLER_2D_ARRAY:
		case GL_SAMPLER_3D:
		case GL_SAMPLER_CUBE:
		case GL_SAMPLER_CUBE_MAP_ARRAY:
		case GL_SAMPLER_2D_SHADOW:
		case GL_SAMPLER_2D_ARRAY_SHADOW:
		case GL_SAMPLER_CUBE_SHADOW:
		case GL_SAMPLER_BUFFER:
		case GL_INT_SAMPLER_2D:
		case GL_INT_SAMPLER_2D_ARRAY:
		case GL_INT_SAMPLER_3D:
		case GL_INT_SAMPLER_CUBE:
		case GL_UNSIGNED_INT_SAMPLER_2D:
		case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
		case GL_UNSIGNED_INT_SAMPLER_3D:
		case GL_UNSIGNED_INT_SAMPLER_CUBE:
			return true;
		default:
			return false;
		}
	}
	// Storage textures = image* uniforms (writable, used with imageLoad /
	// imageStore from compute / fragment).
	static bool is_image_type(GLenum t) {
		switch (t) {
		case GL_IMAGE_2D:
		case GL_IMAGE_2D_ARRAY:
		case GL_IMAGE_3D:
		case GL_IMAGE_CUBE:
		case GL_INT_IMAGE_2D:
		case GL_INT_IMAGE_3D:
		case GL_UNSIGNED_INT_IMAGE_2D:
		case GL_UNSIGNED_INT_IMAGE_3D:
		case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
			return true;
		default:
			return false;
		}
	}

	Reflection reflect() override {
		Reflection out{};

		// Walk GL_UNIFORM: separates sampler / image uniforms; counts
		// per-stage via GL_REFERENCED_BY_*_SHADER props.
		GLint num_uniforms = 0;
		glGetProgramInterfaceiv(program_id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &num_uniforms);
		const GLenum uniform_props[] = {
			GL_TYPE,
			GL_REFERENCED_BY_VERTEX_SHADER,
			GL_REFERENCED_BY_FRAGMENT_SHADER,
			GL_REFERENCED_BY_COMPUTE_SHADER,
			GL_BLOCK_INDEX,
		};
		for (GLint i = 0; i < num_uniforms; i++) {
			GLint values[5] = {0};
			glGetProgramResourceiv(program_id, GL_UNIFORM, i,
								   (GLsizei)std::size(uniform_props), uniform_props,
								   (GLsizei)std::size(values), nullptr, values);
			// Skip uniforms that belong to a named uniform block -- those
			// are counted under GL_UNIFORM_BLOCK below. Default-block
			// uniforms (BLOCK_INDEX == -1) are the sampler / image bindings.
			if (values[4] != -1)
				continue;
			const GLenum type = (GLenum)values[0];
			const bool sampler = is_sampler_type(type);
			const bool image   = is_image_type(type);
			if (!sampler && !image)
				continue;
			auto bump = [&](PerStageCounts& s) {
				if (sampler) s.num_samplers++;
				else         s.num_storage_textures++;
			};
			if (values[1]) bump(out.vertex);
			if (values[2]) bump(out.fragment);
			if (values[3]) bump(out.compute);
		}

		auto count_blocks = [&](GLenum iface, int PerStageCounts::*field) {
			GLint count = 0;
			glGetProgramInterfaceiv(program_id, iface, GL_ACTIVE_RESOURCES, &count);
			const GLenum props[] = {
				GL_REFERENCED_BY_VERTEX_SHADER,
				GL_REFERENCED_BY_FRAGMENT_SHADER,
				GL_REFERENCED_BY_COMPUTE_SHADER,
			};
			for (GLint i = 0; i < count; i++) {
				GLint refs[3] = {0};
				glGetProgramResourceiv(program_id, iface, i,
									   (GLsizei)std::size(props), props,
									   (GLsizei)std::size(refs), nullptr, refs);
				if (refs[0]) out.vertex.*field   += 1;
				if (refs[1]) out.fragment.*field += 1;
				if (refs[2]) out.compute.*field  += 1;
			}
		};
		count_blocks(GL_UNIFORM_BLOCK,         &PerStageCounts::num_uniform_buffers);
		count_blocks(GL_SHADER_STORAGE_BLOCK,  &PerStageCounts::num_storage_buffers);

		return out;
	}
};

IGraphicsShader* wrap_or_fail(uint32_t program_id) {
	if (program_id == 0)
		return nullptr;
	return new OpenGLShaderImpl(program_id);
}

} // namespace

// ---------------------------------------------------------------------------
// Factory entry points (declared in OpenGlDeviceLocal.h). Caching applies to
// vert+frag, vert+frag+geo, single-file (no tess); compute + single-file-tess
// always recompile (small surface, no value in caching them).
// ---------------------------------------------------------------------------

IGraphicsShader* opengl_create_shader_vert_frag(const std::string& vert_path,
												const std::string& frag_path,
												const std::string& defines) {
	const std::string sources[] = {vert_path, frag_path, defines};
	const std::string cache_filename = make_cache_filename(sources);
	const std::string engine_relative[] = {std::string(OPENGL_SHADER_PATH) + vert_path,
										   std::string(OPENGL_SHADER_PATH) + frag_path};
	if (uint32_t cached = try_load_program_binary(cache_filename, engine_relative))
		return new OpenGLShaderImpl(cached);

	uint32_t program = compile_vert_frag(vert_path, frag_path, defines);
	if (program != 0)
		save_program_binary(program, cache_filename);
	return wrap_or_fail(program);
}

IGraphicsShader* opengl_create_shader_vert_frag_geo(const std::string& vert_path,
													const std::string& frag_path,
													const std::string& geo_path,
													const std::string& defines) {
	const std::string sources[] = {vert_path, frag_path, geo_path, defines};
	const std::string cache_filename = make_cache_filename(sources);
	const std::string engine_relative[] = {std::string(OPENGL_SHADER_PATH) + vert_path,
										   std::string(OPENGL_SHADER_PATH) + frag_path,
										   std::string(OPENGL_SHADER_PATH) + geo_path};
	if (uint32_t cached = try_load_program_binary(cache_filename, engine_relative))
		return new OpenGLShaderImpl(cached);

	uint32_t program = compile_vert_frag_geo(vert_path, frag_path, geo_path, defines);
	if (program != 0)
		save_program_binary(program, cache_filename);
	return wrap_or_fail(program);
}

IGraphicsShader* opengl_create_shader_compute(const std::string& compute_path,
											  const std::string& defines) {
	return wrap_or_fail(compile_compute(compute_path, defines));
}

IGraphicsShader* opengl_create_shader_single_file(const std::string& shared_path,
												  const std::string& defines) {
	// Shared file paths are not under Shaders\\ — passed through to the cache
	// timestamp check verbatim, matching the legacy recompile_shared behavior.
	const std::string sources[] = {shared_path, defines};
	const std::string cache_filename = make_cache_filename(sources);
	const std::string engine_relative[] = {shared_path};
	if (uint32_t cached = try_load_program_binary(cache_filename, engine_relative))
		return new OpenGLShaderImpl(cached);

	uint32_t program = compile_vert_frag_single_file(shared_path, defines);
	if (program != 0)
		save_program_binary(program, cache_filename);
	return wrap_or_fail(program);
}

IGraphicsShader* opengl_create_shader_single_file_tess(const std::string& shared_path,
													   const std::string& defines) {
	return wrap_or_fail(compile_vert_frag_tess_single_file(shared_path, defines));
}
