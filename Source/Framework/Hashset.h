#pragma once
#include <vector>
#include <cassert>
template<typename T>
class hash_set;

template<typename T>
struct hash_set_iterator
{
    hash_set_iterator(hash_set<T>* p, uint64_t i);
    hash_set_iterator(uint64_t i) : index(i) {}

    bool operator!=(const hash_set_iterator& other) {
        return index < other.index;
    }
    T* operator*();
    hash_set_iterator<T>& operator++() {
        index++;
        advance();
        return *this;
    }

    void advance();

    uint64_t index = 0;
    hash_set<T>* parent = nullptr;
};

extern const uint64_t HASHSET_TOMBSTONE;
template<typename T>
class hash_set
{
public:
    static const uint64_t INVALID_HANDLE = 0;

    hash_set_iterator<T> begin() {
        return hash_set_iterator<T>(this, 0);
    }
    hash_set_iterator<T> end() {
        return hash_set_iterator<T>(items.size());
    }

    hash_set(uint32_t starting_power_of_2) {
        const uint64_t SIZE = 1 << starting_power_of_2;
        items.clear();
        items.resize(SIZE);
        mask = SIZE - 1;
        num_tombstones = 0;
        num_used = 0;
    }

    void init(uint32_t starting_power_of_2 = 10) {
        const uint64_t SIZE = 1 << starting_power_of_2;
        items.clear();
        items.resize(SIZE);
        mask = SIZE - 1;
        num_tombstones = 0;
        num_used = 0;
    }

    void check_to_rehash(uint64_t new_elements_to_insert = 0) {
        if (num_tombstones + num_used + new_elements_to_insert >= (items.size() >> 1) /* half of size */) {
            rehash(items.size() << 1);
        }
    }

    T* find(T* ptr) const {
        if (ptr == nullptr) return nullptr;

        uint64_t index = std::hash<uint64_t>()((uint64_t)ptr);
        index = index & mask;
        uint64_t count = items.size();
        for (uint64_t i = 0; i < count; i++) {
            uint64_t actual_index = (index + i) & mask;
            if (items[actual_index].ptr == ptr)
                return ptr;
            else if (items[actual_index].ptr == nullptr)
                return nullptr;
        }
        return nullptr;
    }
    void remove(T* ptr) {
        if (ptr == nullptr) return;

        uint64_t index = std::hash<uint64_t>()((uint64_t)ptr);
        index = index & mask;
        uint64_t count = items.size();
        for (uint64_t i = 0; i < count; i++) {
            uint64_t actual_index = (index + i) & mask;
            if (items[actual_index].ptr == ptr) {
                items[actual_index].ptr = nullptr;
                items[actual_index].ptr = (T*)HASHSET_TOMBSTONE;
                num_used--;
                num_tombstones++;
                return;
            }
            else if (items[actual_index].ptr == nullptr)
                return;
        }
    }
    void insert(T* ptr) {
        assert(ptr != nullptr);
        check_to_rehash(1);
        uint64_t index = std::hash<uint64_t>()((uint64_t)ptr);
        index = index & mask;
        uint64_t count = items.size();
        for (uint64_t i = 0; i < count; i++) {
            uint64_t actual_index = (index + i) & mask;
            if (items[actual_index].ptr == ptr || items[actual_index].ptr == nullptr || items[actual_index].ptr == (T*)HASHSET_TOMBSTONE) {

                num_tombstones -= items[actual_index].ptr == (T*)HASHSET_TOMBSTONE;
                num_used += items[actual_index].ptr == nullptr;

                items[actual_index].ptr = ptr;
                return;
            }
        }
        assert(!"hash table didnt update size");
    }
    void clear_all() {
        uint64_t count = items.size();
        items.clear();
        items.resize(count);
        num_used = 0;
        num_tombstones = 0;
    }

    void rehash(uint64_t next_size) {
        mask = next_size - 1;
        std::vector<item> prev_array = std::move(items);
        items.resize(next_size);
        num_tombstones = 0;
        num_used = 0;
        for (auto& item : prev_array) {
            if (item.ptr != nullptr && item.ptr != (T*)HASHSET_TOMBSTONE)
                insert(item.ptr);
        }
    }

    struct item {
        T* ptr = nullptr;
    };

    friend class hash_set_iterator<T>;

    uint64_t mask = 0;
    uint64_t num_tombstones = 0;
    uint64_t num_used = 0;
    std::vector<item> items;
};

template<typename T>
inline T* hash_set_iterator<T>::operator*() {
    return parent->items[index].ptr;
}


template<typename T>
inline hash_set_iterator<T>::hash_set_iterator(hash_set<T>* parent, uint64_t i) {
    this->parent = parent;
    index = i;
    advance();
}

template<typename T>
inline void hash_set_iterator<T>::advance() {
    for (; index < parent->items.size(); index++) {
        if (parent->items[index].ptr != nullptr && parent->items[index].ptr != (T*)HASHSET_TOMBSTONE)
            break;
    }
}

