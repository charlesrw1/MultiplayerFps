#pragma once
#if 1
#include "MaterialPublic.h"
#include <span>
#include <string_view>
#include <optional>

enum BindingBits
{
	BINDING_FRAGMENT = 1,
	BINDING_VERTEX = 2,
};

enum class GraphicsPrimitiveType : int8_t
{
	Triangles,
	TriangleStrip,
	Lines
};

enum class GraphicsVertexAttribType
{
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

enum class GraphicsDeviceType
{
	Unknown,
	OpenGl,
};

enum GraphicsBufferUseFlags
{
	BUFFER_USE_AS_VB = 1,
	BUFFER_USE_AS_IB = 2,
	BUFFER_USE_AS_STORAGE_READ = 4,
	BUFFER_USE_AS_INDIRECT = 8,
	BUFFER_USE_DYNAMIC = 16,
};
enum class VertexInputIndexType : int8_t
{
	uint16,
	uint32
};

enum class GraphicsTextureType : int8_t
{
	t2D,
	t2DArray,
	t3D,
	tCubemap,
	tCubemapArray
};
enum class GraphicsTextureFormat : int8_t
{
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
	bc1_srgb,
	bc3,
	bc4,
	bc5,
	bc6,
	depth16f,
	depth24f,
	depth32f,
	depth24stencil8,
	r11f_g11f_b10f,
	rgba16_snorm,
};

enum class GraphicsFilterType : int8_t
{
	Linear,
	Nearest,
	MipmapLinear,
};

enum class GraphicsImageAccess : int8_t
{
	ReadOnly,
	WriteOnly,
	ReadWrite,
};

enum GraphicsMemoryBarrierBits : uint32_t
{
	BARRIER_SHADER_STORAGE = 1 << 0,
	BARRIER_SHADER_IMAGE_ACCESS = 1 << 1,
	BARRIER_COMMAND = 1 << 2,
	BARRIER_TEXTURE_FETCH = 1 << 3,
};
enum class GraphicsTextureEdge : int8_t
{
	Repeat,
	Clamp
};

enum class GraphicsSamplerFilter : int8_t
{
	Nearest,
	Linear,
	LinearMipmapNearest,
	LinearMipmapLinear,
};

enum class GraphicsSamplerWrap : int8_t
{
	Repeat,
	ClampToEdge,
};

// Per-sample reduction. Min/Max wrap GL_TEXTURE_REDUCTION_MODE_ARB (filter-minmax,
// used by HiZ depth pyramids). SDL3 GPU has no equivalent; replaced with a
// compute min/max pre-pass in Phase 3.
enum class GraphicsSamplerReduction : int8_t
{
	WeightedAverage,
	Min,
	Max,
};

template <typename T> inline void safe_release(T*& ptr) {
	if (ptr) {
		ptr->release();
		ptr = nullptr;
	}
}

class IGraphicsTexture
{
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
	virtual int get_compressed_stride() const = 0;

	virtual void clear_image() = 0;

	// Returns approximate GPU memory usage in bytes (for stats / diagnostics).
	virtual int get_mem_usage() const { return 0; }
};

// used for vertex,index,uniform, and shader storage buffers
class IGraphicsBuffer
{
public:
	virtual ~IGraphicsBuffer() {}
	virtual void upload(const void* data, int size) = 0;
	virtual void sub_upload(const void* data, int size, int offset) = 0;
	virtual void release() = 0;

	virtual uint32_t get_internal_handle() = 0;

	// Returns the current buffer size in bytes (for stats / diagnostics).
	virtual int get_buf_size() const { return 0; }
};

// like a VAO in opengl.
// enacapsulates vertex buffer, index buffer, vertex attribute state, index type
class IGraphicsVertexInput
{
public:
	virtual ~IGraphicsVertexInput() {}
	virtual void release() = 0;

	virtual uint32_t get_internal_handle() = 0;
};

// Standalone sampler object. Bound to a texture slot via gfx().bind_sampler;
// when bound, it overrides the sampler params baked into the texture itself.
class IGraphicsSampler
{
public:
	virtual ~IGraphicsSampler() {}
	virtual void release() = 0;
	virtual uint32_t get_internal_handle() = 0;
};

struct CreateSamplerArgs
{
	GraphicsSamplerFilter min_filter = GraphicsSamplerFilter::Linear;
	GraphicsSamplerFilter mag_filter = GraphicsSamplerFilter::Linear;
	GraphicsSamplerWrap   wrap       = GraphicsSamplerWrap::ClampToEdge;
	GraphicsSamplerReduction reduction = GraphicsSamplerReduction::WeightedAverage;
};

struct ColorTargetInfo
{
	ColorTargetInfo(IGraphicsTexture* texture, int layer = -1, int mip = 0) {
		assert(texture);
		this->texture = texture;
		this->layer = layer;
		this->mip = mip;
	}

