#if 1
// OpenGLDeviceImpl — framebuffer management, render-pass setup, blit, and
// resource factory. Concrete impl types live in OpenGlTextureImpl.cpp and
// OpenGlBufferImpl.cpp; accessed here only through the IGraphics* interfaces
// and factory functions declared in OpenGlDeviceLocal.h.
#include "OpenGlDeviceLocal.h"
#include "DrawLocal.h"
#include <SDL2/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "../External/glad/glad.h"
// ---------------------------------------------------------------------------
// Global state (definitions)
// ---------------------------------------------------------------------------
int total_gfx_mem_usage = 0;

static IGraphicsDevice* g_gfx_instance = nullptr;

IGraphicsDevice& gfx() {
	ASSERT(g_gfx_instance != nullptr);
	return *g_gfx_instance;
}
bool gfx_is_initialized() { return g_gfx_instance != nullptr; }

extern ConfigVar log_shader_compiles;

OpenglDataStatic opengl_stats;

void dump_render_memory_usage() {
	ASSERT(g_gfx_instance != nullptr);
	opengl_stats.dump_to_disk("r_mem_usage.csv");
}

// Forward declare so the class def below can reference and the buffer-impl TU
// can dispatch into it via opengl_backend_buffer_released below.
class OpenGLDeviceImpl;
static void opengl_clear_cached_buffer_binds(IGraphicsBuffer* buf);

void opengl_backend_buffer_released(IGraphicsBuffer* buf) {
	if (g_gfx_instance) opengl_clear_cached_buffer_binds(buf);
}

// ---------------------------------------------------------------------------
// Capability dump + debug-output enable (called once at engine init)
// ---------------------------------------------------------------------------
void gfx_opengl_dump_capabilities() {
	bool supports_compression = false;
	bool supports_sparse_tex = false;
	bool supports_filter_minmax = false;
	bool supports_atomic64 = false;
	bool supports_int64 = false;

	int num_extensions = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
	for (int i = 0; i < num_extensions; i++) {
		const char* ext = (char*)glGetStringi(GL_EXTENSIONS, i);
		if (strcmp(ext, "GL_ARB_sparse_texture") == 0)
			supports_sparse_tex = true;
		else if (strcmp(ext, "GL_EXT_texture_compression_s3tc") == 0)
			supports_compression = true;
		else if (strcmp(ext, "GL_ARB_texture_filter_minmax") == 0)
			supports_filter_minmax = true;
		else if (strcmp(ext, "GL_NV_shader_atomic_int64") == 0)
			supports_atomic64 = true;
		else if (strcmp(ext, "GL_ARB_gpu_shader_int64") == 0)
			supports_int64 = true;
	}

	sys_print(Debug, "###########################\n");
	sys_print(Debug, "#### Extension support ####\n");
	sys_print(Debug, "###########################\n");
	sys_print(Debug, "-GL_ARB_sparse_texture: %s\n", supports_sparse_tex ? "yes" : "no");
	sys_print(Debug, "-GL_ARB_texture_filter_minmax: %s\n", supports_filter_minmax ? "yes" : "no");
	sys_print(Debug, "-GL_EXT_texture_compression_s3tc: %s\n", supports_compression ? "yes" : "no");
	sys_print(Debug, "-GL_NV_shader_atomic_int64: %s\n", supports_atomic64 ? "yes" : "no");
	sys_print(Debug, "-GL_ARB_gpu_shader_int64: %s\n", supports_int64 ? "yes" : "no");

	if (!supports_compression)
		Fatalf("Opengl driver needs GL_EXT_texture_compression_s3tc\n");

	GLint binary_formats = 0;
	glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &binary_formats);
	if (binary_formats == 0)
		Fatalf("Opengl driver must support program binary. (GL_NUM_PROGRAM_BINARY_FORMATS>0)\n");

	sys_print(Debug, "############################\n");
	sys_print(Debug, "#### GL Hardware Values ####\n");
	sys_print(Debug, "############################\n");
	int max_v = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_v);
	sys_print(Debug, "-GL_MAX_UNIFORM_BUFFER_BINDINGS: %d\n", max_v);
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_v);
	sys_print(Debug, "-GL_MAX_TEXTURE_IMAGE_UNITS: %d\n", max_v);
	sys_print(Debug, "-GL_NUM_PROGRAM_BINARY_FORMATS: %d\n", binary_formats);
	glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &max_v);
	sys_print(Debug, "-GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS: %d\n", max_v);
	glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &max_v);
	sys_print(Debug, "-GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS: %d\n", max_v);
	glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &max_v);
	sys_print(Debug, "-GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS: %d\n", max_v);
	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_v);
	sys_print(Debug, "-GL_MAX_ARRAY_TEXTURE_LAYERS: %d\n", max_v);
	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_v);
	sys_print(Debug, "-GL_MAX_COLOR_ATTACHMENTS: %d\n", max_v);
	sys_print(Debug, "\n");
}

static void GLAPIENTRY opengl_debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
													 GLsizei /*length*/, GLchar const* message, void const* /*user_param*/) {
	auto const src_str = [source]() {
		switch (source) {
		case GL_DEBUG_SOURCE_API: return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "WINDOW SYSTEM";
		case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER COMPILER";
		case GL_DEBUG_SOURCE_THIRD_PARTY: return "THIRD PARTY";
		case GL_DEBUG_SOURCE_APPLICATION: return "APPLICATION";
		case GL_DEBUG_SOURCE_OTHER: return "OTHER";
		}
		return "";
	}();
	auto const type_str = [type]() {
		switch (type) {
		case GL_DEBUG_TYPE_ERROR: return "ERROR";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "UNDEFINED_BEHAVIOR";
		case GL_DEBUG_TYPE_PORTABILITY: return "PORTABILITY";
		case GL_DEBUG_TYPE_PERFORMANCE: return "PERFORMANCE";
		case GL_DEBUG_TYPE_MARKER: return "MARKER";
		case GL_DEBUG_TYPE_OTHER: return "OTHER";
		}
		return "";
	}();
	auto const severity_str = [severity]() {
		switch (severity) {
		case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
		case GL_DEBUG_SEVERITY_LOW: return "LOW";
		case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
		case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
		}
		return "";
	}();
	sys_print(Error, "%s, %s, %s, %d: %s\n", src_str, type_str, severity_str, id, message);
}

