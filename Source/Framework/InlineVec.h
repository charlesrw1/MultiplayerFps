#pragma once

#include <cstdint>
#include <type_traits>
#include <cassert>
#include <memory>
#include "Framework/Util.h"
#include <span>

template<typename T, uint32_t INLINE_COUNT>
class InlineVec
{
public:
    using fixed_count_value = std::integral_constant<uint32_t, INLINE_COUNT>;

    using SIZE_TYPE = int;

    InlineVec();
    ~InlineVec();

    // fixme
    InlineVec& operator=(const InlineVec& other);
    InlineVec(const InlineVec& other);
    InlineVec& operator=(InlineVec&& other) = delete;
    InlineVec(InlineVec&& other) = delete;

    void reserve(SIZE_TYPE reserve_size);

    const T& operator[](SIZE_TYPE index) const {
        ASSERT(index < size());
        return data()[index];
    }
    T& operator[](SIZE_TYPE index) {
        ASSERT(index < size());
        return data()[index];
    }

    T* data() {
        return is_allocated_() ? heap.ptr : fixed.inline_;
    }
    const T* data() const {
        return is_allocated_() ? heap.ptr : fixed.inline_;
    }
    std::span<T> get_span() {
        return std::span<T>(data(), size());
    }
    std::span<const T> get_span() const {
        return std::span<const T>(data(), size());
    }


    SIZE_TYPE size() const { return count_ >> 1; }

    SIZE_TYPE fixed_count() { return (SIZE_TYPE)INLINE_COUNT; }
    void push_back(T&& val);

    void push_back(const T& val);

    void resize(SIZE_TYPE newcount) {
        resize(newcount, {});
    }
    void resize(SIZE_TYPE newcount, const T& value);
    void clear() {
        resize(0);
    }
private:
    void set_size_(SIZE_TYPE s) {
        count_ = (s << 1) | (count_ & 1);
    }
    void expand_capacity_(SIZE_TYPE capacity);

    void destroy_(T* start, int count);
    SIZE_TYPE capacity_() const { return is_allocated_() ? heap.allocated : INLINE_COUNT; }
    bool is_allocated_() const { return count_ & 1; }

    struct heap_t {
        T* ptr;
        SIZE_TYPE allocated;
    };
    struct fixed_t {
        T inline_[INLINE_COUNT];
    };
    union {
        fixed_t fixed;
        heap_t heap;
    };
    SIZE_TYPE count_ = 0;
};

template<typename T, uint32_t INLINE_COUNT>
inline InlineVec<T, INLINE_COUNT>::InlineVec(const InlineVec& other) {
    reserve(other.size());
    const T* other_start = other.data();
    const int other_size = other.size();
    T* my_start = data();

    for (int i = 0; i < other_size; i++) {
        T* my_ptr = &my_start[i];
        const T* other_ptr = &other_start[i];
        new(my_ptr)T(*other_ptr);
    }
    set_size_(other.size());
}

template<typename T, uint32_t INLINE_COUNT>
inline void InlineVec<T, INLINE_COUNT>::reserve(SIZE_TYPE reserve_size) {
    if (capacity_() < reserve_size) {
        expand_capacity_(reserve_size);
    }
}

template<typename T, uint32_t INLINE_COUNT>
inline void InlineVec<T, INLINE_COUNT>::push_back(T&& val) {
    SIZE_TYPE cap = capacity_();
    SIZE_TYPE curcount = size();
    if (curcount + 1 > cap) {
        // need to increase capacity
        int new_capacity = cap * 2;
        if (curcount + 1 > new_capacity)
            new_capacity = curcount + 1;
        expand_capacity_(new_capacity);
        assert(is_allocated_());
    }
    assert(curcount < capacity_());
    T* my_ptr = &(data()[curcount]);
    new(my_ptr)T(std::move(val));
    set_size_(curcount + 1);
}

