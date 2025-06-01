#include "Framework/StringName.h"

#include <unordered_map>

#ifdef EDITOR_BUILD
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
	return g_name_map.find(hash) != g_name_map.end() ? g_name_map.find(hash)->second.c_str() : "";
}
#endif

StringName::StringName(const char* name)
{
	if (*name == 0) {
		hash = 0;
		return;
	}

	hash = StringUtils::fnv1a_64(name, StringUtils::const_strlen(name));
#ifdef EDITOR_BUILD
	add_to_nametable(name, hash);
#endif // DEBUG_STRING_NAME
}
#ifdef EDITOR_BUILD
StringName::StringName(const char* name, name_hash_t hash) : hash(hash)
{

	add_to_nametable(name, hash);
}
#endif


