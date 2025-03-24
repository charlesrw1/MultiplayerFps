#pragma once
#include "UI/GUISystemPublic.h"
#include "Framework/MulticastDelegate.h"
#include "UI/UIBuilder.h"
#include "UI/BaseGUI.h"
namespace gui {
NEWCLASS(Button, BaseGUI)
public:

#ifdef EDITOR_BUILD
	virtual const char* get_editor_outliner_icon() const { return "eng/editor/guibutton.png"; }
#endif

	void update_widget_size() override {

		InlineVec<BaseGUI*, 16> children;
		get_gui_children(children);


		if (children.size() == 0)
			desired_size = { 50,50 };
		else {
			auto pad = children[0]->get_padding();
			desired_size = { pad.x + pad.z + children[0]->desired_size.x,
			pad.y + pad.w + children[0]->desired_size.y };
		}
	}
	void update_subwidget_positions() override {

		InlineVec<BaseGUI*, 16> children;
		get_gui_children(children);


		if (children.size() == 1) {
			auto pad = children[0]->get_padding();
			auto corner = ws_position + glm::ivec2(pad.x,pad.y);
			auto sz = ws_size - glm::ivec2(pad.x+pad.z,pad.y+pad.w);

			auto get_coord_for_align = [](int min, int max, guiAlignment align, int size, int& outsize)->int {
				outsize = size;
				if (align == guiAlignment::Left)
					return min;
				if (align == guiAlignment::Right)
					return max - size;
				if (align == guiAlignment::Center) {
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
	void on_released(int x, int y, int b) override {
		on_selected.invoke();
	}

	void paint(UIBuilder& b) override {
		Color32 color = { 128,128,128 };
		if (is_hovering())
			color = { 128,50,50 };
		if (is_dragging())
			color = { 20,50,200 };

		b.draw_solid_rect(
			ws_position,
			ws_size,
			color
		);
	}

	REFLECT();
	MulticastDelegate<> on_selected;
};
}