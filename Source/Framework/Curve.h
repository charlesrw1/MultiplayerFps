#pragma once

#include "Framework/InlineVec.h"
#include "ReflectionProp.h"
#include "EnumDefReflection.h"

// A generic time based curve 

// interpolation applies to entire graph
enum class CrvIntrp : uint8_t	// curve interp
{
	Cubic,
	Linear,
	Constant,
};
ENUM_HEADER(CrvIntrp);

class CurvePoint
{
public:

	float value = 0.0;
	float time = 0.0;
};

class Curve
{
public:
	CrvIntrp curve_interp = CrvIntrp::Linear;
	std::vector<CurvePoint> points;
	float length = 0.0;
};