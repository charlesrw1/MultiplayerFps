#pragma once
#include <cstdint>

struct GlobalEnumDefIdx {
	int16_t enum_idx = -1;
	int16_t val_idx = -1;
};

struct Enum_Def
{
	const char* name = "";
	int count = 0;
	const char** strs = nullptr;
};

struct AutoEnumDef
{
	AutoEnumDef(const char* name, int count, const char** strs);
	uint16_t id=0;
};

namespace Enum
{
	typedef int16_t type_handle_t;

	type_handle_t add_new_def(Enum_Def def);
	const char* get_type_name(type_handle_t handle);
	const char* get_enum_name(type_handle_t handle, int index);
	const Enum_Def& get_enum_def(type_handle_t handle);
	GlobalEnumDefIdx find_for_full_name(const char* name);

}

