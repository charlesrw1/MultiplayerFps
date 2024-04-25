#pragma once
#include "glm/glm.hpp"

#include "EnumDefReflection.h"

// if you modify this enum, change the AutoEnumDef!
extern AutoEnumDef script_parameter_type_def;
enum class script_parameter_type : uint8_t
{
	// integer types
	int_t,
	enum_t,
	bool_t,

	float_t,
};

struct Parameter
{
	bool is_integer_type() const {
		return type != script_parameter_type::float_t;
	}
	void init_for_type(script_parameter_type type) {
		this->type = type;
		if (!is_integer_type()) fval = 0.0;
		else ival = 0;
	}
	script_parameter_type type = script_parameter_type::int_t;
	uint8_t enum_index = 0;	// used for the editor
	union {
		int ival = 0;
		float fval;
	};
};