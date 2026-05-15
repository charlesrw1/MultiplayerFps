#include "EditorCube.h"
#include "UI/UIBuilder.h"
#include "Framework/MeshBuilder.h"
#include "Render/RenderWindow.h"
#include "Debug.h"
#include "Assets/AssetDatabase.h"
#include "Render/Texture.h"
#include "UI/GUISystemPublic.h"
#include "Framework/Config.h"
ConfigVar editorcube("editorcube", "0", CVAR_BOOL, "");

// ── Static cube geometry ───────────────────────────────────────────────��────

// 8 unique corners in local space (before any transforms)
static const glm::vec3 k_corner_local[8] = {
	{-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},  // 0-3: z=-1 ring (face 0)
	{-1,-1, 1}, {1,-1, 1}, {1,1, 1}, {-1,1, 1},  // 4-7: z=+1 ring (face 1)
};

static constexpr float R = 0.7071067811865476f; // 1/sqrt(2)

struct CubeEdgeDef {
	int face_a, face_b;     // which two faces share this edge (indices into face_eye_dirs)
	int corner_a, corner_b; // which two corners (indices into k_corner_local)
	glm::vec3 eye_dir;      // normalized average of the two face eye dirs
};

// 12 edges: 4 around each of the front/back rings + 4 vertical connecting edges.
// Eye dirs are the 45-degree camera positions between two adjacent faces.
static const CubeEdgeDef k_edges[12] = {
	// Front (face 0, +Z) ring
	{0, 5, 0, 1, { 0,-R, R}},  // bottom-front: +Z/-Y
	{0, 3, 1, 2, { R, 0, R}},  // right-front:  +Z/+X
	{0, 4, 2, 3, { 0, R, R}},  // top-front:    +Z/+Y
	{0, 2, 3, 0, {-R, 0, R}},  // left-front:   +Z/-X
	// Back (face 1, -Z) ring
	{1, 5, 4, 5, { 0,-R,-R}},  // bottom-back:  -Z/-Y
	{1, 3, 5, 6, { R, 0,-R}},  // right-back:   -Z/+X
	{1, 4, 6, 7, { 0, R,-R}},  // top-back:     -Z/+Y
	{1, 2, 7, 4, {-R, 0,-R}},  // left-back:    -Z/-X
	// Vertical (connecting rings)
	{2, 5, 0, 4, {-R,-R, 0}},  // bottom-left:  -X/-Y
	{3, 5, 1, 5, { R,-R, 0}},  // bottom-right: +X/-Y
	{3, 4, 2, 6, { R, R, 0}},  // top-right:    +X/+Y
	{2, 4, 3, 7, {-R, R, 0}},  // top-left:     -X/+Y
};

// ── Helpers ─────────────────────────────────────────────────────────────────

static float pt_to_seg_dist(glm::vec2 p, glm::vec2 a, glm::vec2 b)
{
	glm::vec2 ab = b - a;
	float len2 = glm::dot(ab, ab);
	if (len2 < 1e-6f) return glm::length(p - a);
	float t = glm::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f);
	return glm::length(p - (a + t * ab));
}

// ── guiEditorCube ───────────────────────────────────────────────────────────

guiEditorCube::guiEditorCube() {
	const string imgs[6] = {"cube_z", "cube_nz", "cube_nx", "cube_x", "cube_y", "cube_ny"};
	textures.resize(6);
	for (int i = 0; i < 6; i++) {
		string path = "eng/editor/" + imgs[i] + ".png";
		textures.at(i) = g_assets.find<Texture>(path).get();
	}
}

