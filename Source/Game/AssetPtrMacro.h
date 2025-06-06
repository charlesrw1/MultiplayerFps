#pragma once
#include "SerializePtrHelpers.h"
#include "Framework/ReflectionProp.h"

template<typename T>
inline PropertyInfo make_asset_ptr_property(const char* name, uint16_t offset, uint32_t flags, AssetPtr<T>* dummy) {
	return make_struct_property(name, offset, flags, "AssetPtr", T::StaticType.classname);
}

#define REG_ASSET_PTR(name, flags) make_asset_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)