#include "GpuAllocator.h"
#include <algorithm>
#include "Framework/Util.h"
void gpuSpanAllocator::init_clear(int bytes)
{
    if (initialized_size!=0) {
        sys_print(Warning, "gpuSpanAllocator free spans being cleared...\n");
    }
    gpuAllocSpan main_;
    main_.start = 0;
    main_.size = bytes;
    freeSpans.clear();
    freeSpans.push_back({ main_ });
    this->initialized_size = bytes;
}

gpuAllocSpan gpuSpanAllocator::allocate(int bytes) {
    for (auto it = freeSpans.begin(); it != freeSpans.end(); ++it) {
        if (it->s.size >= bytes) {
            gpuAllocSpan result = { it->s.start, bytes };

            if (it->s.size > bytes) {
                it->s.start += bytes;
                it->s.size -= bytes;
            }
            else {
                freeSpans.erase(it);
            }

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

    SpanAndFree newSpan{ span };

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
    if (next != freeSpans.end() &&
        it->s.start + it->s.size == next->s.start) {
        it->s.size += next->s.size;
        freeSpans.erase(next);
    }
}
