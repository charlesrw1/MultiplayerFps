#pragma once
#include "ArenaAllocator.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

using arena_string = std::basic_string<char, std::char_traits<char>, ArenaAllocator<char>>;

template<typename T>
using arena_vec = std::vector<T, ArenaAllocator<T>>;

template<typename Key, typename T>
using arena_map = std::unordered_map<Key, T, std::hash<Key>, std::equal_to<Key>, ArenaAllocator<std::pair<const Key, T>>>;

template<typename Key>
using arena_set = std::unordered_set<Key, std::hash<Key>, std::equal_to<Key>, ArenaAllocator<Key>>;