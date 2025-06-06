#pragma once
#include "ReflectionProp.h"
class StructTypeInfo
{
public:
	StructTypeInfo(const char* name, const PropertyInfoList* p) : structname(name), properties(p) {}
	const char* structname = "";
	const PropertyInfoList* properties = nullptr;
};
#define STRUCT_BODY(...) \
	static StructTypeInfo StructType; \
	static const PropertyInfoList* get_props();