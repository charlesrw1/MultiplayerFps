#pragma once
#include "UI/GUISystemPublic.h"
#include "Framework/MulticastDelegate.h"
#include "UI/UIBuilder.h"
#include "UI/BaseGUI.h"
#include "UI/Widgets/SharedFuncs.h"
class guiButton : public guiBase
{
public:
	CLASS_BODY(guiButton);
	guiButton() {
		recieve_mouse = guiMouseFilter::Block;
	}
#ifdef EDITOR_BUILD
	virtual const char* get_editor_outliner_icon() const { return "eng/editor/guibutton.png"; }
#endif

	void update_widget_size() override {
		update_desired_size_from_one_child(this);
	}
	void update_subwidget_positions() override {
		update_one_child_position(this);
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