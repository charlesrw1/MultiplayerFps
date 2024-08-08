#pragma once
#include "UI/GUIPublic.h"
CLASS_H(GUIButton, GUI)
public:
	void update_widget_size() override {
		if (children.empty())
			desired_size = { 50,50 };
		else {
			auto pad = children[0]->padding;
			desired_size = { pad.x + pad.z + children[0]->desired_size.x,
			pad.y + pad.w + children[0]->desired_size.y };
		}
	}
	void update_subwidget_positions() override {
		if (!children.empty()) {
			auto pad = children[0]->padding;
			auto corner = ws_position + glm::ivec2(pad.x,pad.y);
			auto sz = ws_size - glm::ivec2(pad.x+pad.z,pad.y+pad.w);

			auto get_coord_for_align = [](int min, int max, GuiAlignment align, int size, int& outsize)->int {
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
			};
			glm::ivec2 out_corner{};
			glm::ivec2 out_sz{};

			out_corner.x = get_coord_for_align(corner.x, corner.x + sz.x, children[0]->w_alignment, children[0]->desired_size.x, out_sz.x);
			out_corner.y = get_coord_for_align(corner.y, corner.y + sz.y, children[0]->h_alignment, children[0]->desired_size.y, out_sz.y);


			children[0]->ws_position = out_corner;
			children[0]->ws_size = out_sz;
		}
	}
	void on_pressed(int x, int y, int b) override {
		if (b == 1) {
			is_clicked = true;
		}
	}
	void on_released(int x, int y, int b) override {
		is_clicked = false;

		if (b == 1)
			on_selected.invoke();
	}
	void on_hover_start() override {
		is_hovered = true;
	}
	void on_hover_end() override {
		is_hovered = false;
	}

	void paint(UIBuilder& b) override {
		Color32 color = { 128,128,128 };
		if (is_hovered)
			color = { 128,50,50 };
		if (is_clicked)
			color = { 20,50,200 };

		b.draw_solid_rect(
			ws_position,
			ws_size,
			color
		);
	}

	bool is_hovered = false;
	bool is_clicked = false;

	MulticastDelegate<> on_selected;
};