void gfx_opengl_enable_debug_output() {
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(opengl_debug_message_callback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
}

// ---------------------------------------------------------------------------
// OpenGLSamplerImpl
// ---------------------------------------------------------------------------
static GLenum sampler_filter_to_gl(GraphicsSamplerFilter f) {
	switch (f) {
	case GraphicsSamplerFilter::Nearest:             return GL_NEAREST;
	case GraphicsSamplerFilter::Linear:              return GL_LINEAR;
	case GraphicsSamplerFilter::LinearMipmapNearest: return GL_LINEAR_MIPMAP_NEAREST;
	case GraphicsSamplerFilter::LinearMipmapLinear:  return GL_LINEAR_MIPMAP_LINEAR;
	}
	ASSERT(0 && "sampler_filter_to_gl: unknown filter");
	return GL_LINEAR;
}

class OpenGLSamplerImpl : public IGraphicsSampler
{
public:
	uint32_t id = 0;

	OpenGLSamplerImpl(const CreateSamplerArgs& args) {
		glGenSamplers(1, &id);
		glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER, sampler_filter_to_gl(args.min_filter));
		glSamplerParameteri(id, GL_TEXTURE_MAG_FILTER, sampler_filter_to_gl(args.mag_filter));
		const GLenum wrap = (args.wrap == GraphicsSamplerWrap::ClampToEdge) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
		glSamplerParameteri(id, GL_TEXTURE_WRAP_S, wrap);
		glSamplerParameteri(id, GL_TEXTURE_WRAP_T, wrap);
		glSamplerParameteri(id, GL_TEXTURE_WRAP_R, wrap);
		if (args.reduction == GraphicsSamplerReduction::Min)
			glSamplerParameteri(id, GL_TEXTURE_REDUCTION_MODE_ARB, GL_MIN);
		else if (args.reduction == GraphicsSamplerReduction::Max)
			glSamplerParameteri(id, GL_TEXTURE_REDUCTION_MODE_ARB, GL_MAX);
	}
	~OpenGLSamplerImpl() override { glDeleteSamplers(1, &id); }

	void release() override { delete this; }
	uint32_t get_internal_handle() override { return id; }
};

// ---------------------------------------------------------------------------
// OpenGLDeviceImpl
// ---------------------------------------------------------------------------
ConfigVar use_multiple_fbos("use_multiple_fbos", "1", CVAR_BOOL, "");

// Sentinel value returned by acquire_swapchain_texture(). It has id == 0 and
// no GL resources. Used only as a pointer identity for comparison in
// render-pass and blit code.
static IGraphicsTexture* g_swapchain_sentinel = nullptr;

class OpenGLDeviceImpl : public IGraphicsDevice
{
public:
	// Tracks the implicit pass mode (see IGraphicsDevice::begin_compute_pass).
	// OpenGL doesn't care; the state machine exists to catch missing
	// begin_compute_pass() / set_render_pass() at OpenGL build time so the
	// SDL3 backend (which DOES care) doesn't trip on it later.
	enum class PassMode { None, Render, Compute };
	PassMode current_pass = PassMode::None;

	// Phase 2d frame lifecycle. `in_frame` toggles on begin_frame /
	// submit_and_present so the encoder boundary is observable. OpenGL is
	// lenient — pass ops outside a frame are tolerated (init-time BRDF
	// integration runs before the main loop) — but submit_and_present asserts
	// the pairing so an unbalanced loop is caught.
	bool in_frame = false;

	fbohandle shared_framebuffer   = 0;
	fbohandle shared_framebuffer_2 = 0;

	// ---- GL state cache (folded in from OpenglRenderDevice in Phase 2c) ----
	static const int MAX_SAMPLER_BINDINGS = 32;
	IGraphicsShader* active_program = nullptr;
	texhandle textures_bound[MAX_SAMPLER_BINDINGS]{};
	BlendState blending = BlendState::OPAQUE;
	bool show_backface = false;
	bool depth_test_enabled = true;
	bool depth_write_enabled = true;
	bool depth_less_than_enabled = true;
	bool cullfrontface = false;
	vertexarrayhandle current_vao = 0;

	// Push-constant UBOs — lazy-allocated per (stage, slot). One 128-byte UBO
	// each, orphaned with glInvalidateBufferData on every push to avoid sync.
	// Stage indices: 0 = vertex, 1 = fragment, 2 = compute. Bound at fixed
	// UBO bindings (see kGfxPushConstBindingBase in IGraphicsDevice.h).
	GLuint push_const_ubos[3][IGraphicsDevice::kGfxMaxPushConstSlotsPerStage]{};

	// Cached Phase 2c pipeline fields.
	bool poly_offset_enabled_cached = false;
	float poly_offset_factor_cached = 0.f;
	float poly_offset_units_cached = 0.f;
	RenderPipelineState::ColorWriteMask color_write_masks_cached[RenderPipelineState::MAX_COLOR_ATTACHMENTS]{};

	enum cache_bit
	{
		PROGRAM_BIT,
		BLENDING_BIT,
		BACKFACE_BIT,
		CULLFRONTFACE_BIT,
		DEPTHTEST_BIT,
		DEPTHWRITE_BIT,
		DEPTHLESS_THAN_BIT,
		VAO_BIT,
		POLY_OFFSET_BIT,
		COLOR_MASK0_BIT,
		// COLOR_MASK0..MAX_COLOR_ATTACHMENTS-1 reserved
		TEXTURE0_BIT = COLOR_MASK0_BIT + RenderPipelineState::MAX_COLOR_ATTACHMENTS,
	};
	uint64_t invalid_bits = UINT64_MAX;
	bool is_bit_invalid(uint32_t bit) { return invalid_bits & (1ull << bit); }
	void set_bit_valid(uint32_t bit) { invalid_bits &= ~(1ull << bit); }
	void set_bit_invalid(uint32_t bit) { invalid_bits |= (1ull << bit); }
	void invalidate_all() { invalid_bits = UINT64_MAX; }

