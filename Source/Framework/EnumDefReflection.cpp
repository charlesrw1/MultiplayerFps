#include "Framework/EnumDefReflection.h"
#include <unordered_map>
#include <cassert>


class GlobalEnumDefMgr
{
public:
	static GlobalEnumDefMgr& get() {
		static GlobalEnumDefMgr inst;
		return inst;
	}


	std::unordered_map<std::string, EnumFindResult> name_to_idx;
};


void EnumRegistry::register_enum(const EnumTypeInfo* eti) {
	auto& name_to_idx = GlobalEnumDefMgr::get().name_to_idx;

	for (int i = 0; i < eti->str_count; i++) {
		EnumFindResult efr;
		efr.enum_idx = i;
		efr.typeinfo = eti;
		name_to_idx[eti->strs[i]] = efr;
	}
}

EnumFindResult EnumRegistry::find_enum_by_name(const char* enum_value_name) {
	auto& name_to_idx = GlobalEnumDefMgr::get().name_to_idx;
	auto find = name_to_idx.find(enum_value_name);
	return find == name_to_idx.end() ? EnumFindResult() : find->second;
}

EnumTypeInfo::EnumTypeInfo(const char* name, const char** strs, size_t count) : name(name),strs(strs),str_count(count)
{
	EnumRegistry::register_enum(this);
}

enum MyEnum
{
	aVal, bVal, cVal
};
ENUM_HEADER(MyEnum)

ENUM_START(MyEnum)
	STRINGIFY_EUNM(aVal, 0),
	STRINGIFY_EUNM(bVal, 1)
ENUM_IMPL(MyEnum)
