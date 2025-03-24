#include "UI/UIBuilder.h"
#include "GUISystemLocal.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Widgets/Layouts.h"
#include "Widgets/Visuals.h"
#include "Widgets/Interactables.h"
#include "GameEnginePublic.h"
#include "OnScreenLogGui.h"

#include "Render/Texture.h"
#include "Render/MaterialPublic.h"

// include


ConfigVar ui_debug_press("ui.debug_press", "0", CVAR_BOOL | CVAR_DEV,"");
ConfigVar ui_draw_text_bbox("ui.draw_text_bbox", "0", CVAR_BOOL | CVAR_DEV,"");

UIBuilder::UIBuilder(GuiSystemLocal* s)
{
	sys = s;
	impl = &s->uiBuilderImpl;
	impl->meshbuilder.Begin();
	float x = sys->viewport_position.x;
	float x1 = x + sys->viewport_size.x;
	float y1 = sys->viewport_position.y;
	float y = y1 + sys->viewport_size.y;
	impl->ViewProj = glm::orthoRH(x, x1, y, y1, -1.0f, 1.0f);
}
UIBuilder::~UIBuilder()
{
}


void UIBuilder::draw_solid_rect(glm::ivec2 global_coords,
	glm::ivec2 size,
	Color32 color)
{
	const int start = impl->meshbuilder.get_i().size();
	
	impl->meshbuilder.Push2dQuad(global_coords, size, glm::vec2(0, 1), glm::vec2(1, -1), color);

	auto mat = (MaterialInstance*)sys->ui_default;

	impl->add_drawcall(mat, start);
}

void UIBuilder::draw_rect_with_texture(
	glm::ivec2 global_coords,
	glm::ivec2 size,
	float alpha,
	const Texture* texture
)
{
	const int start = impl->meshbuilder.get_i().size();

	impl->meshbuilder.Push2dQuad(global_coords, size, glm::vec2(0, 1), glm::vec2(1, -1), COLOR_WHITE);

	auto mat = (MaterialInstance*)sys->ui_default;

	impl->add_drawcall(mat, start, texture);
}

static void get_uvs(glm::vec2& top_left, glm::vec2& sz, int x, int y, int w, int h, const GuiFont* f)
{
	const int tw = f->font_texture->width;
	const int th = f->font_texture->height;
	const float wf = w / (float)tw;
	const float hf = h / (float)th;
	const float xf = x / (float)tw;
	const float yf = y / (float)th;
	top_left = { xf,yf };
	sz = { wf,hf };

}

void UIBuilder::draw_text(
	glm::ivec2 global_coords,
	glm::ivec2 size,	// box for text
	const GuiFont* font,
	std::string_view text, Color32 color /* and alpha*/)
{
	const int start = impl->meshbuilder.get_i().size();

	int x = global_coords.x;
	int y = global_coords.y - font->base;
	for(int i=0;i<text.length();i++) {
		char c = text.at(i);

		auto find = font->character_to_glyph.find(c);
		if (find == font->character_to_glyph.end()) {
			x += 10;	// empty character
		}
		else {

			glm::ivec2 coord = { x,y };
			coord.x += find->second.xofs;
			coord.y += find->second.yofs;
			glm::ivec2 sz = { find->second.w,find->second.h };

			glm::vec2 uv, uv_sz;
			get_uvs(uv, uv_sz, find->second.x, find->second.y, find->second.w, find->second.h, font);
			impl->meshbuilder.Push2dQuad(coord, sz, uv, uv_sz, color
			);
			x += find->second.advance;
		}
	}

	auto mat = (MaterialInstance*)g_fonts.fontDefaultMat.get();


	impl->add_drawcall(mat, start,font->font_texture);
}

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