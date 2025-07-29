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

IGraphicsDevice* IGraphicsDevice::inst = nullptr;
extern ConfigVar log_shader_compiles;
class OpenGLProgramImpl : public IGraphicsProgram {
public:
	static string compute_hash_for_program_def(const CreateProgramArgs& args)
	{
		string inp = std::string(args.file_name) + std::string(args.defines);// def.vert + def.frag + def.geo + def.defines;
		return StringUtils::alphanumeric_hash(inp);
	}
	void recompile(const CreateProgramArgs& args) {
		string file_name = std::string(args.file_name);
		string hashed_path = compute_hash_for_program_def(args)+".bin";
		//Shader::compile_vert_frag_single_file(&id, std::string(args.file_name), std::string(args.defines));
		compile_failed = false;
		auto binFile = FileSys::open_read(hashed_path.c_str(), FileSys::SHADER_CACHE);
		auto shaderFile = FileSys::open_read_engine(file_name.c_str());
		if (shaderFile && binFile) {
			if (shaderFile->get_timestamp() <= binFile->get_timestamp()) {
				if (log_shader_compiles.get_bool())
					sys_print(Debug, "Program_Manager::recompile: loading cached binary: %s\n", hashed_path.data());

				// load cached binary
				BinaryReader reader(binFile.get());
				auto sourceType = reader.read_int32();
				auto len = reader.read_int32();
				vector<uint8_t> bytes(len, 0);
				reader.read_bytes_ptr(bytes.data(), bytes.size());

				if (shader_obj.ID != 0) {
					glDeleteProgram(shader_obj.ID);
				}
				shader_obj.ID = glCreateProgram();
				glProgramBinary(shader_obj.ID, sourceType, bytes.data(), bytes.size());
				glValidateProgram(shader_obj.ID);

				GLint success = 0;
				glGetProgramiv(shader_obj.ID, GL_LINK_STATUS, &success);
				if (success == GL_FALSE) {
					GLint logLength = 0;
					glGetProgramiv(shader_obj.ID, GL_INFO_LOG_LENGTH, &logLength);
					std::vector<GLchar> log(logLength);
					glGetProgramInfoLog(shader_obj.ID, logLength, nullptr, log.data());
					sys_print(Error, "Program_Manager::recompile: loading binary failed: %s\n", log.data());
				}
				else {
					return;	// done
				}
			}
		}
		binFile.reset();

		// fail path
		compile_failed = Shader::compile_vert_frag_single_file(&shader_obj, file_name, std::string(args.defines)) != ShaderResult::SHADER_SUCCESS;

		if (!compile_failed) {
			const auto program = shader_obj.ID;
			GLint length = 0;
			glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &length);
			if (log_shader_compiles.get_bool())
				sys_print(Debug, "Program_Manager::recompile: saving cached binary: %s\n", hashed_path.data());
			vector<uint8_t> bytes(length, 0);
			GLenum outType = 0;
			glGetProgramBinary(shader_obj.ID, bytes.size(), nullptr, &outType, bytes.data());
			FileWriter writer(bytes.size() + 8);
			writer.write_int32(outType);
			writer.write_int32(bytes.size());
			writer.write_bytes_ptr(bytes.data(), bytes.size());
			auto outFile = FileSys::open_write(hashed_path.c_str(), FileSys::SHADER_CACHE);
			if (outFile) {
				outFile->write(writer.get_buffer(), writer.get_size());
			}
			else {
				sys_print(Error, "Program_Manager::recompile: couldnt open file to write program binary: %s\n", hashed_path.data());
			}
		}

	}

	OpenGLProgramImpl(const CreateProgramArgs& args) {
		recompile(args);
	}

	bool compile_failed = true;
	Shader shader_obj;

	uint32_t get_internal_handle() override {
		return shader_obj.ID;
	}

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
		default:
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
		default:
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

	OpenGLTextureImpl(const CreateTextureArgs& args) {
		auto type = to_type(args.type);
		glCreateTextures(type, 1, &id);
		const int x = args.width;
		const int y = args.height;
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
		default:
			break;
		}
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
			gtf::r11f_g11f_b10f,
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
		OpenGLProgramImpl* program = new OpenGLProgramImpl(args);
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
#endif