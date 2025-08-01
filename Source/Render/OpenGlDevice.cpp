#if 1
#include "IGraphsDevice.h"
#include <unordered_set>
#include <optional>
#include "DrawLocal.h"
#include "Framework/StringUtils.h"
#include "Framework/BinaryReadWrite.h"
#include <array>
#include <vector>
#include <span>
using std::vector;
using std::array;
template<typename T>
using opt = std::optional<T>;
using std::string;

int total_gfx_mem_usage = 0;

IGraphicsDevice* IGraphicsDevice::inst = nullptr;
extern ConfigVar log_shader_compiles;

class OpenGLTextureImpl : public IGraphicsTexture {
public:
	static double get_bytes_per_pixel(GraphicsTextureFormat fmt) {
		switch (fmt)
		{
		case GraphicsTextureFormat::r8: return 1;
			break;
		case GraphicsTextureFormat::rg8: return 2;
			break;
		case GraphicsTextureFormat::rgb8:return 4;
			break;
		case GraphicsTextureFormat::rgba8:return 4;
			break;
		case GraphicsTextureFormat::r16f: return 2;
			break;
		case GraphicsTextureFormat::rg16f: return 4;
			break;
		case GraphicsTextureFormat::rgb16f: return 8;
			break;
		case GraphicsTextureFormat::rgba16f: return 8;
			break;
		case GraphicsTextureFormat::r32f: return 4;
			break;
		case GraphicsTextureFormat::rg32f: return 8;
			break;
		case GraphicsTextureFormat::bc1: return 0.5;
			break;
		case GraphicsTextureFormat::bc3: return 1;
			break;
		case GraphicsTextureFormat::bc4: return 1;
			break;
		case GraphicsTextureFormat::bc5: return 1;
			break;
		case GraphicsTextureFormat::bc6: return 1;//fixme
			break;
		case GraphicsTextureFormat::depth24f: return 4;
			break;
		case GraphicsTextureFormat::depth32f: return 4;
			break;
		case GraphicsTextureFormat::depth24stencil8: return 4;
			break;
		case GraphicsTextureFormat::r11f_g11f_b10f: return 4;
			break;
		case GraphicsTextureFormat::rgba16_snorm: return 8;
			break;
		default:
			break;
		}
		return 0;
	}

	static int estimate_memory_usage(const CreateTextureArgs& args) {
		double bytes_per_pixel = get_bytes_per_pixel(args.format);
		int total = 0;
		int x = args.width;
		int y = args.height;
		for (int i = 0; i < args.num_mip_maps; i++) {
			total += x * y * bytes_per_pixel;
			x >>= 2;
			y >>= 2;
		}
		if (args.type == GraphicsTextureType::t2DArray)
			total = total * args.depth_3d;
		
		return total;
	}

