#include "StringName.h"

#include <unordered_map>

#ifdef DEBUG_STRING_NAME
static std::unordered_map<uint64_t, std::string> g_name_map;

static void add_to_nametable(const char* name, name_hash_t hash)
{
	auto find = g_name_map.find(hash);
	if (find == g_name_map.end()) {
		g_name_map[hash] = name;
	}
	else if (find->second != name) {
		printf("\n\n\n"
			"!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!\n"
			"		NAME COLLISION %s %s %lld\n"
			"!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!\n\n\n", name, g_name_map[hash].c_str(), hash);
		_CrtDbgBreak();
	}

}
const char* StringName::get_c_str() const
{
	return g_name_map.find(hash) != g_name_map.end() ? g_name_map.find(hash)->second.c_str() : nullptr;
}
#endif

StringName::StringName(const char* name)
{
	hash = StringUtils::fnv1a_64(name, StringUtils::const_strlen(name));
#ifdef DEBUG_STRING_NAME
	add_to_nametable(name, hash);
#endif // DEBUG_STRING_NAME
}
#ifdef DEBUG_STRING_NAME
StringName::StringName(const char* name, name_hash_t hash) : hash(hash)
{
	add_to_nametable(name, hash);
}
#endif


