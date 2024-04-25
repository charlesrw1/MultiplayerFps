#pragma once
#include <assert.h>
#include <cstring>
struct Enum_Def
{
	const char* name = "";
	int count = 0;
	const char** strs = nullptr;

	int find_string(const char* str) {
		for (int i = 0; i < count; i++) {
			if (strcmp(str, strs[i]) == 0)
				return i;
		}
		return -1;
	}
};

class EnumMapManager
{
public:
	static EnumMapManager& get() {
		static EnumMapManager inst;
		return inst;
	}
	void add_def(Enum_Def def) {
		assert(def_count < 256);
		defs[def_count++] = def;
	}

	Enum_Def* get_def_by_index(int index) {
		assert(index < def_count && index >= 0);
		return defs + index;
	}

	int find_def(const char* name) {
		for (int i = 0; i < def_count; i++) {
			if (strcmp(name, defs[i].name) == 0)
				return i;
		}
		return -1;
	}

	int def_count = 0;
	Enum_Def defs[256];
};

struct AutoEnumDef
{
	AutoEnumDef(const char* name, int count, const char** strs) {
		EnumMapManager::get().add_def({ name,count,strs });
	}
};