	static GLenum to_type(GraphicsTextureType type) {
		switch (type)
		{
		case GraphicsTextureType::t2D: return GL_TEXTURE_2D;
			break;
		case GraphicsTextureType::t2DArray: return GL_TEXTURE_2D_ARRAY;
			break;
		case GraphicsTextureType::t3D: return GL_TEXTURE_3D;
			break;
		case GraphicsTextureType::tCubemap: return GL_TEXTURE_CUBE_MAP;
			break;
		default:
			break;
		}
		ASSERT(0 && "OpenGLTextureImpl::to_type undefined");
		return GL_TEXTURE_2D;
	}
	static GLenum get_input_format(GraphicsTextureFormat fmt) {
		switch (fmt)
		{
		case GraphicsTextureFormat::r8: return GL_RED;
			break;
		case GraphicsTextureFormat::rg8: return GL_RG;
			break;
		case GraphicsTextureFormat::rgb8: return GL_RGB;
			break;
		case GraphicsTextureFormat::rgba8: return GL_RGBA;
			break;
		case GraphicsTextureFormat::r16f: return GL_RED;
			break;
		case GraphicsTextureFormat::rg16f: return GL_RG;
			break;
		case GraphicsTextureFormat::rgb16f: return GL_RGB;
			break;
		case GraphicsTextureFormat::rgba16f: return GL_RGBA;
			break;
		case GraphicsTextureFormat::r32f: return GL_RED;
			break;
		case GraphicsTextureFormat::rg32f: return GL_RG;
			break;
		case GraphicsTextureFormat::r11f_g11f_b10f: return GL_RGB;
			break;
		case GraphicsTextureFormat::bc1:
		case GraphicsTextureFormat::bc3:
		case GraphicsTextureFormat::bc4:
		case GraphicsTextureFormat::bc5:
		case GraphicsTextureFormat::bc6:
		case GraphicsTextureFormat::depth24f:
		case GraphicsTextureFormat::depth32f:
		case GraphicsTextureFormat::depth24stencil8:
			return GL_RGB;	// fixme, should be unused
		default:
			break;
		}
		ASSERT(0 && "OpenGLTextureImpl::to_type undefined");
		return GL_RED;
	}
	static GLenum to_format(GraphicsTextureFormat fmt) {
		switch (fmt)
		{
		case GraphicsTextureFormat::r8: return GL_R8;
			break;
		case GraphicsTextureFormat::rg8: return GL_RG8;
			break;
		case GraphicsTextureFormat::rgb8: return GL_RGB8;
			break;
		case GraphicsTextureFormat::rgba8: return GL_RGBA8;
			break;
		case GraphicsTextureFormat::r16f: return GL_R16F;
			break;
		case GraphicsTextureFormat::rg16f: return GL_RG16F;
			break;
		case GraphicsTextureFormat::rgb16f: return GL_RGB16F;
			break;
		case GraphicsTextureFormat::rgba16f: return GL_RGBA16F;
			break;
		case GraphicsTextureFormat::r32f: return GL_R32F;
			break;
		case GraphicsTextureFormat::rg32f: return GL_RG32F;
			break;
		case GraphicsTextureFormat::bc1: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
			break;
		case GraphicsTextureFormat::bc3: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			break;
		case GraphicsTextureFormat::bc4: return GL_COMPRESSED_RED_RGTC1;
			break;
		case GraphicsTextureFormat::bc5: return GL_COMPRESSED_RG_RGTC2;
			break; 
		case GraphicsTextureFormat::depth24f: return GL_DEPTH_COMPONENT24;
			break;
		case GraphicsTextureFormat::depth32f: return GL_DEPTH_COMPONENT32F;
			break;
		case GraphicsTextureFormat::depth24stencil8: return GL_DEPTH24_STENCIL8;
			break;
		case GraphicsTextureFormat::r11f_g11f_b10f: return GL_R11F_G11F_B10F;
			break;
		case GraphicsTextureFormat::rgba16_snorm: return GL_RGBA16_SNORM;
			break;
		}
		ASSERT(0&&"OpenGLTextureImpl: unknown texture format");
		return GL_RGB8;
	}
	static GLenum filter_to_gl(GraphicsFilterType type) {
		switch (type)
		{
		case GraphicsFilterType::Linear: return GL_LINEAR;
			break;
		case GraphicsFilterType::Nearest: return GL_NEAREST;
			break;
		case GraphicsFilterType::MipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
			break;

		}
		ASSERT(0 && "filter_to_gl not defined");
		return 0;
	}
	static GLenum wrap_to_gl(GraphicsTextureEdge type) {
		switch (type)
		{
		case GraphicsTextureEdge::Repeat: return GL_REPEAT;
			break;
		case GraphicsTextureEdge::Clamp: return GL_CLAMP_TO_EDGE;
			break;
		default:
			break;
		}
		ASSERT(0 && "wrap_to_gl not defined");
		return 0;
	}
	int mem_usage = 0;
	OpenGLTextureImpl() = default;
	OpenGLTextureImpl(const CreateTextureArgs& args) {
		mem_usage = estimate_memory_usage(args);
		total_gfx_mem_usage += mem_usage;
		auto type = to_type(args.type);
		glCreateTextures(type, 1, &id);
		const int x = args.width;
		const int y = args.height;
		this->width = args.width;
		this->height = args.height;
		my_fmt = args.format;
		internal_format_gl = to_format(args.format);
		if (args.type == GraphicsTextureType::t2DArray) {
			glTextureStorage3D(id, args.num_mip_maps, internal_format_gl, x, y,args.depth_3d);
		}
		else {
			glTextureStorage2D(id, args.num_mip_maps, internal_format_gl, x, y);
		}

		switch (args.sampler_type)
		{
		case GraphicsSamplerType::AnisotropyDefault:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAX_ANISOTROPY, 16.f);
			break;
		case GraphicsSamplerType::LinearDefault:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			break;
		case GraphicsSamplerType::NearestDefault:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			break;
		case GraphicsSamplerType::LinearClamped:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			break;
		case GraphicsSamplerType::NearestClamped:
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			break;
		case GraphicsSamplerType::CsmShadowmap: {
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTextureParameteri(id, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			float bordercolor[] = { 1.0,1.0,1.0,1.0 };
			glTextureParameterfv(id, GL_TEXTURE_BORDER_COLOR, bordercolor);
		}break;
		case GraphicsSamplerType::AtlasShadowmap: 
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTextureParameteri(id, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			break;
		case GraphicsSamplerType::CubemapDefault:
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			break;
		default:
			break;
		}
	}
	~OpenGLTextureImpl() override {
		glDeleteTextures(1, &id);
		total_gfx_mem_usage -= mem_usage;
	}

	texhandle id = 0;

	uint32_t get_internal_handle() override {
		return id;
	}

	void sub_image_upload(int level, int x, int y, int w, int h, int size, const void* data) override {

		if(is_compressed())
			glCompressedTextureSubImage2D(id, level, x, y, w, h, internal_format_gl, size, data);
		else {
			const GLenum type = is_float_type() ? GL_FLOAT : GL_UNSIGNED_BYTE;
			const GLenum input_fmt = get_input_format(my_fmt);
			glTextureSubImage2D(id, level, x, y, w, h, input_fmt, type, data);
		}
	}
	void release() override {
		delete this;
	}
	bool is_compressed() const {
		int first = (int)GraphicsTextureFormat::bc1;
		int last = (int)GraphicsTextureFormat::bc6;
		int fmt_i = (int)my_fmt;
		return fmt_i >= first && fmt_i <= last;
	}
	bool is_float_type() const {
		using gtf = GraphicsTextureFormat;
		auto types = {
			gtf::r16f,
			gtf::rg16f,
			gtf::rgb16f,
			gtf::rgba16f,
			gtf::r32f,
			gtf::rg32f,
			gtf::depth24f,
			gtf::depth32f,
			gtf::r11f_g11f_b10f,
			gtf::rgba16_snorm,
		};
		for (auto t : types) {
			if (t == my_fmt) return true;
		}
		return false;
	}
	glm::ivec2 get_size() const override {
		return { width,height };
	}
	GraphicsTextureFormat get_texture_format() const override {
		return my_fmt;
	}
	GraphicsTextureType get_texture_type() const override {
		return my_type;
	}
	GraphicsTextureType my_type{};
	GraphicsTextureFormat my_fmt{};
	GLenum internal_format_gl{};
	int width = 0;
	int height = 0;
};
class OpenGLBufferImpl : public IGraphicsBuffer {
public:
	bufferhandle id = 0;
	OpenGLBufferImpl(const CreateBufferArgs& args) {
		glCreateBuffers(1, &id);
		usage_type = (args.flags & BUFFER_USE_DYNAMIC) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
		glNamedBufferData(id, args.size, nullptr, usage_type);
	}
	~OpenGLBufferImpl() override {
		glDeleteBuffers(1, &id);
	}
	void release() override {
		delete this;
	}
	void upload(const void* data, int size) override {
		glNamedBufferData(id, size, data, usage_type);
	}
	void sub_upload(const void* data, int size, int ofs) override {
		glNamedBufferSubData(id, ofs, size, data);
	}
	uint32_t get_internal_handle() override {
		return id;
	}


