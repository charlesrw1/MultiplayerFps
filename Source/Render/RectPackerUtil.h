#pragma once

#include "Framework/Rect2d.h"
#include <vector>
class RectPackerUtil
{
public:
	static std::pair<std::vector<glm::vec2>, int> shelf_pack(const std::vector<Rect2d>& rects, int max_width) {
		int x = 0;
		int y = 0;
		int shelf_height = 0;
		std::vector<glm::vec2> positions;
		for (auto [rx, ry, rw, rh] : rects) {
			if (x + rw > max_width) {
				x = 0;
				y += shelf_height;
				shelf_height = 0;
			}
			positions.push_back(glm::vec2(x, y));
			x += rw;
			shelf_height = glm::max(shelf_height, (int)rh);
		}
		int atlas_height = y + shelf_height;
		return {positions, atlas_height};
	}
};