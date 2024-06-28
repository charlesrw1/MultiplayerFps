#pragma once

#include "Framework/InlineVec.h"


enum class CurveInterpType : uint8_t
{
	Cubic,
	Linear,
	Constant,
};

class CurvePoint
{
public:
	float value = 0.0;
	uint16_t time = 0.0;
};

class Curve
{
public:
	CurveInterpType curve_interp = CurveInterpType::Linear;
	InlineVec<CurvePoint, 2> points;
};