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

void Canvas2dVertexArray::add_text(std::string_view text, glm::vec2 pos, const FontAsset* font, Color32 color,
									guiAnchor anchor, float z) {
	Canvas2dGeometry::build_text(cpu, text, pos, font, color, anchor, z);
}

void Canvas2dVertexArray::add_triangle(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color32 color, float z) {
	Canvas2dGeometry::build_triangle(cpu, p0, p1, p2, color, z);
}

void Canvas2dVertexArray::add_quad(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color32 color, float z) {
	Canvas2dGeometry::build_quad(cpu, p0, p1, p2, p3, color, z);
}

void Canvas2dVertexArray::add_gradient_rect(glm::vec2 pos, glm::vec2 size, Color32 top_left, Color32 top_right,
																			 Color32 bottom_right, Color32 bottom_left, float z) {
	Canvas2dGeometry::build_gradient_rect(cpu, pos, size, top_left, top_right, bottom_right, bottom_left, z);
}

void Canvas2dVertexArray::add_textured_polygon(std::span<const glm::vec2> points, std::span<const glm::vec2> uvs,
																						 Color32 color, float z) {
	Canvas2dGeometry::build_textured_polygon(cpu, points, uvs, color, z);
}

void Canvas2dVertexArray::add_mesh(std::span<const MbVertex> vertices, std::span<const uint32_t> indices,
																 glm::mat4 transform) {
	Canvas2dGeometry::build_mesh(cpu, vertices, indices, transform);
}

void Canvas2dVertexArray::add_circle(glm::vec2 center, float radius, int segments, Color32 color, float z) {
	Canvas2dGeometry::build_circle(cpu, center, radius, segments, color, z);
}

void Canvas2dVertexArray::add_polygon(std::span<const glm::vec2> points, Color32 color, float z) {
	Canvas2dGeometry::build_polygon(cpu, points, color, z);
}

void Canvas2dVertexArray::add_rounded_rect(glm::vec2 pos, glm::vec2 size, float radius, int corner_segments,
																				 Color32 color, float z) {
	Canvas2dGeometry::build_rounded_rect(cpu, pos, size, radius, corner_segments, color, z);
}

void Canvas2dVertexArray::add_nine_slice(glm::vec2 pos, glm::vec2 size, glm::ivec2 texture_size, glm::ivec4 borders,
																					Color32 color, float z) {
	Canvas2dGeometry::build_nine_slice(cpu, pos, size, texture_size, borders, color, z);
}

void Canvas2dVertexArray::add_tiled_sprite(glm::vec2 pos, glm::vec2 size, glm::vec2 tile_size, Color32 color, float z) {
	Canvas2dGeometry::build_tiled_sprite(cpu, pos, size, tile_size, color, z);
}

void Canvas2dVertexArray::add_line(glm::vec2 start, glm::vec2 end, float thickness, Color32 color, float z) {
	Canvas2dGeometry::build_line(cpu, start, end, thickness, color, z);
}

void Canvas2dVertexArray::add_line_strip(std::span<const glm::vec2> points, float thickness, Color32 color,
										  bool closed, float z, CanvasLineCap cap, CanvasLineJoin join, int round_segments) {
	Canvas2dGeometry::build_line_strip(cpu, points, thickness, color, closed, z, cap, join, round_segments);
}

void Canvas2dVertexArray::add_circle_outline(glm::vec2 center, float radius, int segments, float thickness,
											  Color32 color, float z) {
	Canvas2dGeometry::build_circle_outline(cpu, center, radius, segments, thickness, color, z);
}

void Canvas2dVertexArray::add_rect_outline(glm::vec2 pos, glm::vec2 size, float thickness, Color32 color, float z) {
	Canvas2dGeometry::build_rect_outline(cpu, pos, size, thickness, color, z);
}
