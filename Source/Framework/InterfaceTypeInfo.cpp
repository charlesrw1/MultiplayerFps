#include "Framework/InterfaceTypeInfo.h"
#include <vector>

std::unordered_map<std::string, InterfaceTypeInfo*>& InterfaceTypeInfo::get_name_registry() {
	static std::unordered_map<std::string, InterfaceTypeInfo*> inst;
	return inst;
}

std::vector<InterfaceTypeInfo*>& InterfaceTypeInfo::get_id_registry() {
	static std::vector<InterfaceTypeInfo*> inst;
	return inst;
}

InterfaceTypeInfo::InterfaceTypeInfo(const char* name) : name(name) {
	auto& by_name = get_name_registry();
	auto& by_id = get_id_registry();
	id = (int32_t)by_id.size();
	by_name[name] = this;
	by_id.push_back(this);
}

InterfaceTypeInfo* InterfaceTypeInfo::find_interface(const char* name) {
	auto& reg = get_name_registry();
	auto it = reg.find(name);
	return it != reg.end() ? it->second : nullptr;
}

InterfaceTypeInfo* InterfaceTypeInfo::find_interface(int32_t id) {
	auto& reg = get_id_registry();
	if (id >= 0 && id < (int32_t)reg.size())
		return reg[id];
	return nullptr;
}

int32_t InterfaceTypeInfo::get_count() {
	return (int32_t)get_id_registry().size();
}