void guiEditorCube::draw(RenderWindow& builder, float dt, glm::ivec2 mouse_pos) {
	auto& mb = builder.meshbuilder;
	auto transform_to_screen = [&](glm::vec3 in_clip) {
		return glm::vec3((in_clip.x + 1) * ws_sz.x * 0.5 + ws_position.x,
						 (in_clip.y + 1) * ws_sz.y * 0.5 + ws_position.y, 0.0);
	};

	glm::vec3 corners[36] = {
		// Front face
		{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,-1,-1},{1,1,-1},{-1,1,-1},
		// Back face
		{-1,-1,1},{-1,1,1},{1,1,1},{-1,-1,1},{1,1,1},{1,-1,1},
		// Left face
		{-1,-1,-1},{-1,1,-1},{-1,1,1},{-1,-1,-1},{-1,1,1},{-1,-1,1},
		// Right face
		{1,-1,-1},{1,-1,1},{1,1,1},{1,-1,-1},{1,1,1},{1,1,-1},
		// Top face
		{-1,1,-1},{1,1,-1},{1,1,1},{-1,1,-1},{1,1,1},{-1,1,1},
		// Bottom face
		{-1,-1,-1},{-1,-1,1},{1,-1,1},{-1,-1,-1},{1,-1,1},{1,-1,-1},
	};
	const glm::vec2 uvs[36] = {
		{0,0},{1,0},{1,1},{0,0},{1,1},{0,1},
		{1,0},{1,1},{0,1},{1,0},{0,1},{0,0},
		{1,0},{1,1},{0,1},{1,0},{0,1},{0,0},
		{0,0},{1,0},{1,1},{0,0},{1,1},{0,1},
		{0,0},{1,0},{1,1},{0,0},{1,1},{0,1},
		{1,0},{1,1},{0,1},{1,0},{0,1},{0,0},
	};

	const glm::mat4 proj = glm::perspective(glm::radians(90.f), 1.f, 0.001f, 100.f);
	const glm::mat3 rotation_matrix = rotation.get_matrix(dt);

	auto project_vertex = [&](glm::vec3 v) -> glm::vec3 {
		v.z *= -1;
		v = rotation_matrix * v;
		v.z -= 7.f;
		glm::vec4 clip = proj * glm::vec4(v, 1.f);
		clip /= clip.w;
		clip.y *= -1;
		clip *= 7.f;
		return transform_to_screen(clip);
	};

	for (int i = 0; i < 36; i++)
		corners[i] = project_vertex(corners[i]);

	// ── Per-face: winding-based front-face test and 2D centroid ─────────────
	for (int i = 0; i < 6; i++) {
		glm::vec2 a(corners[i*6+0]), b(corners[i*6+1]), c(corners[i*6+2]);
		float signed_area = (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
		face_front[i] = (signed_area < 0.f); // CW in screen-space (Y-down) = front-facing
		glm::vec2 cent(0.f);
		for (int j = 0; j < 6; j++) cent += glm::vec2(corners[i*6+j]);
		face_screen_centers[i] = cent / 6.f;
	}

	// ── Project 8 unique corners for edge drawing/hit-testing ───────────────
	for (int i = 0; i < 8; i++) {
		glm::vec3 s = project_vertex(k_corner_local[i]);
		corner_screen_pts[i] = {s.x, s.y};
	}
	for (int i = 0; i < 12; i++)
		edge_visible[i] = face_front[k_edges[i].face_a] || face_front[k_edges[i].face_b];

	draw_valid = true;

	// ── Compute hover ────────────────────────────────────────────────────────
	const HoverResult hover = compute_hover(mouse_pos);

	// ── Draw faces ───────────────────────────────────────────────────────────
	for (int i = 0; i < 6; i++) {
		const int start  = mb.GetBaseVertex();
		const int starti = mb.get_i().size();
		// Tint face if hovered (only when no edge is hovered — edge takes visual priority)
		Color32 tint = (i == hover.face && hover.edge < 0)
			? Color32{255, 230, 100, 255}
			: Color32{255, 255, 255, 255};
		for (int j = 0; j < 6; j++) {
			MbVertex vert;
			vert.position = corners[i * 6 + j];
			vert.uv       = uvs[i * 6 + j];
			vert.color    = tint;
			mb.AddVertex(vert);
		}
		mb.AddTriangle(start + 2, start, start + 1);
		mb.AddTriangle(start + 5, start + 3, start + 4);
		builder.add_draw_call(UiSystem::inst->get_default_ui_mat(), starti, textures.at(i));
	}

	// ── Draw hovered edge highlight (ortho mode only) ─────────────────────
	if (hover.edge >= 0) {
		const glm::ivec2 pa(corner_screen_pts[k_edges[hover.edge].corner_a]);
		const glm::ivec2 pb(corner_screen_pts[k_edges[hover.edge].corner_b]);
		builder.draw(LineShape{pa, pb, 5, {255, 255, 255, 220}});  // white glow
		builder.draw(LineShape{pa, pb, 3, {255, 160,   0, 255}});  // orange core
	}
}

guiEditorCube::HoverResult guiEditorCube::compute_hover(glm::ivec2 mouse_pos) const
{
	HoverResult r;
	if (!draw_valid) return r;

	const glm::vec2 mp(mouse_pos);

	// Edges take priority in ortho mode (8px threshold)
	if (is_ortho) {
		const float edge_thresh = 8.f;
		float best = edge_thresh;
		for (int i = 0; i < 12; i++) {
			if (!edge_visible[i]) continue;
			float d = pt_to_seg_dist(mp, corner_screen_pts[k_edges[i].corner_a],
			                             corner_screen_pts[k_edges[i].corner_b]);
			if (d < best) { best = d; r.edge = i; }
		}
		if (r.edge >= 0) return r;
	}

	// Face: nearest centroid within half the cube size
	const float face_thresh = (ws_sz.x + ws_sz.y) * 0.5f;
	float best = face_thresh;
	for (int i = 0; i < 6; i++) {
		if (!face_front[i]) continue;
		float d = glm::length(face_screen_centers[i] - mp);
		if (d < best) { best = d; r.face = i; }
	}
	return r;
}

guiEditorCube::CubeClickResult guiEditorCube::test_click(glm::ivec2 mouse_pos) const
{
	const HoverResult h = compute_hover(mouse_pos);
	if (h.edge >= 0) return {true, k_edges[h.edge].eye_dir};
	if (h.face >= 0) return {true, face_eye_dirs[h.face]};
	return {};
}