	GLenum usage_type{};
};
class OpenGLVertexInputImpl : public IGraphicsVertexInput {
public:

	static bool get_type_is_i_ptr(GraphicsVertexAttribType type) {
		using gvat = GraphicsVertexAttribType;
		auto i_types = {
			gvat::i8,gvat::i16,gvat::u8,gvat::u16
		};
		for (auto t : i_types) {
			if (type == t)
				return true;
		}
		return false;
	}
	static bool get_type_is_normalized(GraphicsVertexAttribType type) {
		using gvat = GraphicsVertexAttribType;
		auto normalized_type = {
			gvat::i8_normalized,gvat::i16_normalized,gvat::u8_normalized,gvat::u16_normalized
		};
		for (auto t : normalized_type) {
			if (type == t)
				return true;
		}
		return false;
	}
	static GLenum get_type_enum(GraphicsVertexAttribType type) {
		using gvat = GraphicsVertexAttribType;
		switch (type)
		{
		case GraphicsVertexAttribType::u8: return GL_UNSIGNED_BYTE;
			break;
		case GraphicsVertexAttribType::u16: return GL_UNSIGNED_SHORT;
			break;
		case GraphicsVertexAttribType::i8: return GL_BYTE;
			break;
		case GraphicsVertexAttribType::i16: return GL_SHORT;
			break;
		case GraphicsVertexAttribType::u8_normalized: return GL_UNSIGNED_BYTE;
			break;
		case GraphicsVertexAttribType::u16_normalized: return GL_UNSIGNED_SHORT;
			break;
		case GraphicsVertexAttribType::i8_normalized: return GL_BYTE;
			break;
		case GraphicsVertexAttribType::i16_normalized: return GL_SHORT;
			break;
		case GraphicsVertexAttribType::float32: return GL_FLOAT;
			break;
		default:
			break;
		}
		ASSERT(0 && "OpenGLVertexInputImpl::get_type_enum not defined");
		return GL_FLOAT;
	}


