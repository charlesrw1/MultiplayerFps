#pragma once
#include "Framework/ArrayReflection.h"

template<typename T>
StdVectorCallback<T>* make_vector_atom_callbacks(std::vector<T>* ptr) {
	static StdVectorCallback<T> item(get_atom_value<T>());
	return &item;
}

#define MAKE_VECTOR_ATOM_CALLBACKS_ASSUMED(name) auto vecdef_##name = make_vector_atom_callbacks(&((MyClassType*)0)->name);
#define REG_STDVECTOR_NEW(name, flags) make_list_property(#name, offsetof(TYPE_FROM_START, name), flags, vecdef_##name) 