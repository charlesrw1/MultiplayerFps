#pragma once
#include <vector>

class Std_Allocator
{
public:
	void* allocate(size_t bytes) {
		return std::malloc(bytes);
	}
	void deallocate(void* p) {
		std::free(p);
	}
};

// simple heap array that makes use of custom allocator
template<typename T, typename ALLOC = Std_Allocator>
class Heap_Array
{
	using SIZETYPE = uint32_t;
public:
	Heap_Array(ALLOC a = ALLOC()) : alloc(a) {}
	Heap_Array(SIZETYPE n, ALLOC a = ALLOC()) : alloc(a) {
		set_size(n);
	}
	Heap_Array(const Heap_Array& other, ALLOC a = ALLOC()) : alloc(a) {
		T* new_data = (T*)alloc.allocate(other.allocated);
		memcpy(new_data, other.data, other.allocated * sizeof(T));
		if (data) {
			alloc.deallocate(data);
		}

		data = new_data;
		allocated = other.allocated;
	}
	~Heap_Array() {
		alloc.deallocate(data);
	}
	Heap_Array& operator=(const Heap_Array& other) {
		T* new_data = (T*)alloc.allocate(other.allocated);
		memcpy(new_data, other.data, other.allocated * sizeof(T));
		if (data) {
			alloc.deallocate(data);
		}

		data = new_data;
		allocated = other.allocated;

		return *this;
	}

	void free_ptr() {
		alloc.deallocate(data);
		data = nullptr;
		allocated = 0;
	}

	void set_size(SIZETYPE newsize) {
		if (newsize != allocated) {
			alloc.deallocate(data);
			data = (T*)alloc.allocate(sizeof(T) * newsize);
			allocated = newsize;
		}
	}
	T& at(SIZETYPE index) {
		return data[index];
	}

	T* get_ptr() { return data; }
	SIZETYPE size() const { return allocated; }
private:
	T* data = nullptr;
	SIZETYPE allocated = 0;
	ALLOC alloc;
};