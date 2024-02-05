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

ShaderResult Shader::compile(
	Shader* shader,
	const char* vertex_path,
	const char* fragment_path,
	std::string shader_defines
)
{
	glDeleteShader(shader->ID);
	shader->ID = 0;

	std::string vertex_source;
	std::string fragment_source;

	std::stringstream ss(shader_defines);
	std::string temp_define;
	std::string defines_with_directive;
	while (std::getline(ss, temp_define, ',') && temp_define.size() > 0) {
		defines_with_directive.append("#define " + temp_define + '\n');
	}

	bool result = read_and_add_recursive(vertex_path, vertex_source);
	if (!result) {
		return ShaderResult::SHADER_PARSE_FAIL;
	}
	size_t version_line = vertex_source.find("#version");
	version_line = vertex_source.find('\n', version_line);
	vertex_source.insert(version_line + 1, defines_with_directive);

	result = read_and_add_recursive(fragment_path, fragment_source);
	if (!result) {
		return ShaderResult::SHADER_PARSE_FAIL;
	}
	version_line = fragment_source.find("#version");
	version_line = fragment_source.find('\n', version_line);
	fragment_source.insert(version_line + 1, defines_with_directive);

	unsigned int vertex;
	unsigned int fragment;
	unsigned int geometry = 0;
	unsigned int program;
	const char* vsource_c = vertex_source.c_str();
	const char* fsource_c = fragment_source.c_str();
	int success = 0;
	char infolog[512];

	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vsource_c, NULL);
	glCompileShader(vertex);

	glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex, 512, NULL, infolog);
		printf("Error: vertex shader (%s) compiliation failed: %s\n", vertex_path, infolog);

		return ShaderResult::SHADER_COMPILE_FAIL;
	}

	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fsource_c, NULL);
	glCompileShader(fragment);

	glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment, 512, NULL, infolog);
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
	glDeleteShader(geometry);

	shader->ID = program;

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
