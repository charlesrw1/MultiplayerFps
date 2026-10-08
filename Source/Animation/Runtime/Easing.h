#pragma once
#include "Framework/EnumDefReflection.h"

NEWENUM(Easing, uint8_t){
	Linear, CubicEaseIn, CubicEaseOut, CubicEaseInOut, Constant,
};

inline float evaluate_easing(Easing type, float t) {
	switch (type) {
	case Easing::Linear:
		return t;
		break;
	case Easing::CubicEaseIn:
		return t * t * t;
		break;
	case Easing::CubicEaseOut: {
		float oneminus = 1.f - t;
		return 1.0f - oneminus * oneminus * oneminus;
	} break;
	case Easing::CubicEaseInOut: {
		float othert = -2.f * t + 2.f;
		return (t < 0.5f) ? 4.f * t * t * t : 1.0f - othert * othert * othert * 0.5f;
	} break;
	case Easing::Constant: {
		return 1.f;
	} break;
	default:
		return t;
		break;
	}
}
