#include "Shader.h"
#include <string>
#include <fstream>
#include <sstream>
#include "glad/glad.h"
#include <glm/gtc/type_ptr.hpp>
#include "Framework/Util.h"

#include "Framework/StringUtils.h"
static const char* const SHADER_PATH = "Shaders\\";
static const char* const INCLUDE_SPECIFIER = "#include";
using std::string;
#include <vector>
using std::vector;
class SourceAndLinenums
{
public:
	bool empty() const { return source.empty(); }
	const char* c_str() const { return source.c_str(); }
	void print_error(string driver_error) {
		if (driver_error.find("ERROR: 0:") == 0) {
			try {
				auto sub = driver_error.substr(9);
				auto split = StringUtils::split(sub);
				auto first = split.at(0);
				int line_num = std::stoi(first.substr(0, first.size() - 1));
				print_error_for_line_num(line_num);
			}
			catch (...) {
			}
		}
	}
	void print_error_for_line_num(int line) {
		int best_fit = -1;
		for (int i = 0; i < ranges.size(); i++) {
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

	string source;
	int line_count = 0;
	struct FileAndRange
	{
		string filename;
		int line_start = 0;
		int line_count = 0;

		int input_line_start = 0;
	};
	vector<FileAndRange> ranges;
};

static bool read_and_add_recursive(std::string filepath, SourceAndLinenums& text, bool path_is_relative) {
	std::string path = path_is_relative ? SHADER_PATH : "";
	path += filepath;
	std::ifstream infile(path);
	if (!infile) {
		sys_print(Error, "ERROR: Couldn't open path %s\n", filepath.c_str());
		return false;
	}

	// text += "#line 0 \"";
	// text += filepath;
	// text += "\"\n";

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

			// text += "#line ";
			// text += std::to_string(linenum);
			// text += " \"";
			// text += filepath;
			// text += "\"\n";
		}

		text.line_count++;
	}
	cur_range.line_count = text.line_count - cur_range.line_start;
	text.ranges.push_back(cur_range);

	return true;
}

