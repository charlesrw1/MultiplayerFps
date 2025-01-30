#pragma once
#include "SerializePtrHelpers.h"
#include "Framework/ArrayReflection.h"
template<typename T>
struct GetAtomValueWrapper<AssetPtr<T>> {
	static PropertyInfo get() {
		return make_struct_property("", 0, PROP_DEFAULT, "AssetPtr", T::StaticType.classname);
	}
};