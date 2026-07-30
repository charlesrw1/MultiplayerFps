#include "Render/Canvas2dVertexArray.h"
#include "Render/Canvas2dTypes.h"

void Canvas2dVertexArray::add_sprite(glm::vec2 pos, glm::vec2 size, glm::vec2 uv_ul, glm::vec2 uv_size, Color32 color,
									  float z) {
	Canvas2dGeometry::build_sprite(cpu, pos, size, uv_ul, uv_size, color, z);
}

void Canvas2dVertexArray::add_sprite_transformed(glm::vec2 size, glm::vec2 uv_ul, glm::vec2 uv_size, Color32 color,
												  glm::mat4 transform, float z) {
	Canvas2dGeometry::build_sprite_transformed(cpu, size, uv_ul, uv_size, color, transform, z);
}

void Canvas2dVertexArray::add_text(std::string_view text, glm::vec2 pos, const GuiFont* font, Color32 color,
									guiAnchor anchor, float z) {
	Canvas2dGeometry::build_text(cpu, text, pos, font, color, anchor, z);
}

void Canvas2dVertexArray::add_triangle(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color32 color, float z) {
	Canvas2dGeometry::build_triangle(cpu, p0, p1, p2, color, z);
}

void Canvas2dVertexArray::add_quad(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color32 color, float z) {
	Canvas2dGeometry::build_quad(cpu, p0, p1, p2, p3, color, z);
}

void Canvas2dVertexArray::add_circle(glm::vec2 center, float radius, int segments, Color32 color, float z) {
	Canvas2dGeometry::build_circle(cpu, center, radius, segments, color, z);
}

void Canvas2dVertexArray::add_polygon(std::span<const glm::vec2> points, Color32 color, float z) {
	Canvas2dGeometry::build_polygon(cpu, points, color, z);
}

void Canvas2dVertexArray::add_line(glm::vec2 start, glm::vec2 end, float thickness, Color32 color, float z) {
	Canvas2dGeometry::build_line(cpu, start, end, thickness, color, z);
}

void Canvas2dVertexArray::add_line_strip(std::span<const glm::vec2> points, float thickness, Color32 color,
										  bool closed, float z) {
	Canvas2dGeometry::build_line_strip(cpu, points, thickness, color, closed, z);
}

void Canvas2dVertexArray::add_circle_outline(glm::vec2 center, float radius, int segments, float thickness,
											  Color32 color, float z) {
	Canvas2dGeometry::build_circle_outline(cpu, center, radius, segments, thickness, color, z);
}

void Canvas2dVertexArray::add_rect_outline(glm::vec2 pos, glm::vec2 size, float thickness, Color32 color, float z) {
	Canvas2dGeometry::build_rect_outline(cpu, pos, size, thickness, color, z);
}
