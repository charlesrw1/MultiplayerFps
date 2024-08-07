#pragma once
#include <cstdint>

struct Rect2d
{
	Rect2d(int16_t x, int16_t y, int16_t w, int16_t h) : x(x), y(y), w(w), h(h) {}
	Rect2d() = default;

	int16_t x = 0;
	int16_t y = 0;
	int16_t w = 0;
	int16_t h = 0;

	bool is_point_inside(int16_t px, int16_t py) const {
		return px >= x && px < x + w && py >= y && py < y + h;
	}
};