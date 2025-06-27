#pragma once
#include <vector>
template<typename T>
class RingBuffer
{
public:
    RingBuffer(int count = 0) :buf(count) {}
    bool empty() const {
        return size() == 0;
    }
    int size() const {
        return count;
    }
    // push to end
    void push_back(const T& item) {
        if (count >= buf.size()) {
            resize_internal_space(std::max(buf.size() * 2, 4ull));
        }
        count++;
        (*this)[size() - 1] = item;
    }
    // pop from start
    void pop_front() {
        assert(count > 0);
        start = get_buf_index(1);
        count--;
    }

    const T& operator[](int index) const {
        return buf.at(get_buf_index(index));
    }
    T& operator[](int index) {
        return buf.at(get_buf_index(index));
    }

    void resize_internal_space(int newcount) {
        std::vector<T> newbuf(newcount);
        for (int i = 0; i < count; i++) {
            newbuf[i] = std::move((*this)[i]);
        }
        start = 0;
        buf = std::move(newbuf);
    }
private:
    int get_buf_index(int i) const {
        //assert(i < count);
        return (start + i) % buf.size();
    }

    std::vector<T> buf;
    int start = 0;
    int count = 0;
};
