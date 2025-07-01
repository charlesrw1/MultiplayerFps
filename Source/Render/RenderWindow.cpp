#include "RenderWindow.h"


RenderWindow::RenderWindow()
{
}

RenderWindow::~RenderWindow()
{
}

void RenderWindow::set_view(int x0, int x1, int y0, int y1)
{
}

void RenderWindow::add_scissor_rect(Rect2d rect)
{
	UIDrawCmd cmd;
	cmd.type = UIDrawCmd::Type::SetScissor;
	cmd.sc.enable = true;
	cmd.sc.rect = rect;
	drawCmds.push_back(cmd);
}

void RenderWindow::remove_scissor()
{
	UIDrawCmd cmd;
	cmd.type = UIDrawCmd::Type::SetScissor;
	cmd.sc.enable = false;
	drawCmds.push_back(cmd);
}

void RenderWindow::add_draw_call(const MaterialInstance* mat, int start, const Texture* tex_override)
{
	const int count = meshbuilder.get_i().size() - start;

	UIDrawCall* lastDC = nullptr;
	if (!drawCmds.empty() && drawCmds.back().type == UIDrawCmd::Type::DrawCall) {
		lastDC = &drawCmds.back().dc;
	}

	if (!lastDC || lastDC->mat != mat || lastDC->texOverride != tex_override) {
		UIDrawCall dc;
		dc.index_count = count;
		dc.index_start = start;
		dc.mat = const_cast<MaterialInstance*>(mat);// fix
		dc.texOverride = tex_override;
		UIDrawCmd cmd;
		cmd.type = UIDrawCmd::Type::DrawCall;
		cmd.dc = dc;
		drawCmds.push_back(cmd);
	}
	else {
		lastDC->index_count += count;
	}
}

#include "UI/UILoader.h"
#include "UI/GUISystemPublic.h"
#include "Render/Texture.h"
void RenderWindow::draw(RectangleShape rect_shape)
{
	const int start = meshbuilder.get_i().size();
	meshbuilder.Push2dQuad(rect_shape.rect.get_pos(), rect_shape.rect.get_size(), glm::vec2(0, 1), glm::vec2(1, -1), rect_shape.color);
	auto mat = (MaterialInstance*)UiSystem::inst->get_default_ui_mat();
	add_draw_call(mat, start, rect_shape.texture);
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
void RenderWindow::draw(TextShape text_shape)
{
	if (!text_shape.font)
		text_shape.font = UiSystem::inst->defaultFont;
	const int start = meshbuilder.get_i().size();

	int x = text_shape.rect.x;
	int y = text_shape.rect.y - text_shape.font->base;
	auto text = text_shape.text;
	auto font = text_shape.font;
	for (int i = 0; i < text.length(); i++) {
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
			if (text_shape.with_drop_shadow) {
				glm::ivec2 ofs(text_shape.drop_shadow_ofs);
				meshbuilder.Push2dQuad(coord + ofs, sz, uv, uv_sz, text_shape.drop_shadow_color);
			}
			meshbuilder.Push2dQuad(coord, sz, uv, uv_sz, text_shape.color);
			x += find->second.advance;
		}
	}

	auto mat = (MaterialInstance*)UiSystem::inst->fontDefaultMat;
	add_draw_call(mat, start, font->font_texture);
}
