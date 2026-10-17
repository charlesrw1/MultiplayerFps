#pragma once
#include "ReflectionProp.h"

// REFLECT(...) flags reference: @docs [[reflection_macros]]
// Codegen-only helper: applies a PropertyAttributes literal to a PropertyInfo from make_property.
inline PropertyInfo apply_attrs(PropertyInfo prop, const PropertyAttributes& a) {
	prop.attrs = a;
	return prop;
}

#define START_PROPS(type)                                                                                              \
	using TYPE_FROM_START = type;                                                                                      \
	static PropertyInfo props[] = {
#define END_PROPS(type)                                                                                                \
	}                                                                                                                  \
	;                                                                                                                  \
	static PropertyInfoList properties = {props, sizeof(props) / sizeof(PropertyInfo), #type};                         \
	return &properties;
