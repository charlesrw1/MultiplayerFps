#pragma once
#if 1
#include "MaterialPublic.h"
#include <span>
#include <string>
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

// Polygon rasterization fill mode. Mirrors SDL_GPUFillMode.
// Phase 2c folds this onto RenderPipelineState::fill_mode.
enum class GraphicsFillMode : int8_t
{
	Fill,
	Line,
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

	// Clamp the mip range visible to sampling. Used by the specular-prefilter
	// pass to read mip N-1 while rendering into mip N of the same cubemap.
	// Wraps glTextureParameteri(GL_TEXTURE_BASE_LEVEL/MAX_LEVEL).
	virtual void set_mip_range(int base, int max) = 0;

	// Auto-generate the mipmap chain from mip 0. Wraps glGenerateTextureMipmap.
	virtual void generate_mipmaps() = 0;

	// Read back a sub-image at (mip, layer) into dest. layer selects the cube
	// face for tCubemap (0..5) or array slice for t2DArray/tCubemapArray; pass
	// -1 for non-layered textures. dest_size_bytes must match the layer's
	// pixel-format size. Backend infers format/type from get_texture_format().
	// Currently supports rgb16f, rgba8, depth* (debug/editor/bake paths).
	virtual void download(int mip, int layer, void* dest, int dest_size_bytes) = 0;

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

// Compiled+linked GPU program (vert+frag / vert+frag+geo / vert+frag+tess /
// compute). Owns the underlying GL program (or future SDL3 GPU shader objects).
// Created via gfx().create_shader_*; destroyed via release(). Bound as part of
// RenderPipelineState; cf. Pipeline model in docs/rendering/gfx_abstraction.md.
class IGraphicsShader
{
public:
	virtual ~IGraphicsShader() {}
	virtual void release() = 0;
	virtual uint32_t get_internal_handle() = 0;

	// Per-stage resource counts (Phase 2 B2). Required by
	// SDL_GPUShaderCreateInfo, which wants {num_samplers,
	// num_storage_textures, num_storage_buffers, num_uniform_buffers}
	// up-front for each stage. OpenGL impl walks GL_UNIFORM,
	// GL_UNIFORM_BLOCK, GL_SHADER_STORAGE_BLOCK with
	// GL_REFERENCED_BY_{VERTEX,FRAGMENT,COMPUTE}_SHADER.
	struct PerStageCounts
	{
		int num_samplers         = 0; // sampler*/image-only-read uniforms
		int num_storage_textures = 0; // image* (writable images)
		int num_storage_buffers  = 0; // SSBO blocks
		int num_uniform_buffers  = 0; // UBO blocks
	};
	struct Reflection
	{
		PerStageCounts vertex;
		PerStageCounts fragment;
		PerStageCounts compute;
	};
	virtual Reflection reflect() = 0;
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
// Pipeline state struct. set_pipeline diffs this against an internal cache and
// emits only the deltas (program / blend / depth / vao / etc).
struct RenderPipelineState
{
	RenderPipelineState() = default;

	bool backface_culling = true;
	bool cull_front_face = false;
	bool depth_testing = true;
	bool depth_less_than = false;
	bool depth_writes = true;
	BlendState blend = BlendState::OPAQUE;
	// Phase 1.7d: was program_handle (int index into Program_Manager). Now an
	// IGraphicsShader* — set_pipeline can bind without indirection and Phase 2c
	// SDL3 pipeline-cache hashing can read the pointer straight from the struct.
	IGraphicsShader* program = nullptr;
	// Phase 2c: was vertexarrayhandle. Backend resolves to GL handle / SDL3
	// vertex-layout state at bind time. Pipeline cache hashes the pointer.
	IGraphicsVertexInput* vao = nullptr;

	// Phase 2c: folded from former immediate setters. SDL3 GPU bakes these at
	// pipeline-create time; OpenGL backend keeps them in the invalid-bits cache.

	// Polygon offset for shadow bias / decals.
	bool polygon_offset_enabled = false;
	float polygon_offset_factor = 0.f;
	float polygon_offset_units = 0.f;
	// Per-attachment color write mask (RGBA). Indices match the active
	// framebuffer's draw-buffer slots. Default: write-all.
	static const int MAX_COLOR_ATTACHMENTS = 8;
	struct ColorWriteMask { bool r = true, g = true, b = true, a = true; };
	ColorWriteMask color_write_masks[MAX_COLOR_ATTACHMENTS]{};
};

class IGraphicsDevice
{
public:
	virtual ~IGraphicsDevice() {}

