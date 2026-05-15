// NavMeshBaker — collect static-geometry triangles + volume AABBs, run Recast → Detour.
// Editor-only TU.

#ifdef EDITOR_BUILD

#include "NavMeshBaker.h"
#include "NavMeshVolumeComponent.h"
#include "NavMeshSettingsComponent.h"
#include "RuntimeNavManager.h"

#include "recastnavigation/Recast.h"
#include "recastnavigation/DetourNavMesh.h"
#include "recastnavigation/DetourNavMeshBuilder.h"
#include "recastnavigation/DetourStatus.h"

#include "Game/Components/MeshComponent.h"
#include "Game/Entity.h"
#include "Render/Model.h"
#include "Framework/MathLib.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Debug.h"

#include <vector>
#include <cmath>

namespace {

struct TriangleSoup
{
	std::vector<float> verts;  // xyz triples
	std::vector<int>   tris;   // vertex index triples
};

bool aabb_intersect(const glm::vec3& a_min, const glm::vec3& a_max,
					const glm::vec3& b_min, const glm::vec3& b_max) {
	return !(a_max.x < b_min.x || a_min.x > b_max.x
		  || a_max.y < b_min.y || a_min.y > b_max.y
		  || a_max.z < b_min.z || a_min.z > b_max.z);
}

void collect_static_triangles(const std::vector<NavMeshVolumeComponent*>& volumes,
							  TriangleSoup& out, glm::vec3& bounds_min, glm::vec3& bounds_max) {
	bounds_min = glm::vec3( std::numeric_limits<float>::infinity());
	bounds_max = glm::vec3(-std::numeric_limits<float>::infinity());

	// Volume world-space AABBs (axis-aligned approximation: rotate corners then re-bound).
	struct WorldVol { glm::vec3 mn, mx; };
	std::vector<WorldVol> world_vols;
	world_vols.reserve(volumes.size());
	for (auto* v : volumes) {
		// Volume bounds = owner's world transform applied to a unit cube spanning [-0.5, +0.5].
		// Owner's scale gizmo drives the volume extent (matches GiVolumeComponent convention).
		glm::mat4 tx = v->get_ws_transform();
		glm::vec3 mn( std::numeric_limits<float>::infinity());
		glm::vec3 mx(-std::numeric_limits<float>::infinity());
		for (int corner = 0; corner < 8; corner++) {
			glm::vec3 local((corner & 1) ? 0.5f : -0.5f,
							(corner & 2) ? 0.5f : -0.5f,
							(corner & 4) ? 0.5f : -0.5f);
			glm::vec3 w = glm::vec3(tx * glm::vec4(local, 1.f));
			mn = glm::min(mn, w);
			mx = glm::max(mx, w);
		}
		world_vols.push_back({mn, mx});
		bounds_min = glm::min(bounds_min, mn);
		bounds_max = glm::max(bounds_max, mx);
	}

	auto& objs = eng->get_level()->get_all_objects();
	for (auto kv : objs) {
		auto* mc = kv->cast_to<MeshComponent>();
		if (!mc || !mc->get_nav_static())
			continue;
		const Model* model = mc->get_model();
		if (!model)
			continue;
		const RawMeshData* raw = model->get_raw_mesh_data();
		if (!raw)
			continue;

		// Mesh-level AABB cull against any volume.
		Bounds mb = model->get_bounds();
		glm::mat4 mtx = mc->get_ws_transform();
		// World-bounded box from the model's local AABB.
		glm::vec3 mn( std::numeric_limits<float>::infinity());
		glm::vec3 mx(-std::numeric_limits<float>::infinity());
		for (int corner = 0; corner < 8; corner++) {
			glm::vec3 local((corner & 1) ? mb.bmax.x : mb.bmin.x,
							(corner & 2) ? mb.bmax.y : mb.bmin.y,
							(corner & 4) ? mb.bmax.z : mb.bmin.z);
			glm::vec3 w = glm::vec3(mtx * glm::vec4(local, 1.f));
			mn = glm::min(mn, w);
			mx = glm::max(mx, w);
		}
		bool keep = false;
		for (auto& wv : world_vols) {
			if (aabb_intersect(mn, mx, wv.mn, wv.mx)) { keep = true; break; }
		}
		if (!keep)
			continue;

		// Emit triangles per submesh (model->parts), transforming vertices to world space.
		int base_vert_index = out.verts.size() / 3;
		int vcount = raw->get_num_verticies(sizeof(uint16_t));
		for (int i = 0; i < vcount; i++) {
			glm::vec3 lp = raw->get_vertex_at_index(i).pos;
			glm::vec3 wp = glm::vec3(mtx * glm::vec4(lp, 1.f));
			out.verts.push_back(wp.x);
			out.verts.push_back(wp.y);
			out.verts.push_back(wp.z);
		}
		int parts = model->get_num_parts();
		for (int p = 0; p < parts; p++) {
			const Submesh& sm = model->get_part(p);
			int idx_count = sm.element_count;
			int idx_start = sm.element_offset / (int)sizeof(uint16_t);
			for (int i = 0; i + 2 < idx_count; i += 3) {
				int i0 = raw->get_index_at_index(idx_start + i + 0);
				int i1 = raw->get_index_at_index(idx_start + i + 1);
				int i2 = raw->get_index_at_index(idx_start + i + 2);
				out.tris.push_back(base_vert_index + sm.base_vertex + i0);
				out.tris.push_back(base_vert_index + sm.base_vertex + i1);
				out.tris.push_back(base_vert_index + sm.base_vertex + i2);
			}
		}
	}
}

NavMeshSettingsComponent* find_unique_settings() {
	NavMeshSettingsComponent* found = nullptr;
	int count = 0;
	for (auto kv : eng->get_level()->get_all_objects()) {
		if (auto* s = kv->cast_to<NavMeshSettingsComponent>()) {
			found = s;
			count++;
		}
	}
	if (count == 0) {
		sys_print(Warning, "bake_nav: no NavMeshSettingsComponent in level — add one before baking\n");
		return nullptr;
	}
	if (count > 1) {
		sys_print(Warning, "bake_nav: multiple NavMeshSettingsComponents in level (%d) — using first\n", count);
	}
	return found;
}

} // namespace

