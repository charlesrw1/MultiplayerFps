#pragma once
#include <cstdint>
#include <glm/glm.hpp>
struct Rect2d
{
	Rect2d(int16_t x, int16_t y, int16_t w, int16_t h) : x(x), y(y), w(w), h(h) {}
	Rect2d() = default;
	Rect2d(glm::ivec2 size) {
		w = size.x;
		h = size.y;
	}
	Rect2d(glm::ivec2 pos, glm::ivec2 size) {
		w = size.x;
		h = size.y;
		x = pos.x;
		y = pos.y;
	}


	int16_t x = 0;
	int16_t y = 0;
	int16_t w = 0;
	int16_t h = 0;

	bool is_point_inside(int16_t px, int16_t py) const {
		return px >= x && px < x + w && py >= y && py < y + h;
	}
	bool is_point_inside(glm::ivec2 point) const {
		return is_point_inside(point.x, point.y);
	}

	glm::ivec2 get_size() const { return { w,h }; }
	glm::ivec2 get_pos() const { return { x,y }; }
};