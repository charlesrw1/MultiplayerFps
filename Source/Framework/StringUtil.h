#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#ifdef _DEBUG
#include <cassert>
#endif

// from vkguide
namespace StringUtils {

	// FNV-1a 32bit hashing algorithm.
	constexpr uint32_t fnv1a_32(char const* s, std::size_t count)
	{
		return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
	}
	constexpr uint64_t fnv1a_64(const char* s, std::size_t count)
	{
		return ((count ? fnv1a_64(s, count - 1) : 14695981039346656037ull /* offset */) ^ s[count]) * 1099511628211ull /* prime */;
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

	struct StringHash2
	{
		uint32_t computedHash=0;
		const char* str = "";
		constexpr StringHash2(const char* s) noexcept : computedHash(0)
		{
			computedHash = fnv1a_32(s, const_strlen(s));
			str = s;
		}
	
		StringHash2(const StringHash2& other) = default;
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
		data[0] = 0;
		data_len = 0;

		int s = strlen(str);
		if (len < s) s = len;
		if (s + 1 < BUFSIZE) {
			memcpy(data, str, s);
			data[s] = 0;
			data_len = s;
		}
	}
	int size() { return data_len; }
	const char* c_str() const { return data; }

	char* get_data() { return data; }
private:
	int data_len = 0;
	char data[BUFSIZE];
};

class StringView
{
public:
	StringView() {}
	StringView(const char* str) {
		str_start = str;
		str_len = strlen(str);
	}
	StringView(const char* str, int len) {
		str_start = str;
		str_len = len;

#ifdef _DEBUG
		for (int i = 0; i < len; i++) {
			assert(str_start[i] != '\0');
		}
#endif // _DEBUG

	}

	bool cmp(const char* other) const {
		if (!str_start) return false;

		const char* p1 = str_start;
		int count = 0;
		while (*other) {
			if (count > str_len) return false;
			if (*p1 != *other) return false;
			p1++;
			other++;
			count++;
		}
		return count == str_len;
	}

	Stack_String<256> to_stack_string() {
		return Stack_String<256>(str_start, str_len);
	}
	bool is_empty() const { return str_len == 0; }

	bool operator==(const StringView& other) const {
		if (str_len != other.str_len) {
			return false;
		}
		return std::memcmp(str_start, other.str_start, str_len) == 0;
	}

	const char* str_start = nullptr;
	int str_len = 0;
};
