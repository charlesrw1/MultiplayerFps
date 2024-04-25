#include "Parameter.h"

static const char* strs[] = {
	"int_t ",
	"enum_t",
	"bool_t",
	"float_t"
};

static_assert((sizeof(strs) / sizeof(char*)) == ((int)script_parameter_type::float_t + 1), "out of sync");
AutoEnumDef script_parameter_type_def = AutoEnumDef("", sizeof(strs)/sizeof(char*), strs);