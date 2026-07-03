#include "Framework/StringName.h"

#include <string>
#include <unordered_map>

// Always-on debug name table (not gated to EDITOR_BUILD): get_c_str() must work in every
// build, since it's the only path back to a readable name once a StringName crosses into
// Lua as a bare hash (see get_stringname_from_lua in ScriptFunctionCodegen.cpp). Memory cost
// is one std::string per unique identifier, trivial next to everything else in a game engine.
static std::unordered_map<uint64_t, std::string>& get_g_name_map() {
	static std::unordered_map<uint64_t, std::string> inst;
	return inst;
};

static void add_to_nametable(const char* name, name_hash_t hash) {
	auto& g_name_map = get_g_name_map();
	auto find = g_name_map.find(hash);
	if (find == g_name_map.end()) {
		g_name_map[hash] = name;
	} else if (find->second != name) {
		printf("\n\n\n"
			   "!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!\n"
			   "		NAME COLLISION %s %s %lld\n"
			   "!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!\n\n\n",
			   name, g_name_map[hash].c_str(), hash);
		_CrtDbgBreak();
	}
}
const char* StringName::get_c_str() const {
	auto& g_name_map = get_g_name_map();

	return g_name_map.find(hash) != g_name_map.end() ? g_name_map.find(hash)->second.c_str() : "";
}

StringName::StringName(const char* name) {
	if (*name == 0) {
		hash = 0;
		return;
	}

	hash = StringUtils_Hash::fnv1a_64(name, StringUtils_Hash::const_strlen(name));
	add_to_nametable(name, hash);
}
StringName::StringName(const char* name, name_hash_t hash) : hash(hash) {
	add_to_nametable(name, hash);
}

StringName StringName::intern(const char* str, size_t len) {
	if (len == 0)
		return StringName();

	name_hash_t hash = StringUtils_Hash::fnv1a_64(str, len);
	add_to_nametable(str, hash);
	return StringName(hash);
}
