#include "Shader.h"
#include <string>
#include <fstream>
#include <sstream>
#include "glad/glad.h"
#include <glm/gtc/type_ptr.hpp>

const char* const SHADER_PATH = "Shaders\\";
const char* const INCLUDE_SPECIFIER = "#include";


static bool read_and_add_recursive(std::string filepath, std::string& text)
{
	std::string path(SHADER_PATH);
	path += filepath;
	std::ifstream infile(path);
	if (!infile) {
		printf("ERROR: Couldn't open path %s\n", filepath.c_str());
		return false;
	}
	std::string line;
	while (std::getline(infile, line))
	{
		size_t pos = line.find(INCLUDE_SPECIFIER);
		if (pos == std::string::npos)
			text.append(line + '\n');
		else {
			size_t start_file = line.find('\"');
			if (start_file == std::string::npos) {
				printf("ERROR: include not followed with filepath\n");
				return false;
			}
			size_t end_file = line.rfind('\"');
			if (end_file == start_file) {
				printf("ERROR: include missing ending quote\n");
				return false;
			}
			read_and_add_recursive(line.substr(start_file + 1, end_file - start_file - 1), text);
		}
	}

	return true;
}


static std::string get_definess_with_directive(std::string& defines)
{
	std::stringstream ss(defines);
	std::string temp_define;
	std::string defines_with_directive;
	while (std::getline(ss, temp_define, ',') && temp_define.size() > 0) {
		defines_with_directive.append("#define " + temp_define + '\n');
	}
	return defines_with_directive;
}
static std::string get_source(const char* path, const std::string& defines)
{
	std::string source;
	bool result = read_and_add_recursive(path, source);
	if (!result) {
		return {};
	}
	size_t version_line = source.find("#version");
	version_line = source.find('\n', version_line);
	source.insert(version_line + 1, defines);

	return source;
}
static bool make_shader(const char* source, GLenum type, uint32_t* gl_shader, char* error_buf, int error_buf_size)
{
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

ShaderResult Shader::compile(
	Shader* shader,
	const char* vertex_path,
	const char* fragment_path,
	std::string shader_defines
)
{
	glDeleteShader(shader->ID);
	shader->ID = 0;

	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	std::string vertex_source = get_source(vertex_path, defines_with_directive);
	std::string fragment_source = get_source(fragment_path, defines_with_directive);

	if (vertex_source.empty() || fragment_source.empty()) {
		printf("Parse fail %s %s\n", vertex_path, fragment_path);
		return ShaderResult::SHADER_PARSE_FAIL;
	}

	unsigned int vertex;
	unsigned int fragment;
	unsigned int program;
	int success = 0;
	char infolog[512];

	bool good = make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512);
	if (!good) {
		printf("Error: vertex shader (%s) compiliation failed: %s\n", vertex_path, infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512);
	if (!good) {
		printf("Error: fragment shader (%s) compiliation failed: %s\n", fragment_path, infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infolog);
		printf("Error: shader program link failed: %s\n", infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	shader->ID = program;

	return ShaderResult::SHADER_SUCCESS;
}

ShaderResult Shader::compute_compile(Shader* shader, const char* compute_path, std::string shader_defines)
{
	glDeleteShader(shader->ID);
	shader->ID = 0;

	char infolog[512];
	int success = 0;
	std::string compute_source;
	bool result = read_and_add_recursive(compute_path, compute_source);
	if (!result) {
		return ShaderResult::SHADER_PARSE_FAIL;
	}

	std::stringstream ss(shader_defines);
	std::string temp_define;
	std::string defines_with_directive;
	while (std::getline(ss, temp_define, ',')) {
		defines_with_directive.append("#define " + temp_define + '\n');
	}
	size_t version_line = compute_source.find("#version");
	version_line = compute_source.find('\n', version_line);
	compute_source.insert(version_line + 1, defines_with_directive);



	const char* source_c = compute_source.c_str();
	uint32_t compute = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute, 1, &source_c, NULL);
	glCompileShader(compute);

	glGetShaderiv(compute, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(compute, 512, NULL, infolog);
		printf("Error: compute shader (%s) compiliation failed: %s\n", compute_path, infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	shader->ID = glCreateProgram();
	glAttachShader(shader->ID, compute);
	glLinkProgram(shader->ID);

	glGetProgramiv(shader->ID, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shader->ID, 512, NULL, infolog);
		printf("Error: shader program link failed: %s\n", infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	glDeleteShader(compute);

	return ShaderResult::SHADER_SUCCESS;
}

void Shader::use()
{
	glUseProgram(ID);
}
void Shader::set_bool(const char* name, bool value)
{
	glUniform1i(glGetUniformLocation(ID, name), (int)value);
}
void Shader::set_int(const char* name, int value)
{
	glUniform1i(glGetUniformLocation(ID, name), value);
}
void Shader::set_float(const char* name, float value)
{
	glUniform1f(glGetUniformLocation(ID, name), value);
}
void Shader::set_mat4(const char* name, glm::mat4 value)
{
	glUniformMatrix4fv(glGetUniformLocation(ID, name), 1, GL_FALSE, glm::value_ptr(value));
}
void Shader::set_vec3(const char* name, glm::vec3 value)
{
	glUniform3f(glGetUniformLocation(ID, name), value.r, value.g, value.b);
}
void Shader::set_vec2(const char* name, glm::vec2 value)
{
	glUniform2f(glGetUniformLocation(ID, name), value.x, value.y);
}

void Shader::set_vec4(const char* name, glm::vec4 value)
{
	glUniform4f(glGetUniformLocation(ID, name), value.x, value.y, value.z, value.w);
}
void Shader::set_ivec2(const char* name, glm::ivec2 value)
{
	glUniform2i(glGetUniformLocation(ID, name), value.x, value.y);
}

void Shader::set_block_binding(const char* name, int block_binding)
{
	glUniformBlockBinding(ID, glGetUniformBlockIndex(ID, name), block_binding);
}
