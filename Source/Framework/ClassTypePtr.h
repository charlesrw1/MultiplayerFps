#pragma once
#include "PropertyEd.h"
#include "ReflectionProp.h"

template <typename T> struct ClassTypePtr
{
	~ClassTypePtr() { static_assert(sizeof(ClassTypePtr<T>) == sizeof(void*), "classtypeptr needs to be ptr sized"); }
	ClassTypePtr() = default;
	ClassTypePtr(const ClassTypeInfo& i) : ptr(&i) {}

	const ClassTypeInfo* ptr = nullptr;
};
