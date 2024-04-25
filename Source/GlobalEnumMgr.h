#pragma once
#include "EnumDefReflection.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <cassert>

class GlobalEnumDefMgr
{
public:
	static GlobalEnumDefMgr& get() {
		static GlobalEnumDefMgr inst;
		return inst;
	}

	const char* get_enum_type_name(int16_t enum_type) {
		return enum_defs.at(enum_type).name;
	}
	const char* get_enum_name(int16_t enum_type, int enum_idx) {
		assert(enum_idx >= 0 && enum_idx < enum_defs.at(enum_type).count);
		return enum_defs.at(enum_type).strs[enum_idx];
	}

	uint16_t add_enum(Enum_Def def) {
		enum_defs.push_back(def);
		int16_t id = enum_defs.size() - 1;

		std::string buf;
		for (int i = 0; i < def.count; i++) {
			buf.clear();
			buf += def.name;
			buf += "::";
			buf += def.strs[i];
#ifdef _DEBUG
			const auto& find = name_to_idx.find(buf);
			assert(find == name_to_idx.end());
#endif
			name_to_idx[buf] = { id, int16_t(i) };
		}

		return id;
	}

	GlobalEnumDefIdx get_for_name(const char* str) {
		const auto& find = name_to_idx.find(str);
		if (find == name_to_idx.end())
			return GlobalEnumDefIdx();
		else
			return find->second;
	}

	const Enum_Def& get_enum_def(uint16_t idx) {
		return enum_defs[idx];
	}

	std::vector<Enum_Def> enum_defs;
	std::unordered_map<std::string, GlobalEnumDefIdx> name_to_idx;
};