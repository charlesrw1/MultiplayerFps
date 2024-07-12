#pragma once

#include "Framework/InlineVec.h"
#include "ReflectionProp.h"
#include "EnumDefReflection.h"
#include <glm/glm.hpp>
// A generic time based curve 

enum class CurvePointType : uint8_t
{
	Linear,	// linear interp
	Constant,	// no interp
	Auto,		// tangents determiend by adjacent points
	SplitTangents,	// 2 handle free tangents
	Aligned,		// free tangents but they are kept aligned
};
ENUM_HEADER(CurvePointType);

class CurvePoint
{
public:

	float value = 0.0;
	float time = 0.0;
	glm::vec2 tangent0=glm::vec2(-1,0);
	glm::vec2 tangent1=glm::vec2(1,0);
	CurvePointType type = CurvePointType::Linear;
};

class Curve
{
public:
	std::vector<CurvePoint> points;
	float length = 0.0;
};