	IGraphicsTexture* texture = nullptr;
	int layer = -1;
	int mip = 0;

	// Load-op: if wants_clear, the pass begins by clearing this attachment to
	// clear_color. Mirrors SDL_GPUColorTargetInfo.{load_op, clear_color}.
	bool wants_clear = false;
	glm::vec4 clear_color = glm::vec4(0, 0, 0, 1);
};

struct RenderPassState
{
	std::span<const ColorTargetInfo> color_infos;
	IGraphicsTexture* depth_info = nullptr;
	int depth_layer = -1;

	bool wants_depth_clear = false;
	float clear_depth_val = 0.0;
};

struct VertexLayout
{
	VertexLayout(int index, int count, GraphicsVertexAttribType type, int stride, int offset) {
		this->index = index;
		this->count = count;
		this->type = type;
		this->stride = stride;
		this->offset = offset;
	}

	int index = 0;
	int count = 0;
	GraphicsVertexAttribType type{};
	int stride = 0;
	int offset = 0;
};

struct CreateVertexInputArgs
{
	IGraphicsBuffer* vertex = nullptr;
	IGraphicsBuffer* index = nullptr;
	std::span<const VertexLayout> layout;
	VertexInputIndexType index_type = VertexInputIndexType::uint16;
};

enum class GraphicsSamplerType
{
	AnisotropyDefault,
	LinearDefault,
	NearestDefault,
	NearestClamped,
	LinearClamped,
	CsmShadowmap,
	AtlasShadowmap,
	CubemapDefault,
	DepthPyramid,
	LinearNoMipmaps,
};

struct CreateTextureArgs
{
	GraphicsTextureType type = GraphicsTextureType::t2D;
	GraphicsTextureFormat format = GraphicsTextureFormat::rgba8;
	int width = 0;
	int height = 0;
	int num_mip_maps = 1;
	int depth_3d = 0;
	GraphicsSamplerType sampler_type = GraphicsSamplerType::AnisotropyDefault;

	bool float_input_is_16f = false;
};

struct CreateBufferArgs
{
	int size = 0;
	GraphicsBufferUseFlags flags = {};
};

struct GraphicsBlitTarget
{
	IGraphicsTexture* texture = nullptr;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	int mip = 0;
	int layer = -1;
};
struct GraphicsBlitInfo
{
	GraphicsBlitTarget src;
	GraphicsBlitTarget dest;
	GraphicsFilterType filter = GraphicsFilterType::Nearest;

	void set_width_height_both(int w, int h) {
		src.w = dest.w = w;
		src.h = dest.h = h;
	}
};
class IGraphicsDevice
{
public:
	virtual ~IGraphicsDevice() {}

	virtual GraphicsDeviceType get_device_type() = 0;
	virtual IGraphicsTexture* get_swapchain_texture() = 0;

	// render passes describe what you are rendering to
	virtual void set_render_pass(const RenderPassState& state) = 0;
	virtual void blit_textures(const GraphicsBlitInfo& info) = 0;

	virtual IGraphicsTexture* create_texture(const CreateTextureArgs& args) = 0;
	virtual IGraphicsBuffer* create_buffer(const CreateBufferArgs& args) = 0;
	virtual IGraphicsVertexInput* create_vertex_input(const CreateVertexInputArgs& args) = 0;

	// ---- Phase 1 wrap surface (GL-shaped). Methods are added per sub-phase.

	// Scissor rect. Coordinates in pixels, origin bottom-left to match GL.
	virtual void set_scissor(int x, int y, int w, int h) = 0;
	virtual void disable_scissor() = 0;

	// Draw an indexed primitive batch. byte_offset is the byte offset into the
	// currently bound index buffer (matches glDrawElementsBaseVertex's indices*).
	virtual void draw_elements_base_vertex(GraphicsPrimitiveType mode, int count,
										   VertexInputIndexType index_type,
										   int byte_offset, int base_vertex) = 0;

	// Transitional: binds a GL UBO handle to a binding point. Will be replaced
	// by bind_uniform_buffer(int, IGraphicsBuffer*, int, int) once UBOs migrate
	// off raw bufferhandle in a later sub-phase. The _raw suffix flags the leak.
	virtual void bind_uniform_buffer_base_raw(int slot, uint32_t buffer_handle) = 0;

	// Block until all submitted GPU work has completed. Debug/editor readback
	// only; do NOT call on hot paths. Wraps glFlush + glFinish.
	virtual void wait_for_gpu_idle() = 0;

