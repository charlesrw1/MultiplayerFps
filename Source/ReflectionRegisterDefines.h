#pragma once

#define REG_INT(name, flags, hint) make_integer_property(#name,offsetof(TYPE_FROM_START, name), flags, sizeof(TYPE_FROM_START::name), hint)
#define REG_FLOAT(name, flags, hint) make_float_property(#name,offsetof(TYPE_FROM_START, name), flags, hint)
#define REG_ENUM(name, flags, hint, enum_id) make_enum_property(#name,offsetof(TYPE_FROM_START, name), flags,sizeof(TYPE_FROM_START::name), enum_id)
#define REG_BOOL(name, flags, hint) make_bool_property(#name,offsetof(TYPE_FROM_START, name), flags, hint)
#define REG_STDSTRING(name, flags) make_string_property(#name,offsetof(TYPE_FROM_START, name), flags)


#define REG_STRUCT_CUSTOM_TYPE(name, flags, customtype) make_struct_property(#name,offsetof(TYPE_FROM_START, name), flags, customtype)
#define REG_STDSTRING_CUSTOM_TYPE(name, flags, customtype) make_string_property(#name,offsetof(TYPE_FROM_START, name), flags, customtype)

#define REG_INT_W_CUSTOM(name, flags, hint, custom) make_integer_property(#name,offsetof(TYPE_FROM_START, name), flags, sizeof(TYPE_FROM_START::name), hint, custom)


#define START_PROPS(type) using TYPE_FROM_START = type;  static PropertyInfo props[] = {
#define END_PROPS(type)  }; \
 static PropertyInfoList properties = { props, sizeof(props) / sizeof(PropertyInfo), #type }; \
return &properties;

