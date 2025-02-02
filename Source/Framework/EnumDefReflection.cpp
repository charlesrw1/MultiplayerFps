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
	std::unordered_map<std::string, const EnumTypeInfo*> name_to_type;
};

void EnumRegistry::register_enum(const EnumTypeInfo* eti) {
	auto& name_to_idx = GlobalEnumDefMgr::get().name_to_idx;
	auto& name_to_type = GlobalEnumDefMgr::get().name_to_type;

	name_to_type.insert({ eti->name,eti });
	for (int i = 0; i < eti->str_count; i++) {
		EnumFindResult efr;
		efr.enum_idx = i;
		efr.value = eti->strs[i].value;
		efr.typeinfo = eti;
		name_to_idx[(std::string(eti->name)+"::")+eti->strs[i].name] = efr;
	}
}
const EnumTypeInfo* EnumRegistry::find_enum_type(const char* enum_type) {
	auto& name_to_type = GlobalEnumDefMgr::get().name_to_type;
	auto find = name_to_type.find(enum_type);
	return find == name_to_type.end() ? nullptr : find->second;
}
const EnumTypeInfo* EnumRegistry::find_enum_type(const std::string& str) {
	auto& name_to_type = GlobalEnumDefMgr::get().name_to_type;
	auto find = name_to_type.find(str);
	return find == name_to_type.end() ? nullptr : find->second;
}
EnumFindResult EnumRegistry::find_enum_by_name(const char* enum_value_name) {
	auto& name_to_idx = GlobalEnumDefMgr::get().name_to_idx;
	auto find = name_to_idx.find(enum_value_name);
	return find == name_to_idx.end() ? EnumFindResult() : find->second;
}
EnumFindResult EnumRegistry::find_enum_by_name(const std::string& str) {
	auto& name_to_idx = GlobalEnumDefMgr::get().name_to_idx;
	auto find = name_to_idx.find(str);
	return find == name_to_idx.end() ? EnumFindResult() : find->second;
}

EnumTypeInfo::EnumTypeInfo(const char* name, const EnumIntPair* strs, size_t count) : name(name),strs(strs),str_count(count)
{
	EnumRegistry::register_enum(this);
}

const EnumIntPair* EnumTypeInfo::find_for_value(int64_t value) const
{
	for (int i = 0; i < str_count; i++)
		if (strs[i].value == value)
			return &strs[i];
	return nullptr;
}