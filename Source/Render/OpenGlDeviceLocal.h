#pragma once
// Internal shared header for the OpenGL device split.
// Included by OpenGlDevice.cpp, OpenGlTextureImpl.cpp, OpenGlBufferImpl.cpp.
// Do NOT include from any public header.

#include "IGraphicsDevice.h"
#include "DrawLocal.h"
#include "Framework/StringUtils.h"
#include "Framework/BinaryReadWrite.h"
#include <array>
#include <vector>
#include <span>
#include <optional>

using std::array;
using std::vector;
template <typename T> using opt = std::optional<T>;
using std::string;

// Global GPU memory accounting (defined in OpenGlDevice.cpp)
extern int total_gfx_mem_usage;

class OpenglDataStatic
{
public:
	// Stores base-interface pointers so this TU does not need the concrete classes.
	hash_set<IGraphicsTexture> all_textures;
	hash_set<IGraphicsBuffer>  all_buffers;

	void dump_to_disk(std::string str);
};

// Defined in OpenGlDevice.cpp
extern OpenglDataStatic opengl_stats;

// ---------------------------------------------------------------------------
// Factory functions (implemented in OpenGlTextureImpl.cpp / OpenGlBufferImpl.cpp)
// Used by OpenGLDeviceImpl so it does not need the concrete class definitions.
// ---------------------------------------------------------------------------

// Returns a default-constructed sentinel texture (no GL resources allocated).
// Caller owns lifetime; must not be released via IGraphicsTexture::release().
IGraphicsTexture* opengl_make_swapchain_sentinel();

IGraphicsTexture*     opengl_create_texture(const CreateTextureArgs& args);
IGraphicsBuffer*      opengl_create_buffer(const CreateBufferArgs& args);
IGraphicsVertexInput* opengl_create_vertex_input(const CreateVertexInputArgs& args);

// Shader factories — implemented in OpenGlShaderImpl.cpp. Phase 1.7a thin
// wrappers around the legacy Shader::compile* helpers; the GL calls migrate
// into the backend in 1.7b.
IGraphicsShader* opengl_create_shader_vert_frag(const std::string& vert_path,
												const std::string& frag_path,
												const std::string& defines);
IGraphicsShader* opengl_create_shader_vert_frag_geo(const std::string& vert_path,
													const std::string& frag_path,
													const std::string& geo_path,
													const std::string& defines);
IGraphicsShader* opengl_create_shader_compute(const std::string& compute_path,
											  const std::string& defines);
IGraphicsShader* opengl_create_shader_single_file(const std::string& shared_path,
												  const std::string& defines);
IGraphicsShader* opengl_create_shader_single_file_tess(const std::string& shared_path,
													   const std::string& defines);

// Format mapping helper — also used by OpenglDataStatic::dump_to_disk
const char* opengl_texture_format_to_str(GraphicsTextureFormat fmt);

// Filter enum to GL enum — used by blit_textures
GLenum opengl_filter_to_gl(GraphicsFilterType type);
