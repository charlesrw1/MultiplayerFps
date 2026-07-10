#include "OpenGlRmlUiRenderInterface.h"
#include "../External/glad/glad.h"
#include "Framework/Log.h"

namespace {
const char* vertex_shader_src = R"(
#version 330 core
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_texcoord;
uniform vec2 u_translation;
uniform mat4 u_projection;
out vec4 frag_color;
out vec2 frag_texcoord;
void main() {
    frag_color = in_color;
    frag_texcoord = in_texcoord;
    gl_Position = u_projection * vec4(in_position + u_translation, 0.0, 1.0);
}
)";

const char* fragment_shader_src = R"(
#version 330 core
in vec4 frag_color;
in vec2 frag_texcoord;
uniform sampler2D u_tex;
out vec4 out_color;
void main() {
    out_color = frag_color * texture(u_tex, frag_texcoord);
}
)";

unsigned int compile_shader(unsigned int type, const char* src) {
	unsigned int shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);
	int ok = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024];
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		sys_print(Error, "OpenGlRmlUiRenderInterface: shader compile failed: %s\n", log);
	}
	return shader;
}
} // namespace

OpenGlRmlUiRenderInterface::OpenGlRmlUiRenderInterface() {
	unsigned int vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
	unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
	shader_program = glCreateProgram();
	glAttachShader(shader_program, vs);
	glAttachShader(shader_program, fs);
	glLinkProgram(shader_program);
	int ok = 0;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[1024];
		glGetProgramInfoLog(shader_program, sizeof(log), nullptr, log);
		sys_print(Error, "OpenGlRmlUiRenderInterface: program link failed: %s\n", log);
	}
	glDeleteShader(vs);
	glDeleteShader(fs);

	uniform_translation = glGetUniformLocation(shader_program, "u_translation");
	uniform_projection = glGetUniformLocation(shader_program, "u_projection");

	// 1x1 white texture used for untextured geometry (RmlUi passes handle 0
	// for "no texture"; RenderGeometry substitutes this instead).
	glGenTextures(1, &white_texture);
	glBindTexture(GL_TEXTURE_2D, white_texture);
	unsigned char white_pixel[4] = { 255, 255, 255, 255 };
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

OpenGlRmlUiRenderInterface::~OpenGlRmlUiRenderInterface() {
	glDeleteTextures(1, &white_texture);
	glDeleteProgram(shader_program);
	for (auto& [handle, tex] : texture_map)
		glDeleteTextures(1, &tex);
	for (auto& [handle, geo] : geometry_map) {
		glDeleteVertexArrays(1, &geo.vao);
		glDeleteBuffers(1, &geo.vbo);
		glDeleteBuffers(1, &geo.ebo);
	}
}

void OpenGlRmlUiRenderInterface::begin_frame(int viewport_w, int viewport_h) {
	viewport_width = viewport_w;
	viewport_height = viewport_h;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, viewport_w, viewport_h);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied alpha
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);

	glUseProgram(shader_program);

	// Orthographic projection matching pixel coordinates, origin top-left.
	float l = 0.0f, r = (float)viewport_w, b = (float)viewport_h, t = 0.0f, n = -1.0f, f = 1.0f;
	float ortho[16] = {
		2.0f / (r - l), 0, 0, 0,
		0, 2.0f / (t - b), 0, 0,
		0, 0, -2.0f / (f - n), 0,
		-(r + l) / (r - l), -(t + b) / (t - b), -(f + n) / (f - n), 1.0f
	};
	glUniformMatrix4fv(uniform_projection, 1, GL_FALSE, ortho);
	glUniform1i(glGetUniformLocation(shader_program, "u_tex"), 0);
}

void OpenGlRmlUiRenderInterface::end_frame() {
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glUseProgram(0);
	glBindVertexArray(0);
}

Rml::CompiledGeometryHandle OpenGlRmlUiRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
	CompiledGeometry geo;
	glGenVertexArrays(1, &geo.vao);
	glGenBuffers(1, &geo.vbo);
	glGenBuffers(1, &geo.ebo);

	glBindVertexArray(geo.vao);
	glBindBuffer(GL_ARRAY_BUFFER, geo.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Rml::Vertex), vertices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Rml::Vertex), (void*)offsetof(Rml::Vertex, position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Rml::Vertex), (void*)offsetof(Rml::Vertex, colour));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Rml::Vertex), (void*)offsetof(Rml::Vertex, tex_coord));

	glBindVertexArray(0);

	geo.index_count = (int)indices.size();
	Rml::CompiledGeometryHandle handle = next_geometry_handle++;
	geometry_map[handle] = geo;
	return handle;
}

void OpenGlRmlUiRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {
	auto it = geometry_map.find(geometry);
	if (it == geometry_map.end())
		return;

	glActiveTexture(GL_TEXTURE0);
	if (texture != 0) {
		auto tex_it = texture_map.find(texture);
		glBindTexture(GL_TEXTURE_2D, tex_it != texture_map.end() ? tex_it->second : white_texture);
	} else {
		glBindTexture(GL_TEXTURE_2D, white_texture);
	}

	glUniform2f(uniform_translation, translation.x, translation.y);
	glBindVertexArray(it->second.vao);
	glDrawElements(GL_TRIANGLES, it->second.index_count, GL_UNSIGNED_INT, nullptr);
}

void OpenGlRmlUiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
	auto it = geometry_map.find(geometry);
	if (it == geometry_map.end())
		return;
	glDeleteVertexArrays(1, &it->second.vao);
	glDeleteBuffers(1, &it->second.vbo);
	glDeleteBuffers(1, &it->second.ebo);
	geometry_map.erase(it);
}

Rml::TextureHandle OpenGlRmlUiRenderInterface::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
	// Image loading goes through RmlUiSystem's FileInterface + engine asset
	// pipeline at a higher level; this backend only accepts pixels via
	// GenerateTexture. Unimplemented file-path texture loading here means
	// <img> src= must be routed through the engine's texture loader instead
	// (documented limitation for v1).
	sys_print(Warning, "OpenGlRmlUiRenderInterface::LoadTexture not implemented for source '%s'\n", source.c_str());
	texture_dimensions = { 0, 0 };
	return 0;
}

Rml::TextureHandle OpenGlRmlUiRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
	unsigned int tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, source_dimensions.x, source_dimensions.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, source.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	Rml::TextureHandle handle = next_texture_handle++;
	texture_map[handle] = tex;
	return handle;
}

void OpenGlRmlUiRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
	auto it = texture_map.find(texture);
	if (it == texture_map.end())
		return;
	glDeleteTextures(1, &it->second);
	texture_map.erase(it);
}

void OpenGlRmlUiRenderInterface::EnableScissorRegion(bool enable) {
	if (enable)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
}

void OpenGlRmlUiRenderInterface::SetScissorRegion(Rml::Rectanglei region) {
	// GL scissor origin is bottom-left; RmlUi's region origin is top-left.
	glScissor(region.Left(), viewport_height - region.Top() - region.Height(), region.Width(), region.Height());
}