bool NavMeshBaker::bake_current_level() {
	if (!eng || !eng->get_level()) {
		sys_print(Warning, "bake_nav: no active level\n");
		return false;
	}
	auto* settings = find_unique_settings();
	if (!settings)
		return false;

	std::vector<NavMeshVolumeComponent*> volumes;
	for (auto kv : eng->get_level()->get_all_objects()) {
		if (auto* v = kv->cast_to<NavMeshVolumeComponent>())
			volumes.push_back(v);
	}
	if (volumes.empty()) {
		sys_print(Warning, "bake_nav: no NavMeshVolumeComponents in level\n");
		return false;
	}

	TriangleSoup soup;
	glm::vec3 bmin, bmax;
	collect_static_triangles(volumes, soup, bmin, bmax);
	if (soup.tris.empty()) {
		sys_print(Warning, "bake_nav: no static triangles fall within any volume\n");
		return false;
	}

	const int nverts = (int)(soup.verts.size() / 3);
	const int ntris  = (int)(soup.tris.size() / 3);

	rcConfig cfg{};
	cfg.cs                     = settings->cell_size;
	cfg.ch                     = settings->cell_height;
	cfg.walkableSlopeAngle     = settings->agent_max_slope_deg;
	cfg.walkableHeight         = (int)std::ceil(settings->agent_height / cfg.ch);
	cfg.walkableClimb          = (int)std::floor(settings->agent_max_climb / cfg.ch);
	cfg.walkableRadius         = (int)std::ceil(settings->agent_radius / cfg.cs);
	cfg.maxEdgeLen             = (int)(settings->edge_max_len / cfg.cs);
	cfg.maxSimplificationError = settings->edge_max_error;
	cfg.minRegionArea          = (int)rcSqr(settings->region_min_size);
	cfg.mergeRegionArea        = (int)rcSqr(settings->region_merge_size);
	cfg.maxVertsPerPoly        = settings->verts_per_poly;
	cfg.detailSampleDist       = settings->detail_sample_dist < 0.9f ? 0 : cfg.cs * settings->detail_sample_dist;
	cfg.detailSampleMaxError   = cfg.ch * settings->detail_sample_max_error;

	float fb_min[3] = {bmin.x, bmin.y, bmin.z};
	float fb_max[3] = {bmax.x, bmax.y, bmax.z};
	rcVcopy(cfg.bmin, fb_min);
	rcVcopy(cfg.bmax, fb_max);
	rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

	rcContext ctx(false);
	rcHeightfield* hf = rcAllocHeightfield();
	if (!hf || !rcCreateHeightfield(&ctx, *hf, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) {
		rcFreeHeightField(hf);
		sys_print(Error, "bake_nav: rcCreateHeightfield failed\n");
		return false;
	}
	std::vector<unsigned char> tri_areas(ntris, 0);
	rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, soup.verts.data(), nverts,
							soup.tris.data(), ntris, tri_areas.data());
	if (!rcRasterizeTriangles(&ctx, soup.verts.data(), nverts, soup.tris.data(),
							  tri_areas.data(), ntris, *hf, cfg.walkableClimb)) {
		rcFreeHeightField(hf);
		sys_print(Error, "bake_nav: rcRasterizeTriangles failed\n");
		return false;
	}

	rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *hf);
	rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf);
	rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *hf);

	rcCompactHeightfield* chf = rcAllocCompactHeightfield();
	if (!chf || !rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf, *chf)) {
		rcFreeHeightField(hf); rcFreeCompactHeightfield(chf);
		sys_print(Error, "bake_nav: rcBuildCompactHeightfield failed\n");
		return false;
	}
	rcFreeHeightField(hf);

	if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf)
		|| !rcBuildDistanceField(&ctx, *chf)
		|| !rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea)) {
		rcFreeCompactHeightfield(chf);
		sys_print(Error, "bake_nav: region build failed\n");
		return false;
	}

	rcContourSet* cset = rcAllocContourSet();
	if (!cset || !rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset)) {
		rcFreeContourSet(cset); rcFreeCompactHeightfield(chf);
		sys_print(Error, "bake_nav: rcBuildContours failed\n");
		return false;
	}

	rcPolyMesh* pmesh = rcAllocPolyMesh();
	if (!pmesh || !rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pmesh)) {
		rcFreePolyMesh(pmesh); rcFreeContourSet(cset); rcFreeCompactHeightfield(chf);
		sys_print(Error, "bake_nav: rcBuildPolyMesh failed\n");
		return false;
	}

	rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
	if (!dmesh || !rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, cfg.detailSampleDist,
										 cfg.detailSampleMaxError, *dmesh)) {
		rcFreePolyMeshDetail(dmesh); rcFreePolyMesh(pmesh);
		rcFreeContourSet(cset); rcFreeCompactHeightfield(chf);
		sys_print(Error, "bake_nav: rcBuildPolyMeshDetail failed\n");
		return false;
	}
	rcFreeContourSet(cset);
	rcFreeCompactHeightfield(chf);

	// Mark every poly as walkable (area type 1 is the Recast default for walkable).
	for (int i = 0; i < pmesh->npolys; i++) {
		if (pmesh->areas[i] == RC_WALKABLE_AREA)
			pmesh->areas[i] = 1;
		if (pmesh->areas[i] != 0)
			pmesh->flags[i] = 0xffff;
	}

	dtNavMeshCreateParams params{};
	params.verts            = pmesh->verts;
	params.vertCount        = pmesh->nverts;
	params.polys            = pmesh->polys;
	params.polyAreas        = pmesh->areas;
	params.polyFlags        = pmesh->flags;
	params.polyCount        = pmesh->npolys;
	params.nvp              = pmesh->nvp;
	params.detailMeshes     = dmesh->meshes;
	params.detailVerts      = dmesh->verts;
	params.detailVertsCount = dmesh->nverts;
	params.detailTris       = dmesh->tris;
	params.detailTriCount   = dmesh->ntris;
	params.walkableHeight   = settings->agent_height;
	params.walkableRadius   = settings->agent_radius;
	params.walkableClimb    = settings->agent_max_climb;
	rcVcopy(params.bmin, pmesh->bmin);
	rcVcopy(params.bmax, pmesh->bmax);
	params.cs               = cfg.cs;
	params.ch               = cfg.ch;
	params.buildBvTree      = true;

	unsigned char* nav_data = nullptr;
	int            nav_size = 0;
	if (!dtCreateNavMeshData(&params, &nav_data, &nav_size)) {
		rcFreePolyMeshDetail(dmesh); rcFreePolyMesh(pmesh);
		sys_print(Error, "bake_nav: dtCreateNavMeshData failed\n");
		return false;
	}
	rcFreePolyMeshDetail(dmesh);
	rcFreePolyMesh(pmesh);

	DtNavMeshPtr mesh(dtAllocNavMesh());
	if (!mesh) {
		dtFree(nav_data);
		sys_print(Error, "bake_nav: dtAllocNavMesh failed\n");
		return false;
	}
	if (dtStatusFailed(mesh->init(nav_data, nav_size, DT_TILE_FREE_DATA))) {
		dtFree(nav_data);
		sys_print(Error, "bake_nav: dtNavMesh::init failed\n");
		return false;
	}
	RuntimeNavManager::inst->set_navmesh_from_loading(std::move(mesh));
	sys_print(Info, "bake_nav: success (%d tris in, %d polys out)\n", ntris, params.polyCount);
	return true;
}

#endif // EDITOR_BUILD
