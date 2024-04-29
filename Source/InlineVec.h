#pragma once


template<typename T, uint16_t INLINE_COUNT>
class InlineVec
{
public:
	InlineVec() {
	}
	~InlineVec() {
		if (allocated <= INLINE_COUNT)
			delete_inline();
	}

	const T& operator[](int index) const {
		if (allocated > INLINE_COUNT) {
			assert(index < count);
			return heap[index];
		}
		else
			return inline_[index];
	}
	T& operator[](int index) {
		if (allocated > INLINE_COUNT) {
			assert(index < count);
			return heap[index];
		}
		else
			return inline_[index];
	}
	void assign_memory(T* t, uint16_t count) {
		ASSERT(count > INLINE_COUNT);
		ASSERT(this->count <= INLINE_COUNT);
		delete_inline();
		heap = t;
		this->allocated = count;
		this->count = count;
	}
	int num_allocated() { return allocated; }
	int num_used() { return count; }

	void delete_inline() {
		for (int i = 0; i < INLINE_COUNT; i++)
			inline_[i].~T();
	}

	union {
		T inline_[INLINE_COUNT];
		T* heap;
	};
	uint16_t allocated = INLINE_COUNT;
	uint16_t count = 0;
};