	virtual GraphicsDeviceType get_device_type() = 0;

	// ---- Phase 2d frame lifecycle / command encoder ----------------------
	//
	// Frame pairing: `begin_frame()` -> ... -> `submit_and_present()`.
	// Acquire the swapchain target via `acquire_swapchain_texture()` exactly
	// once per frame (the returned pointer can be referenced multiple times).
	//
	// On SDL3 GPU these map to: AcquireGPUCommandBuffer / AcquireGPUSwapchainTexture
	// / SubmitGPUCommandBuffer + Present. On OpenGL: begin/acquire are bookkeeping
	// only and submit_and_present wraps SDL_GL_SwapWindow.
	//
	// Render/compute pass split is enforced inside this frame window
	// (see set_render_pass / begin_compute_pass). Init-time GPU work that
	// predates the main loop is tolerated by the OpenGL backend; SDL3 backend
	// will require wrapping such work in begin/submit pairs explicitly.
	virtual void begin_frame() = 0;
	virtual IGraphicsTexture* acquire_swapchain_texture() = 0;
	virtual void submit_and_present() = 0;

	// ---- Pipeline state ---------------------------------------------------
	// Apply the pipeline state struct. Backend diffs against cached state and
	// emits only the deltas. Bind state cleared by reset_state_cache().
	virtual void set_pipeline(const RenderPipelineState& state) = 0;

	// Depth-write toggle. Still exposed as an immediate setter because
	// clear_framebuffer needs to ensure the depth-mask is enabled around
	// glClear independent of the cached pipeline state.
	virtual void set_depth_write_enabled(bool enabled) = 0;

	// Returns the currently-bound shader (or nullptr). Used by uniform-setter
	// call sites that want to write into the active program without restating
	// the pipeline. Identical pointer to the last set_pipeline.
	virtual IGraphicsShader* get_active_shader() = 0;

	// Force the cached pipeline state to dirty so the next set_pipeline rebinds
	// everything. Call after a pass that bypassed the cache (raw glUseProgram /
	// raw glBindVertexArray inside ImGui / compute paths) so the cache doesn't
	// elide a needed bind on the next draw.
	virtual void reset_state_cache() = 0;

	// Viewport rect (pixels, origin bottom-left). Wraps glViewport.
	virtual void set_viewport(int x, int y, int w, int h) = 0;

	// Clear the currently-bound framebuffer's color and/or depth attachments.
	// Routes through set_depth_write_enabled(true) when clearing depth so the
	// cached depth-mask state stays in sync. Wraps glClear.
	virtual void clear_framebuffer(bool clear_depth, bool clear_color, float depth_value = 0.f) = 0;


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

	// Block until all submitted GPU work has completed. Debug/editor readback
	// only; do NOT call on hot paths. Wraps glFlush + glFinish.
	virtual void wait_for_gpu_idle() = 0;

	// Non-indexed draw. mode + count follow glDrawArrays semantics.
	virtual void draw_arrays(GraphicsPrimitiveType mode, int first, int count) = 0;

	// Bind a texture to a sampler/texture slot.
	virtual void bind_texture(int slot, IGraphicsTexture* tex) = 0;

	// Bind a UBO to a binding point.
	virtual void bind_uniform_buffer_base(int slot, IGraphicsBuffer* buf) = 0;

	// ---- Push constants (Phase 2 B1) --------------------------------------
	//
	// Per-draw / per-dispatch uniform uploads. Mirrors SDL3 GPU's
	// SDL_PushGPU{Vertex,Fragment,Compute}UniformData(cmdbuf, slot, data, size).
	// Block-shaped: each call replaces the contents of one (stage, slot) push
	// block. There is no name-based scalar setter on top — the GLSL declares a
	// matching `layout(std140, binding = N) uniform <Block> { ... }` and the
	// C++ side fills a POD struct mirroring the std140 layout.
	//
	// Slot count and size are bounded to the SDL3 GPU minimums: 4 slots per
	// stage, 128 bytes per slot. Exceeding either is an ASSERT.
	//
	// OpenGL backend mapping (one UBO binding per stage*slot, kept clear of
	// the 0..8 data-UBO range used by current_frame/cull/per-pass/shadow):
	//   binding = kGfxPushConstBindingBase + stage_index * kGfxMaxPushConstSlotsPerStage + slot
	//   stage_index: 0 = vertex, 1 = fragment, 2 = compute.
	// gfx_push_const_binding() below is the canonical mapping helper; shader
	// declarations should use the literal binding number (matching this formula)
	// for readability.
	static constexpr int kGfxMaxPushConstSlotsPerStage = 4;
	static constexpr int kGfxPushConstMaxBytes        = 128;
	static constexpr int kGfxPushConstBindingBase     = 12;

