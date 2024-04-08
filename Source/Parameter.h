#pragma once
#include "glm/glm.hpp"

enum class script_parameter_type
{
	animvec2,
	animfloat,
	animint,
};

struct Parameter
{
	script_parameter_type type = script_parameter_type::animfloat;
	union {
		float fval = 0.0;
		int ival;
	};
};