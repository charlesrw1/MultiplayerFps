#if 1
// OpenGLDeviceImpl — framebuffer management, render-pass setup, blit, and
// resource factory. Concrete impl types live in OpenGlTextureImpl.cpp and
// OpenGlBufferImpl.cpp; accessed here only through the IGraphics* interfaces
// and factory functions declared in OpenGlDeviceLocal.h.
#include "OpenGlDeviceLocal.h"

// ---------------------------------------------------------------------------
// Global state (definitions)
// ---------------------------------------------------------------------------
int total_gfx_mem_usage = 0;

IGraphicsDevice* IGraphicsDevice::inst = nullptr;
extern ConfigVar log_shader_compiles;

OpenglDataStatic opengl_stats;

void dump_render_memory_usage() {
	ASSERT(IGraphicsDevice::inst != nullptr);
	opengl_stats.dump_to_disk("r_mem_usage.csv");
}

// ---------------------------------------------------------------------------
// OpenGLDeviceImpl
// ---------------------------------------------------------------------------
ConfigVar use_multiple_fbos("use_multiple_fbos", "1", CVAR_BOOL, "");

// Sentinel value returned by get_swapchain_texture(). It has id == 0 and no
// GL resources. Used only as a pointer identity for comparison in render-pass
// and blit code.
static IGraphicsTexture* g_swapchain_sentinel = nullptr;

class OpenGLDeviceImpl : public IGraphicsDevice
{
public:
	fbohandle shared_framebuffer   = 0;
	fbohandle shared_framebuffer_2 = 0;

	OpenGLDeviceImpl(ThingerBobber* f) : thinger_bobber(f) {
		ASSERT(f != nullptr);
		glGenFramebuffers(1, &shared_framebuffer);
		glGenFramebuffers(1, &shared_framebuffer_2);
		g_swapchain_sentinel = opengl_make_swapchain_sentinel();
	}

	~OpenGLDeviceImpl() override {
		// Sentinel was allocated with new in OpenGlTextureImpl.cpp; release it.
		if (g_swapchain_sentinel) {
			g_swapchain_sentinel->release();
			g_swapchain_sentinel = nullptr;
		}
	}

	GraphicsDeviceType get_device_type() override { return GraphicsDeviceType::OpenGl; }

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

		int min_width  = 100'000;
		int min_height = 100'000;

		if (!state.depth_info && state.color_infos.size() == 1 &&
			state.color_infos[0].texture == g_swapchain_sentinel) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		} else {
			set_framebuffer_with_info(state, shared_framebuffer, min_width, min_height);
		}

		glViewport(0, 0, min_width, min_height);
		if (state.wants_color_clear || state.wants_depth_clear) {
			glClearDepth(state.clear_depth_val);
			if (state.use_gray_clear)
				glClearColor(0.1, 0.1, 0.1, 1);
			else
				glClearColor(0, 0.5, 0, 1);
			GLbitfield mask{};
			if (state.wants_depth_clear) mask |= GL_DEPTH_BUFFER_BIT;
			if (state.wants_color_clear)  mask |= GL_COLOR_BUFFER_BIT;
			thinger_bobber->set_depth_write_enabled(true);
			glClear(mask);
		}
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

	IGraphicsTexture* get_swapchain_texture() override {
		ASSERT(g_swapchain_sentinel != nullptr);
		return g_swapchain_sentinel;
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

private:
	ThingerBobber* thinger_bobber = nullptr;
	opt<RenderPassState> cur_pass;
};

IGraphicsDevice* IGraphicsDevice::create_opengl_device(ThingerBobber* f) {
	ASSERT(f != nullptr);
	return new OpenGLDeviceImpl(f);
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
