#include "Shader.h"
#include "IGraphicsDevice.h"

// Thin value wrapper over IGraphicsShader. Compile/link surface lives in
// OpenGlShaderImpl.cpp (phase 1.7b); uniform setters dispatch to the wrapped
// IGraphicsShader (phase 1.7c). The legacy ID field is kept as a mirror for
// callers that still pass program ids around (eg. set_pipeline's program
// field) — it folds onto IGraphicsShader* in phase 2c.

void Shader::use() {
	if (!gfx_handle) return;
	gfx_handle->use();
}
void Shader::set_bool(const char* name, bool value) {
	if (!gfx_handle) return;
	gfx_handle->set_bool(name, value);
}
void Shader::set_int(const char* name, int value) {
	if (!gfx_handle) return;
	gfx_handle->set_int(name, value);
}
void Shader::set_uint(const char* name, unsigned int value) {
	if (!gfx_handle) return;
	gfx_handle->set_uint(name, value);
}
void Shader::set_float(const char* name, float value) {
	if (!gfx_handle) return;
	gfx_handle->set_float(name, value);
}
void Shader::set_mat4(const char* name, glm::mat4 value) {
	if (!gfx_handle) return;
	gfx_handle->set_mat4(name, value);
}
void Shader::set_vec3(const char* name, glm::vec3 value) {
	if (!gfx_handle) return;
	gfx_handle->set_vec3(name, value);
}
void Shader::set_vec2(const char* name, glm::vec2 value) {
	if (!gfx_handle) return;
	gfx_handle->set_vec2(name, value);
}
void Shader::set_vec4(const char* name, glm::vec4 value) {
	if (!gfx_handle) return;
	gfx_handle->set_vec4(name, value);
}
void Shader::set_ivec2(const char* name, glm::ivec2 value) {
	if (!gfx_handle) return;
	gfx_handle->set_ivec2(name, value);
}
void Shader::set_ivec3(const char* name, glm::ivec3 value) {
	if (!gfx_handle) return;
	gfx_handle->set_ivec3(name, value);
}
void Shader::set_block_binding(const char* name, int block_binding) {
	if (!gfx_handle) return;
	gfx_handle->set_block_binding(name, block_binding);
}
