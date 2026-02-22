#pragma once
#if 1
#include "MaterialPublic.h"
#include <span>
#include <string_view>
#include <optional>

// A FAILED HALF IMPLMENTED GARBAGE F*KING REFACTOR

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

enum class GraphicsTextureType : int8_t {
	t2D,
	t2DArray,
	t3D,
	tCubemap,
	tCubemapArray
};
enum class GraphicsTextureFormat : int8_t {
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
	r11f_g11f_b10f,
	rgba16_snorm,
};

enum class GraphicsFilterType : int8_t {
	Linear,
	Nearest,
	MipmapLinear,
};
enum class GraphicsTextureEdge : int8_t {
	Repeat,
	Clamp
};


template<typename T>
inline void safe_release(T*& ptr) {
	if (ptr) {
		ptr->release();
		ptr = nullptr;
	}
}

class IGraphicsTexture {
public:
	virtual ~IGraphicsTexture() {}
	virtual void sub_image_upload(int layer, int x, int y, int w, int h, int size, const void* data) = 0;
	virtual void sub_image_upload_3d(int z, int layer, int x, int y, int w, int h, int size, const void* data) = 0;

	virtual void release() = 0;
	virtual uint32_t get_internal_handle() = 0;

	virtual bool is_compressed() const = 0;
	virtual int get_num_mips() const = 0;
	virtual glm::ivec2 get_size() const = 0;
	virtual GraphicsTextureFormat get_texture_format() const = 0;
	virtual GraphicsTextureType get_texture_type() const = 0;
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



// like a VAO in opengl.
// enacapsulates vertex buffer, index buffer, vertex attribute state, index type
class IGraphicsVertexInput {
public:
	virtual ~IGraphicsVertexInput() {}
	virtual void release() = 0;

	virtual uint32_t get_internal_handle() = 0;
};


struct ColorTargetInfo {
	ColorTargetInfo(IGraphicsTexture* texture, int layer = -1, int mip = 0) {
		assert(texture);
		this->texture = texture;
		this->layer = layer;
		this->mip = mip;
	}

	IGraphicsTexture* texture = nullptr;
	int layer = -1;
	int mip = 0;
};

struct RenderPassState {
	std::span<const ColorTargetInfo> color_infos;
	IGraphicsTexture* depth_info = nullptr;
	int depth_layer = -1;

	void set_clear_both(bool b) {
		wants_color_clear = wants_depth_clear = b;
	}

	bool wants_color_clear = false;
	bool wants_depth_clear = false;
	float clear_depth_val = 0.0;

	bool use_gray_clear = false;
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


enum class GraphicsSamplerType {
	AnisotropyDefault,
	LinearDefault,
	NearestDefault,
	NearestClamped,
	LinearClamped,
	CsmShadowmap,
	AtlasShadowmap,
	CubemapDefault,
};


struct CreateTextureArgs {
	GraphicsTextureType type = GraphicsTextureType::t2D;
	GraphicsTextureFormat format = GraphicsTextureFormat::rgba8;
	int width = 0;
	int height = 0;
	int num_mip_maps = 1;
	int depth_3d = 0;
	GraphicsSamplerType sampler_type=GraphicsSamplerType::AnisotropyDefault;

	bool float_input_is_16f = false;
};

struct CreateBufferArgs {
	int size = 0;
	GraphicsBufferUseFlags flags = {};
};

struct GraphicsBlitTarget {
	IGraphicsTexture* texture = nullptr;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	int mip = 0;
	int layer = -1;
};
struct GraphicsBlitInfo {
	GraphicsBlitTarget src;
	GraphicsBlitTarget dest;
	GraphicsFilterType filter = GraphicsFilterType::Nearest;

	void set_width_height_both(int w, int h) {
		src.w = dest.w = w;
		src.h = dest.h = h;
	}
};
class ThingerBobber {
public:
	virtual void set_depth_write_enabled(bool b) = 0;
};
class IGraphicsDevice {
public:
	static IGraphicsDevice* inst;
	static IGraphicsDevice* create_opengl_device(ThingerBobber* FUUUUUUUCK);

	virtual ~IGraphicsDevice() {}

	virtual GraphicsDeviceType get_device_type() = 0;
	virtual IGraphicsTexture* get_swapchain_texture() = 0;

	// render passes describe what you are rendering to
	virtual void set_render_pass(const RenderPassState& state) = 0;
	virtual void blit_textures(const GraphicsBlitInfo& info) = 0;

	virtual IGraphicsTexture* create_texture(const CreateTextureArgs& args) = 0;
	virtual IGraphicsBuffer* create_buffer(const CreateBufferArgs& args) = 0;
	virtual IGraphicsVertexInput* create_vertex_input(const CreateVertexInputArgs& args) = 0;
};
#endif