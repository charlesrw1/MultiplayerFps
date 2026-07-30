#include "Render/Canvas2dTypes.h"
#include "Framework/MeshBuilder.h"
#include "UI/UILoader.h" // GuiFont -- renamed FontAsset in a later build-order step
#include "Render/Texture.h"
#include <glm/gtc/constants.hpp>

namespace {
void get_glyph_uvs(glm::vec2& top_left, glm::vec2& sz, int x, int y, int w, int h, const GuiFont* f) {
	auto size = f->font_texture->get_size();
	const float tw = (float)size.x;
	const float th = (float)size.y;
	top_left = {x / tw, y / th};
	sz = {w / tw, h / th};
}

// no-wrap single-line measurement, matches GuiHelpers::calc_text_size_no_wrap
Rect2d measure_no_wrap(std::string_view sv, const GuiFont* font) {
	int x = 0;
	int y = -font->base;
	for (char c : sv) {
		auto find = font->character_to_glyph.find(c);
		if (find == font->character_to_glyph.end())
			x += 10; // empty character
		else
			x += find->second.advance;
	}
	return Rect2d(0, y, x, font->lineHeight);
}
} // namespace

void Canvas2dGeometry::build_sprite(MeshBuilder& mb, glm::vec2 pos, glm::vec2 size, glm::vec2 uv_ul, glm::vec2 uv_size,
									 Color32 color, float z) {
	int start = mb.GetBaseVertex();
	MbVertex corners[4];
	corners[0].position = glm::vec3(pos, z);
	corners[1].position = glm::vec3(pos.x + size.x, pos.y, z);
	corners[2].position = glm::vec3(pos.x + size.x, pos.y + size.y, z);
	corners[3].position = glm::vec3(pos.x, pos.y + size.y, z);
	corners[0].uv = uv_ul;
	corners[1].uv = glm::vec2(uv_ul.x + uv_size.x, uv_ul.y);
	corners[2].uv = uv_ul + uv_size;
	corners[3].uv = glm::vec2(uv_ul.x, uv_ul.y + uv_size.y);
	for (int i = 0; i < 4; i++)
		corners[i].color = color;
	for (int i = 0; i < 4; i++)
		mb.AddVertex(corners[i]);
	mb.AddTriangle(start + 2, start + 1, start + 0);
	mb.AddTriangle(start + 3, start + 2, start + 0);
}

void Canvas2dGeometry::build_sprite_transformed(MeshBuilder& mb, glm::vec2 size, glm::vec2 uv_ul, glm::vec2 uv_size,
												 Color32 color, glm::mat4 transform, float z) {
	int start = mb.GetBaseVertex();
	glm::vec2 local[4] = {glm::vec2(0, 0), glm::vec2(size.x, 0), size, glm::vec2(0, size.y)};
	glm::vec2 uvs[4] = {uv_ul, glm::vec2(uv_ul.x + uv_size.x, uv_ul.y), uv_ul + uv_size,
						 glm::vec2(uv_ul.x, uv_ul.y + uv_size.y)};
	for (int i = 0; i < 4; i++) {
		glm::vec4 p = transform * glm::vec4(local[i], z, 1.f);
		MbVertex v;
		v.position = glm::vec3(p);
		v.uv = uvs[i];
		v.color = color;
		mb.AddVertex(v);
	}
	mb.AddTriangle(start + 2, start + 1, start + 0);
	mb.AddTriangle(start + 3, start + 2, start + 0);
}

Rect2d Canvas2dGeometry::build_text(MeshBuilder& mb, std::string_view text, glm::vec2 pos, const GuiFont* font,
									 Color32 color, guiAnchor anchor, float z) {
	Rect2d measured = measure_no_wrap(text, font);
	glm::vec2 anchor_vec = UIAnchorPos::get_anchor_vec(anchor);
	glm::vec2 origin = pos - anchor_vec * glm::vec2(measured.w, measured.h);

	int x = (int)origin.x;
	int y = (int)origin.y - font->base;
	for (char c : text) {
		auto find = font->character_to_glyph.find(c);
		if (find == font->character_to_glyph.end()) {
			x += 10; // empty character
			continue;
		}
		glm::ivec2 coord = {x + find->second.xofs, y + find->second.yofs};
		glm::vec2 sz = {(float)find->second.w, (float)find->second.h};
		glm::vec2 uv, uv_sz;
		get_glyph_uvs(uv, uv_sz, find->second.x, find->second.y, find->second.w, find->second.h, font);
		build_sprite(mb, glm::vec2(coord), sz, uv, uv_sz, color, z);
		x += find->second.advance;
	}
	return measured;
}