template<typename T, uint32_t INLINE_COUNT>
inline void InlineVec<T, INLINE_COUNT>::push_back(const T& val) {
    SIZE_TYPE cap = capacity_();
    SIZE_TYPE curcount = size();
    if (curcount + 1 > cap) {
        // need to increase capacity
        int new_capacity = cap * 2;
        if (curcount + 1 > new_capacity)
            new_capacity = curcount + 1;
        expand_capacity_(new_capacity);
        assert(is_allocated_());
    }
    assert(curcount < capacity_());
    T* my_ptr = &(data()[curcount]);
    new(my_ptr)T(val);
    set_size_(curcount + 1);
}

template<typename T, uint32_t INLINE_COUNT>
inline void InlineVec<T, INLINE_COUNT>::resize(SIZE_TYPE newcount, const T& value) {
    if (newcount == size())
        return;

    SIZE_TYPE cap = capacity_();
    SIZE_TYPE curcount = size();
    if (curcount + 1 > cap) {
        // need to increase capacity
        int new_capacity = cap * 2;
        if (curcount + 1 > new_capacity)
            new_capacity = curcount + 1;
        expand_capacity_(new_capacity);
        assert(is_allocated_());
    }


    if (newcount > size()) {
        T* start_ptr = data();
        for (int i = curcount; i < newcount; i++) {
            T* my_ptr = &start_ptr[i];
            new(my_ptr)T(value);
        }
    }
    else {
        SIZE_TYPE newend = newcount;
        T* start = data() + newend;
        const int count = size() - newend;
        assert(count >= 0);
        destroy_(start, count);
    }

    set_size_(newcount);
}

template<typename T, uint32_t INLINE_COUNT>
inline void InlineVec<T, INLINE_COUNT>::expand_capacity_(SIZE_TYPE capacity) {

    T* const new_ptr = (T*)std::malloc(capacity * sizeof(T));
    T* const cur_start_ptr = data();
    const int cur_data_count = size();
    assert(capacity >= cur_data_count);

    if constexpr (std::is_trivially_copyable<T>::value) {
        std::memcpy(new_ptr, cur_start_ptr, cur_data_count * sizeof(T));
    }
    else {
        for (int i = 0; i < cur_data_count; i++) {
            T* my_ptr = &cur_start_ptr[i];
            T* other_ptr = &new_ptr[i];
            new(other_ptr)T(std::move(*my_ptr));
        }
    }

    destroy_(cur_start_ptr, cur_data_count);

    heap.ptr = new_ptr;
    heap.allocated = capacity;

    if (is_allocated_())
        std::free(cur_start_ptr);   // dont call destructor, we just did

    count_ |= 1;    // sets allocated bit
    assert(is_allocated_());
}

template<typename T, uint32_t INLINE_COUNT>
inline void InlineVec<T, INLINE_COUNT>::destroy_(T* start, int count) {
    if constexpr (!std::is_trivially_destructible<T>()) {
        std::destroy(start, start + count);
    }
}
template<typename T, uint32_t INLINE_COUNT>
inline InlineVec<T, INLINE_COUNT>::InlineVec() {
}
template<typename T, uint32_t INLINE_COUNT>
inline InlineVec<T, INLINE_COUNT>::~InlineVec() {
    destroy_(data(), size());
}
template<typename T, uint32_t INLINE_COUNT>
inline InlineVec<T, INLINE_COUNT>& InlineVec<T,INLINE_COUNT>::operator=(const InlineVec<T, INLINE_COUNT>& other) {
    destroy_(data(), size());
    const T* const other_ptr_start = other.data();
    const int other_size = other.size();
    reserve(other_size);
    T* const my_start = data();
    for (int i = 0; i < other_size; i++) {
        T* my_ptr = &my_start[i];
        const T* other_ptr = &other_ptr_start[i];
        new(my_ptr)T(*other_ptr);
    }
    set_size_(other_size);
    return *this;
}