	// Read back a 2D texture's mip 0 image into dest. dest_size_bytes must
	// match width * height * bytes-per-pixel for the texture's format. Backend
	// infers GL format/type from tex->get_texture_format(). Currently supports
	// depth32f and rgba8 (editor pick/depth paths). Debug/editor only.
	virtual void download_texture_2d(IGraphicsTexture* tex, int mip,
									 void* dest, int dest_size_bytes) = 0;

	// Non-indexed draw. mode + count follow glDrawArrays semantics.
	virtual void draw_arrays(GraphicsPrimitiveType mode, int first, int count) = 0;

	// Bind a texture to a sampler/texture slot.
	virtual void bind_texture(int slot, IGraphicsTexture* tex) = 0;

	// Bind a UBO to a binding point. Non-raw variant for buffers created via
	// create_buffer; the raw variant above is the bridge for the global
	// ubo.current_frame still on bufferhandle.
	virtual void bind_uniform_buffer_base(int slot, IGraphicsBuffer* buf) = 0;

	// Clamp the mip range visible to sampling. Used by the specular-prefilter
	// pass to read mip N-1 while rendering into mip N of the same cubemap.
	// Wraps glTextureParameteri(GL_TEXTURE_BASE_LEVEL/MAX_LEVEL).
	virtual void set_mip_range(IGraphicsTexture* tex, int base, int max) = 0;

	// Read back a sub-image at (mip, layer) into dest. layer selects the cube
	// face for tCubemap (0..5) or array slice for t2DArray/tCubemapArray; pass
	// -1 for non-layered textures. dest_size_bytes must match the layer's
	// pixel-format size. Backend infers format/type from get_texture_format().
	// Currently supports rgb16f, rgba8, depth* (debug/editor/bake paths).
	virtual void download_texture(IGraphicsTexture* tex, int mip, int layer,
								  void* dest, int dest_size_bytes) = 0;

	// ---- Phase 1.3a wrap surface (compute) --------------------------------

	// Launch a compute dispatch. Wraps glDispatchCompute.
	virtual void dispatch_compute(int groups_x, int groups_y, int groups_z) = 0;

	// Memory barrier. bits is a bitwise-OR of GraphicsMemoryBarrierBits.
	// Wraps glMemoryBarrier with the corresponding GL_*_BARRIER_BIT mask.
	virtual void memory_barrier(uint32_t bits) = 0;

	// Bind a texture mip+layer to an image slot for compute read/write/RW.
	// layer == -1 binds all layers (3D / 2D-array / cubemap as a layered image,
	// glBindImageTexture's `layered=GL_TRUE`). layer >= 0 binds a single slice
	// (`layered=GL_FALSE, layer=layer`). The image format is inferred from
	// tex->get_texture_format().
	virtual void bind_image_for_compute(int slot, IGraphicsTexture* tex,
										int mip, int layer,
										GraphicsImageAccess access) = 0;

	// Bind an SSBO to a binding slot. Non-raw variant for buffers created via
	// create_buffer; _raw is the transitional bridge for sites still holding a
	// raw bufferhandle (e.g. draw.ubo.current_frame, batch-built MDI buffer).
	virtual void bind_storage_buffer_base(int slot, IGraphicsBuffer* buf) = 0;
	virtual void bind_storage_buffer_base_raw(int slot, uint32_t buffer_handle) = 0;

	// ---- Phase 1.3b wrap surface (samplers + buffer clear) -----------------

	virtual IGraphicsSampler* create_sampler(const CreateSamplerArgs& args) = 0;

	// Bind a sampler to a texture slot; overrides the bound texture's own
	// sampler params for the duration of the bind. Pass nullptr to unbind
	// (revert to texture-default sampling). Wraps glBindSampler.
	virtual void bind_sampler(int slot, IGraphicsSampler* sampler) = 0;

	// Fill a buffer with a repeated 32-bit value (currently R32UI). Used to
	// reset MDI count/visibility buffers between frames. Wraps
	// glClearNamedBufferData.
	virtual void clear_buffer_uint32(IGraphicsBuffer* buf, uint32_t value) = 0;

	// ---- Phase 1.3c wrap surface (buffer readback) -------------------------

	// Read [offset, offset+size) bytes from buf into dest. Callers are
	// responsible for issuing the right memory_barrier()/wait_for_gpu_idle()
	// beforehand so the data is visible. Debug/editor/GI-relocate readback
	// only; never call on a hot path. Wraps glGetNamedBufferSubData.
	virtual void download_buffer(IGraphicsBuffer* buf, int offset, int size, void* dest) = 0;

	// ---- Phase 1.4a wrap surface (lighting) --------------------------------

