#pragma once
#include "Framework/ArrayReflection.h"
#include "EntityPtr.h"

template<typename T>
struct GetAtomValueWrapper<obj<T>> {
	static PropertyInfo get() {
		return make_struct_property("", 0, PROP_DEFAULT, "ObjPtr", T::StaticType.classname);
	}
};