	OpenGLVertexInputImpl(const CreateVertexInputArgs& args) {
		assert(args.vertex);
		index_type = args.index_type;
		glGenVertexArrays(1, &vao_id);
		OpenGLBufferImpl* vbuffer = (OpenGLBufferImpl*)args.vertex;
		OpenGLBufferImpl* ibuffer = (OpenGLBufferImpl*)args.index;
		has_index = ibuffer != nullptr;

		glBindVertexArray(vao_id);
		if(ibuffer)
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuffer->id);
		glBindBuffer(GL_ARRAY_BUFFER,vbuffer->id);
		for (const auto& atr : args.layout) {
			const int loc = atr.index;
			const int count = atr.count;
			const int stride = atr.stride;
			const int64_t ofs = (int64_t)atr.offset;
			const GLenum type = get_type_enum(atr.type);

			if (get_type_is_i_ptr(atr.type)) {
				glVertexAttribIPointer(loc, count, type,stride, (void*)ofs);
			}
			else {
				const bool is_normalized = get_type_is_normalized(atr.type);
				glVertexAttribPointer(loc, count, type, is_normalized, stride, (void*)ofs);
			}
			glEnableVertexAttribArray(loc);
		}
		glBindVertexArray(0);
	}
	~OpenGLVertexInputImpl() override {
		glDeleteVertexArrays(1, &vao_id);
	}
	void release() override {
		delete this;
	}
	uint32_t get_internal_handle() override {
		return vao_id;
	}

	bool has_index = true;
	VertexInputIndexType index_type{};
	uint32 vao_id = 0;
};
ConfigVar use_multiple_fbos("use_multiple_fbos", "1", CVAR_BOOL, "");
class OpenGLDeviceImpl : public IGraphicsDevice
{
public:

	fbohandle get_next_fbo() {
		return shared_framebuffer;
	}


	fbohandle shared_framebuffer = 0;
	fbohandle shared_framebuffer_2 = 0;

	OpenGLTextureImpl swapchain_fake_img;

	OpenGLDeviceImpl(ThingerBobber* f) : thinger_bobber(f) {
		glGenFramebuffers(1, &shared_framebuffer);
		glGenFramebuffers(1, &shared_framebuffer_2);
	}
	~OpenGLDeviceImpl() override {

	}
	// Inherited via IGraphicsDevice
	GraphicsDeviceType get_device_type() override
	{
		return GraphicsDeviceType::OpenGl;
	}

