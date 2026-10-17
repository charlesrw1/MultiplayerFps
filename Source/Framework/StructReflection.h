#pragma once
#include "ReflectionProp.h"
struct Serializer;
class StructTypeInfo
{
public:
	StructTypeInfo(const char* name, const PropertyInfoList* p, void (*custom_serialize)(void*, Serializer&))
		: structname(name), properties(p), custom_serialize(custom_serialize) {}
	const char* structname = "";
	const PropertyInfoList* properties = nullptr;
	void (*custom_serialize)(void*, Serializer&) = nullptr;
};
#define STRUCT_BODY(...)                                                                                               \
	static StructTypeInfo StructType;                                                                                  \
	static const PropertyInfoList* get_props();
