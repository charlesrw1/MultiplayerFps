#pragma once
#include "Framework/StructReflection.h"
#include "Framework/Util.h"

struct lColor {
	STRUCT_BODY();
	REF int r = 255;
	REF int g = 255;
	REF int b = 255;
	REF int a = 255;

	Color32 to_color32() const {
		Color32 c;
		c.r = r;
		c.g = g;
		c.b = b;
		c.a = a;
		return c;
	}
};