	OpenGLDeviceImpl() {
		glGenFramebuffers(1, &shared_framebuffer);
		glGenFramebuffers(1, &shared_framebuffer_2);
		g_swapchain_sentinel = opengl_make_swapchain_sentinel();

		// Default GL state (was Renderer::InitGlState in DrawLocal_Device.cpp).
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glClearColor(0.5f, 0.3f, 0.2f, 1.f);
		glDepthFunc(GL_LEQUAL);
		// Reverse-Z setup: output [0,1] (not [-1,1]); clear depth to 0.
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
		glClearDepth(0.0);
	}

	~OpenGLDeviceImpl() override {
		// Sentinel was allocated with new in OpenGlTextureImpl.cpp; release it.
		if (g_swapchain_sentinel) {
			g_swapchain_sentinel->release();
			g_swapchain_sentinel = nullptr;
		}
		for (auto& stage : push_const_ubos) {
			for (GLuint& ubo : stage) {
				if (ubo) glDeleteBuffers(1, &ubo);
				ubo = 0;
			}
		}
	}

	GraphicsDeviceType get_device_type() override { return GraphicsDeviceType::OpenGl; }

	// ---- State-cache setters (formerly OpenglRenderDevice methods) ---------

	void bind_texture_unit_internal(int slot, uint32_t id) {
		ASSERT(slot >= 0 && slot < MAX_SAMPLER_BINDINGS);
		bool invalid = is_bit_invalid(TEXTURE0_BIT + slot);
		if (invalid || textures_bound[slot] != id) {
			set_bit_valid(TEXTURE0_BIT + slot);
			glBindTextureUnit(slot, id);
			textures_bound[slot] = id;
			draw.stats.texture_binds++;
		}
	}

	void set_vao_internal(vertexarrayhandle vao) {
		bool invalid = is_bit_invalid(VAO_BIT);
		if (invalid || vao != current_vao) {
			set_bit_valid(VAO_BIT);
			current_vao = vao;
			glBindVertexArray(vao);
			draw.stats.vertex_array_changes++;
		}
	}

	void set_blend_state_internal(BlendState blend) {
		bool invalid = is_bit_invalid(BLENDING_BIT);
		if (invalid || blend != blending) {
			if (blend == BlendState::OPAQUE)
				glDisable(GL_BLEND);
			else if (blend == BlendState::ADD) {
				if (invalid || blending == BlendState::OPAQUE) glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
			} else if (blend == BlendState::BLEND) {
				if (invalid || blending == BlendState::OPAQUE) glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			} else if (blend == BlendState::MULT) {
				if (invalid || blending == BlendState::OPAQUE) glEnable(GL_BLEND);
				glBlendFunc(GL_DST_COLOR, GL_ZERO);
			} else if (blend == BlendState::SCREEN) {
				if (invalid || blending == BlendState::OPAQUE) glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
			} else if (blend == BlendState::PREMULT_BLEND) {
				if (invalid || blending == BlendState::OPAQUE) glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}
			blending = blend;
			set_bit_valid(BLENDING_BIT);
			draw.stats.blend_changes++;
		}
	}

	void set_show_backfaces_internal(bool show_backfaces) {
		bool invalid = is_bit_invalid(BACKFACE_BIT);
		if (invalid || show_backfaces != this->show_backface) {
			if (show_backfaces) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
			set_bit_valid(BACKFACE_BIT);
			this->show_backface = show_backfaces;
		}
	}

