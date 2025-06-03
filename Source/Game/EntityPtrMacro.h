#pragma once
#include "EntityPtr.h"
#include "Framework/ReflectionProp.h"

template<typename T>
inline PropertyInfo make_entity_ptr_property(const char* name, uint16_t offset, uint32_t flags, obj<T>* dummy) {
	return make_struct_property(name, offset, flags, "ObjPtr", T::StaticType.classname);
}
#define REG_ENTITY_PTR(name, flags) make_entity_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)
