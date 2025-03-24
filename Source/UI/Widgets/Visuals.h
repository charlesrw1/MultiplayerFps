#pragma once
#include "UI/BaseGUI.h"
#include "UI/UILoader.h"
#include <string>
#include "Framework/Config.h"
#include "Framework/Reflection2.h"
#include "UI/UIBuilder.h"
extern ConfigVar ui_draw_text_bbox;
namespace gui {

NEWCLASS(Box, BaseGUI)
public:
	Box() {
		recieve_events = false;
	}

	REFLECT();
	Color32 color=Color32();

	void paint(UIBuilder& b) final {
		b.draw_solid_rect(
			ws_position,
			ws_size,
			color
		);
	}
};


NEWCLASS(Text, BaseGUI)
public:
	Text() {
		recieve_events = false;
	}

#ifdef EDITOR_BUILD
	virtual const char* get_editor_outliner_icon() const { return "eng/editor/guitext.png"; }
#endif

	REFLECT();
	Color32 color = Color32();
	REFLECT();
	std::string text;
	REFLECT();
	AssetPtr<GuiFont> myFont;

	Rect2d text_size{};

	void update_widget_size() final {
		auto font = (myFont) ? myFont : g_fonts.get_default_font();
		text_size =  GuiHelpers::calc_text_size_no_wrap(text.c_str(), font);
		desired_size = { text_size.w,text_size.h };	//fixme
	}

	void paint(UIBuilder& b) final {
		auto font = (myFont) ? myFont : g_fonts.get_default_font();
		std::string_view sv(text);

		if(ui_draw_text_bbox.get_bool())
			b.draw_solid_rect(ws_position, ws_size, COLOR_CYAN);
		glm::ivec2 text_offset = { 0,font->base };

		b.draw_text(ws_position+glm::ivec2{2,2}+ text_offset, ws_size, font, sv, COLOR_BLACK);
		b.draw_text(ws_position+ text_offset, ws_size, font, sv, color);
	}
};

}