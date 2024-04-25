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