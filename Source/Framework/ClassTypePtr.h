#pragma once
#include "PropertyEd.h"
#include "ReflectionProp.h"

template<typename T>
struct ClassTypePtr
{
	~ClassTypePtr() {
		static_assert(sizeof(ClassTypePtr<T>) == sizeof(void*), "classtypeptr needs to be ptr sized");
	}
	ClassTypePtr() = default;
	ClassTypePtr(const ClassTypeInfo& i) : ptr(&i) {}

	const ClassTypeInfo* ptr = nullptr;
};

template<typename T>
inline PropertyInfo make_classtype_ptr(const char* varname, uint32_t ofs, uint32_t flags, const ClassTypePtr<T>* DUMMY) {
	PropertyInfo prop;
	prop.name = varname;
	prop.offset = ofs;
	prop.custom_type_str = "ClassTypePtr";
	prop.flags = flags;
	prop.range_hint = T::StaticType.classname;
	prop.type = core_type_id::Struct;

	return prop;
}
#define REG_CLASSTYPE_PTR(name, flags) \
	make_classtype_ptr(#name, offsetof(TYPE_FROM_START, name), flags, &((MyClassType*)0)->name)