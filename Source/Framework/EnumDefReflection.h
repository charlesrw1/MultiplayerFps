#pragma once
#include <cstdint>

struct EnumTypeInfo {

	EnumTypeInfo(const char* name, const char** strs, size_t count);

	const char* get_name() const { return name; }
	const char* get_enum_str(int index) const {
		return (index >= 0 && index < str_count) ? strs[index] : "";
	}

	const char* name = "";
	const char** strs = nullptr;
	int str_count = 0;
	int id = 0;
};
template<typename T>
struct EnumTrait;

#define ENUM_HEADER(Type) \
template<> \
struct EnumTrait<Type> {\
	static EnumTypeInfo StaticType;\
};

#define ENUM_START(Type) \
static const char* Type##strs[] = {

#define STRINGIFY_EUNM(val, expected) ( 1 / (int)!( (int)val - expected ) ) ? #val : ""

#define ENUM_IMPL(Type) \
}; \
EnumTypeInfo EnumTrait<Type>::StaticType = EnumTypeInfo(#Type, Type##strs, sizeof(Type##strs)/sizeof(const char*));

struct EnumFindResult
{
	const EnumTypeInfo* typeinfo = nullptr;
	int enum_idx = -1;
};

class EnumRegistry
{
public:
	static void register_enum(const EnumTypeInfo* eti);
	static EnumFindResult find_enum_by_name(const char* enum_value_name);
};

