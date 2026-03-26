#include "GpuAllocator.h"
#include <algorithm>
#include "Framework/Util.h"
void gpuSpanAllocator::init_clear(int bytes) {
	if (initialized_size != 0) {
		sys_print(Warning, "gpuSpanAllocator free spans being cleared...\n");
	}
	gpuAllocSpan main_;
	main_.start = 0;
	main_.size = bytes;
	freeSpans.clear();
	freeSpans.push_back({main_});
	this->initialized_size = bytes;
}

static inline size_t round_up(size_t size, size_t align) {
	return (size + align - 1) & ~(align - 1);
}

static inline size_t normalize_alloc_size(size_t size) {
	const size_t PAGE = 2048;

	if (size <= PAGE)
		return round_up(size, 256);

	return round_up(size, PAGE);
}

gpuAllocSpan gpuSpanAllocator::allocate(int bytes, int align_to_size) {
	bytes += align_to_size; // slightly wasteful
	// normalize to big stuff
	bytes = (int)normalize_alloc_size(bytes);

	for (auto it = freeSpans.begin(); it != freeSpans.end(); ++it) {
		if (it->s.size >= bytes) {
			gpuAllocSpan result = {it->s.start, bytes};

			if (it->s.size > bytes) {
				it->s.start += bytes;
				it->s.size -= bytes;
			} else {
				freeSpans.erase(it);
			}
			result.aligned_start = result.start;
			const int modulo = result.start % align_to_size;
			if (modulo != 0)
				result.aligned_start += align_to_size - modulo;

			return result;
		}
	}

	return {}; // size==0, out of mem
}
#include <cassert>
void gpuSpanAllocator::free(gpuAllocSpan span) {
	if (span.start == 0)
		return;
	assert(span.size != 0);

	SpanAndFree newSpan{span};

	auto it = std::lower_bound(freeSpans.begin(), freeSpans.end(), newSpan,
							   [](const SpanAndFree& a, const SpanAndFree& b) { return a.s.start < b.s.start; });

	it = freeSpans.insert(it, newSpan);

	merge_with_neighbors(it);
}

void gpuSpanAllocator::merge_with_neighbors(std::vector<SpanAndFree>::iterator it) {
	if (it != freeSpans.begin()) {
		auto prev = std::prev(it);
		if (prev->s.start + prev->s.size == it->s.start) {
			prev->s.size += it->s.size;
			it = freeSpans.erase(it);
		}
	}
	auto next = std::next(it);
	if (next != freeSpans.end() && it->s.start + it->s.size == next->s.start) {
		it->s.size += next->s.size;
		freeSpans.erase(next);
	}
}
