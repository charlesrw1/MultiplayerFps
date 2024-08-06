#pragma once
#include "UI/GUIPublic.h"

CLASS_H(GUIFullscreen, GUI)
public:
	// size which determines how widgets are placed relative to anchors

	void update_subwidget_positions() override {
		for (int i = 0; i < children.size(); i++) {
			auto child = children[i];
			glm::ivec2 pivot = { child->pivot_ofs.x*child->desired_size.x,child->pivot_ofs.y*child->desired_size.y };
			auto get_corner_func = [&](int i) {
				auto ls_pos = glm::ivec2(0, 0);
				if (i == 0) ls_pos = child->ls_position;
				if (i == 1)ls_pos = child->ls_position + child->ls_sz;

				return child->anchor.convert_ws_coord(i,
					ls_pos, ws_position,
					ws_size);
			};
			auto top_r = get_corner_func(0);
			auto bot_r = get_corner_func(1);

			child->ws_position = top_r - pivot;
			child->ws_size = bot_r-top_r;
		}
	}
};