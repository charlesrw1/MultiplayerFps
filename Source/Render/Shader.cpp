#include "Shader.h"
#include <string>
#include <fstream>
#include <sstream>
#include "glad/glad.h"
#include <glm/gtc/type_ptr.hpp>
#include "Framework/Util.h"

static const char* const SHADER_PATH = "Shaders\\";
static const char* const INCLUDE_SPECIFIER = "#include";


static bool read_and_add_recursive(std::string filepath, std::string& text, bool path_is_relative)
{
	std::string path = path_is_relative ? SHADER_PATH : "";
	path += filepath;
	std::ifstream infile(path);
	if (!infile) {
		sys_print(Error, "ERROR: Couldn't open path %s\n", filepath.c_str());
		return false;
	}

	//text += "#line 0 \"";
	//text += filepath;
	//text += "\"\n";

	std::string line;
	int linenum = 1;
	while (std::getline(infile, line)) {

		size_t pos = line.find(INCLUDE_SPECIFIER);
		if (pos == std::string::npos)
			text.append(line + '\n');
		else if(pos==0) {
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
			read_and_add_recursive(line.substr(start_file + 1, end_file - start_file - 1), text, true /* so includes inside _glsl files work*/);

			//text += "#line ";
			//text += std::to_string(linenum);
			//text += " \"";
			//text += filepath;
			//text += "\"\n";
		}


		linenum++;
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
static std::string get_source(const char* path, const std::string& defines, bool paths_are_relative = true)
{
	std::string source = "#version 460 core\n";;
	source += defines;
	bool result = read_and_add_recursive(path, source, paths_are_relative);
	if (!result) {
		return {};
	}
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



ShaderResult Shader::compile_vert_frag_tess_single_file(
	Shader* shader,
	const char* shared_path,
	std::string shader_defines
)
{
	if (shader->ID != 0)
		glDeleteProgram(shader->ID);
	shader->ID = 0;


	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	std::string vertex_source = get_source(shared_path, defines_with_directive + "\n#define _VERTEX_SHADER\n#line 0\n", false);
	std::string fragment_source = get_source(shared_path, defines_with_directive + "\n#define _FRAGMENT_SHADER\n#line 0\n", false);
	std::string tess_eval_source = get_source(shared_path, defines_with_directive + "\n#define _TESS_EVAL_SHADER\n#line 0\n", false);
	std::string tess_ctrl_source = get_source(shared_path, defines_with_directive + "\n#define _TESS_CTRL_SHADER\n#line 0\n", false);


	if (vertex_source.empty() || fragment_source.empty() || tess_eval_source.empty()||tess_ctrl_source.empty()) {
		sys_print(Error, "Parse fail %s\n", shared_path);
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
		sys_print(Error,"Error: vertex shader (%s) compiliation failed: %s\n", shared_path, infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", shared_path, infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(tess_eval_source.c_str(), GL_TESS_EVALUATION_SHADER, &tess_eval, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: tesselation eval shader (%s) compiliation failed: %s\n", shared_path, infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(tess_ctrl_source.c_str(), GL_TESS_CONTROL_SHADER, &tess_control, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: tesselation control shader (%s) compiliation failed: %s\n", shared_path, infolog);
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

ShaderResult Shader::compile_vert_frag_single_file(
	Shader* shader,
	const char* shared_path,
	std::string shader_defines
)
{
	if (shader->ID != 0)
		glDeleteProgram(shader->ID);
	shader->ID = 0;

	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	std::string vertex_source = get_source(shared_path, defines_with_directive+"\n#define _VERTEX_SHADER\n#line 0\n", false);
	std::string fragment_source = get_source(shared_path, defines_with_directive+"\n#define _FRAGMENT_SHADER\n#line 0\n",false);

	if (vertex_source.empty() || fragment_source.empty()) {
		sys_print(Error, "Parse fail %s\n", shared_path);
		return ShaderResult::SHADER_PARSE_FAIL;
	}

	unsigned int vertex;
	unsigned int fragment;
	unsigned int program;
	int success = 0;
	char infolog[512];

	bool good = make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", shared_path, infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", shared_path, infolog);
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

bool Shader::compile(
	Shader& shader,
	const char* vertex_path,
	const char* fragment_path,
	const char* geometry_path,
	std::string shader_defines
)
{
	if (shader.ID != 0)
		glDeleteProgram(shader.ID);
	shader.ID = 0;

	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	std::string vertex_source = get_source(vertex_path, defines_with_directive);
	std::string fragment_source = get_source(fragment_path, defines_with_directive);
	std::string geometry_source = get_source(geometry_path, defines_with_directive);


	if (vertex_source.empty() || fragment_source.empty() || geometry_source.empty()) {
		sys_print(Error, "Parse fail %s %s %s\n", vertex_path, fragment_path, geometry_path);
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
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", vertex_path, infolog);
		return false;
	}
	good = make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", fragment_path, infolog);
		return false;
	}
	good = make_shader(geometry_source.c_str(), GL_GEOMETRY_SHADER, &geometry, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: geometry shader (%s) compiliation failed: %s\n", geometry_path, infolog);
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

ShaderResult Shader::compile(
	Shader* shader,
	const char* vertex_path,
	const char* fragment_path,
	std::string shader_defines
)
{
	if(shader->ID!=0)
		glDeleteProgram(shader->ID);
	shader->ID = 0;

	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	std::string vertex_source = get_source(vertex_path, defines_with_directive);
	std::string fragment_source = get_source(fragment_path, defines_with_directive);

	if (vertex_source.empty() || fragment_source.empty()) {
		sys_print(Error, "Parse fail %s %s\n", vertex_path, fragment_path);
		return ShaderResult::SHADER_PARSE_FAIL;
	}

	unsigned int vertex;
	unsigned int fragment;
	unsigned int program;
	int success = 0;
	char infolog[512];

	bool good = make_shader(vertex_source.c_str(), GL_VERTEX_SHADER, &vertex, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: vertex shader (%s) compiliation failed: %s\n", vertex_path, infolog);
		return ShaderResult::SHADER_COMPILE_FAIL;
	}
	good = make_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, &fragment, infolog, 512);
	if (!good) {
		sys_print(Error, "Error: fragment shader (%s) compiliation failed: %s\n", fragment_path, infolog);
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

ShaderResult Shader::compute_compile(Shader* shader, const char* compute_path, std::string shader_defines)
{
	if(shader->ID!=0)
		glDeleteProgram(shader->ID);
	shader->ID = 0;

	char infolog[512];
	int success = 0;
	std::string defines_with_directive = get_definess_with_directive(shader_defines);
	std::string compute_source = get_source(compute_path, defines_with_directive);

	if (compute_source.empty()) {
		sys_print(Error, "Parse fail %s\n", compute_source.c_str());
		return ShaderResult::SHADER_PARSE_FAIL;
	}

	const char* source_c = compute_source.c_str();
	uint32_t compute = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute, 1, &source_c, NULL);
	glCompileShader(compute);

	glGetShaderiv(compute, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(compute, 512, NULL, infolog);
		sys_print(Error, "Error: compute shader (%s) compiliation failed: %s\n", compute_path, infolog);

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
void Shader::set_uint(const char* name, unsigned int value)
{
	glUniform1ui(glGetUniformLocation(ID, name), value);
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
