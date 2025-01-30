#pragma once
#include "Framework/ArrayReflection.h"
#include "EntityPtr.h"
template<>
struct GetAtomValueWrapper<EntityPtr> {
	static PropertyInfo get();
};
