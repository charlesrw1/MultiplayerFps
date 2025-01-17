#pragma once
#include "UI/GUIPublic.h"
#include "UI/UILoader.h"
#include <string>
#include "Framework/Config.h"
CLASS_H(GUIBox, GUI)
public:
	GUIBox() {
		recieve_events = false;
	}
	Color32 color{};

	virtual void paint(UIBuilder& b) override {
		b.draw_solid_rect(
			ws_position,
			ws_size,
			color
		);
	}
};

extern ConfigVar ui_draw_text_bbox;

CLASS_H(GUIText, GUI)
public:
	GUIText() {
		recieve_events = false;
	}
	Color32 color{};
	std::string text;
	const GuiFont* myFont = nullptr;

	void update_widget_size() override {
		auto font = (myFont) ? myFont : g_fonts.get_default_font();
		desired_size = GuiHelpers::calc_text_size_no_wrap(text.c_str(), font);
	}

	void paint(UIBuilder& b) override {
		auto font = (myFont) ? myFont : g_fonts.get_default_font();
		StringView sv;
		sv.str_start = text.c_str();
		sv.str_len = text.size();
		if(ui_draw_text_bbox.get_bool())
			b.draw_solid_rect(ws_position, ws_size, COLOR_CYAN);
		b.draw_text(ws_position+glm::ivec2{2,2}, ws_size, font, sv, COLOR_BLACK);
		b.draw_text(ws_position, ws_size, font, sv, color);
	}
};