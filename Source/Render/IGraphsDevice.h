#pragma once

#include "MaterialPublic.h"
#include <span>
#include <string_view>
enum BindingBits {
	BINDING_FRAGMENT=1,
	BINDING_VERTEX=2,
};

enum class GraphicsPrimitiveType : int8_t {
	Triangles,
	TriangleStrip,
	Lines
};

enum class GraphicsVertexAttribType {
	u8,
	u16,
	i8,
	i16,
	u8_normalized,
	u16_normalized,
	i8_normalized,
	i16_normalized,
	float32,
};

enum class GraphicsDeviceType {
	Unknown,
	OpenGl,
	Vulkan,
	Directx12,
	Metal,
};

enum GraphicsBufferUseFlags {
	BUFFER_USE_AS_VB = 1,
	BUFFER_USE_AS_IB = 2,
	BUFFER_USE_AS_STORAGE_READ = 4,
	BUFFER_USE_AS_INDIRECT = 8,
	BUFFER_USE_DYNAMIC = 16,
};
enum class VertexInputIndexType : int8_t {
	uint16,
	uint32
};

enum class GraphicsTextureType {
	t2D,
	t2DArray,
	t3D,
	tCubemap,
};
enum class GraphicsTextureFormat {
	r8,
	rg8,
	rgb8,
	rgba8,
	r16f,
	rg16f,
	rgb16f,
	rgba16f,
	r32f,
	rg32f,
	bc1,
	bc3,
	bc4,
	bc5,
	bc6,
	depth24f,
	depth32f,
	depth24stencil8,
};

enum class GraphicsFilterType : int8_t {
	Linear,
	Nearest
};
enum class GraphicsTextureEdge : int8_t {
	Repeat,
	Clamp
};


class IGraphicsTexture {
public:
	virtual ~IGraphicsTexture() {}
	virtual void sub_image_upload(int layer, int x, int y, int w, int h, int size, const void* data) = 0;
	virtual void release() = 0;

	virtual uint32_t get_internal_handle() = 0;

};

// used for vertex,index,uniform, and shader storage buffers
class IGraphicsBuffer {
public:
	virtual ~IGraphicsBuffer() {}
	virtual void upload(const void* data, int size) = 0;
	virtual void sub_upload(const void* data, int size, int offset) = 0;
	virtual void release() = 0;

	virtual uint32_t get_internal_handle() = 0;
};

class IGraphicsProgram {
public:
	virtual ~IGraphicsProgram() {}
	virtual void release() = 0;
	virtual uint32_t get_internal_handle() = 0;
};

// like a VAO in opengl.
// enacapsulates vertex buffer, index buffer, vertex attribute state, index type
class IGraphicsVertexInput {
public:
	virtual ~IGraphicsVertexInput() {}
	virtual void release() = 0;

	virtual uint32_t get_internal_handle() = 0;
};


struct RenderPassState {
	std::span<IGraphicsTexture* const> color_infos;
	IGraphicsTexture* depth_info = nullptr;
	bool wants_color_clear = false;
	bool wants_depth_clear = false;
	float clear_depth = 0.0;
};



struct GraphicsPipelineState {
	bool backface_culling = true;
	bool cull_front_face = false;
	bool depth_testing = true;
	bool depth_less_than = false;
	bool depth_writes = true;
	//blend_state blend = blend_state::OPAQUE;
	GraphicsPrimitiveType primitive = GraphicsPrimitiveType::Triangles;
	IGraphicsProgram* program = nullptr;
	IGraphicsVertexInput* vertex_input = nullptr;
};


struct VertexLayout {
	VertexLayout(int index, int count, GraphicsVertexAttribType type, int stride, int offset) {
		this->index = index;
		this->count = count;
		this->type = type;
		this->stride = stride;
		this->offset = offset;
	}

	int index=0;
	int count=0;
	GraphicsVertexAttribType type{};
	int stride=0;
	int offset=0;
};


struct CreateVertexInputArgs {
	IGraphicsBuffer* vertex = nullptr;
	IGraphicsBuffer* index = nullptr;
	std::span<const VertexLayout> layout;
	VertexInputIndexType index_type = VertexInputIndexType::uint16;
};

// opengl:
// compiles the fragment and vertex source code. dumps a binary obj to disk
// sdl3:
//	have spriv/dxil/metal ...
//  for dynamic compilation:
//		takes file name. loads the file. runs the platforms compilier. dumps bin to disk.
//		load the binary. done.
//	for ahead of time compilied:
//		just load binary if it exists. otherwise, error.

struct CreateProgramArgs {
	std::string_view file_name;	// a glsl file with vertex and fragment #ifdef'd
	std::string_view defines;	// comma seperated list
};


struct CreateSamplerArgs {
	GraphicsFilterType min_filter = GraphicsFilterType::Linear;
	GraphicsFilterType max_filter = GraphicsFilterType::Linear;
	GraphicsFilterType mipmap_filter = GraphicsFilterType::Linear;
	GraphicsTextureEdge u_edge = GraphicsTextureEdge::Repeat;
	GraphicsTextureEdge v_edge = GraphicsTextureEdge::Repeat;
	GraphicsTextureEdge w_edge = GraphicsTextureEdge::Repeat;
};
struct CreateTextureArgs {
	GraphicsTextureType type = GraphicsTextureType::t2D;
	GraphicsTextureFormat format = GraphicsTextureFormat::rgba8;
	int width = 0;
	int height = 0;
	int num_mip_maps = 0;
	int depth_3d = 0;
	CreateSamplerArgs sampler_args;
};

struct CreateBufferArgs {
	int size = 0;
	GraphicsBufferUseFlags flags = {};
};


class IGraphicsDevice {
public:
	static IGraphicsDevice* inst;
	static IGraphicsDevice* create_opengl_device();

	virtual ~IGraphicsDevice() {}

	virtual GraphicsDeviceType get_device_type() = 0;

	// pipelines bundle all the state you need for a draw call
	virtual void set_pipeline(const GraphicsPipelineState& state) = 0;

	// render passes describe what you are rendering to
	virtual void set_render_pass(const RenderPassState& state) = 0;
	virtual void end_render_pass() = 0;

	// when starting a renderpass, viewport is automatically set. can also manually set it here
	// same for scissor. starting renderpass clears scissor, can set and clear it manually also
	virtual void set_viewport(int x, int y, int w, int h) = 0;
	virtual void set_scissor(int x, int y, int w, int h) = 0;
	virtual void clear_scissor() = 0;

	virtual void draw_arrays(int offset, int count) = 0;
	virtual void draw_elements(int offset, int count) = 0;
	virtual void multidraw_elements_indirect(IGraphicsBuffer* buffer, int offset, int count) = 0;

	virtual void bind_storage_buffers(std::span<IGraphicsBuffer* const> buffers) = 0;
	virtual void bind_textures(std::span<IGraphicsTexture* const> textures) = 0;

	virtual IGraphicsProgram* create_program(const CreateProgramArgs& args) = 0;
	virtual IGraphicsTexture* create_texture(const CreateTextureArgs& args) = 0;
	virtual IGraphicsBuffer* create_buffer(const CreateBufferArgs& args) = 0;
	virtual IGraphicsVertexInput* create_vertex_input(const CreateVertexInputArgs& args) = 0;
};