	void set_depth_test_enabled_internal(bool enabled) {
		bool invalid = is_bit_invalid(DEPTHTEST_BIT);
		if (invalid || enabled != this->depth_test_enabled) {
			if (enabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
			set_bit_valid(DEPTHTEST_BIT);
			this->depth_test_enabled = enabled;
		}
	}

	void set_depth_write_enabled(bool enabled) override {
		bool invalid = is_bit_invalid(DEPTHWRITE_BIT);
		if (invalid || enabled != this->depth_write_enabled) {
			if (enabled) glDepthMask(GL_TRUE); else glDepthMask(GL_FALSE);
			set_bit_valid(DEPTHWRITE_BIT);
			this->depth_write_enabled = enabled;
		}
	}

	void set_cull_front_face_internal(bool enabled) {
		bool invalid = is_bit_invalid(CULLFRONTFACE_BIT);
		if (invalid || enabled != this->cullfrontface) {
			if (enabled) glCullFace(GL_FRONT); else glCullFace(GL_BACK);
			set_bit_valid(CULLFRONTFACE_BIT);
			this->cullfrontface = enabled;
		}
	}

	void set_depth_less_than_internal(bool less_than) {
		bool invalid = is_bit_invalid(DEPTHLESS_THAN_BIT);
		if (invalid || less_than != this->depth_less_than_enabled) {
			if (less_than) glDepthFunc(GL_LEQUAL); else glDepthFunc(GL_GEQUAL);
			set_bit_valid(DEPTHLESS_THAN_BIT);
			this->depth_less_than_enabled = less_than;
		}
	}

	void set_shader_internal(IGraphicsShader* shader) {
		if (shader == nullptr) {
			active_program = nullptr;
			glUseProgram(0);
			set_bit_valid(PROGRAM_BIT);
			return;
		}
		bool invalid = is_bit_invalid(PROGRAM_BIT);
		if (invalid || shader != active_program) {
			set_bit_valid(PROGRAM_BIT);
			active_program = shader;
			glUseProgram(shader->get_internal_handle());
			draw.stats.program_changes++;
		}
	}

	IGraphicsShader* get_active_shader() override { return active_program; }

	void set_polygon_offset_internal(bool enabled, float factor, float units) {
		bool invalid = is_bit_invalid(POLY_OFFSET_BIT);
		if (invalid || enabled != poly_offset_enabled_cached ||
			factor != poly_offset_factor_cached || units != poly_offset_units_cached) {
			if (enabled) {
				glEnable(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(factor, units);
			} else {
				glDisable(GL_POLYGON_OFFSET_FILL);
			}
			poly_offset_enabled_cached = enabled;
			poly_offset_factor_cached = factor;
			poly_offset_units_cached = units;
			set_bit_valid(POLY_OFFSET_BIT);
		}
	}

	void set_color_write_mask_internal(int slot, const RenderPipelineState::ColorWriteMask& m) {
		ASSERT(slot >= 0 && slot < RenderPipelineState::MAX_COLOR_ATTACHMENTS);
		bool invalid = is_bit_invalid(COLOR_MASK0_BIT + slot);
		auto& c = color_write_masks_cached[slot];
		if (invalid || m.r != c.r || m.g != c.g || m.b != c.b || m.a != c.a) {
			glColorMaski((GLuint)slot,
						 m.r ? GL_TRUE : GL_FALSE, m.g ? GL_TRUE : GL_FALSE,
						 m.b ? GL_TRUE : GL_FALSE, m.a ? GL_TRUE : GL_FALSE);
			c = m;
			set_bit_valid(COLOR_MASK0_BIT + slot);
		}
	}

	void set_pipeline(const RenderPipelineState& s) override {
		set_shader_internal(s.program);
		set_blend_state_internal(s.blend);
		set_vao_internal(s.vao ? s.vao->get_internal_handle() : 0);
		set_cull_front_face_internal(s.cull_front_face);
		set_depth_test_enabled_internal(s.depth_testing);
		set_show_backfaces_internal(!s.backface_culling);
		set_depth_write_enabled(s.depth_writes);
		set_depth_less_than_internal(s.depth_less_than);
		set_polygon_offset_internal(s.polygon_offset_enabled, s.polygon_offset_factor, s.polygon_offset_units);
		for (int i = 0; i < RenderPipelineState::MAX_COLOR_ATTACHMENTS; i++)
			set_color_write_mask_internal(i, s.color_write_masks[i]);
	}

	void reset_state_cache() override {
		active_program = nullptr;
		invalidate_all();
	}

	void set_viewport(int x, int y, int w, int h) override {
		glViewport(x, y, w, h);
	}

	void clear_framebuffer(bool clear_depth, bool clear_color, float depth_value) override {
		if (!clear_depth && !clear_color) return;
		glClearDepth(depth_value);
		glClearColor(0, 0, 0, 1);
		GLbitfield mask = 0;
		if (clear_depth) mask |= GL_DEPTH_BUFFER_BIT;
		if (clear_color) mask |= GL_COLOR_BUFFER_BIT;
		// glDepthMask gates glClear too — sync the cached state.
		set_depth_write_enabled(true);
		glClear(mask);
		draw.stats.framebuffer_clears++;
	}

	// Returns the GL handle of tex, which must be an OpenGLTextureImpl.
	// We retrieve it via the public interface so this TU does not need the
	// concrete class definition.
	static texhandle tex_id(IGraphicsTexture* tex) {
		ASSERT(tex != nullptr);
		return tex->get_internal_handle();
	}

	// Returns the dimensions of tex via get_size().
	static glm::ivec2 tex_size(IGraphicsTexture* tex) {
		ASSERT(tex != nullptr);
		return tex->get_size();
	}

	void set_framebuffer_with_info(const RenderPassState& state, fbohandle fbo,
								   int& min_width, int& min_height) {
		ASSERT(fbo != 0 || state.color_infos.empty());
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		for (int i = 0; i < (int)state.color_infos.size(); i++) {
			const ColorTargetInfo& info = state.color_infos[i];
			ASSERT(info.texture != g_swapchain_sentinel);
			texhandle handle = tex_id(info.texture);
			if (info.layer == -1)
				glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + i, handle, info.mip);
			else
				glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0 + i, handle, info.mip, info.layer);
			glm::ivec2 sz = tex_size(info.texture);
			min_width  = std::min(sz.x, min_width);
			min_height = std::min(sz.y, min_height);
		}

		const int max_attachments = 32;
		std::array<unsigned int, max_attachments> gbuffer_attachments;
		for (int i = 0; i < (int)state.color_infos.size(); i++)
			gbuffer_attachments[i] = GL_COLOR_ATTACHMENT0 + i;
		glNamedFramebufferDrawBuffers(fbo, (int)state.color_infos.size(), gbuffer_attachments.data());

		if (state.depth_info) {
			ASSERT(state.depth_info != g_swapchain_sentinel);
			texhandle handle = tex_id(state.depth_info);
			if (state.depth_layer == -1)
				glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, handle, 0);
			else
				glNamedFramebufferTextureLayer(fbo, GL_DEPTH_ATTACHMENT, handle, 0, state.depth_layer);
			glm::ivec2 sz = tex_size(state.depth_info);
			min_width  = std::min(sz.x, min_width);
			min_height = std::min(sz.y, min_height);
		} else {
			glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, 0, 0);
		}
	}

	void set_render_pass(const RenderPassState& state) override {
		ASSERT(shared_framebuffer != 0);
		cur_pass = state;
		current_pass = PassMode::Render;

		int min_width  = 100'000;
		int min_height = 100'000;

		if (!state.depth_info && state.color_infos.size() == 1 &&
			state.color_infos[0].texture == g_swapchain_sentinel) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		} else {
			set_framebuffer_with_info(state, shared_framebuffer, min_width, min_height);
		}

		glViewport(0, 0, min_width, min_height);

		// Per-attachment color clears (load-op on each ColorTargetInfo).
		bool any_color_clear = false;
		for (int i = 0; i < (int)state.color_infos.size(); i++) {
			const ColorTargetInfo& info = state.color_infos[i];
			if (!info.wants_clear) continue;
			any_color_clear = true;
			glClearBufferfv(GL_COLOR, i, &info.clear_color.x);
		}

		if (state.wants_depth_clear) {
			// glClearBuffer obeys glDepthMask; force depth-write on through the
			// cached setter so the next draw doesn't think it's still disabled.
			set_depth_write_enabled(true);
			glClearBufferfv(GL_DEPTH, 0, &state.clear_depth_val);
		}
		draw.stats.framebuffer_changes++;
		(void)any_color_clear;
	}

	void blit_textures(const GraphicsBlitInfo& info) override {
		ASSERT(info.src.texture && info.dest.texture);
		fbohandle src_fbo  = shared_framebuffer;
		fbohandle dest_fbo = shared_framebuffer_2;
		{
			int dummyx = 0, dummyy = 0;
			RenderPassState fake1;
			auto colors1 = {ColorTargetInfo(info.src.texture, info.src.layer, info.src.mip)};
			fake1.color_infos = colors1;
			set_framebuffer_with_info(fake1, src_fbo, dummyx, dummyy);

			if (info.dest.texture == g_swapchain_sentinel) {
				dest_fbo = 0; // backbuffer
			} else {
				RenderPassState fake2;
				auto colors2 = {ColorTargetInfo(info.dest.texture, info.dest.layer, info.dest.mip)};
				fake2.color_infos = colors2;
				set_framebuffer_with_info(fake2, dest_fbo, dummyx, dummyy);
			}
		}
		glBlitNamedFramebuffer(src_fbo, dest_fbo,
							   info.src.x,  info.src.y,  info.src.w,  info.src.h,
							   info.dest.x, info.dest.y, info.dest.w, info.dest.h,
							   GL_COLOR_BUFFER_BIT,
							   opengl_filter_to_gl(info.filter));
	}

	IGraphicsTexture* acquire_swapchain_texture() override {
		ASSERT(g_swapchain_sentinel != nullptr);
		return g_swapchain_sentinel;
	}

	void begin_frame() override {
		in_frame = true;
		current_pass = PassMode::None;
	}

	void submit_and_present() override {
		ASSERT(in_frame && "submit_and_present without begin_frame");
		ASSERT(window != nullptr);
		SDL_GL_SwapWindow(window);
		current_pass = PassMode::None;
		in_frame = false;
	}

	IGraphicsTexture* create_texture(const CreateTextureArgs& args) override {
		ASSERT(args.width > 0 && args.height > 0);
		return opengl_create_texture(args);
	}
	IGraphicsBuffer* create_buffer(const CreateBufferArgs& args) override {
		ASSERT(args.size >= 0);
		return opengl_create_buffer(args);
	}
	IGraphicsVertexInput* create_vertex_input(const CreateVertexInputArgs& args) override {
		ASSERT(args.vertex != nullptr);
		return opengl_create_vertex_input(args);
	}

	void copy_texture(IGraphicsTexture* src, int src_mip, int src_layer,
					  IGraphicsTexture* dst, int dst_mip, int dst_layer,
					  int w, int h) override {
		ASSERT(src && dst);
		auto to_gl_target = [](GraphicsTextureType t) -> GLenum {
			switch (t) {
			case GraphicsTextureType::t2D:           return GL_TEXTURE_2D;
			case GraphicsTextureType::t2DArray:      return GL_TEXTURE_2D_ARRAY;
			case GraphicsTextureType::t3D:           return GL_TEXTURE_3D;
			case GraphicsTextureType::tCubemap:      return GL_TEXTURE_CUBE_MAP;
			case GraphicsTextureType::tCubemapArray: return GL_TEXTURE_CUBE_MAP_ARRAY;
			}
			ASSERT(0); return GL_TEXTURE_2D;
		};
		glCopyImageSubData(src->get_internal_handle(), to_gl_target(src->get_texture_type()),
						   src_mip, 0, 0, src_layer,
						   dst->get_internal_handle(), to_gl_target(dst->get_texture_type()),
						   dst_mip, 0, 0, dst_layer,
						   w, h, 1);
	}

	IGraphicsShader* create_shader_vert_frag(const std::string& vert_path,
											 const std::string& frag_path,
											 const std::string& defines) override {
		return opengl_create_shader_vert_frag(vert_path, frag_path, defines);
	}
	IGraphicsShader* create_shader_vert_frag_geo(const std::string& vert_path,
												  const std::string& frag_path,
												  const std::string& geo_path,
												  const std::string& defines) override {
		return opengl_create_shader_vert_frag_geo(vert_path, frag_path, geo_path, defines);
	}
	IGraphicsShader* create_shader_compute(const std::string& compute_path,
											const std::string& defines) override {
		return opengl_create_shader_compute(compute_path, defines);
	}
	IGraphicsShader* create_shader_single_file(const std::string& shared_path,
												const std::string& defines) override {
		return opengl_create_shader_single_file(shared_path, defines);
	}
	IGraphicsShader* create_shader_single_file_tess(const std::string& shared_path,
													 const std::string& defines) override {
		return opengl_create_shader_single_file_tess(shared_path, defines);
	}

	// ---- Phase 1 wrap impls -----------------------------------------------

	void set_scissor(int x, int y, int w, int h) override {
		ASSERT(w >= 0 && h >= 0);
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, w, h);
	}
	void disable_scissor() override { glDisable(GL_SCISSOR_TEST); }

	void draw_elements_base_vertex(GraphicsPrimitiveType mode, int count,
								   VertexInputIndexType index_type,
								   int byte_offset, int base_vertex) override {
		ASSERT(count >= 0 && byte_offset >= 0);
		const GLenum gl_mode =
			(mode == GraphicsPrimitiveType::Triangles)     ? GL_TRIANGLES :
			(mode == GraphicsPrimitiveType::TriangleStrip) ? GL_TRIANGLE_STRIP :
															 GL_LINES;
		const GLenum gl_type = (index_type == VertexInputIndexType::uint16)
								   ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElementsBaseVertex(gl_mode, count, gl_type,
								 (const void*)(intptr_t)byte_offset, base_vertex);
		draw.stats.total_draw_calls++;
	}

	void wait_for_gpu_idle() override {
		glFlush();
		glFinish();
	}

	void draw_arrays(GraphicsPrimitiveType mode, int first, int count) override {
		ASSERT(first >= 0 && count >= 0);
		const GLenum gl_mode =
			(mode == GraphicsPrimitiveType::Triangles)     ? GL_TRIANGLES :
			(mode == GraphicsPrimitiveType::TriangleStrip) ? GL_TRIANGLE_STRIP :
															 GL_LINES;
		glDrawArrays(gl_mode, first, count);
		draw.stats.total_draw_calls++;
	}

	void bind_texture(int slot, IGraphicsTexture* tex) override {
		ASSERT(slot >= 0);
		bind_texture_unit_internal(slot, tex ? tex->get_internal_handle() : 0);
	}

	void bind_uniform_buffer_base(int slot, IGraphicsBuffer* buf) override {
		ASSERT(slot >= 0 && buf != nullptr);
		glBindBufferBase(GL_UNIFORM_BUFFER, slot, buf->get_internal_handle());
	}

	// ---- Push constants (Phase 2 B1) --------------------------------------
	// Lazy-create a 128B UBO per (stage, slot), orphan via glInvalidateBufferData
	// to let the driver rename the storage, then sub-upload and re-bind at the
	// fixed binding point. Naive impl by design — option 2 of three considered
	// (see docs/rendering/gfx_abstraction.md B1 perf discussion). Ring buffer
	// (option 1) is the SDL3-backend path.
	void push_constants_internal(int stage_idx, int slot, const void* data, int size) {
		ASSERT(stage_idx >= 0 && stage_idx < 3);
		ASSERT(slot >= 0 && slot < IGraphicsDevice::kGfxMaxPushConstSlotsPerStage);
		ASSERT(data != nullptr);
		ASSERT(size > 0 && size <= IGraphicsDevice::kGfxPushConstMaxBytes);
		GLuint& ubo = push_const_ubos[stage_idx][slot];
		if (ubo == 0) {
			glCreateBuffers(1, &ubo);
			glNamedBufferData(ubo, IGraphicsDevice::kGfxPushConstMaxBytes,
							  nullptr, GL_STREAM_DRAW);
		}
		glInvalidateBufferData(ubo);
		glNamedBufferSubData(ubo, 0, size, data);
		const int binding = IGraphicsDevice::kGfxPushConstBindingBase
						  + stage_idx * IGraphicsDevice::kGfxMaxPushConstSlotsPerStage
						  + slot;
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, ubo);
	}
	void push_vertex_constants  (int slot, const void* data, int size) override { push_constants_internal(0, slot, data, size); }
	void push_fragment_constants(int slot, const void* data, int size) override { push_constants_internal(1, slot, data, size); }
	void push_compute_constants (int slot, const void* data, int size) override { push_constants_internal(2, slot, data, size); }

	void begin_compute_pass() override {
		// SDL3 GPU: ends any open render/compute pass; the actual SDL3
		// BeginGPUComputePass is deferred until the first dispatch_compute so
		// writable-resource bindings are known by then. OpenGL: pure state flip.
		current_pass = PassMode::Compute;
	}

	void dispatch_compute(int groups_x, int groups_y, int groups_z) override {
		ASSERT(groups_x >= 0 && groups_y >= 0 && groups_z >= 0);
		ASSERT(current_pass == PassMode::Compute &&
			   "dispatch_compute outside compute pass — call begin_compute_pass() first");
		glDispatchCompute(groups_x, groups_y, groups_z);
	}

	void memory_barrier(uint32_t bits) override {
		ASSERT(bits != 0);
		GLbitfield mask = 0;
		if (bits & BARRIER_SHADER_STORAGE)      mask |= GL_SHADER_STORAGE_BARRIER_BIT;
		if (bits & BARRIER_SHADER_IMAGE_ACCESS) mask |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
		if (bits & BARRIER_COMMAND)             mask |= GL_COMMAND_BARRIER_BIT;
		if (bits & BARRIER_TEXTURE_FETCH)       mask |= GL_TEXTURE_FETCH_BARRIER_BIT;
		glMemoryBarrier(mask);
	}

	static GLenum image_access_to_gl(GraphicsImageAccess access) {
		switch (access) {
		case GraphicsImageAccess::ReadOnly:  return GL_READ_ONLY;
		case GraphicsImageAccess::WriteOnly: return GL_WRITE_ONLY;
		case GraphicsImageAccess::ReadWrite: return GL_READ_WRITE;
		}
		ASSERT(0 && "image_access_to_gl: unknown access");
		return GL_READ_WRITE;
	}

	static GLenum image_format_to_gl(GraphicsTextureFormat fmt) {
		switch (fmt) {
		case GraphicsTextureFormat::r8:             return GL_R8;
		case GraphicsTextureFormat::rg8:            return GL_RG8;
		case GraphicsTextureFormat::rgba8:          return GL_RGBA8;
		case GraphicsTextureFormat::r16f:           return GL_R16F;
		case GraphicsTextureFormat::rg16f:          return GL_RG16F;
		case GraphicsTextureFormat::rgba16f:        return GL_RGBA16F;
		case GraphicsTextureFormat::r32f:           return GL_R32F;
		case GraphicsTextureFormat::rg32f:          return GL_RG32F;
		case GraphicsTextureFormat::r11f_g11f_b10f: return GL_R11F_G11F_B10F;
		case GraphicsTextureFormat::rgba16_snorm:   return GL_RGBA16_SNORM;
		default: break;
		}
		ASSERT(0 && "image_format_to_gl: format not supported as compute image");
		return GL_R32F;
	}

	void bind_image_for_compute(int slot, IGraphicsTexture* tex, int mip, int layer,
								GraphicsImageAccess access) override {
		ASSERT(slot >= 0 && tex != nullptr && mip >= 0);
		const GLboolean layered = (layer == -1) ? GL_TRUE : GL_FALSE;
		const GLint     layer_idx = (layer == -1) ? 0 : layer;
		glBindImageTexture(slot, tex->get_internal_handle(), mip, layered, layer_idx,
						   image_access_to_gl(access), image_format_to_gl(tex->get_texture_format()));
	}

	void bind_storage_buffer_base(int slot, IGraphicsBuffer* buf) override {
		ASSERT(slot >= 0 && buf != nullptr);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, buf->get_internal_handle());
	}
	void bind_storage_buffer_range(int slot, IGraphicsBuffer* buf,
								   int offset, int size) override {
		ASSERT(slot >= 0 && buf != nullptr && offset >= 0 && size > 0);
		glBindBufferRange(GL_SHADER_STORAGE_BUFFER, slot,
						  buf->get_internal_handle(), offset, size);
	}

	IGraphicsSampler* create_sampler(const CreateSamplerArgs& args) override {
		return new OpenGLSamplerImpl(args);
	}

	void bind_sampler(int slot, IGraphicsSampler* sampler) override {
		ASSERT(slot >= 0);
		glBindSampler(slot, sampler ? sampler->get_internal_handle() : 0);
	}

	void clear_buffer_uint32(IGraphicsBuffer* buf, uint32_t value) override {
		ASSERT(buf != nullptr);
		glClearNamedBufferData(buf->get_internal_handle(), GL_R32UI, GL_RED_INTEGER,
							   GL_UNSIGNED_INT, &value);
	}

	void download_buffer(IGraphicsBuffer* buf, int offset, int size, void* dest) override {
		ASSERT(buf != nullptr && dest != nullptr && offset >= 0 && size > 0);
		glGetNamedBufferSubData(buf->get_internal_handle(), offset, size, dest);
	}

	void set_line_width(float width) override {
		ASSERT(width > 0.0f);
		glLineWidth(width);
	}

	void set_polygon_fill_mode(GraphicsFillMode mode) override {
		glPolygonMode(GL_FRONT_AND_BACK,
					  mode == GraphicsFillMode::Line ? GL_LINE : GL_FILL);
	}

	void push_debug_group(const char* name) override {
		ASSERT(name);
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
	}

	void pop_debug_group() override {
		glPopDebugGroup();
	}

	IGraphicsTimerQuery* create_timer_query() override;

	// Phase 2c: cache last-bound indirect/parameter buffer so back-to-back
	// MDI calls against the same buffer emit zero rebinds.
	IGraphicsBuffer* current_indirect = nullptr;
	IGraphicsBuffer* current_parameter = nullptr;

	void bind_indirect_internal(IGraphicsBuffer* buf) {
		if (buf == current_indirect) return;
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buf ? buf->get_internal_handle() : 0);
		current_indirect = buf;
	}

	void bind_parameter_internal(IGraphicsBuffer* buf) {
		if (buf == current_parameter) return;
		glBindBuffer(GL_PARAMETER_BUFFER, buf ? buf->get_internal_handle() : 0);
		current_parameter = buf;
	}

	void multi_draw_elements_indirect(GraphicsPrimitiveType mode,
									  VertexInputIndexType index_type,
									  IGraphicsBuffer* indirect,
									  int byte_offset,
									  int draw_count,
									  int stride,
									  const void* client_ptr = nullptr) override {
		ASSERT(draw_count >= 0 && stride > 0 && byte_offset >= 0);
		const GLenum gl_mode =
			(mode == GraphicsPrimitiveType::Triangles)     ? GL_TRIANGLES :
			(mode == GraphicsPrimitiveType::TriangleStrip) ? GL_TRIANGLE_STRIP :
															 GL_LINES;
		const GLenum gl_type = (index_type == VertexInputIndexType::uint16)
								   ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		if (indirect) {
			bind_indirect_internal(indirect);
			glMultiDrawElementsIndirect(gl_mode, gl_type,
										(const void*)(intptr_t)byte_offset, draw_count, stride);
		} else {
			// Client-side MDI fallback (legacy path).
			bind_indirect_internal(nullptr);
			glMultiDrawElementsIndirect(gl_mode, gl_type, client_ptr, draw_count, stride);
		}
		draw.stats.total_draw_calls++;
	}

	void draw_elements_instanced_base_vertex_base_instance(
		GraphicsPrimitiveType mode, int count, VertexInputIndexType index_type,
		int byte_offset, int instance_count, int base_vertex,
		uint32_t base_instance) override {
		ASSERT(count >= 0 && byte_offset >= 0 && instance_count >= 0);
		const GLenum gl_mode =
			(mode == GraphicsPrimitiveType::Triangles)     ? GL_TRIANGLES :
			(mode == GraphicsPrimitiveType::TriangleStrip) ? GL_TRIANGLE_STRIP :
															 GL_LINES;
		const GLenum gl_type = (index_type == VertexInputIndexType::uint16)
								   ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElementsInstancedBaseVertexBaseInstance(
			gl_mode, count, gl_type, (const void*)(intptr_t)byte_offset,
			instance_count, base_vertex, base_instance);
	}

	void draw_elements(GraphicsPrimitiveType mode, int count,
					   VertexInputIndexType index_type,
					   int byte_offset) override {
		ASSERT(count >= 0 && byte_offset >= 0);
		const GLenum gl_mode =
			(mode == GraphicsPrimitiveType::Triangles)     ? GL_TRIANGLES :
			(mode == GraphicsPrimitiveType::TriangleStrip) ? GL_TRIANGLE_STRIP :
															 GL_LINES;
		const GLenum gl_type = (index_type == VertexInputIndexType::uint16)
								   ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		glDrawElements(gl_mode, count, gl_type, (const void*)(intptr_t)byte_offset);
	}

	// ---- Phase 1.8 wrap impls (window / swapchain / imgui) ---------------

	SDL_Window*   window     = nullptr;
	SDL_GLContext gl_context = nullptr;

	void set_vsync(bool enable) override {
		SDL_GL_SetSwapInterval(enable ? 1 : 0);
	}

	void imgui_init() override {
		ASSERT(window != nullptr && gl_context != nullptr);
		ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
		ImGui_ImplOpenGL3_Init();
	}

	void imgui_shutdown() override {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
	}

	void imgui_new_frame() override {
		ImGui_ImplSDL2_NewFrame();
		ImGui_ImplOpenGL3_NewFrame();
	}

	void imgui_render_draw_data() override {
		// Bind the default framebuffer (window backbuffer) so the imgui pass
		// lands on the screen rather than whichever offscreen target the last
		// scene pass left bound. Mirrors the SDL3 GPU model where the swapchain
		// texture is acquired and bound by the encoder before imgui draws.
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	bool imgui_process_event(const SDL_Event* event) override {
		ASSERT(event != nullptr);
		return ImGui_ImplSDL2_ProcessEvent(event);
	}

	void multi_draw_elements_indirect_count(GraphicsPrimitiveType mode,
											VertexInputIndexType index_type,
											IGraphicsBuffer* indirect,
											int indirect_byte_offset,
											IGraphicsBuffer* count,
											int count_byte_offset,
											int max_draw_count,
											int stride) override {
		ASSERT(indirect != nullptr && count != nullptr);
		ASSERT(indirect_byte_offset >= 0 && count_byte_offset >= 0 && max_draw_count >= 0 && stride > 0);
		const GLenum gl_mode =
			(mode == GraphicsPrimitiveType::Triangles)     ? GL_TRIANGLES :
			(mode == GraphicsPrimitiveType::TriangleStrip) ? GL_TRIANGLE_STRIP :
															 GL_LINES;
		const GLenum gl_type = (index_type == VertexInputIndexType::uint16)
								   ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
		bind_indirect_internal(indirect);
		bind_parameter_internal(count);
		glMultiDrawElementsIndirectCount(gl_mode, gl_type,
										 (const void*)(intptr_t)indirect_byte_offset,
										 (GLintptr)count_byte_offset,
										 max_draw_count, stride);
	}

private:
	opt<RenderPassState> cur_pass;
};