	void set_framebuffer_with_info(const RenderPassState& state, fbohandle framebuffer_to_use, int& min_width, int& min_height) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_to_use);
		for (int i = 0; i < state.color_infos.size(); i++) {
			const ColorTargetInfo& info = state.color_infos[i];
			OpenGLTextureImpl* texture = (OpenGLTextureImpl*)info.texture;
			assert(texture != &swapchain_fake_img);
			if (info.layer == -1)
				glNamedFramebufferTexture(framebuffer_to_use, GL_COLOR_ATTACHMENT0 + i, texture->id, info.mip);
			else
				glNamedFramebufferTextureLayer(framebuffer_to_use, GL_COLOR_ATTACHMENT0 + i, texture->id, info.mip, info.layer);
			min_width = std::min(texture->width, min_width);
			min_height = std::min(texture->height, min_height);
		}

		const int max_attachments = 32;
		std::array<unsigned int, max_attachments> gbuffer_attachments;
		for (int i = 0; i < state.color_infos.size(); i++)
			gbuffer_attachments[i] = GL_COLOR_ATTACHMENT0 + i;
		glNamedFramebufferDrawBuffers(framebuffer_to_use, state.color_infos.size(), gbuffer_attachments.data());


		if (state.depth_info) {
			OpenGLTextureImpl* texture = (OpenGLTextureImpl*)state.depth_info;
			assert(texture != &swapchain_fake_img);

			if(state.depth_layer==-1)
				glNamedFramebufferTexture(framebuffer_to_use, GL_DEPTH_ATTACHMENT, texture->id, 0);
			else
				glNamedFramebufferTextureLayer(framebuffer_to_use, GL_DEPTH_ATTACHMENT, texture->id,0, state.depth_layer);
			min_width = std::min(texture->width, min_width);
			min_height = std::min(texture->height, min_height);
		}
		else {
			glNamedFramebufferTexture(framebuffer_to_use, GL_DEPTH_ATTACHMENT, 0, 0);
		}
	}

	void set_render_pass(const RenderPassState& state) override {
		cur_pass = state;

		int min_width = 100'000;
		int min_height = 100'000;

		if (!state.depth_info && state.color_infos.size() == 1 && state.color_infos[0].texture == &swapchain_fake_img) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		else {
			set_framebuffer_with_info(state, get_next_fbo(), min_width, min_height);
		}

		glViewport(0, 0, min_width, min_height);
		if (state.wants_color_clear || state.wants_depth_clear) {
			glClearDepth(state.clear_depth_val);
			glClearColor(0, 0.5, 0, 1);
			GLbitfield mask{};
			if (state.wants_depth_clear)
				mask |= GL_DEPTH_BUFFER_BIT;
			if (state.wants_color_clear)
				mask |= GL_COLOR_BUFFER_BIT;


			thinger_bobber->set_depth_write_enabled(true);
			glClear(mask);
			//activeStats.framebuffer_clears++;
		}
		//activeStats.framebuffer_changes++;

	}
	


	void blit_textures(const GraphicsBlitInfo& info) override {
		
		fbohandle src_fbo = shared_framebuffer;
		fbohandle dest_fbo = shared_framebuffer_2;
		{
			int dummyx = 0, dummyy = 0;
			RenderPassState fake1;
			auto colors1 = {
				ColorTargetInfo(info.src.texture,info.src.layer,info.src.mip)
			};
			fake1.color_infos = colors1;
			set_framebuffer_with_info(fake1, src_fbo, dummyx, dummyy);

			if (info.dest.texture == &swapchain_fake_img) {
				dest_fbo = 0;	// backbuffer
			}
			else {
				RenderPassState fake2;
				auto colors2 = {
					ColorTargetInfo(info.dest.texture,info.dest.layer,info.dest.mip)
				};
				fake2.color_infos = colors2;
				set_framebuffer_with_info(fake2, dest_fbo, dummyx, dummyy);
			}
		}

		glBlitNamedFramebuffer(src_fbo, dest_fbo,
			info.src.x,info.src.y,info.src.w,info.src.h,info.dest.x,info.dest.y,info.dest.w,info.dest.h,
			GL_COLOR_BUFFER_BIT,
			OpenGLTextureImpl::filter_to_gl(info.filter));
	}

	IGraphicsTexture* get_swapchain_texture() override {
		return &swapchain_fake_img;
	}

	IGraphicsTexture* create_texture(const CreateTextureArgs& args) override {
		return new OpenGLTextureImpl(args);
	}
	IGraphicsBuffer* create_buffer(const CreateBufferArgs& args) override
	{
		return new OpenGLBufferImpl(args);
	}
	IGraphicsVertexInput* create_vertex_input(const CreateVertexInputArgs& args) override
	{
		return new OpenGLVertexInputImpl(args);
	}

private:
	ThingerBobber* thinger_bobber = nullptr;
	opt<RenderPassState> cur_pass;
};
IGraphicsDevice* IGraphicsDevice::create_opengl_device(ThingerBobber* f) {
	return new OpenGLDeviceImpl(f);
}
#endif