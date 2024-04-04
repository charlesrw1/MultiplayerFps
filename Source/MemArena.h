#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Util.h"

class Memory_Arena
{
public:
	void init(const char* name, uint32_t size);
	void shutdown();

	void free_top() {
		top_pointer = (uintptr_t)buffer + buffer_len;
	}

	void free_bottom() {
		bottom_pointer = (uintptr_t)buffer;
	}

	void* alloc_top(uint32_t size);
	void* alloc_bottom(uint32_t size);

	uintptr_t get_top_marker() {
		return top_pointer;
	}
	uintptr_t get_bottom_marker() {
		return bottom_pointer;
	}
	void free_top_to_marker(uintptr_t mark) {
		top_pointer = mark;
	}
	void free_bottom_to_marker(uintptr_t mark) {
		bottom_pointer = mark;
	}

	const char* debug_name = "";
	uintptr_t bottom_pointer = 0;
	uintptr_t top_pointer = 0;
	uint8_t* buffer = nullptr;
	uint32_t buffer_len = 0;
};

inline void Memory_Arena::init(const char* name, uint32_t size)
{
	ASSERT(buffer == nullptr);
	debug_name = name;
	buffer = new uint8_t[size];
	bottom_pointer = (uintptr_t)buffer;
	top_pointer = (uintptr_t)buffer + size;
	buffer_len = size;
}
inline void Memory_Arena::shutdown()
{
	delete buffer;
	buffer = nullptr;
	bottom_pointer = top_pointer = 0;
	buffer_len = 0;
}

inline uintptr_t align_backward(uintptr_t ptr)
{
	uintptr_t modulo = ptr % 16;
	return ptr - modulo;
}

inline void* Memory_Arena::alloc_top(uint32_t size)
{
	uintptr_t ret = align_backward(top_pointer - size);

	if (ret <= bottom_pointer) {
		sys_print("OUT OF MEMORY %s\n", debug_name);
		fflush(stdout);
		std::abort();
		return nullptr;
	}
	top_pointer = ret;
	return (void*)ret;
}

inline uintptr_t align_forward(uintptr_t ptr)
{
	uintptr_t modulo = ptr % 16;
	if (modulo != 0)
		return ptr + 16 - modulo;
	return ptr;
}

inline void* Memory_Arena::alloc_bottom(uint32_t size)
{
	uintptr_t ret = align_forward(bottom_pointer);

	if (ret + size >= top_pointer) {
		sys_print("OUT OF MEMORY %s\n", debug_name);
		fflush(stdout);
		std::abort();
		return nullptr;
	}
	bottom_pointer += size;
	return (void*)ret;
}