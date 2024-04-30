#pragma once

#include <cstdint>
#include <type_traits>
#include <cassert>
#include <memory>
template<typename T, uint32_t INLINE_COUNT>
class InlineVec
{
public:
    using fixed_count_value = std::integral_constant<uint32_t, INLINE_COUNT>;

    using SIZE_TYPE = size_t;

    InlineVec() {
    }
    ~InlineVec() {
        destroy_(data(), data() + size());
    }

    // fixme
    InlineVec& operator=(const InlineVec& other)  {
        destroy_(data(), data() + size());
        reserve(other.size());
        const T* ptr = other.data();
        const T* end = other.data() + other.size();
        T* my_start = data();
        for (; ptr != end; ptr++, my_start++)
            new(my_start)T(*ptr);
        return *this;
    }
    InlineVec(const InlineVec& other) {
        destroy_(data(), data() + size());
        reserve(other.size());
        const T* ptr = other.data();
        const T* end = other.data() + other.size();
        T* my_start = data();
        for (; ptr != end; ptr++, my_start++)
            new(my_start)T(*ptr);
    }
    InlineVec& operator=(InlineVec&& other) = delete;
    InlineVec(InlineVec&& other) = delete;

    void reserve(SIZE_TYPE reserve_size) {
        if (capacity_() < reserve_size) {
            expand_capacity_(reserve_size);
        }
    }

    const T& operator[](SIZE_TYPE index) const {
        assert(index < size());
        return data()[index];
    }
    T& operator[](SIZE_TYPE index) {
        assert(index < size());
        return data()[index];
    }

    T* data() {
        return is_allocated_() ? heap.ptr : fixed.inline_;
    }
    const T* data() const {
        return is_allocated_() ? heap.ptr : fixed.inline_;
    }
    SIZE_TYPE size() const { return count_ >> 1; }

    SIZE_TYPE fixed_count() { return (SIZE_TYPE)INLINE_COUNT; }
    void push_back(T&& val) {
        SIZE_TYPE cap = capacity_();
        SIZE_TYPE curcount = size();
        if (curcount + 1 > cap)
            expand_capacity_((curcount + 1 > cap * 2) ? curcount + 1 : cap * 2);
        new(data() + curcount)T(std::move(val));

        set_size_(curcount + 1);
    }

    void resize(SIZE_TYPE newcount) {
        resize(newcount, {});
    }
    void resize(SIZE_TYPE newcount, const T& value) {
        if (newcount == size()) return;

        SIZE_TYPE cap = capacity_();
        SIZE_TYPE curcount = size();
        if (newcount > cap)
            expand_capacity_((newcount > cap * 2) ? newcount : cap * 2);


        if (newcount > size()) {
            T* start = data() + curcount;
            T* end = data() + newcount;

            for (; start != end; start++)
                new(start)T(value);
        }
        else {
            SIZE_TYPE newend = newcount;
            T* end = data() + curcount;
            T* start = data() + newcount;
            destroy_(start, end);
        }

        set_size_(newcount);
    }
private:
    void set_size_(SIZE_TYPE s) {
        count_ = (s << 1) | (count_ & 1);
    }
    void expand_capacity_(SIZE_TYPE capacity) {
        T* new_p = (T*)std::malloc(capacity * sizeof(T));
        T* cur_p = data();
        T* end_cur_p = cur_p + size();

        if (std::is_trivially_copy_constructible<T>()) {
            std::copy(cur_p, end_cur_p, new_p); // undefined behavior maybe??
        }
        else {
            T* ptr = cur_p;
            T* new_ptr = new_p;
            for (; ptr != end_cur_p; ptr++, new_ptr++) {
                new(new_ptr)T(std::move(*ptr));
            }
        }

        destroy_(cur_p, end_cur_p);

        heap.ptr = new_p;
        heap.allocated = capacity;

        if (is_allocated_())
            std::free(cur_p);

        count_ |= 1;
    }

    void destroy_(T* start, T* end) {
        if constexpr (!std::is_trivially_destructible<T>()) {
            for (; start != end; start++) {
                start->~T();
            }
        }
    }
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