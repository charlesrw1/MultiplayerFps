#pragma once
#include "UI/BaseGUI.h"
#include "UI/Widgets/SharedFuncs.h"

/*

auto child = children[i];

			auto sz_to_use = child->get_actual_sz_to_use();

			const glm::vec2 pivot_ofs = child->get_pivot_ofs();
			glm::ivec2 pivot = { pivot_ofs.x* sz_to_use.x,pivot_ofs.y* sz_to_use.y };

			auto anchor = child->get_anchor_pos();

			auto get_corner_func = [&](int i) -> glm::ivec2 {
				auto ls_pos = glm::ivec2(0, 0);
				if (i == 0) ls_pos = child->get_ls_position();
				if (i == 1)ls_pos = child->get_ls_position() + sz_to_use;

				return anchor.convert_ws_coord(i,
					ls_pos, ws_position,
					ws_size);
			};
			auto top_r = get_corner_func(0);
			auto bot_r = get_corner_func(1);

			child->ws_position = top_r - pivot;
			child->ws_size = bot_r-top_r;
		}

*/