static std::string get_definess_with_directive(std::string& defines) {
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

static SourceAndLinenums get_source(const std::string& path, const std::string& defines,
									bool paths_are_relative = true) {
	// std::string source = "#version 460 core\n";
	// source += defines;

	SourceAndLinenums source;
	source.source = "#version 460 core\n";
	source.source += defines;
	source.line_count += count_characters(source.source, '\n');

	bool result = read_and_add_recursive(path, source, paths_are_relative);
	if (!result) {
		return {};
	}
	return source;
}
static bool make_shader(const char* source, GLenum type, uint32_t* gl_shader, char* error_buf, int error_buf_size) {
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

ShaderResult Shader::compile_vert_frag_tess_single_file(Shader* shader, const std::string& shared_path,
														std::string shader_defines) {
	if (shader->ID != 0)
		glDeleteProgram(shader->ID);
	shader->ID = 0;

	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	SourceAndLinenums vertex_source =
		get_source(shared_path, defines_with_directive + "\n#define _VERTEX_SHADER\n#line 0\n", false);
	SourceAndLinenums fragment_source =
		get_source(shared_path, defines_with_directive + "\n#define _FRAGMENT_SHADER\n#line 0\n", false);
	SourceAndLinenums tess_eval_source =
		get_source(shared_path, defines_with_directive + "\n#define _TESS_EVAL_SHADER\n#line 0\n", false);
	SourceAndLinenums tess_ctrl_source =
		get_source(shared_path, defines_with_directive + "\n#define _TESS_CTRL_SHADER\n#line 0\n", false);

	if (vertex_source.empty() || fragment_source.empty() || tess_eval_source.empty() || tess_ctrl_source.empty()) {
		sys_print(Error, "Parse fail  single file with tess %s\n", shared_path.c_str());
		return ShaderResult::SHADER_PARSE_FAIL;
	}

	unsigned int vertex{};
	unsigned int fragment{};
	unsigned int tess_control{};
	unsigned int tess_eval{};

	unsigned int program{};
	int success = 0;
	char infolog[512];

	bool good = make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(tess_eval_source.c_str(), GL_TESS_EVALUATION_SHADER, &tess_eval, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: tesselation eval shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(tess_ctrl_source.c_str(), GL_TESS_CONTROL_SHADER, &tess_control, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: tesselation control shader (%s) compiliation failed: %s\n", shared_path.c_str(),
				  infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glAttachShader(program, tess_control);
	glAttachShader(program, tess_eval);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infolog);
		sys_print(Error, "Error: shader program link failed: %s\n", infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);
	glDeleteShader(tess_eval);
	glDeleteShader(tess_control);

	shader->ID = program;

	return ShaderResult::SHADER_SUCCESS;
}

ShaderResult Shader::compile_vert_frag_single_file(Shader* shader, const std::string& shared_path,
												   std::string shader_defines) {
	if (shader->ID != 0)
		glDeleteProgram(shader->ID);
	shader->ID = 0;

	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	SourceAndLinenums vertex_source =
		get_source(shared_path, defines_with_directive + "\n#define _VERTEX_SHADER\n#line 0\n", false);
	SourceAndLinenums fragment_source =
		get_source(shared_path, defines_with_directive + "\n#define _FRAGMENT_SHADER\n#line 0\n", false);

	if (vertex_source.empty() || fragment_source.empty()) {
		sys_print(Error, "Shader::compile_vert_frag_single_file: Parse fail single file %s\n", shared_path.c_str());
		return ShaderResult::SHADER_PARSE_FAIL;
	}

	unsigned int vertex;
	unsigned int fragment;
	unsigned int program;
	int success = 0;
	char infolog[512];
	infolog[0] = 0;
	bool good = make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		vertex_source.print_error(infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", shared_path.c_str(), infolog);
		fragment_source.print_error(infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infolog);
		sys_print(Error, "Error: shader program link failed: %s\n", infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	shader->ID = program;

	return ShaderResult::SHADER_SUCCESS;
}

bool Shader::compile(Shader& shader, const std::string& vertex_path, const std::string& fragment_path,
					 const std::string& geometry_path, std::string shader_defines) {
	if (shader.ID != 0)
		glDeleteProgram(shader.ID);
	shader.ID = 0;

	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	SourceAndLinenums vertex_source = get_source(vertex_path, defines_with_directive);
	SourceAndLinenums fragment_source = get_source(fragment_path, defines_with_directive);
	SourceAndLinenums geometry_source = get_source(geometry_path, defines_with_directive);

	if (vertex_source.empty() || fragment_source.empty() || geometry_source.empty()) {
		sys_print(Error, "Parse fail normal w geo %s %s %s\n", vertex_path.c_str(), fragment_path.c_str(),
				  geometry_path.c_str());
		return false;
	}

	unsigned int vertex;
	unsigned int fragment;
	unsigned int geometry;
	unsigned int program;
	int success = 0;
	char infolog[512];

	bool good = make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", vertex_path.c_str(), infolog);
		vertex_source.print_error(infolog);
		return false;
	}
	good = make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", fragment_path.c_str(), infolog);
		fragment_source.print_error(infolog);

		return false;
	}
	good = make_shader(geometry_source.c_str(), GL_GEOMETRY_SHADER, &geometry, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: geometry shader (%s) compiliation failed: %s\n", geometry_path.c_str(), infolog);
		return false;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glAttachShader(program, geometry);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infolog);
		sys_print(Error, "Error: shader program link failed: %s\n", infolog);

		return false;
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);
	glDeleteShader(geometry);

	shader.ID = program;

	return true;
}

ShaderResult Shader::compile(Shader* shader, const std::string& vertex_path, const std::string& fragment_path,
							 std::string shader_defines) {
	if (shader->ID != 0)
		glDeleteProgram(shader->ID);
	shader->ID = 0;

	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	SourceAndLinenums vertex_source = get_source(vertex_path, defines_with_directive);
	SourceAndLinenums fragment_source = get_source(fragment_path, defines_with_directive);

	if (vertex_source.empty() || fragment_source.empty()) {
		sys_print(Error, "Parse fail normal %s %s\n", vertex_path.c_str(), fragment_path.c_str());
		return ShaderResult::SHADER_PARSE_FAIL;
	}

	unsigned int vertex;
	unsigned int fragment;
	unsigned int program;
	int success = 0;
	char infolog[512];

	bool good = make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", vertex_path.c_str(), infolog);
		vertex_source.print_error(infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", fragment_path.c_str(), infolog);
		fragment_source.print_error(infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infolog);
		sys_print(Error, "Error: shader program link failed: %s\n", infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	shader->ID = program;

	return ShaderResult::SHADER_SUCCESS;
}

ShaderResult Shader::compute_compile(Shader* shader, const std::string& compute_path, std::string shader_defines) {
	if (shader->ID != 0)
		glDeleteProgram(shader->ID);
	shader->ID = 0;

	char infolog[512];
	int success = 0;
	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	SourceAndLinenums compute_source = get_source(compute_path, defines_with_directive);

	if (compute_source.empty()) {
		sys_print(Error, "Parse fail compute %s\n", compute_source.c_str());
		return ShaderResult::SHADER_PARSE_FAIL;
	}

	const char* source_c = compute_source.c_str();
	uint32_t compute = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute, 1, &source_c, NULL);
	glCompileShader(compute);

	glGetShaderiv(compute, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(compute, 512, NULL, infolog);

		sys_print(Error, "Error: compute shader (%s) compiliation failed: %s\n", compute_path.c_str(), infolog);
		compute_source.print_error(infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	shader->ID = glCreateProgram();
	glAttachShader(shader->ID, compute);
	glLinkProgram(shader->ID);

	glGetProgramiv(shader->ID, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shader->ID, 512, NULL, infolog);
		sys_print(Error, "Error: shader program link failed: %s\n", infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	glDeleteShader(compute);

	return ShaderResult::SHADER_SUCCESS;
}

void Shader::use() {
	glUseProgram(ID);
}
void Shader::set_bool(const char* name, bool value) {
	glUniform1i(glGetUniformLocation(ID, name), (int)value);
}
void Shader::set_int(const char* name, int value) {
	glUniform1i(glGetUniformLocation(ID, name), value);
}
void Shader::set_uint(const char* name, unsigned int value) {
	glUniform1ui(glGetUniformLocation(ID, name), value);
}
void Shader::set_float(const char* name, float value) {
	glUniform1f(glGetUniformLocation(ID, name), value);
}
void Shader::set_mat4(const char* name, glm::mat4 value) {
	glUniformMatrix4fv(glGetUniformLocation(ID, name), 1, GL_FALSE, glm::value_ptr(value));
}
void Shader::set_vec3(const char* name, glm::vec3 value) {
	glUniform3f(glGetUniformLocation(ID, name), value.r, value.g, value.b);
}
void Shader::set_vec2(const char* name, glm::vec2 value) {
	glUniform2f(glGetUniformLocation(ID, name), value.x, value.y);
}

void Shader::set_vec4(const char* name, glm::vec4 value) {
	glUniform4f(glGetUniformLocation(ID, name), value.x, value.y, value.z, value.w);
}
void Shader::set_ivec2(const char* name, glm::ivec2 value) {
	glUniform2i(glGetUniformLocation(ID, name), value.x, value.y);
}
void Shader::set_ivec3(const char* name, glm::ivec3 value) {
	glUniform3i(glGetUniformLocation(ID, name), value.x, value.y, value.z);
}

void Shader::set_block_binding(const char* name, int block_binding) {
	glUniformBlockBinding(ID, glGetUniformBlockIndex(ID, name), block_binding);
}
