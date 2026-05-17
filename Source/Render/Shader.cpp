#include "Shader.h"
#include "glad/glad.h"
#include <glm/gtc/type_ptr.hpp>

// Uniform setters + use() + set_block_binding. Compile/link surface lives in
// OpenGlShaderImpl.cpp (phase 1.7b). These setters migrate to IGraphicsShader
// in phase 1.7c.

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
