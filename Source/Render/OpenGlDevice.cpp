#include "IGraphsDevice.h"
#include <unordered_set>
#include <optional>
#include "DrawLocal.h"
#include <array>
template<typename T>
using opt = std::optional<T>;

IGraphicsDevice* IGraphicsDevice::inst = nullptr;

class OpenGLProgramImpl : public IGraphicsProgram {
public:
	Shader id;

	void release() override {

	}
};
class OpenGLTextureImpl : public IGraphicsTexture {
public:
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
		default:
			break;
		}
		ASSERT(0&&"OpenGLTextureImpl: unknown texture format");
		return GL_RGB8;
	}

	OpenGLTextureImpl(const CreateTextureArgs& args) {
		auto type = to_type(args.type);
		glCreateTextures(type, 1, &id);
		const int x = args.width;
		const int y = args.height;
		my_fmt = args.format;
		internal_format_gl = to_format(args.format);
		glTextureStorage2D(id, args.num_mip_maps, internal_format_gl, x, y);
	}
	~OpenGLTextureImpl() override {
		glDeleteTextures(1, &id);
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
		};
		for (auto t : types) {
			if (t == my_fmt) return true;
		}
		return false;
	}

	GraphicsTextureFormat my_fmt{};
	GLenum internal_format_gl{};
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
		assert(args.index && args.vertex);
		index_type = args.index_type;
		glGenVertexArrays(1, &vao_id);
		OpenGLBufferImpl* vbuffer = (OpenGLBufferImpl*)args.vertex;
		OpenGLBufferImpl* ibuffer = (OpenGLBufferImpl*)args.index;

		glBindVertexArray(vao_id);
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


	VertexInputIndexType index_type{};
	uint32 vao_id = 0;
};

class OpenGLDeviceImpl : public IGraphicsDevice
{
public:
	OpenGLDeviceImpl() {

	}
	~OpenGLDeviceImpl() override {

	}
	// Inherited via IGraphicsDevice
	GraphicsDeviceType get_device_type() override
	{
		return GraphicsDeviceType::OpenGl;
	}
	void set_pipeline(const GraphicsPipelineState& state) override
	{
	}
	void set_render_pass(const RenderPassState& state) override {
		assert(!cur_pass.has_value());
		cur_pass = state;
	}
	void end_render_pass() override {
		assert(cur_pass.has_value());
		cur_pass = std::nullopt;
	}

	void set_viewport(int x, int y, int w, int h) override
	{
	}
	void set_scissor(int x, int y, int w, int h) override
	{
	}
	void clear_scissor() override
	{
	}
	void draw_arrays(int ofs, int count) override
	{
	}
	void draw_elements(int ofs, int count) override
	{
	}
	void multidraw_elements_indirect(IGraphicsBuffer* buffer, int ofs, int count) override
	{
	}
	void bind_storage_buffers(std::span<IGraphicsBuffer* const> buffers) override
	{
	}
	void bind_textures(std::span<IGraphicsTexture* const> textures) override
	{
	}
	IGraphicsProgram* create_program(const CreateProgramArgs& args) override
	{
		OpenGLProgramImpl* program = new OpenGLProgramImpl;
		Shader::compile_vert_frag_single_file(&program->id, std::string(args.file_name), std::string(args.defines));
		return program;
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
	opt<RenderPassState> cur_pass;
};
IGraphicsDevice* IGraphicsDevice::create_opengl_device() {
	return new OpenGLDeviceImpl();
}