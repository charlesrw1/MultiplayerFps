#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include "Framework/Util.h"
#include <mutex>

template<typename T>
class Pool_Allocator;
template<typename T>
struct ScopedPoolPtr
{
	ScopedPoolPtr(T* ptr, Pool_Allocator<T>* parent) : ptr(ptr),parent(parent) {}
	~ScopedPoolPtr();
	ScopedPoolPtr(const ScopedPoolPtr&) = delete;
	ScopedPoolPtr& operator=(const ScopedPoolPtr&) = delete;
	ScopedPoolPtr(ScopedPoolPtr&& other) {
		this->ptr = other.ptr;
		this->parent = other.parent;
		other.ptr = nullptr;
		other.parent = nullptr;
	}

	T* get() const {
		return ptr;
	}
	T& operator*() const {
		assert(ptr);
		return *ptr;
	}
	Pool_Allocator<T>& get_parent() const {
		assert(parent);
		return *parent;
	}
private:
	T* ptr = nullptr;
	Pool_Allocator<T>* parent = nullptr;
};

// doesnt construct/destruct the object
template<typename T>
class Pool_Allocator
{
public:
	Pool_Allocator(int num_objs, const char* debug_name = "") : debug_name(debug_name) {
		obj_size = sizeof(T);
		
		if (sizeof(T) % 8 != 0)
			obj_size += 8 - (obj_size % 8);

		allocated_size = obj_size * num_objs;
		memory = new uint8_t[allocated_size];
		memset(memory, 0, allocated_size);
		// set next ptrs
		for (int i = 0; i < num_objs - 1; i++) {
			int offset = obj_size * i;
			uint8_t** ptr = (uint8_t**)(memory + offset);
			uint8_t* next_ptr = (memory + uint64_t(obj_size) * (uint64_t(i) + 1));
			*ptr = next_ptr;
		}
		first_free = memory;
	}
	~Pool_Allocator() {
		delete memory;
	}

	T* allocate() {
		std::lock_guard<std::mutex> lock(mutex);

		if (first_free == nullptr) {
			Fatalf("memory pool (%s) full\n",debug_name);
		}
		uint8_t* next = *((uint8_t**)first_free);
		uint8_t* ret = first_free;
		first_free = next;
		used_objects++;
		return (T*)ret;
	}
	ScopedPoolPtr<T> allocate_scoped() {
		return ScopedPoolPtr<T>(allocate(), this);
	}
	void free(T* ptr) {
		if (ptr == nullptr) return;

		std::lock_guard<std::mutex> lock(mutex);

		assert((void*)ptr >= memory && (void*)ptr < memory + allocated_size);
		uint8_t** next_ptr = (uint8_t**)ptr;
		*next_ptr = first_free;
		first_free = (uint8_t*)ptr;
		used_objects--;
	}

	int obj_size = 0;
	int allocated_size = 0;
	uint8_t* memory = nullptr;

	uint8_t* first_free = nullptr;
	const char* debug_name = "";
	// debugging
	int used_objects = 0;

	std::mutex mutex;
};

template<typename T>
inline ScopedPoolPtr<T>::~ScopedPoolPtr()
{
	if (parent)
		parent->free(ptr);
}