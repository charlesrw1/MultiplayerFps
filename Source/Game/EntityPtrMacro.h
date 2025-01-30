#pragma once
#include "EntityPtr.h"
#include "Framework/ReflectionProp.h"
inline PropertyInfo make_entity_ptr_property(const char* name, uint16_t offset, uint32_t flags) {
	return make_struct_property(name, offset, flags, "EntityPtr", "");
}
#define REG_ENTITY_PTR(name, flags) make_entity_ptr_property(#name, offsetof(MyClassType,name),flags)
