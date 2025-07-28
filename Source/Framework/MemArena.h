#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Framework/Util.h"
#include <span>
class Memory_Arena
{
public:
	void init(const char* name, int64_t size);

	Memory_Arena() = default;
	~Memory_Arena() {
		delete buffer;
		buffer = nullptr;
		bottom_pointer = top_pointer = 0;
		buffer_len = 0;
	}
	Memory_Arena(const Memory_Arena& other) = delete;
	Memory_Arena& operator=(const Memory_Arena& other) = delete;
	Memory_Arena(Memory_Arena&& other) = delete;


	void free_top() {
		top_pointer = (uintptr_t)buffer + buffer_len;
	}

	void free_bottom() {
		bottom_pointer = (uintptr_t)buffer;
	}

	void* alloc_top(int64_t size);
	void* alloc_bottom(int64_t size);

	template<typename T>
	T* alloc_top_type(int count) {
		return (T*)alloc_top(count * sizeof(T));
	}
	template<typename T>
	T* alloc_bottom_type(int count) {
		return (T*)alloc_bottom(count * sizeof(T));
	}
	template<typename T>
	std::span<T> alloc_top_span(int count) {
		std::span<T> out(alloc_top_type<T>(count), count);
		return out;
	}
	template<typename T>
	std::span<T> alloc_bottom_span(int count) {
		std::span<T> out(alloc_bottom_type<T>(count), count);
		return out;
	}

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

private:
	const char* debug_name = "";
	uintptr_t bottom_pointer = 0;
	uintptr_t top_pointer = 0;
	uint8_t* buffer = nullptr;
	int64_t buffer_len = 0;
};

inline void Memory_Arena::init(const char* name, int64_t size)
{
	ASSERT(buffer == nullptr);
	debug_name = name;
	buffer = new uint8_t[size];
	bottom_pointer = (uintptr_t)buffer;
	top_pointer = (uintptr_t)buffer + size;
	buffer_len = size;
}


inline uintptr_t align_backward(uintptr_t ptr)
{
	uintptr_t modulo = ptr % 16;
	return ptr - modulo;
}

inline void* Memory_Arena::alloc_top(int64_t size)
{
	uintptr_t ret = align_backward(top_pointer - size);

	if (ret <= bottom_pointer) {
		sys_print(Error, "OUT OF MEMORY %s\n", debug_name);
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

inline void* Memory_Arena::alloc_bottom(int64_t size)
{
	uintptr_t ret = align_forward(bottom_pointer);

	if (ret + size >= top_pointer) {
		sys_print(Error,"OUT OF MEMORY %s\n", debug_name);
		fflush(stdout);
		std::abort();
		return nullptr;
	}
	bottom_pointer = ret + size;
	return (void*)ret;
}