static void opengl_clear_cached_buffer_binds(IGraphicsBuffer* buf) {
	auto* impl = static_cast<OpenGLDeviceImpl*>(g_gfx_instance);
	if (impl->current_indirect == buf) impl->current_indirect = nullptr;
	if (impl->current_parameter == buf) impl->current_parameter = nullptr;
}

// GL_TIMESTAMP query wrapper. SDL3 backend will return a stub (no SDL3 GPU
// timestamp query API exists).
class OpenGLTimerQueryImpl : public IGraphicsTimerQuery
{
public:
	OpenGLTimerQueryImpl() { glGenQueries(1, &query_id); }
	~OpenGLTimerQueryImpl() override {
		if (query_id) glDeleteQueries(1, &query_id);
	}
	void release() override { delete this; }

	void record_timestamp() override {
		glQueryCounter(query_id, GL_TIMESTAMP);
		armed = true;
	}

	bool is_available() override {
		if (!armed) return false;
		GLint avail = 0;
		glGetQueryObjectiv(query_id, GL_QUERY_RESULT_AVAILABLE, &avail);
		return avail == GL_TRUE;
	}

	uint64_t read_timestamp_ns() override {
		ASSERT(armed);
		GLuint64 ns = 0;
		glGetQueryObjectui64v(query_id, GL_QUERY_RESULT, &ns);
		return ns;
	}

private:
	GLuint query_id = 0;
	bool armed = false;
};

