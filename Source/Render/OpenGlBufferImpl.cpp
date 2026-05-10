#if 1
// OpenGLBufferImpl and OpenGLVertexInputImpl — GPU buffer and VAO management.
// Provides factory functions declared in OpenGlDeviceLocal.h.
#include "OpenGlDeviceLocal.h"

// ---------------------------------------------------------------------------
// OpenGLBufferImpl
// ---------------------------------------------------------------------------
class OpenGLBufferImpl : public IGraphicsBuffer
{
public:
	bufferhandle id = 0;

	OpenGLBufferImpl(const CreateBufferArgs& args) {
		ASSERT(args.size > 0);
		glCreateBuffers(1, &id);
		usage_type = (args.flags & BUFFER_USE_DYNAMIC) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
		glNamedBufferData(id, args.size, nullptr, usage_type);
		this->buffer_size = args.size;
		opengl_stats.all_buffers.insert(static_cast<IGraphicsBuffer*>(this));
	}

	~OpenGLBufferImpl() override {
		glDeleteBuffers(1, &id);
		opengl_stats.all_buffers.remove(static_cast<IGraphicsBuffer*>(this));
	}

	void release() override { delete this; }

	void upload(const void* data, int size) override {
		ASSERT(size > 0 && data);
		this->buffer_size = size;
		glNamedBufferData(id, size, data, usage_type);
	}

	void sub_upload(const void* data, int size, int ofs) override {
		ASSERT(size > 0 && data && ofs >= 0);
		glNamedBufferSubData(id, ofs, size, data);
	}

	uint32_t get_internal_handle() override { return id; }
	int get_buf_size() const override { return buffer_size; }

	int    buffer_size = 0;
	GLenum usage_type{};
};

// ---------------------------------------------------------------------------
// OpenGLVertexInputImpl
// ---------------------------------------------------------------------------
class OpenGLVertexInputImpl : public IGraphicsVertexInput
{
public:
	static bool get_type_is_i_ptr(GraphicsVertexAttribType type) {
		ASSERT((int)type >= 0);
		using gvat = GraphicsVertexAttribType;
		auto i_types = {gvat::i8, gvat::i16, gvat::u8, gvat::u16};
		for (auto t : i_types)
			if (type == t)
				return true;
		return false;
	}

	static bool get_type_is_normalized(GraphicsVertexAttribType type) {
		ASSERT((int)type >= 0);
		using gvat = GraphicsVertexAttribType;
		auto normalized_type = {gvat::i8_normalized, gvat::i16_normalized, gvat::u8_normalized, gvat::u16_normalized};
		for (auto t : normalized_type)
			if (type == t)
				return true;
		return false;
	}

	static GLenum get_type_enum(GraphicsVertexAttribType type) {
		ASSERT((int)type >= 0);
		using gvat = GraphicsVertexAttribType;
		switch (type) {
		case gvat::u8:             return GL_UNSIGNED_BYTE;
		case gvat::u16:            return GL_UNSIGNED_SHORT;
		case gvat::i8:             return GL_BYTE;
		case gvat::i16:            return GL_SHORT;
		case gvat::u8_normalized:  return GL_UNSIGNED_BYTE;
		case gvat::u16_normalized: return GL_UNSIGNED_SHORT;
		case gvat::i8_normalized:  return GL_BYTE;
		case gvat::i16_normalized: return GL_SHORT;
		case gvat::float32:        return GL_FLOAT;
		default: break;
		}
		ASSERT(0 && "OpenGLVertexInputImpl::get_type_enum not defined");
		return GL_FLOAT;
	}

	OpenGLVertexInputImpl(const CreateVertexInputArgs& args) {
		ASSERT(args.vertex != nullptr);
		index_type = args.index_type;
		glGenVertexArrays(1, &vao_id);
		OpenGLBufferImpl* vbuffer = (OpenGLBufferImpl*)args.vertex;
		OpenGLBufferImpl* ibuffer = (OpenGLBufferImpl*)args.index;
		has_index = ibuffer != nullptr;

		glBindVertexArray(vao_id);
		if (ibuffer)
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuffer->id);
		glBindBuffer(GL_ARRAY_BUFFER, vbuffer->id);
		for (const auto& atr : args.layout) {
			const int    loc    = atr.index;
			const int    count  = atr.count;
			const int    stride = atr.stride;
			const int64_t ofs   = (int64_t)atr.offset;
			const GLenum  type  = get_type_enum(atr.type);

			if (get_type_is_i_ptr(atr.type)) {
				glVertexAttribIPointer(loc, count, type, stride, (void*)ofs);
			} else {
				const bool is_normalized = get_type_is_normalized(atr.type);
				glVertexAttribPointer(loc, count, type, is_normalized, stride, (void*)ofs);
			}
			glEnableVertexAttribArray(loc);
		}
		glBindVertexArray(0);
	}

	~OpenGLVertexInputImpl() override { glDeleteVertexArrays(1, &vao_id); }
	void release() override { delete this; }
	uint32_t get_internal_handle() override { return vao_id; }

	bool has_index = true;
	VertexInputIndexType index_type{};
	uint32 vao_id = 0;
};

// ---------------------------------------------------------------------------
// Factory functions (declared in OpenGlDeviceLocal.h)
// ---------------------------------------------------------------------------
IGraphicsBuffer* opengl_create_buffer(const CreateBufferArgs& args) {
	ASSERT(args.size > 0);
	return new OpenGLBufferImpl(args);
}

IGraphicsVertexInput* opengl_create_vertex_input(const CreateVertexInputArgs& args) {
	ASSERT(args.vertex != nullptr);
	return new OpenGLVertexInputImpl(args);
}
#endif
