#pragma once

#include <cstdint>
#include <vector>
#include <Framework/InlineVec.h>
#include <Framework/Handle.h>
class PMSubBuffer
{
public:
	void* syncobjs[3]; /* GLsync type */
	uint32_t index = 0; // 0, 1, or 2
	uint32_t start = 0;
	uint32_t size = 0;
};

// Triple buffered, persistently mapped opengl buffer
// use for dynamic streaming stuff
class PersistentlyMappedBuffer
{
public:
	// *initializes with a tripple buffered buffer of size size_per_buffer
	//   ie, if you want a max of 100 verticies with sizeof(Vertex)==40, size_per_buffer = 4,000	
	// *call this again if you need to resize the buffer (invalidates all handles!)
	// *call with 0 to free data
	void init(uint32_t size_per_buffer);

	// waits for fence and returns a valid writing pointer for the subbuffer
	// only call this once per drawing frame
	void* wait_and_get_write_ptr(handle<PMSubBuffer> subbuffer);
	// locks current range, call this every frame when gpu is accessing this subbuffer
	void lock_current_range(handle<PMSubBuffer> subbuffer);
	// allocates a subbuffer using a stack allocator
	// if you want a big buffer size of 100 verticies which will be divided into 10 smaller buffers,
	// call init(sizeof(Vertex)*100), then allocate_subbuffer(sizeof(Vertex)*10) ten times
	handle<PMSubBuffer> allocate_subbuffer(uint32_t size);
	// get offset of a subbbuffer in the main buffer, this accounts for where the allocation is and what the sync buffer is
	uintptr_t get_offset(handle<PMSubBuffer> subbuffer) {
		auto& sb = get_sub(subbuffer);
		return size_per_buffer * sb.index + sb.start;
	}

	// binds the currently used range to binding point
	// calls glBindBufferRange()
	void bind_buffer(uint32_t target /* array_buffer, etc. */, int binding, uintptr_t offset, size_t size);

	const static uint32_t ARRAY_BUFFER;
	const static uint32_t ELEMENT_ARRAY_BUFFER;
	const static uint32_t UNIFORM_BUFFER;

	uint32_t get_handle() const {
		return buffer;
	}
	uint32_t get_per_buffer_size() const {
		return size_per_buffer;
	}
	uint32_t get_total_size() const {
		return total_size;
	}

	const PMSubBuffer& get_sub(handle<PMSubBuffer> handle) const {
		return sub_buffers[handle.id];
	}
private:
	//uintptr_t get_offset() {
	//	return  (index * size_per_buffer);
	//}
	PMSubBuffer& get_sub(handle<PMSubBuffer> handle) {
		return sub_buffers[handle.id];
	}

	InlineVec<PMSubBuffer, 1> sub_buffers;

	//void* syncobjs[3]; /* GLsync type */

	void* ptr = nullptr;
	//uint32_t index = 0;
	uint32_t size_per_buffer = 0;
	uint32_t size_per_buffer_used = 0;
	uint32_t total_size = 0;
	uint32_t buffer = 0;
};
