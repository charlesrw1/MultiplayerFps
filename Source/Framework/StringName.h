#pragma once
#include <cstdint>
#include "Framework/StringUtil.h"

typedef uint64_t name_hash_t;

#define NAME(x)                                                                                                        \
	StringName(                                                                                                        \
		x, std::integral_constant<name_hash_t, StringUtils_Hash::fnv1a_64(x, StringUtils_Hash::const_strlen(x))>())

class StringName
{
public:
	StringName() : hash(0) {}
	StringName(const char* name);
	explicit StringName(name_hash_t hash) : hash(hash) {}
	StringName(const char* name, name_hash_t hash);

	// Hashes [str,str+len) directly with no intermediate std::string allocation and registers
	// the debug name. str must be null-terminated at str[len] (guaranteed by lua_tolstring).
	// Used at the Lua/C++ boundary to avoid re-hashing through a temporary std::string on
	// every call (see get_stringname_from_lua in ScriptFunctionCodegen.cpp).
	static StringName intern(const char* str, size_t len);

	StringName(const StringName& other) { hash = other.hash; }
	StringName& operator=(const StringName& other) {
		hash = other.hash;
		return *this;
	}

	bool operator==(const StringName& other) const { return hash == other.hash; }
	bool operator!=(const StringName& other) const { return hash != other.hash; }
	const char* get_c_str() const;
#ifdef EDITOR_BUILD
#endif
	name_hash_t get_hash() const { return hash; }
	bool is_null() const { return hash == 0; }

private:
	name_hash_t hash = 0;
};