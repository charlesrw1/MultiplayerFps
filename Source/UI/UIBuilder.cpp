#include "UI/UIBuilder.h"


#include <glm/gtc/matrix_transform.hpp>

#include "Widgets/Layouts.h"
#include "Widgets/Visuals.h"
#include "Widgets/Interactables.h"
#include "GameEnginePublic.h"
#include "OnScreenLogGui.h"

#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
#include "Render/RenderWindow.h"

// include


ConfigVar ui_debug_press("ui.debug_press", "0", CVAR_BOOL | CVAR_DEV,"");
ConfigVar ui_draw_text_bbox("ui.draw_text_bbox", "0", CVAR_BOOL | CVAR_DEV,"");

//UIBuilder::UIBuilder(UiSystem* s, UIBuilderImpl* builder)
//{
//	sys = s;
//	impl = builder;
//	auto rect = s->get_viewport().get_rect();
//
//	float x = sys->viewport_position.x;
//	float x1 = x + sys->viewport_size.x;
//	float y1 = sys->viewport_position.y;
//	float y = y1 + sys->viewport_size.y;
//	impl->ViewProj = glm::orthoRH(x, x1, y, y1, -1.0f, 1.0f);
//	
//}

Rect2d GuiHelpers::calc_text_size_no_wrap(const char* str, const GuiFont* font)
{
	int x = 0;
	int y = -font->base;
	while (*str) {
		char c = *str;
		auto find = font->character_to_glyph.find(c);
		if (find == font->character_to_glyph.end()) {
			x += 10;	// empty character
		}
		else
			x += find->second.advance;

		str++;
	}
	return Rect2d(0, y, x, font->lineHeight);
}
Rect2d GuiHelpers::calc_text_size(const char* str, const GuiFont* font, int force_width)
{
	ASSERT(font);

	if (force_width == -1)
		return calc_text_size_no_wrap(str, font);

	std::string currentLine;
	std::string currentWord;

	int x = 0;
	int y = -font->base;
	while (*(str++)) {
		char c = *str;
		if (c == ' ' || c == '\n') {
			auto sz = calc_text_size_no_wrap((currentLine + currentWord).c_str(), font);
			if (sz.w > force_width)
			{
				x = glm::max((int)sz.w, x);
				y += font->lineHeight;
				currentLine = currentWord + " ";
			}
			else
				currentLine += currentWord + " ";
			currentWord.clear();
			if (c == '\n') {
				y += font->lineHeight;
				currentLine.clear();
			}
		}
		else
			currentWord += c;
	}
	if (!currentWord.empty()) {
		currentLine += currentWord;

		x = glm::max((int)calc_text_size_no_wrap(currentLine.c_str(), font).w, x);
		y += font->lineHeight;
	}

	return Rect2d(0,-font->base,x,(y+font->base));
}