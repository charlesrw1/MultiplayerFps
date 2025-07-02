#include "UI/UIBuilder.h"


#include <glm/gtc/matrix_transform.hpp>

#include "Widgets/Layouts.h"
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

Rect2d GuiHelpers::calc_text_size_no_wrap(std::string_view sv, const GuiFont* font)
{
	if (!font) font = UiSystem::inst->defaultFont;
	assert(font);
	int x = 0;
	int y = -font->base;
	for (char c : sv) {
		auto find = font->character_to_glyph.find(c);
		if (find == font->character_to_glyph.end()) {
			x += 10;	// empty character
		}
		else
			x += find->second.advance;
	}
	return Rect2d(0, y, x, font->lineHeight);
}
glm::ivec2 GuiHelpers::calc_layout(glm::ivec2 in_pos, guiAnchor anchor, Rect2d viewport)
{
	auto sz = viewport.get_size();
	switch (anchor)
	{
	case guiAnchor::TopLeft: return in_pos;
		break;
	case guiAnchor::TopRight: return { sz.x + in_pos.x, in_pos.y };
		break;
	case guiAnchor::BotLeft: return { in_pos.x,sz.y + in_pos.y };
		break;
	case guiAnchor::BotRight: return { sz.x + in_pos.x,sz.y + in_pos.y };
		break;
	case guiAnchor::Center: return { in_pos.x+sz.x/2,sz.y/2 + in_pos.y };
		break;
	case guiAnchor::Top: return { in_pos.x + sz.x / 2,in_pos.y };
		break;
	case guiAnchor::Bottom: return { in_pos.x + sz.x / 2,in_pos.y  + sz.y};
		break;
	case guiAnchor::Right: return { in_pos.x + sz.x,in_pos.y + sz.y/2 };
		break;
	case guiAnchor::Left:  return { in_pos.x ,in_pos.y + sz.y / 2 };
		break;
	default: return in_pos;
		break;
	}
}
Rect2d GuiHelpers::calc_text_size(std::string_view sv, const GuiFont* font, int force_width)
{
	if (!font) font = UiSystem::inst->defaultFont;
	ASSERT(font);

	if (force_width == -1)
		return calc_text_size_no_wrap(sv, font);

	std::string currentLine;
	std::string currentWord;

	int x = 0;
	int y = -font->base;
	for (char c : sv) {
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