	virtual void push_vertex_constants  (int slot, const void* data, int size) = 0;
	virtual void push_fragment_constants(int slot, const void* data, int size) = 0;
	virtual void push_compute_constants (int slot, const void* data, int size) = 0;

	// ---- Phase 1.3a wrap surface (compute) --------------------------------

	// Begin a compute pass. Backend implicitly ends any open render or compute
	// pass first. Compute calls (`dispatch_compute`, `bind_image_for_compute`,
	// `memory_barrier`) are only legal between `begin_compute_pass()` and the
	// next `set_render_pass(...)` / next `begin_compute_pass()`.
	//
	// Why this exists: SDL3 GPU requires explicit `BeginGPUComputePass` /
	// `EndGPUComputePass` and forbids mixing draws with compute. Surfacing the
	// begin/end pair would noise every caller for zero portability gain, so the
	// boundary is implicit — `set_render_pass()` switches to render mode,
	// `begin_compute_pass()` switches to compute, backend emits the SDL3
	// begin/end transitions automatically. OpenGL backend is a pure state flip.
	virtual void begin_compute_pass() = 0;

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

	// Bind an SSBO to a binding slot.
	virtual void bind_storage_buffer_base(int slot, IGraphicsBuffer* buf) = 0;

	// Bind a sub-range of an SSBO to a binding slot. Wraps
	// glBindBufferRange(GL_SHADER_STORAGE_BUFFER, …).
	virtual void bind_storage_buffer_range(int slot, IGraphicsBuffer* buf,
										   int offset, int size) = 0;

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

	// Wraps glLineWidth. Used by debug meshbuilder line rendering. Kept as an
	// immediate setter (not on RenderPipelineState) because the wireframe pass
	// straddles many draws; SDL3 GPU silently caps to 1.
	virtual void set_line_width(float width) = 0;

	// ---- Phase 1.4b / 2c wrap surface (batched indirect draws) -------------
	//
	// Indirect-buffer binding folded onto the draw-call signatures in Phase 2c
	// (was bind_indirect_buffer / bind_parameter_buffer). OpenGL backend
	// caches the last-bound buffer per slot — back-to-back MDI calls against
	// the same indirect buffer emit zero rebinds. SDL3 GPU treats the buffer
	// pointer as a draw-call parameter directly.

	// glMultiDrawElementsIndirectCount: pulls `<= max_draw_count` indirect
	// draws from `indirect` starting at `indirect_byte_offset`, using the
	// count at `count_byte_offset` in `count`. `stride` is the byte stride
	// between commands (typically sizeof(DrawElementsIndirectCommand)).
	virtual void multi_draw_elements_indirect_count(GraphicsPrimitiveType mode,
													VertexInputIndexType index_type,
													IGraphicsBuffer* indirect,
													int indirect_byte_offset,
													IGraphicsBuffer* count,
													int count_byte_offset,
													int max_draw_count,
													int stride) = 0;

	// ---- Phase 1.8 wrap surface (window / swapchain / imgui) ---------------

	// Toggle swap-interval. true = vsync on (interval 1), false = off (0).
	// Wraps SDL_GL_SetSwapInterval.
	virtual void set_vsync(bool enable) = 0;

	// ImGui platform/renderer lifecycle. The backend owns the platform
	// (SDL2) + renderer (OpenGL3) backends; callers do not include the
	// imgui_impl_* headers. `imgui_render_draw_data` binds the default
	// framebuffer internally before issuing the draw.
	virtual void imgui_init() = 0;
	virtual void imgui_shutdown() = 0;
	virtual void imgui_new_frame() = 0;
	virtual void imgui_render_draw_data() = 0;
	virtual bool imgui_process_event(const union SDL_Event* event) = 0;

