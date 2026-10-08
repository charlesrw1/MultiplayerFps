#pragma once
#include <string_view>
#include <span>
#include "glm/glm.hpp"
#include "Framework/Util.h"
#include "Framework/MeshBuilder.h"
#include "Render/Canvas2dGpuMesh.h"
#include "Render/Canvas2dTypes.h"
#include "UI/UiAnchor.h"

class FontAsset;

// User-facing persistent 2d geometry buffer (tilemaps, etc.) -- CPU accumulation via
// `cpu`, uploaded to the GPU only on an explicit upload() call, never implicitly by
// Canvas2d's per-frame flush. Deliberately not REF/Lua-exposed: building large
// persistent geometry is a C++/gameplay-component-level task in this engine (matching
// e.g. MeshBuilderComponent), not a scripting one.
class Canvas2dVertexArray
{
public:
	~Canvas2dVertexArray() { gpu_mesh.release(); }
	Canvas2dVertexArray() = default;
	Canvas2dVertexArray(const Canvas2dVertexArray&) = delete;
	Canvas2dVertexArray& operator=(const Canvas2dVertexArray&) = delete;
	// Recorded batches retain the address of gpu_mesh, so moving an array while it may
	// be queued would be just as unsafe as copying its owning GPU pointers.
	Canvas2dVertexArray(Canvas2dVertexArray&&) = delete;
	Canvas2dVertexArray& operator=(Canvas2dVertexArray&&) = delete;

	// Filled shapes (two triangles / triangle fan, added to the index buffer as TRIANGLES)
	void add_sprite(glm::vec2 pos, glm::vec2 size, glm::vec2 uv_ul, glm::vec2 uv_size, Color32 color, float z = 0.f);
	void add_sprite_transformed(glm::vec2 size, glm::vec2 uv_ul, glm::vec2 uv_size, Color32 color,
								glm::mat4 transform, float z = 0.f);
	void add_text(std::string_view text, glm::vec2 pos, const FontAsset* font, Color32 color,
				  guiAnchor anchor = guiAnchor::TopLeft, float z = 0.f);
	void add_triangle(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color32 color, float z = 0.f);
	void add_quad(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color32 color,
				  float z = 0.f); // arbitrary (non-axis-aligned) quad
	void add_gradient_rect(glm::vec2 pos, glm::vec2 size, Color32 top_left, Color32 top_right,
						   Color32 bottom_right, Color32 bottom_left, float z = 0.f);
	void add_textured_polygon(std::span<const glm::vec2> points, std::span<const glm::vec2> uvs, Color32 color,
							  float z = 0.f);
	void add_mesh(std::span<const MbVertex> vertices, std::span<const uint32_t> indices,
			  glm::mat4 transform = glm::mat4(1.f));
	void add_circle(glm::vec2 center, float radius, int segments, Color32 color, float z = 0.f); // filled fan
	void add_polygon(std::span<const glm::vec2> points, Color32 color, float z = 0.f);			// convex fan
	void add_rounded_rect(glm::vec2 pos, glm::vec2 size, float radius, int corner_segments, Color32 color,
						  float z = 0.f);
	void add_nine_slice(glm::vec2 pos, glm::vec2 size, glm::ivec2 texture_size, glm::ivec4 borders, Color32 color,
						float z = 0.f);
	void add_tiled_sprite(glm::vec2 pos, glm::vec2 size, glm::vec2 tile_size, Color32 color, float z = 0.f);

	// Stroked/outline shapes (emitted as thickened quads, still TRIANGLES -- one index
	// buffer for the whole array)
	void add_line(glm::vec2 start, glm::vec2 end, float thickness, Color32 color, float z = 0.f);
	void add_line_strip(std::span<const glm::vec2> points, float thickness, Color32 color, bool closed = false,
						 float z = 0.f, CanvasLineCap cap = CanvasLineCap::Butt,
						 CanvasLineJoin join = CanvasLineJoin::Miter, int round_segments = 8);
	void add_circle_outline(glm::vec2 center, float radius, int segments, float thickness, Color32 color,
							 float z = 0.f);
	void add_rect_outline(glm::vec2 pos, glm::vec2 size, float thickness, Color32 color, float z = 0.f);

	void clear() { cpu.Begin(); }
	// index-count snapshot for sub-range redraw: lets a caller build e.g. a tilemap in
	// one array, remember an offset partway through, and later issue two
	// Canvas2d::draw_vertex_array calls over sub-ranges with different DrawSettings from
	// one uploaded buffer.
	int get_element_offset() { return (int)cpu.get_i().size(); }
	int get_element_count() const { return (int)cpu.get_i().size(); }
	bool is_uploaded() const { return gpu_mesh.vao != nullptr; }
	void upload() { gpu_mesh.upload_from(cpu); }

	MeshBuilder cpu;
	Canvas2dGpuMesh gpu_mesh;
};
