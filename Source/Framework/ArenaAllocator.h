#pragma once
#include "MemArena.h"

#include <type_traits>

struct ArenaScope
{
    enum Type {
        TOP,
        BOTTOM
    };
    ArenaScope(Memory_Arena& arena, Type where = BOTTOM) :where(where), arena(arena) {
        if (where==BOTTOM)
            marker = arena.get_bottom_marker();
        else
            marker = arena.get_top_marker();
    }
    ~ArenaScope() {
        if (where==BOTTOM)
            arena.free_bottom_to_marker(marker);
        else
            arena.free_top_to_marker(marker);
    }
    ArenaScope(const ArenaScope& other) = delete;
    ArenaScope(ArenaScope&& other) = delete;
    ArenaScope& operator=(const ArenaScope& other) = delete;

    void* alloc(size_t size) {
        if (where==BOTTOM)
            return arena.alloc_bottom(size);
        else
            return arena.alloc_top(size);
    }
private:
    const Type where = Type::BOTTOM;
    uintptr_t marker = 0;
    Memory_Arena& arena;
};

template<typename T>
class ArenaAllocator {
public:
    using value_type = T;

    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind {
        using other = ArenaAllocator<U>;
    };

    ArenaAllocator(ArenaScope& arena) noexcept : arena_(&arena) {}

    template<typename U>
    ArenaAllocator(const ArenaAllocator<U>& other) noexcept : arena_(other.arena_) {}

    T* allocate(std::size_t n) {
        return static_cast<T*>(arena_->alloc(n * sizeof(T)));
    }

    void deallocate(T* /*p*/, std::size_t /*n*/) noexcept {
        // no-op
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new ((void*)p) U(std::forward<Args>(args)...);
    }

    template<typename U>
    void destroy(U* p) {
        p->~U();
    }

    bool operator==(const ArenaAllocator& other) const noexcept {
        return arena_ == other.arena_;
    }

    bool operator!=(const ArenaAllocator& other) const noexcept {
        return !(*this == other);
    }

    ArenaScope* arena_;
};

// Make Memory_Arena pointer accessible to other template instantiations
template<typename T, typename U>
inline bool operator==(const ArenaAllocator<T>& a, const ArenaAllocator<U>& b) {
    return a.arena_ == b.arena_;
}

template<typename T, typename U>
inline bool operator!=(const ArenaAllocator<T>& a, const ArenaAllocator<U>& b) {
    return !(a == b);
}