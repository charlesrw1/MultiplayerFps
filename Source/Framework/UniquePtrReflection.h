#pragma once
#include "ReflectionProp.h"
#include <memory>

template<typename T>
inline PropertyInfo make_unique_ptr_prop(const char* name, uint16_t offset, std::unique_ptr<T>* dummy, int flags)
{
	PropertyInfo pi;
	pi.name = name;
	pi.offset = offset;
	pi.type = core_type_id::StdUniquePtr;
	pi.range_hint = T::StaticType.classname;
	pi.flags = flags;
	return pi;
}
#define REG_UNIQUE_PTR(name, flags) \
	make_unique_ptr_prop(#name,offsetof(TYPE_FROM_START, name), &((TYPE_FROM_START*)0)->name, flags)
