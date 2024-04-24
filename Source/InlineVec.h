#pragma once


template<typename T, int INLINE_COUNT>
class InlineVec
{
public:
	InlineVec() {
	}
	~InlineVec() {
		if (count <= INLINE_COUNT)
			delete_inline();
	}

	const T& operator[](int index) const {
		if (count > INLINE_COUNT) {
			assert(index < count);
			return heap[index];
		}
		else
			return inline_[index];
	}
	T& operator[](int index) {
		if (count > INLINE_COUNT) {
			assert(index < count);
			return heap[index];
		}
		else
			return inline_[index];
	}
	void assign_memory(T* t, int count) {
		ASSERT(count > INLINE_COUNT);
		ASSERT(this->count <= INLINE_COUNT);
		delete_inline();
		heap = t;
		this->count = count;
	}
	int size() const {
		return count;
	}
	void delete_inline() {
		for (int i = 0; i < INLINE_COUNT; i++)
			inline_[i].~T();
	}

	union {
		T inline_[INLINE_COUNT];
		T* heap;
	};
	int count = 0;
};
