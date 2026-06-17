#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>

struct InterfaceTypeInfo
{
	const char* name = "";
	int32_t id = -1;

	InterfaceTypeInfo(const char* name);

	static InterfaceTypeInfo* find_interface(const char* name);
	static InterfaceTypeInfo* find_interface(int32_t id);
	static int32_t get_count();

private:
	static std::unordered_map<std::string, InterfaceTypeInfo*>& get_name_registry();
	static std::vector<InterfaceTypeInfo*>& get_id_registry();
};

#define INTERFACE_BODY()                                                                                               \
	static InterfaceTypeInfo StaticInterfaceType;
