#include "RenderWindow.h"
#include <glm/gtc/constants.hpp>

RenderWindow::RenderWindow() {}

RenderWindow::~RenderWindow() {}

void RenderWindow::set_view(int x0, int x1, int y0, int y1) {}

void RenderWindow::add_scissor_rect(Rect2d rect) {
	UIDrawCmdUnion cmd;
	cmd.type = UiDrawCmdType::SetScissor;
	cmd.scissorCmd.x = rect.x;
	cmd.scissorCmd.y = rect.y;
	cmd.scissorCmd.w = rect.w;
	cmd.scissorCmd.h = rect.h;
	drawCmds.push_back(cmd);
}

void RenderWindow::remove_scissor() {
	UIDrawCmdUnion cmd;
	cmd.type = UiDrawCmdType::ClearScissor;
	drawCmds.push_back(cmd);
}

void RenderWindow::add_draw_call(const MaterialInstance* mat, int start, const Texture* tex_override) {
	const int count = meshbuilder.get_i().size() - start;

	UIDrawCmdUnion* lastDC = nullptr;
	if (!drawCmds.empty() && drawCmds.back().type == UiDrawCmdType::DrawCall) {
		lastDC = &drawCmds.back();
	}
	if (!lastDC || cur_material != mat || cur_tex_0 != tex_override) {
		if (cur_material != mat) {
			cur_material = (MaterialInstance*)mat;
			UIDrawCmdUnion cmd = {};
			cmd.type = UiDrawCmdType::SetPipeline;
			cmd.pipelineCmd.mat = cur_material;
			drawCmds.push_back(cmd);
		}
		cur_tex_0 = (Texture*)tex_override;
		UIDrawCmdUnion cmd = {};
		cmd.type = UiDrawCmdType::SetTexture;
		cmd.textureCmd.binding = 0;
		cmd.textureCmd.tex = cur_tex_0;
		drawCmds.push_back(cmd);

		cmd = {};
		cmd.type = UiDrawCmdType::DrawCall;
		cmd.drawCmd.index_count = count;
		cmd.drawCmd.index_start = start;
		cmd.drawCmd.base_vertex = 0;
		drawCmds.push_back(cmd);
	} else {
		lastDC->drawCmd.index_count += count;
	}
}

#include "UI/UILoader.h"
#include "UI/GUISystemPublic.h"
#include "Render/Texture.h"
void RenderWindow::draw(RectangleShape rect_shape) {
	const int start = meshbuilder.get_i().size();
	meshbuilder.Push2dQuad(rect_shape.rect.get_pos(), rect_shape.rect.get_size(), glm::vec2(0, 1), glm::vec2(1, -1),
						   rect_shape.color);
	auto mat = (MaterialInstance*)UiSystem::inst->get_default_ui_mat();
	add_draw_call(mat, start, rect_shape.texture);
}

static void get_uvs(glm::vec2& top_left, glm::vec2& sz, int x, int y, int w, int h, const GuiFont* f) {
	auto size = f->font_texture->get_size();
	const int tw = size.x;
	const int th = size.y;
	const float wf = w / (float)tw;
	const float hf = h / (float)th;
	const float xf = x / (float)tw;
	const float yf = y / (float)th;
	top_left = {xf, yf};
	sz = {wf, hf};
}

void TextShape::draw_text_to_meshbuilder(const TextShape& text_shape, MeshBuilder& meshbuilder) {
	int x = text_shape.rect.x;
	int y = text_shape.rect.y - text_shape.font->base;
	auto text = text_shape.text;
	auto font = text_shape.font;
	for (int i = 0; i < text.length(); i++) {
		char c = text.at(i);

		auto find = font->character_to_glyph.find(c);
		if (find == font->character_to_glyph.end()) {
			x += 10; // empty character
		} else {

			glm::ivec2 coord = {x, y};
			coord.x += find->second.xofs;
			coord.y += find->second.yofs;
			glm::ivec2 sz = {find->second.w, find->second.h};

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
}

void RenderWindow::draw(TextShape text_shape) {
	if (!text_shape.font)
		text_shape.font = UiSystem::inst->defaultFont;

	const int start = meshbuilder.get_i().size();
	TextShape::draw_text_to_meshbuilder(text_shape, meshbuilder);
	auto mat = (MaterialInstance*)UiSystem::inst->fontDefaultMat;
	add_draw_call(mat, start, text_shape.font->font_texture.get());
}

void RenderWindow::draw(LineShape line_shape) {
	const int start = meshbuilder.get_i().size();
	glm::vec2 a(line_shape.start);
	glm::vec2 b(line_shape.end);
	glm::vec2 dir = b - a;
	float len = glm::length(dir);
	if (len < 0.001f)
		return;
	dir /= len;
	glm::vec2 perp(-dir.y, dir.x);
	float half = line_shape.thickness * 0.5f;
	glm::vec2 p0 = a + perp * half;
	glm::vec2 p1 = a - perp * half;
	glm::vec2 p2 = b - perp * half;
	glm::vec2 p3 = b + perp * half;
	int base = meshbuilder.GetBaseVertex();
	meshbuilder.AddVertex(MbVertex(glm::vec3(p0, 0), line_shape.color));
	meshbuilder.AddVertex(MbVertex(glm::vec3(p1, 0), line_shape.color));
	meshbuilder.AddVertex(MbVertex(glm::vec3(p2, 0), line_shape.color));
	meshbuilder.AddVertex(MbVertex(glm::vec3(p3, 0), line_shape.color));
	meshbuilder.AddQuad(base, base + 1, base + 2, base + 3);
	auto mat = (MaterialInstance*)UiSystem::inst->get_default_ui_mat();
	add_draw_call(mat, start, nullptr);
}

void RenderWindow::draw(CircleShape circle_shape) {
	const int start = meshbuilder.get_i().size();
	if (circle_shape.filled) {
		meshbuilder.Push2dCircle(glm::vec2(circle_shape.center), (float)circle_shape.radius, circle_shape.segments,
								 circle_shape.color);
	} else {
		const float step = (2.0f * glm::pi<float>()) / circle_shape.segments;
		for (int i = 0; i < circle_shape.segments; i++) {
			float a0 = i * step;
			float a1 = (i + 1) * step;
			glm::vec2 pa = glm::vec2(circle_shape.center) + glm::vec2(cos(a0), sin(a0)) * (float)circle_shape.radius;
			glm::vec2 pb = glm::vec2(circle_shape.center) + glm::vec2(cos(a1), sin(a1)) * (float)circle_shape.radius;
			int base = meshbuilder.GetBaseVertex();
			meshbuilder.AddVertex(MbVertex(glm::vec3(pa, 0), circle_shape.color));
			meshbuilder.AddVertex(MbVertex(glm::vec3(pb, 0), circle_shape.color));
			meshbuilder.AddLine(base, base + 1);
		}
	}
	auto mat = (MaterialInstance*)UiSystem::inst->get_default_ui_mat();
	add_draw_call(mat, start, nullptr);
}
