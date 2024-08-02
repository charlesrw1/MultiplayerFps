#pragma once
#include <unordered_map>
#include "Framework/StringUtil.h"
#include "ReflectionProp.h"
struct StringViewHasher
{
    std::size_t operator()(const StringView& k) const
    {
        constexpr uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325;
        constexpr uint64_t FNV_PRIME = 0x100000001b3;
        uint64_t hash = FNV_OFFSET_BASIS;
        const char* str = k.str_start;
        while (*str) {
            hash ^= static_cast<uint64_t>(*str++);
            hash *= FNV_PRIME;
        }
        return hash;
    }
};


struct PropHashTable
{
    std::unordered_map<StringView, PropertyInfo*, StringViewHasher> prop_table;
};