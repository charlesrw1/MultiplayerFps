#pragma once
#include <vector>

template<typename T>
class hash_map;

template<typename T>
struct hash_map_iterator
{
    hash_map_iterator(hash_map<T>* p, uint64_t i);
    hash_map_iterator(uint64_t i) : index(i) {}

    bool operator!=(const hash_map_iterator& other) {
        return index < other.index;
    }
    T* operator*();
    hash_map_iterator<T>& operator++() {
        index++;
        advance();
        return *this;
    }

    void advance();

    uint64_t index = 0;
    hash_map<T>* parent = nullptr;
};

template<typename T>
class hash_map
{
public:
    static const uint64_t INVALID_HANDLE = 0;
    static const uint64_t TOMBSTONE = UINT64_MAX;

    hash_map_iterator<T> begin() {
        return hash_map_iterator<T>(this, 0);
    }
    hash_map_iterator<T> end() {
        return hash_map_iterator<T>(items.size());
    }

    hash_map(uint32_t starting_power_of_2) {
        const uint64_t SIZE = 1ull << starting_power_of_2;
        items.clear();
        items.resize(SIZE);
        mask = SIZE - 1;
        num_tombstones = 0;
        num_used = 0;
    }

    void init(uint32_t starting_power_of_2 = 10) {
        const uint64_t SIZE = 1ull << starting_power_of_2;
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

    T* find(uint64_t handle) const {
        if (handle == 0) return nullptr;

        uint64_t index = std::hash<uint64_t>()(handle);
        index = index & mask;
        uint64_t count = items.size();
        for (uint64_t i = 0; i < count; i++) {
            uint64_t actual_index = (index + i) & mask;
            if (items[actual_index].handle == handle)
                return items[actual_index].ptr;
            else if (items[actual_index].handle == 0)
                return nullptr;
        }
        return nullptr;
    }
    void remove(uint64_t handle) {
        if (handle == 0) return;

        uint64_t index = std::hash<uint64_t>()(handle);
        index = index & mask;
        uint64_t count = items.size();
        for (uint64_t i = 0; i < count; i++) {
            uint64_t actual_index = (index + i) & mask;
            if (items[actual_index].handle == handle) {
                items[actual_index].ptr = nullptr;
                items[actual_index].handle = TOMBSTONE;
                num_used--;
                num_tombstones++;
                return;
            }
            else if (items[actual_index].handle == 0)
                return;
        }
    }
    void insert(uint64_t handle, T* ptr) {
        assert(handle != 0);
        check_to_rehash(1);
        uint64_t index = std::hash<uint64_t>()(handle);
        index = index & mask;
        uint64_t count = items.size();
        for (uint64_t i = 0; i < count; i++) {
            uint64_t actual_index = (index + i) & mask;
            if (items[actual_index].handle == handle || items[actual_index].handle == 0 || items[actual_index].handle == TOMBSTONE) {

                num_tombstones -= items[actual_index].handle == TOMBSTONE;
                num_used += items[actual_index].handle != handle;

                items[actual_index].ptr = ptr;
                items[actual_index].handle = handle;
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
            if (item.ptr != nullptr)
                insert(item.handle, item.ptr);
        }
    }

    struct item {
        uint64_t handle = 0;
        T* ptr = nullptr;
    };

    friend class hash_map_iterator<T>;

    uint64_t mask = 0;
    uint64_t num_tombstones = 0;
    uint64_t num_used = 0;
    std::vector<item> items;
};

template<typename T>
inline T* hash_map_iterator<T>::operator*() {
    return parent->items[index].ptr;
}


template<typename T>
inline hash_map_iterator<T>::hash_map_iterator(hash_map<T>* parent, uint64_t i) {
    this->parent = parent;
    index = i;
    advance();
}

template<typename T>
inline void hash_map_iterator<T>::advance() {
    for (; index < parent->items.size(); index++) {
        if (parent->items[index].handle != 0 && parent->items[index].handle != hash_map<T>::TOMBSTONE)
            break;
    }
}

