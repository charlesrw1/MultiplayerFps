#pragma once
#include "UI/BaseGUI.h"
#include "UI/Widgets/SharedFuncs.h"

namespace gui {
NEWCLASS(Fullscreen, BaseGUI)
public:

	// size which determines how widgets are placed relative to anchors

	void update_widget_size() override {

	}

	void update_subwidget_positions() override {

		InlineVec<BaseGUI*, 16> children;
		get_gui_children(children);

		for (int i = 0; i < children.size(); i++) {
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
	}

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final { return "eng/editor/guifullscreen.png"; }
#endif

	REFLECT();
	int z_order = 0;
};

NEWCLASS(HorizontalBox, BaseGUI)
public:
#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final { return "eng/editor/guihorizontalbox.png"; }
#endif
	void update_widget_size() final {
		update_desired_size_flow(this, 0);
	}
	void update_subwidget_positions() final {
		update_child_positions_flow(this, 0, 0);
	}
};

NEWCLASS(VerticalBox,BaseGUI)
public:
	VerticalBox() {
		uses_clip_test = true;
		eat_scroll_event = true;
		recieve_mouse = guiMouseFilter::Block;
	}

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final { return "eng/editor/guiverticalbox.png"; }
#endif
	void on_mouse_scroll(const SDL_MouseWheelEvent& wheel) override;

	bool scrollable = false;
	REFLECT();
	int start = 0;

	void update_widget_size() final {
		update_desired_size_flow(this, 1);
	}
	void update_subwidget_positions() final {
		update_child_positions_flow(this, 1, start);
	}
};

}