#pragma once
#include "Framework/EnumDefReflection.h"


NEWENUM(Easing, uint8_t)
{
	Linear,
	CubicEaseIn,
	CubicEaseOut,
	CubicEaseInOut,
};

inline float evaluate_easing(Easing type, float t)
{
	switch (type)
	{
	case Easing::Linear:
		return t;
		break;
	case Easing::CubicEaseIn:
		return t * t * t;
		break;
	case Easing::CubicEaseOut: {
		float oneminus = 1 - t;
		return 1.0 - oneminus * oneminus * oneminus;
	} break;
	case Easing::CubicEaseInOut: {
		float othert = -2 * t + 2;
		return (t < 0.5) ? 4 * t * t * t : 1.0 - othert * othert * othert * 0.5;
	}break;
	default:
		return t;
		break;
	}
}