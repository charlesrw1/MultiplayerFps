#pragma once
#include "UI/GUIPublic.h"

CLASS_H(GUIFullscreen, GUI)
public:
	// size which determines how widgets are placed relative to anchors

	void update_widget_size() override {

	}

	void update_subwidget_positions() override {
		for (int i = 0; i < children.size(); i++) {
			auto child = children[i].get();

			auto sz_to_use = (child->use_desired_size) ? child->desired_size : child->ls_sz;

			glm::ivec2 pivot = { child->pivot_ofs.x* sz_to_use.x,child->pivot_ofs.y* sz_to_use.y };
			auto get_corner_func = [&](int i) {
				auto ls_pos = glm::ivec2(0, 0);
				if (i == 0) ls_pos = child->ls_position;
				if (i == 1)ls_pos = child->ls_position + sz_to_use;

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

class LayoutUtils
{
public:
	static int get_coord_for_align(int min, int max, GuiAlignment align, int size, int& outsize) {
		outsize = size;
		if (align == GuiAlignment::Left)
			return min;
		if (align == GuiAlignment::Right)
			return max - size;
		if (align == GuiAlignment::Center) {
			auto w = max - min;
			return (w - size) * 0.5 + min;
		}
		// else align == fill
		outsize = (max - min);
		return min;
	}

};

CLASS_H(GUIVerticalBox,GUI)
public:
	void update_widget_size() override {
		desired_size = ls_sz;

		glm::ivec2 cursor = { 0,0 };
		for (int i = 0; i < children.size(); i++) {
			auto child = children[i].get();

			auto pad = child->padding;
			auto sz = child->desired_size +  glm::ivec2(pad.x + pad.z, pad.y + pad.w);

			cursor.x = glm::max(cursor.x, sz.x);
			cursor.y += sz.y;
		}
		desired_size = cursor;
	}
	void update_subwidget_positions() override {

		glm::ivec2 cursor = ws_position;

		for (int i = 0; i < children.size(); i++) {
			auto child = children[i].get();

			auto pad = child->padding;
			auto corner = cursor + glm::ivec2(pad.x, pad.y);
			auto sz = ws_size - glm::ivec2(pad.x + pad.z, pad.y + pad.w);

			glm::ivec2 out_corner = cursor;
			out_corner.y += pad.y;
			glm::ivec2 out_sz = { 0,child->desired_size.y + pad.y + pad.w };

			out_corner.x = LayoutUtils::get_coord_for_align(corner.x, corner.x + sz.x, child->w_alignment, child->desired_size.x, out_sz.x);

			child->ws_position = out_corner;
			child->ws_size = out_sz;

			cursor.y = out_corner.y + out_sz.y;
		}
	}
};