IGraphicsTimerQuery* OpenGLDeviceImpl::create_timer_query() {
	return new OpenGLTimerQueryImpl();
}

// ---- Moved from DrawLocal_Debug.cpp so glGetError stays inside the backend.
bool CheckGlErrorInternal_(const char* file, int line) {
	GLenum error_code = glGetError();
	bool has_error = false;
	while (error_code != GL_NO_ERROR) {
		has_error = true;
		const char* error_name = "Unknown error";
		switch (error_code) {
		case GL_INVALID_ENUM:                  error_name = "GL_INVALID_ENUM"; break;
		case GL_INVALID_VALUE:                 error_name = "GL_INVALID_VALUE"; break;
		case GL_INVALID_OPERATION:             error_name = "GL_INVALID_OPERATION"; break;
		case GL_STACK_OVERFLOW:                error_name = "GL_STACK_OVERFLOW"; break;
		case GL_STACK_UNDERFLOW:               error_name = "GL_STACK_UNDERFLOW"; break;
		case GL_OUT_OF_MEMORY:                 error_name = "GL_OUT_OF_MEMORY"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION: error_name = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
		default: break;
		}
		sys_print(Error, "%s | %s (%d)\n", error_name, file, line);
		error_code = glGetError();
	}
	return has_error;
}

