#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <cstdlib>

class Pool_Allocator
{
public:
	Pool_Allocator(int obj_size, int num_objs) {
		if (obj_size % 8 != 0)
			obj_size += 8 - (obj_size % 8);

		this->obj_size = obj_size;
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

	void* allocate() {
		if (first_free == nullptr) {
			printf("memory pool full\n");
			std::abort();
		}
		uint8_t* next = *((uint8_t**)first_free);
		uint8_t* ret = first_free;
		first_free = next;
		used_objects++;
		return ret;
	}
	void free(void* ptr) {
		assert(ptr >= memory && ptr < memory + allocated_size);
		uint8_t** next_ptr = (uint8_t**)ptr;
		*next_ptr = first_free;
		first_free = (uint8_t*)ptr;
		used_objects--;
	}

	int obj_size = 0;
	int allocated_size = 0;
	uint8_t* memory = nullptr;

	uint8_t* first_free = nullptr;

	// debugging
	int used_objects = 0;
};