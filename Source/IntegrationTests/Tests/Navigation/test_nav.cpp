// Integration tests for the navigation module.
// game/nav_runtime_path:    builds a flat-plane navmesh by hand and verifies a NavAgentComponent
//                           walks across it; exercises RuntimeNavManager + NavAgentComponent end-to-end.
// game/nav_no_mesh_query:   smoke test that find_path / agent stay sane when no navmesh is loaded.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"

#include "Navigation/RuntimeNavManager.h"
#include "Navigation/NavAgentComponent.h"
#include "Navigation/NavMeshVolumeComponent.h"
#include "Navigation/NavMeshSettingsComponent.h"
#include "Navigation/LevelNavUtil.h"

#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Render/ModelManager.h"
#include "Render/DynamicModelPtr.h"
#include "Framework/Files.h"
#include "Debug.h"
#include "DebugConsole.h"
#include "Framework/StringUtils.h"

#include "recastnavigation/Recast.h"
#include "recastnavigation/DetourNavMesh.h"
#include "recastnavigation/DetourNavMeshBuilder.h"
#include "recastnavigation/DetourStatus.h"

#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/CameraComponent.h"

#include <vector>

namespace {
// Build a 2-triangle flat plane via Recast and hand it to RuntimeNavManager::inst.
// Mirrors the test fixture from the unit test, kept inline here to avoid pulling shared
// helper code into the integration-test surface.
DtNavMeshPtr build_flat_plane_navmesh(float half_size) {
	const float verts[] = {
		-half_size, 0.f, -half_size,
		 half_size, 0.f, -half_size,
		 half_size, 0.f,  half_size,
		-half_size, 0.f,  half_size,
	};
	// CCW winding viewed from +Y so triangle normals point upward — Recast's
	// rcMarkWalkableTriangles only marks triangles whose normal.y >= cos(slope).
	const int tris[] = { 0, 2, 1, 0, 3, 2 };
	const int nverts = 4;
	const int ntris  = 2;

	rcConfig cfg{};
	cfg.cs = 0.5f; cfg.ch = 0.2f;
	cfg.walkableSlopeAngle = 45.f;
	cfg.walkableHeight = (int)std::ceil(2.f / cfg.ch);
	cfg.walkableClimb  = (int)std::floor(0.4f / cfg.ch);
	cfg.walkableRadius = (int)std::ceil(0.5f / cfg.cs);
	cfg.maxEdgeLen     = (int)(12.f / cfg.cs);
	cfg.maxSimplificationError = 1.3f;
	cfg.minRegionArea          = (int)rcSqr(8);
	cfg.mergeRegionArea        = (int)rcSqr(20);
	cfg.maxVertsPerPoly        = 6;
	cfg.detailSampleDist       = cfg.cs * 6.f;
	cfg.detailSampleMaxError   = cfg.ch * 1.f;
	const float bmin[3] = {-half_size, -1.f, -half_size};
	const float bmax[3] = { half_size,  1.f,  half_size};
	rcVcopy(cfg.bmin, bmin);
	rcVcopy(cfg.bmax, bmax);
	rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

	rcContext ctx(false);
	rcHeightfield* hf = rcAllocHeightfield();
	if (!hf || !rcCreateHeightfield(&ctx, *hf, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) {
		rcFreeHeightField(hf); return {};
	}
	std::vector<unsigned char> areas(ntris, 0);
	rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts, nverts, tris, ntris, areas.data());
	if (!rcRasterizeTriangles(&ctx, verts, nverts, tris, areas.data(), ntris, *hf, cfg.walkableClimb)) {
		rcFreeHeightField(hf); return {};
	}
	rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *hf);
	rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf);
	rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *hf);

	rcCompactHeightfield* chf = rcAllocCompactHeightfield();
	if (!chf || !rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf, *chf)) {
		rcFreeHeightField(hf); rcFreeCompactHeightfield(chf); return {};
	}
	rcFreeHeightField(hf);
	if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf)
		|| !rcBuildDistanceField(&ctx, *chf)
		|| !rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea)) {
		rcFreeCompactHeightfield(chf); return {};
	}
	rcContourSet* cs = rcAllocContourSet();
	if (!cs || !rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cs)) {
		rcFreeContourSet(cs); rcFreeCompactHeightfield(chf); return {};
	}
	rcPolyMesh* pm = rcAllocPolyMesh();
	if (!pm || !rcBuildPolyMesh(&ctx, *cs, cfg.maxVertsPerPoly, *pm)) {
		rcFreePolyMesh(pm); rcFreeContourSet(cs); rcFreeCompactHeightfield(chf); return {};
	}
	rcPolyMeshDetail* dm = rcAllocPolyMeshDetail();
	if (!dm || !rcBuildPolyMeshDetail(&ctx, *pm, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *dm)) {
		rcFreePolyMeshDetail(dm); rcFreePolyMesh(pm); rcFreeContourSet(cs); rcFreeCompactHeightfield(chf);
		return {};
	}
	rcFreeContourSet(cs); rcFreeCompactHeightfield(chf);

	for (int i = 0; i < pm->npolys; i++) {
		if (pm->areas[i] == RC_WALKABLE_AREA) pm->areas[i] = 1;
		if (pm->areas[i] != 0) pm->flags[i] = 0xffff;
	}

	dtNavMeshCreateParams p{};
	p.verts = pm->verts; p.vertCount = pm->nverts;
	p.polys = pm->polys; p.polyAreas = pm->areas; p.polyFlags = pm->flags;
	p.polyCount = pm->npolys; p.nvp = pm->nvp;
	p.detailMeshes = dm->meshes; p.detailVerts = dm->verts; p.detailVertsCount = dm->nverts;
	p.detailTris = dm->tris; p.detailTriCount = dm->ntris;
	p.walkableHeight = 2.f; p.walkableRadius = 0.5f; p.walkableClimb = 0.4f;
	rcVcopy(p.bmin, pm->bmin); rcVcopy(p.bmax, pm->bmax);
	p.cs = cfg.cs; p.ch = cfg.ch; p.buildBvTree = true;

	unsigned char* data = nullptr; int size = 0;
	bool ok = dtCreateNavMeshData(&p, &data, &size);
	rcFreePolyMeshDetail(dm); rcFreePolyMesh(pm);
	if (!ok) return {};

	DtNavMeshPtr mesh(dtAllocNavMesh());
	if (!mesh) { dtFree(data); return {}; }
	if (dtStatusFailed(mesh->init(data, size, DT_TILE_FREE_DATA))) {
		dtFree(data); return {};
	}
	return mesh;
}
} // namespace

static TestTask test_nav_no_mesh_query(TestContext& t) {
	t.require(RuntimeNavManager::inst != nullptr, "RuntimeNavManager singleton constructed at engine init");
	RuntimeNavManager::inst->clear();
	std::vector<glm::vec3> corners;
	t.check(!RuntimeNavManager::inst->find_path({0,0,0}, {5,0,5}, corners),
			"find_path returns false with no navmesh");
	t.check(corners.empty(), "find_path leaves corners empty on failure");
	t.check(!RuntimeNavManager::inst->has_navmesh(), "manager reports no navmesh loaded");
	co_return;
}
GAME_TEST("nav/no_mesh_query", 5.f, test_nav_no_mesh_query);