	// ---- Shader factory ---------------------------------------------------

	// Compile + link a GPU program. Path arguments are passed through to the
	// shader source loader (resolved relative to the shader root). Defines is
	// a comma-separated list of #define names. vert+frag, vert+frag+geo, and
	// single-file (non-tess) variants transparently consult the program-binary
	// cache; compute and single-file-tess always recompile.
	//
	// On compile/link failure: returns nullptr (caller is expected to mark
	// the program as failed and continue running with a fallback). On success:
	// returns a heap-allocated IGraphicsShader owning the underlying program.
	// Caller releases via IGraphicsShader::release().
	virtual IGraphicsShader* create_shader_vert_frag(const std::string& vert_path,
													 const std::string& frag_path,
													 const std::string& defines = {}) = 0;
	virtual IGraphicsShader* create_shader_vert_frag_geo(const std::string& vert_path,
														  const std::string& frag_path,
														  const std::string& geo_path,
														  const std::string& defines = {}) = 0;
	virtual IGraphicsShader* create_shader_compute(const std::string& compute_path,
													const std::string& defines = {}) = 0;
	virtual IGraphicsShader* create_shader_single_file(const std::string& shared_path,
														const std::string& defines = {}) = 0;
	virtual IGraphicsShader* create_shader_single_file_tess(const std::string& shared_path,
															 const std::string& defines = {}) = 0;

	// Polygon rasterization fill mode (Fill = solid, Line = wireframe).
	// Wraps glPolygonMode. Kept immediate (not on RenderPipelineState) because
	// the wireframe debug pass straddles many draws; SDL3 GPU has no equivalent
	// and the wireframe pass will be re-expressed when that backend lands.
	virtual void set_polygon_fill_mode(GraphicsFillMode mode) = 0;

	// Copy a (mip, layer) sub-image between two textures (same format, same
	// dimensions). Wraps glCopyImageSubData; SDL3 backend uses
	// SDL_BlitGPUTexture or copy pass in compute. Both layers are absolute
	// (face index for cubemap, slice for array, 0 for plain 2D).
	virtual void copy_texture(IGraphicsTexture* src, int src_mip, int src_layer,
							  IGraphicsTexture* dst, int dst_mip, int dst_layer,
							  int w, int h) = 0;

	// ---- Phase 1.4c wrap surface (orchestration) ---------------------------

	// glMultiDrawElementsIndirect against a GPU buffer at `byte_offset`. If
	// `indirect` is nullptr, the call falls back to client-side MDI reading
	// from `client_ptr` (legacy path used by Render_Lists when
	// use_client_buffer_mdi is set). draw_count = number of commands; stride
	// = byte stride between commands.
	virtual void multi_draw_elements_indirect(GraphicsPrimitiveType mode,
											  VertexInputIndexType index_type,
											  IGraphicsBuffer* indirect,
											  int byte_offset,
											  int draw_count,
											  int stride,
											  const void* client_ptr = nullptr) = 0;

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

};

struct SDL_Window;

// Global accessor for the active graphics device.
//
// OpenGL backend init order (engine startup):
//   1. gfx_opengl_pre_window_setup()  — sets GL context attribs (major/minor,
//      depth bits, double-buffer). MUST be called before SDL_CreateWindow.
//   2. SDL_CreateWindow(..., SDL_WINDOW_OPENGL | ...)
//   3. gfx_init_opengl(window)        — creates SDL_GL context, loads glad,
//      constructs the device. The backend takes ownership of the GL context
//      for the rest of the process lifetime.
// Tear down with gfx_shutdown() (destroys the device and the GL context).
//
// gfx() asserts initialization.
IGraphicsDevice& gfx();
bool gfx_is_initialized();
void gfx_opengl_pre_window_setup();
void gfx_init_opengl(SDL_Window* window);
void gfx_shutdown();

// OpenGL-specific init helpers. Renderer calls these once at startup. SDL3
// backend will expose parallel `gfx_sdl3gpu_*` variants; the renderer picks
// the right pair based on the active backend.
void gfx_opengl_dump_capabilities();
void gfx_opengl_enable_debug_output();
#endif