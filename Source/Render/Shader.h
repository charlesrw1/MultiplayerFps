#ifndef SHADER_H
#define SHADER_H
#include <string>
#include "glm/glm.hpp"

enum class ShaderResult
{
	SHADER_SUCCESS,
	SHADER_PARSE_FAIL,
	SHADER_COMPILE_FAIL,
};

class Shader
{
public:
	unsigned int ID = 0;

	static ShaderResult compile(
		Shader* shader,
		const std::string& vertpath,
		const std::string& frag,
		std::string shader_defines = {}
	);
	static ShaderResult compute_compile(
		Shader* shader,
		const std::string& path,
		std::string shader_defines = {}
	);

	static bool compile(
		Shader& shader,
		const std::string& vertpath,
		const std::string& fragpath,
		const std::string& geopath,

		std::string shader_defines = {}
	);

	// use ifdefs for vertex/fragment
	static ShaderResult compile_vert_frag_single_file(
		Shader* shader,
		const std::string& path,
		std::string shader_defines = {}
	);
	// this is getting out of hand...
	static ShaderResult compile_vert_frag_tess_single_file(
		Shader* shader,
		const std::string& path,
		std::string shader_defines = {}
	);


	void use();

	void set_bool(const char* name, bool value);
	void set_int(const char* name, int value);
	void set_uint(const char* name, unsigned int value);
	void set_float(const char* name, float value);
	void set_mat4(const char* name, glm::mat4 value);
	void set_vec4(const char* name, glm::vec4 value);
	void set_vec3(const char* name, glm::vec3 value);
	void set_vec2(const char* name, glm::vec2 value);
	void set_ivec2(const char* name, glm::ivec2 value);

	void set_block_binding(const char* name, int binding);
};

#endif // !SHADER_H
