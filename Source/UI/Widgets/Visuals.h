#pragma once
#include "UI/BaseGUI.h"
#include "UI/UILoader.h"
#include <string>
#include "Framework/Config.h"
#include "Framework/Reflection2.h"
#include "UI/UIBuilder.h"
#include "SharedFuncs.h"
#include "Game/SerializePtrHelpers.h"

GENERATED_CLASS_INCLUDE("Render/Texture.h");

extern ConfigVar ui_draw_text_bbox;
class Texture;

// Places a widget in another widget
class guiBox : public guiBase
{
public:
	CLASS_BODY(guiBox);

	~guiBox();
	guiBox();

	REFLECT();
	Color32 color = COLOR_WHITE;
	REFLECT();
	AssetPtr<Texture> texture;
	REFLECT();
	bool is_nine_patch = false;
	REFLECT();
	int16_t nine_patch_margin = 0;	// in px

	void update_widget_size() final {
		update_desired_size_from_one_child(this);
	}
	void update_subwidget_positions() final {
		update_one_child_position(this);
	}

	void paint(UIBuilder& b) final {
		b.draw_solid_rect(
			ws_position,
			ws_size,
			color
		);
	}
};


class guiImage : public guiBase
{
public:
	CLASS_BODY(guiImage);

#ifdef EDITOR_BUILD
	virtual const char* get_editor_outliner_icon() const { return "eng/editor/guiimage.png"; }
#endif
};

class guiText : public guiBase
{
public:
	CLASS_BODY(guiText);
	guiText() {
		recieve_mouse = guiMouseFilter::Ignore;
	}

#ifdef EDITOR_BUILD
	virtual const char* get_editor_outliner_icon() const { return "eng/editor/guitext.png"; }
#endif

	REFLECT();
	Color32 color = COLOR_WHITE;
	REFLECT();
	std::string text;
	REFLECT();
	AssetPtr<GuiFont> myFont;
	REFLECT();
	int drop_shadow = 0;

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

		if(drop_shadow!=0)
			b.draw_text(ws_position+glm::ivec2{ drop_shadow,drop_shadow }+ text_offset, ws_size, font, sv, COLOR_BLACK);
		b.draw_text(ws_position+ text_offset, ws_size, font, sv, color);
	}
};