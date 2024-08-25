#pragma once
#include <cstdint>
struct Guid
{
	uint64_t a=0;
	uint64_t b=0;

    bool is_valid() const {
        return !(a == 0 && b == 0);
    }
};

static_assert(sizeof(Guid) == 16, "guid wrong size");

// The compare operator is required by std::unordered_map
inline bool operator == (const Guid& a, const Guid& b) {
    return std::memcmp(&a, &b, sizeof(Guid)) == 0;
}

// Specialize std::hash
namespace std {
    template<> struct hash<Guid>
    {
        size_t operator()(const Guid& guid) const noexcept {
            const std::uint64_t* p = reinterpret_cast<const std::uint64_t*>(&guid);
            std::hash<std::uint64_t> hash;
            return hash(p[0]) ^ hash(p[1]);
        }
    };
}