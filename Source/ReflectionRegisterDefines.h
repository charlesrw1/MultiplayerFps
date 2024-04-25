#pragma once

#define REG_INT(type, name, flags, hint) make_integer_property(#name,offsetof(type, name), flags, sizeof(type::name), hint)
#define REG_FLOAT(type, name, flags, hint) make_float_property(#name,offsetof(type, name), flags, hint)
#define REG_ENUM(type, name, flags, hint, enum_id) make_enum_property(#name,offsetof(type, name), flags,sizeof(type::name), enum_id)
#define REG_BOOL(type, name, flags, hint) make_bool_property(#name,offsetof(type, name), flags, hint)
#define REG_STDSTRING(type, name, flags) make_string_property(#name,offsetof(type, name), flags)

#define START_PROPS  static PropertyInfo props[] = {
#define END_PROPS  }; \
 properties = { props, sizeof(props) / sizeof(PropertyInfo) };

#define MAKEPROPLIST(name) { name, sizeof(name) / sizeof(PropertyInfo) }
