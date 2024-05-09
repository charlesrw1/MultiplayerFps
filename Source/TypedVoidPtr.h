#pragma once
#include "StringName.h"
struct TypedVoidPtr	// what in the god damn
{
	TypedVoidPtr() = default;
	TypedVoidPtr(StringName name, void* ptr) : name(name), ptr(ptr) {}

	StringName name;
	void* ptr = nullptr;
};