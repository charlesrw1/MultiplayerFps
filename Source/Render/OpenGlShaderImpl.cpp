// OpenGLShaderImpl + GLSL source loader + GL program compile/link + program
// binary cache. Owns every gl* call that touches a shader/program in the
// engine. Phase 1.7d folded the binary cache (was in DrawLocal_Device.cpp)
// and the Shader::compile* helpers (was in Shader.cpp) into this TU and
// deleted the legacy Shader struct.

#include "OpenGlDeviceLocal.h"
#include "glad/glad.h"
#include <fstream>
#include <sstream>

extern ConfigVar log_shader_compiles;

namespace {

constexpr const char* SHADER_PATH = "Shaders\\";
constexpr const char* INCLUDE_SPECIFIER = "#include";

class SourceAndLinenums
{
public:
	bool empty() const { return source.empty(); }
	const char* c_str() const { return source.c_str(); }
	void print_error(std::string driver_error) {
		if (driver_error.find("ERROR: 0:") == 0) {
			try {
				auto sub = driver_error.substr(9);
				auto split = StringUtils::split(sub);
				auto first = split.at(0);
				int line_num = std::stoi(first.substr(0, first.size() - 1));
				print_error_for_line_num(line_num);
			} catch (...) {
			}
		}
	}
	void print_error_for_line_num(int line) {
		int best_fit = -1;
		for (int i = 0; i < (int)ranges.size(); i++) {
			int s = ranges[i].line_start;
			int c = ranges[i].line_count;
			if (line >= s && line < s + c) {
				best_fit = i;
				break;
			}
		}
		if (best_fit != -1) {
			auto& r = ranges[best_fit];
			const int input_start = r.input_line_start;
			const int ofs = line - r.line_start;
			const int line_in_source = ofs + input_start;
			sys_print(Error, "shader error in %s on line %d\n", ranges[best_fit].filename.c_str(), line_in_source);
		}
	}

	std::string source;
	int line_count = 0;
	struct FileAndRange
	{
		std::string filename;
		int line_start = 0;
		int line_count = 0;
		int input_line_start = 0;
	};
	std::vector<FileAndRange> ranges;
};

bool read_and_add_recursive(std::string filepath, SourceAndLinenums& text, bool path_is_relative) {
	std::string path = path_is_relative ? SHADER_PATH : "";
	path += filepath;
	std::ifstream infile(path);
	if (!infile) {
		sys_print(Error, "ERROR: Couldn't open path %s\n", filepath.c_str());
		return false;
	}

	SourceAndLinenums::FileAndRange cur_range;
	cur_range.filename = filepath;
	cur_range.line_start = text.line_count;
	cur_range.input_line_start = 0;

	std::string line;
	while (std::getline(infile, line)) {
		size_t pos = line.find(INCLUDE_SPECIFIER);
		if (pos == std::string::npos) {
			text.source.append(line + '\n');
		} else if (pos == 0) {
			size_t start_file = line.find('\"');
			if (start_file == std::string::npos) {
				sys_print(Error, "ERROR: include not followed with filepath\n");
				return false;
			}
			size_t end_file = line.rfind('\"');
			if (end_file == start_file) {
				sys_print(Error, "ERROR: include missing ending quote\n");
				return false;
			}

			cur_range.line_count = text.line_count - cur_range.line_start;
			text.ranges.push_back(cur_range);
			const int pre_start = cur_range.input_line_start + cur_range.line_count;
			read_and_add_recursive(line.substr(start_file + 1, end_file - start_file - 1), text,
								   true /* so includes inside _glsl files work*/);

			cur_range.input_line_start = pre_start + 1;
			cur_range.line_start = text.line_count;
		}
		text.line_count++;
	}
	cur_range.line_count = text.line_count - cur_range.line_start;
	text.ranges.push_back(cur_range);

	return true;
}

std::string get_definess_with_directive(std::string defines) {
	std::stringstream ss(defines);
	std::string temp_define;
	std::string defines_with_directive;
	while (std::getline(ss, temp_define, ',') && temp_define.size() > 0) {
		defines_with_directive.append("#define " + temp_define + '\n');
	}
	return defines_with_directive;
}

int count_characters(const std::string& str, char ch) {
	int s = 0;
	for (auto c : str)
		s += int(c == ch);
	return s;
}

SourceAndLinenums get_source(const std::string& path, const std::string& defines,
							 bool paths_are_relative = true) {
	SourceAndLinenums source;
	source.source = "#version 460 core\n";
	source.source += defines;
	source.line_count += count_characters(source.source, '\n');

	bool result = read_and_add_recursive(path, source, paths_are_relative);
	if (!result)
		return {};
	return source;
}

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
// Shaders\\ (SHADER_PATH prefix added internally).
uint32_t compile_vert_frag(const std::string& vert_path, const std::string& frag_path,
						   const std::string& defines) {
	std::string defines_directive = get_definess_with_directive(defines);
	SourceAndLinenums vertex_source = get_source(vert_path, defines_directive);
	SourceAndLinenums fragment_source = get_source(frag_path, defines_directive);

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
	std::string defines_directive = get_definess_with_directive(defines);
	SourceAndLinenums vertex_source = get_source(vert_path, defines_directive);
	SourceAndLinenums fragment_source = get_source(frag_path, defines_directive);
	SourceAndLinenums geometry_source = get_source(geo_path, defines_directive);

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
	std::string defines_directive = get_definess_with_directive(defines);
	SourceAndLinenums vertex_source =
		get_source(shared_path, defines_directive + "\n#define _VERTEX_SHADER\n#line 0\n", false);
	SourceAndLinenums fragment_source =
		get_source(shared_path, defines_directive + "\n#define _FRAGMENT_SHADER\n#line 0\n", false);

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
	std::string defines_directive = get_definess_with_directive(defines);
	SourceAndLinenums vertex_source =
		get_source(shared_path, defines_directive + "\n#define _VERTEX_SHADER\n#line 0\n", false);
	SourceAndLinenums fragment_source =
		get_source(shared_path, defines_directive + "\n#define _FRAGMENT_SHADER\n#line 0\n", false);
	SourceAndLinenums tess_eval_source =
		get_source(shared_path, defines_directive + "\n#define _TESS_EVAL_SHADER\n#line 0\n", false);
	SourceAndLinenums tess_ctrl_source =
		get_source(shared_path, defines_directive + "\n#define _TESS_CTRL_SHADER\n#line 0\n", false);

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
	std::string defines_directive = get_definess_with_directive(defines);
	SourceAndLinenums compute_source = get_source(compute_path, defines_directive);

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
	const std::string engine_relative[] = {std::string(SHADER_PATH) + vert_path,
										   std::string(SHADER_PATH) + frag_path};
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
	const std::string engine_relative[] = {std::string(SHADER_PATH) + vert_path,
										   std::string(SHADER_PATH) + frag_path,
										   std::string(SHADER_PATH) + geo_path};
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