void gfx_opengl_pre_window_setup() {
	// SDL_GL attribute setup MUST run before SDL_CreateWindow so the window
	// gets created with a compatible GL pixel format. After this returns the
	// engine is responsible for SDL_CreateWindow(..., SDL_WINDOW_OPENGL | ...);
	// gfx_init_opengl(window) then takes over context creation + glad load.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
}

void gfx_init_opengl(SDL_Window* window) {
	ASSERT(g_gfx_instance == nullptr);
	ASSERT(window != nullptr);

	SDL_GLContext ctx = SDL_GL_CreateContext(window);
	if (!ctx)
		Fatalf("gfx_init_opengl: SDL_GL_CreateContext failed: %s\n", SDL_GetError());

	sys_print(Debug, "OpenGL loaded\n");
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	sys_print(Debug, "Vendor: %s\n",   glGetString(GL_VENDOR));
	sys_print(Debug, "Renderer: %s\n", glGetString(GL_RENDERER));
	sys_print(Debug, "Version: %s\n\n", glGetString(GL_VERSION));

	SDL_GL_SetSwapInterval(0);

	auto* impl = new OpenGLDeviceImpl();
	impl->window     = window;
	impl->gl_context = ctx;
	g_gfx_instance = impl;
}

void gfx_shutdown() {
	if (g_gfx_instance) {
		auto* impl = static_cast<OpenGLDeviceImpl*>(g_gfx_instance);
		SDL_GLContext ctx = impl->gl_context;
		delete impl;
		g_gfx_instance = nullptr;
		if (ctx)
			SDL_GL_DeleteContext(ctx);
	}
}

// ---------------------------------------------------------------------------
// OpenglDataStatic::dump_to_disk (out-of-line method)
// ---------------------------------------------------------------------------
void OpenglDataStatic::dump_to_disk(std::string str) {
	ASSERT(!str.empty());
	std::string output;
	for (auto t : all_textures)
		output += string_format("texture,%s,%d\n",
								opengl_texture_format_to_str(t->get_texture_format()),
								t->get_mem_usage());
	for (auto t : all_buffers)
		output += string_format("buffer,,%d\n", t->get_buf_size());
	auto fileptr = FileSys::open_write(str.c_str(), FileSys::ENGINE_DIR);
	fileptr->write(output.data(), output.size());
}
#endif
