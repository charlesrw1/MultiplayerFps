// NavMeshDebugDraw — cvar-driven runtime visualisation of the navmesh + agents.
// Filled navmesh polys go through a Particle_Object with navmesh_fill.mm (translucent green);
// wireframe edges + agent paths + AABBs use Debug::add_line/add_box/add_sphere (1-frame lifetime).
//
// @docs [[navigation#debug-draw]]

#include "NavMeshDebugDraw.h"
#include "RuntimeNavManager.h"
#include "NavAgentComponent.h"
#include "NavMeshVolumeComponent.h"

#include "recastnavigation/DetourNavMesh.h"

#include "Framework/Config.h"
#include "Framework/Util.h"
#include "Framework/MeshBuilder.h"
#include "Debug.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "Render/MaterialPublic.h"

namespace {
ConfigVar nav_debug_navmesh("nav.debug.navmesh", "0", CVAR_BOOL, "Overlay baked navmesh polys");
ConfigVar nav_debug_tile_grid("nav.debug.tile_grid", "0", CVAR_BOOL, "Overlay navmesh tile boundaries");
ConfigVar nav_debug_agents("nav.debug.agents", "0", CVAR_BOOL, "Draw paths + arrive radii for every NavAgent");
ConfigVar nav_debug_volumes("nav.debug.volumes", "0", CVAR_BOOL, "Wireframe NavMeshVolumeComponent AABBs");

const Color32 kNavmeshEdge   = Color32(0, 0xc0, 0x40, 0xff);
const Color32 kNavmeshFill   = Color32(0, 0xff, 0x40, 0xff); // color attribute carried on the meshbuilder
const Color32 kTileBoundary  = Color32(0xff, 0xa0, 0, 0xff);
const Color32 kAgentPath     = Color32(0x40, 0x80, 0xff, 0xff);
const Color32 kAgentArrive   = Color32(0xff, 0xff, 0, 0xff);
const Color32 kVolumeWire    = Color32(0, 0xff, 0xff, 0xff);

// Persistent renderer-scene slots for the navmesh overlay.
//   * Particle_Object draws GL_TRIANGLES from a MeshBuilder + material — the only existing path
//     for filled debug geometry. Used for the translucent green fill.
//   * MeshBuilder_Object draws GL_LINES; we register a second slot for the edge wireframe so we
//     can set depth_tested = true (the global Debug::add_line context is depth-disabled and
//     shared, so we can't reuse it for depth-tested lines).
struct NavmeshOverlayRenderer
{
	MeshBuilder fill_mb;
	MeshBuilder line_mb;
	handle<Particle_Object>    fill_handle;
	handle<MeshBuilder_Object> line_handle;
	MaterialInstance* fill_material = nullptr;
	bool registered = false;
};
NavmeshOverlayRenderer g_nav_overlay;

void ensure_overlay_registered() {
	if (g_nav_overlay.registered)
		return;
	g_nav_overlay.fill_material = MaterialInstance::load("navmesh_fill.mm");
	if (!g_nav_overlay.fill_material) {
		sys_print(Warning, "nav.debug.navmesh: failed to load navmesh_fill.mm\n");
		return;
	}
	g_nav_overlay.fill_handle = idraw->get_scene()->register_particle_obj();
	g_nav_overlay.line_handle = idraw->get_scene()->register_meshbuilder();
	g_nav_overlay.registered  = true;
}

void release_overlay() {
	if (!g_nav_overlay.registered)
		return;
	idraw->get_scene()->remove_particle_obj(g_nav_overlay.fill_handle);
	idraw->get_scene()->remove_meshbuilder(g_nav_overlay.line_handle);
	g_nav_overlay.registered = false;
	g_nav_overlay.fill_mb.Begin(); g_nav_overlay.fill_mb.End();
	g_nav_overlay.line_mb.Begin(); g_nav_overlay.line_mb.End();
}

void build_overlay_geometry(const dtNavMesh& mesh) {
	auto& fmb = g_nav_overlay.fill_mb;
	auto& lmb = g_nav_overlay.line_mb;
	fmb.Begin();
	lmb.Begin();
	// Fan-triangulate each poly's own vertex ring. Detail-mesh subdivisions would track ground
	// height more precisely, but for a debug overlay (raised 2 cm above the surface) the poly
	// fan is plenty.
	for (int t = 0; t < mesh.getMaxTiles(); t++) {
		const dtMeshTile* tile = mesh.getTile(t);
		if (!tile || !tile->header)
			continue;
		const dtMeshHeader* header = tile->header;
		for (int p = 0; p < header->polyCount; p++) {
			const dtPoly& poly = tile->polys[p];
			if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
				continue;
			const int vcount = poly.vertCount;
			if (vcount < 3)
				continue;
			const int fill_base = fmb.GetBaseVertex();
			for (int v = 0; v < vcount; v++) {
				const float* a = &tile->verts[poly.verts[v] * 3];
				fmb.AddVertex(MbVertex(glm::vec3(a[0], a[1] + 0.02f, a[2]), kNavmeshFill));
			}
			for (int v = 1; v + 1 < vcount; v++) {
				fmb.AddTriangle(fill_base, fill_base + v, fill_base + v + 1);
			}
			// Edge wireframe — slightly higher than the fill so it sits on top.
			for (int v = 0; v < vcount; v++) {
				const float* a = &tile->verts[poly.verts[v] * 3];
				const float* b = &tile->verts[poly.verts[(v + 1) % vcount] * 3];
				lmb.PushLine(glm::vec3(a[0], a[1] + 0.03f, a[2]),
							 glm::vec3(b[0], b[1] + 0.03f, b[2]),
							 kNavmeshEdge);
			}
		}
	}
	fmb.End();
	lmb.End();
}

void update_overlay_pass(bool enable, const dtNavMesh* mesh) {
	if (!enable || !mesh) {
		release_overlay();
		return;
	}
	ensure_overlay_registered();
	if (!g_nav_overlay.registered)
		return;

	build_overlay_geometry(*mesh);

	Particle_Object po;
	po.meshbuilder = &g_nav_overlay.fill_mb;
	po.material    = g_nav_overlay.fill_material;
	po.transform   = glm::mat4(1.f); // geometry already in world space
	idraw->get_scene()->update_particle_obj(g_nav_overlay.fill_handle, po);

	MeshBuilder_Object lo;
	lo.meshbuilder          = &g_nav_overlay.line_mb;
	lo.transform            = glm::mat4(1.f);
	lo.visible              = true;
	lo.depth_tested         = true;
	lo.use_background_color = false;
	idraw->get_scene()->update_meshbuilder(g_nav_overlay.line_handle, lo);
}

// Edge wireframe lives in build_overlay_geometry — see line_mb usage there. The old
// Debug::add_line-based draw_navmesh_edges was removed because that path bypasses depth-testing.

void draw_tile_grid(const dtNavMesh& mesh) {
	for (int t = 0; t < mesh.getMaxTiles(); t++) {
		const dtMeshTile* tile = mesh.getTile(t);
		if (!tile || !tile->header)
			continue;
		const dtMeshHeader* h = tile->header;
		glm::vec3 mn(h->bmin[0], h->bmin[1], h->bmin[2]);
		glm::vec3 mx(h->bmax[0], h->bmax[1], h->bmax[2]);
		glm::vec3 center = (mn + mx) * 0.5f;
		glm::vec3 size   = (mx - mn);
		Debug::add_box(center, size, kTileBoundary, 0.f, false);
	}
}

void draw_agents() {
	auto* level = eng->get_level();
	if (!level)
		return;
	for (auto kv : level->get_all_objects()) {
		auto* agent = kv->cast_to<NavAgentComponent>();
		if (!agent)
			continue;
		const bool draw_this = nav_debug_agents.get_bool() || agent->debug_draw_path;
		if (!draw_this)
			continue;
		const auto& corners = agent->get_corners();
		for (size_t i = 1; i < corners.size(); i++)
			Debug::add_line(corners[i - 1], corners[i], kAgentPath, 0.f, false);
		if (!corners.empty()) {
			Debug::add_sphere(corners.back(), agent->get_arrive_radius(), kAgentArrive, 0.f, false);
		}
	}
}

void draw_volumes() {
	auto* level = eng->get_level();
	if (!level)
		return;
	for (auto kv : level->get_all_objects()) {
		auto* v = kv->cast_to<NavMeshVolumeComponent>();
		if (!v)
			continue;
		// Unit cube [-0.5, +0.5] transformed by the owner's world transform.
		const glm::mat4 tx = v->get_ws_transform();
		glm::vec3 mn( std::numeric_limits<float>::infinity());
		glm::vec3 mx(-std::numeric_limits<float>::infinity());
		for (int c = 0; c < 8; c++) {
			glm::vec3 local((c & 1) ? 0.5f : -0.5f,
							(c & 2) ? 0.5f : -0.5f,
							(c & 4) ? 0.5f : -0.5f);
			glm::vec3 w = glm::vec3(tx * glm::vec4(local, 1.f));
			mn = glm::min(mn, w); mx = glm::max(mx, w);
		}
		Debug::add_box((mn + mx) * 0.5f, mx - mn, kVolumeWire, 0.f, false);
	}
}
} // namespace

void NavDebugDraw::tick() {
	const bool agents_on  = nav_debug_agents.get_bool();
	const bool volumes_on = nav_debug_volumes.get_bool();
	const bool mesh_on    = nav_debug_navmesh.get_bool();
	const bool tiles_on   = nav_debug_tile_grid.get_bool();

	const dtNavMesh* mesh = (RuntimeNavManager::inst && RuntimeNavManager::inst->has_navmesh())
								? RuntimeNavManager::inst->get_navmesh()
								: nullptr;

	// Persistent renderer-scene slots — always synced so toggling the cvar releases the slot.
	// Both fill triangles and wireframe edges go through here so the edges are depth-tested
	// (Debug::add_line shares a depth-disabled meshbuilder slot used by every Debug shape).
	update_overlay_pass(mesh_on, mesh);

	if (!agents_on && !volumes_on && !mesh_on && !tiles_on) {
		draw_agents();
		return;
	}
	if (volumes_on)
		draw_volumes();
	if (mesh && tiles_on)
		draw_tile_grid(*mesh);
	draw_agents();
}
