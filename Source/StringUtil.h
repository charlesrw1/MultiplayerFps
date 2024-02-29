#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// from vkguide
namespace StringUtils {

	// FNV-1a 32bit hashing algorithm.
	constexpr uint32_t fnv1a_32(char const* s, std::size_t count)
	{
		return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
	}

	constexpr size_t const_strlen(const char* s)
	{
		size_t size = 0;
		while (s[size]) { size++; };
		return size;
	}

	struct StringHash
	{
		uint32_t computedHash;

		constexpr StringHash(uint32_t hash) noexcept : computedHash(hash) {}

		constexpr StringHash(const char* s) noexcept : computedHash(0)
		{
			computedHash = fnv1a_32(s, const_strlen(s));
		}
		constexpr StringHash(const char* s, std::size_t count)noexcept : computedHash(0)
		{
			computedHash = fnv1a_32(s, count);
		}
		StringHash(const StringHash& other) = default;
	};

}

template<int BUFSIZE>
class Stack_String
{
public:
	Stack_String() {
		data_len = 0;
		data[0] = 0;
	}
	Stack_String(const char* str) {
		int s = strlen(str);
		if (s + 1 < BUFSIZE) {
			memcpy(data, str, s);
			data[s] = 0;
			data_len = s;
		}
	}
	Stack_String(const char* str, int len) {
		int s = strlen(str);
		if (len < s) s = len;
		if (s + 1 < BUFSIZE) {
			memcpy(data, str, s);
			data[s] = 0;
			data_len = s;
		}
	}
	int size() { return data_len; }
	const char* c_str() { return data; }
private:
	int data_len = 0;
	char data[BUFSIZE];
};