void Canvas2dGeometry::build_triangle(MeshBuilder& mb, glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color32 color,
									   float z) {
	int start = mb.GetBaseVertex();
	mb.AddVertex(MbVertex(glm::vec3(p0, z), color));
	mb.AddVertex(MbVertex(glm::vec3(p1, z), color));
	mb.AddVertex(MbVertex(glm::vec3(p2, z), color));
	mb.AddTriangle(start, start + 1, start + 2);
}

void Canvas2dGeometry::build_quad(MeshBuilder& mb, glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3,
								   Color32 color, float z) {
	int start = mb.GetBaseVertex();
	mb.AddVertex(MbVertex(glm::vec3(p0, z), color));
	mb.AddVertex(MbVertex(glm::vec3(p1, z), color));
	mb.AddVertex(MbVertex(glm::vec3(p2, z), color));
	mb.AddVertex(MbVertex(glm::vec3(p3, z), color));
	mb.AddQuad(start, start + 1, start + 2, start + 3);
}

void Canvas2dGeometry::build_circle(MeshBuilder& mb, glm::vec2 center, float radius, int segments, Color32 color,
									 float z) {
	if (segments <= 0)
		return;
	const float step = (2.0f * glm::pi<float>()) / segments;
	const int base = mb.GetBaseVertex();
	mb.AddVertex(MbVertex(glm::vec3(center, z), color));
	for (int i = 0; i < segments; i++) {
		float angle = i * step;
		glm::vec2 p = center + glm::vec2(cos(angle), sin(angle)) * radius;
		mb.AddVertex(MbVertex(glm::vec3(p, z), color));
	}
	for (int i = 0; i < segments; i++)
		mb.AddTriangle(base, base + 1 + (i + 1) % segments, base + 1 + i);
}

void Canvas2dGeometry::build_polygon(MeshBuilder& mb, std::span<const glm::vec2> points, Color32 color, float z) {
	if (points.size() < 3)
		return;
	const int base = mb.GetBaseVertex();
	for (auto& p : points)
		mb.AddVertex(MbVertex(glm::vec3(p, z), color));
	for (size_t i = 1; i + 1 < points.size(); i++)
		mb.AddTriangle(base, base + (int)i, base + (int)i + 1);
}

void Canvas2dGeometry::build_line(MeshBuilder& mb, glm::vec2 start, glm::vec2 end, float thickness, Color32 color,
								   float z) {
	glm::vec2 dir = end - start;
	float len = glm::length(dir);
	if (len < 0.001f)
		return;
	dir /= len;
	glm::vec2 perp(-dir.y, dir.x);
	float half = thickness * 0.5f;
	glm::vec2 p0 = start + perp * half;
	glm::vec2 p1 = start - perp * half;
	glm::vec2 p2 = end - perp * half;
	glm::vec2 p3 = end + perp * half;
	build_quad(mb, p0, p3, p2, p1, color, z);
}

void Canvas2dGeometry::build_line_strip(MeshBuilder& mb, std::span<const glm::vec2> points, float thickness,
										 Color32 color, bool closed, float z) {
	if (points.size() < 2)
		return;
	for (size_t i = 0; i + 1 < points.size(); i++)
		build_line(mb, points[i], points[i + 1], thickness, color, z);
	if (closed)
		build_line(mb, points.back(), points.front(), thickness, color, z);
}

void Canvas2dGeometry::build_circle_outline(MeshBuilder& mb, glm::vec2 center, float radius, int segments,
											 float thickness, Color32 color, float z) {
	if (segments <= 0)
		return;
	const float step = (2.0f * glm::pi<float>()) / segments;
	for (int i = 0; i < segments; i++) {
		float a0 = i * step;
		float a1 = (i + 1) * step;
		glm::vec2 pa = center + glm::vec2(cos(a0), sin(a0)) * radius;
		glm::vec2 pb = center + glm::vec2(cos(a1), sin(a1)) * radius;
		build_line(mb, pa, pb, thickness, color, z);
	}
}

void Canvas2dGeometry::build_rect_outline(MeshBuilder& mb, glm::vec2 pos, glm::vec2 size, float thickness,
										   Color32 color, float z) {
	build_line(mb, pos, glm::vec2(pos.x + size.x, pos.y), thickness, color, z);
	build_line(mb, glm::vec2(pos.x, pos.y + size.y), pos + size, thickness, color, z);
	build_line(mb, pos, glm::vec2(pos.x, pos.y + size.y), thickness, color, z);
	build_line(mb, glm::vec2(pos.x + size.x, pos.y), pos + size, thickness, color, z);
}
