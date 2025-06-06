#pragma once
#include "StructReflection.h"

struct BoolButton
{
	STRUCT_BODY();
	bool b = false;
	bool check_and_swap() {
		bool out = b;
		b = false;
		return out;
	}
};