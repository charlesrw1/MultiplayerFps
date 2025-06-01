#pragma once
#include <cstdint>
#include "Framework/StringUtil.h"


typedef uint64_t name_hash_t;


#define NAME(x) StringName(x, std::integral_constant<name_hash_t,StringUtils::fnv1a_64(x, StringUtils::const_strlen(x))>())

class StringName
{
public:
	StringName() : hash(0) {}
	StringName(const char* name);
	StringName(name_hash_t hash) : hash(hash) {}
#ifndef EDITOR_BUILD
	StringName(const char* name, name_hash_t hash) : hash(hash) {}
#else
	StringName(const char* name, name_hash_t hash);
#endif
	StringName(const StringName& other) { hash = other.hash; }

	bool operator==(const StringName& other) const { return hash == other.hash; }
	bool operator!=(const StringName& other) const { return hash != other.hash; }
#ifdef EDITOR_BUILD
	const char* get_c_str() const;
#endif
	name_hash_t get_hash() const { return hash; }
	bool is_null() const {
		return hash == 0;
	}
private:
	name_hash_t hash = 0;
};