	// Wraps glLineWidth. Used by debug meshbuilder line rendering.
	virtual void set_line_width(float width) = 0;

	// Bind a buffer to GL_DRAW_INDIRECT_BUFFER for the next MDI draw, or
	// nullptr to unbind. Wraps glBindBuffer(GL_DRAW_INDIRECT_BUFFER, ...).
	// Phase 2c folds indirect binding into pipeline/encoder state.
	virtual void bind_indirect_buffer(IGraphicsBuffer* buf) = 0;

	// ---- Phase 1.4b wrap surface (batched indirect draws) ------------------

	// Bind a buffer to GL_PARAMETER_BUFFER (supplies per-batch draw counts to
	// glMultiDrawElementsIndirectCount). nullptr unbinds. Phase 2c folds
	// parameter binding into pipeline/encoder state.
	virtual void bind_parameter_buffer(IGraphicsBuffer* buf) = 0;

	// Immediate polygon-offset state for shadow biasing. enabled toggles
	// GL_POLYGON_OFFSET_FILL; factor/units mirror glPolygonOffset.
	// Phase 2c bakes polygon offset into IGraphicsRasterPipeline.
	virtual void set_polygon_offset(bool enabled, float factor, float units) = 0;

	// glMultiDrawElementsIndirectCount: pulls `<= max_draw_count` indirect
	// draws from the currently bound GL_DRAW_INDIRECT_BUFFER starting at
	// `indirect_byte_offset`, using the count at `count_byte_offset` in the
	// bound GL_PARAMETER_BUFFER. `stride` is the byte stride between commands
	// (typically sizeof(DrawElementsIndirectCommand)).
	virtual void multi_draw_elements_indirect_count(GraphicsPrimitiveType mode,
													VertexInputIndexType index_type,
													int indirect_byte_offset,
													int count_byte_offset,
													int max_draw_count,
													int stride) = 0;

	// ---- Phase 1.4c wrap surface (orchestration) ---------------------------

	// glMultiDrawElementsIndirect. `indirect` is interpreted as a byte offset
	// into the bound GL_DRAW_INDIRECT_BUFFER when one is bound, or as a CPU
	// pointer when buffer 0 is bound (client-side MDI). draw_count = number
	// of commands; stride = byte stride between commands.
	virtual void multi_draw_elements_indirect(GraphicsPrimitiveType mode,
											  VertexInputIndexType index_type,
											  const void* indirect,
											  int draw_count,
											  int stride) = 0;

	// glDrawElementsInstancedBaseVertexBaseInstance. byte_offset is the byte
	// offset into the bound index buffer (matches glDraw*Elements's indices*).
	virtual void draw_elements_instanced_base_vertex_base_instance(
		GraphicsPrimitiveType mode, int count, VertexInputIndexType index_type,
		int byte_offset, int instance_count, int base_vertex,
		uint32_t base_instance) = 0;

	// glDrawElements. byte_offset is the byte offset into the bound index
	// buffer (matches glDrawElements's indices*).
	virtual void draw_elements(GraphicsPrimitiveType mode, int count,
							   VertexInputIndexType index_type,
							   int byte_offset) = 0;

	// Bind a sub-range of a raw GL buffer to an SSBO slot. Wraps
	// glBindBufferRange(GL_SHADER_STORAGE_BUFFER, …). The _raw suffix flags
	// that the buffer still lives as a raw `bufferhandle`; folds into
	// IGraphicsBuffer* in a later sub-phase.
	virtual void bind_storage_buffer_range_raw(int slot, uint32_t buffer_handle,
											   int offset, int size) = 0;

	// Bind a raw GL buffer to GL_DRAW_INDIRECT_BUFFER (handle 0 unbinds).
	// _raw escape for sites whose buffer is still a `bufferhandle`.
	virtual void bind_indirect_buffer_raw(uint32_t buffer_handle) = 0;

	// glNamedBufferData on a raw buffer handle. _raw escape for sites whose
	// buffer is still a `bufferhandle`; folds into IGraphicsBuffer::upload
	// (and Phase 2e's ring buffer) once those buffers migrate.
	virtual void upload_buffer_raw(uint32_t buffer_handle, int size,
								   const void* data) = 0;

	// glNamedBufferSubData on a raw buffer handle. _raw escape; see above.
	virtual void sub_upload_buffer_raw(uint32_t buffer_handle, int offset,
									   int size, const void* data) = 0;
};

// Global accessor for the active graphics device. Initialize the OpenGL backend
// with gfx_init_opengl() during renderer init; tear it down with gfx_shutdown()
// before exit. gfx() asserts initialization.
IGraphicsDevice& gfx();
bool gfx_is_initialized();
void gfx_init_opengl();
void gfx_shutdown();
#endif