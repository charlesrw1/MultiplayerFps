#pragma once
#include <vector>
// [static vertex]
// [streaming vertex]

// [static material]
// [streaming material]

struct gpuAllocSpan
{
	int start = 0;
	int size = 0;
	int aligned_start = 0;
};

class gpuSpanAllocator
{
public:
	void init_clear(int bytes);
	gpuAllocSpan allocate(int bytes, int align_to_size);
	void free(gpuAllocSpan span);
	int get_init_sized() const { return initialized_size; }

private:
	struct SpanAndFree
	{
		gpuAllocSpan s;
	};
	void merge_with_neighbors(std::vector<SpanAndFree>::iterator it);
	int initialized_size = 0;
	std::vector<SpanAndFree> freeSpans;
};
