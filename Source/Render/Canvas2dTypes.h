#pragma once
#include <string_view>
#include <span>
#include <cstdint>
#include "glm/glm.hpp"
#include "Framework/Util.h"
#include "Framework/Rect2d.h"
#include "Render/DrawTypedefs.h"
#include "UI/UiAnchor.h"

class MaterialInstance;
class Texture;
class IGraphicsTexture;
class Canvas2dGpuMesh;
class MeshBuilder;
class FontAsset;

// Per-draw-call GPU state. Two draws only batch together if every field here matches
// (plus contiguity/transform rules -- see Canvas2dRecorder).
struct DrawSettings
{
	const Texture* texture = nullptr;
	BlendState blend = BlendState::BLEND;
	const MaterialInstance* custom_shader = nullptr; // nullptr = system default sprite/text material
	float z = 0.f;
	bool depth_test = false; // opt-in; default matches legacy painter's-order behavior
};

// One contiguous run of indices drawn with one consistent GPU state. Built incrementally
// at record time (see Canvas2dRecorder::submit) -- never sorted.
struct Canvas2dBatch
{
	Texture* target = nullptr;
	Rect2d viewport{};
	glm::mat4 view_proj{1.f};
	bool scissor_enabled = false;
	Rect2d scissor{};
	bool depth_test = false;
	BlendState blend = BlendState::BLEND;
	const MaterialInstance* material = nullptr;
	const Texture* texture = nullptr;

	Canvas2dGpuMesh* source = nullptr;	   // the transient per-frame arena, or one VertexArray's gpu_mesh
	glm::mat4 transform{1.f};			   // only meaningful when source is a persistent VertexArray
	int index_start = 0;
	int index_count = 0;
	int base_vertex = 0;

	// Load-op for this batch's target, stamped from record state at creation time.
	// Canvas2dBackendLocal only honors this for the first batch touching a given
	// target in a flush (see its already_cleared tracking).
	bool wants_clear_color = false;
	Color32 clear_color = COLOR_BLACK;
	bool wants_clear_depth = false;

	// internal bookkeeping for the incremental extend-vs-close decision, not part of
	// the public batch "shape" described in the design doc
	uint64_t state_version = 0;
};

// Geometry builders shared by both the transient (Canvas2d::draw_*) and persistent
// (Canvas2dVertexArray::add_*) recording paths, so the layout logic exists exactly once.
namespace Canvas2dGeometry {
	void build_sprite(MeshBuilder& mb, glm::vec2 pos, glm::vec2 size, glm::vec2 uv_ul, glm::vec2 uv_size, Color32 color,
					   float z);
	void build_sprite_transformed(MeshBuilder& mb, glm::vec2 size, glm::vec2 uv_ul, glm::vec2 uv_size, Color32 color,
								  glm::mat4 transform, float z);
	// returns the measured (unanchored) text rect, in the same units as calc_text_size
	Rect2d build_text(MeshBuilder& mb, std::string_view text, glm::vec2 pos, const FontAsset* font, Color32 color,
					   guiAnchor anchor, float z);
	void build_triangle(MeshBuilder& mb, glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color32 color, float z);
	void build_quad(MeshBuilder& mb, glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color32 color, float z);
	// Arbitrary (non-axis-aligned) textured quad with per-corner UVs -- for geometry that
	// can't be expressed as an affine transform of a rect (e.g. a perspective-projected
	// gizmo face). p0..p3/uv0..uv3 must be given in matching winding order.
	void build_quad_textured(MeshBuilder& mb, glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec2 uv0,
							  glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3, Color32 color, float z);
	void build_circle(MeshBuilder& mb, glm::vec2 center, float radius, int segments, Color32 color, float z);
	void build_polygon(MeshBuilder& mb, std::span<const glm::vec2> points, Color32 color, float z);
	void build_line(MeshBuilder& mb, glm::vec2 start, glm::vec2 end, float thickness, Color32 color, float z);
	void build_line_strip(MeshBuilder& mb, std::span<const glm::vec2> points, float thickness, Color32 color,
						   bool closed, float z);
	void build_circle_outline(MeshBuilder& mb, glm::vec2 center, float radius, int segments, float thickness,
							   Color32 color, float z);
	void build_rect_outline(MeshBuilder& mb, glm::vec2 pos, glm::vec2 size, float thickness, Color32 color, float z);
}
