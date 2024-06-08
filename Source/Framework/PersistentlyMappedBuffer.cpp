#include "PersistentlyMappedBuffer.h"
#include "glad/glad.h"
#include <cassert>
#include "Framework/Util.h"

static_assert(sizeof(void*) == sizeof(GLsync), "GLsync not the size of void*");

const uint32_t PersistentlyMappedBuffer::ARRAY_BUFFER = GL_ARRAY_BUFFER;
const uint32_t PersistentlyMappedBuffer::ELEMENT_ARRAY_BUFFER = GL_ELEMENT_ARRAY_BUFFER;
const uint32_t PersistentlyMappedBuffer::UNIFORM_BUFFER = GL_UNIFORM_BUFFER;

handle<PMSubBuffer> PersistentlyMappedBuffer::allocate_subbuffer(uint32_t size)
{
	if (size_per_buffer_used + size > size_per_buffer) {
		Fatalf("!!! PersistentlyMappedBuffer OUT OF MEMORY (%u + %u > %u)\n", size_per_buffer_used, size, size_per_buffer);
	}
	PMSubBuffer buf;
	buf.index = 0;
	buf.syncobjs[0] = buf.syncobjs[1] = buf.syncobjs[2]=nullptr;
	buf.start = size_per_buffer_used;
	buf.size = size;
	size_per_buffer_used += size;

	int handle = sub_buffers.size();
	sub_buffers.push_back(buf);

	return{ handle };
}

// initializes with a tripple buffered buffer of size size_per_buffer
// call this again if you need to resize the buffer
void PersistentlyMappedBuffer::init(uint32_t size_per_buffer) {
	if (buffer != 0) {
		glUnmapNamedBuffer(buffer);
		glDeleteBuffers(1, &buffer);

		for (int i = 0; i < sub_buffers.size(); i++) {
			for (int j = 0; j < 3; j++) {
				if (sub_buffers[i].syncobjs[j] != nullptr) {
					glDeleteSync((GLsync)sub_buffers[i].syncobjs[j]);
					sub_buffers[i].syncobjs[j] = nullptr;
				}
			}
		}

		sub_buffers.resize(0);
	}

	this->size_per_buffer = size_per_buffer;
	total_size = size_per_buffer * 3;

	if (size_per_buffer == 0) {
		ptr = nullptr;
		buffer = 0;
		return;
	}

	glCreateBuffers(1, &buffer);
	uint32_t mapflags = GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT;
	glNamedBufferStorage(buffer, total_size, nullptr, mapflags);
	ptr = glMapNamedBufferRange(buffer, 0, total_size, mapflags);
}

// waits for fence and returns the pointer to the area good for writing
// only call this once per frame to get pointer as it increments what sub-buffer it uses

inline void* PersistentlyMappedBuffer::wait_and_get_write_ptr(handle<PMSubBuffer> subbuffer) {
	assert(subbuffer.is_valid());

	auto& sb = get_sub(subbuffer);
	sb.index = (sb.index + 1) % 3;

	if (sb.syncobjs[sb.index] != nullptr) {
		GLbitfield waitFlags = 0;
		GLuint64 waitDuration = 0;
		while (1) {
			GLenum waitRet = glClientWaitSync((GLsync)sb.syncobjs[sb.index], waitFlags, waitDuration);
			if (waitRet == GL_ALREADY_SIGNALED || waitRet == GL_CONDITION_SATISFIED) {
				return (uint8_t*)ptr + get_offset(subbuffer);
			}

			if (waitRet == GL_WAIT_FAILED) {
				assert(!"Not sure what to do here. Probably raise an exception or something.");
				return nullptr;
			}

			// After the first time, need to start flushing, and wait for a looong time.
			waitFlags = GL_SYNC_FLUSH_COMMANDS_BIT;
			waitDuration = 100'000'000;
		}
	}
	return (uint8_t*)ptr + get_offset(subbuffer);
}

// call this after you are done writing

 void PersistentlyMappedBuffer::lock_current_range(handle<PMSubBuffer> subbuffer) {
	 assert(subbuffer.is_valid());
	 auto& sb = get_sub(subbuffer);
	 if (sb.syncobjs[sb.index] != nullptr)
		 glDeleteSync((GLsync)sb.syncobjs[sb.index]);

	 sb.syncobjs[sb.index] = (void*)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);	// reinterpret_cast to void*
}

 // binds the currently used range to binding point

  void PersistentlyMappedBuffer::bind_buffer(uint32_t target, int binding, uintptr_t offset, size_t size) {
	 glBindBufferRange(target, binding, buffer, offset, size);
 }
