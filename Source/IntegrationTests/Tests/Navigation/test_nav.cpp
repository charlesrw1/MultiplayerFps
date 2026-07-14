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

static TestTask test_nav_runtime_path(TestContext& t) {
	eng->load_level("eng/template.tmap");
	co_await t.wait_ticks(2);
	t.require(eng->get_level() != nullptr, "level present");
	t.require(RuntimeNavManager::inst != nullptr, "manager constructed");

	auto mesh = build_flat_plane_navmesh(20.f);
	t.require((bool)mesh, "Recast flat-plane fixture built");
	RuntimeNavManager::inst->set_navmesh_from_loading(std::move(mesh));
	t.require(RuntimeNavManager::inst->has_navmesh(), "manager has navmesh after handoff");

	// Direct find_path probe — endpoints land on the plane.
	std::vector<glm::vec3> corners;
	t.require(RuntimeNavManager::inst->find_path({-5,0,-5}, {5,0,5}, corners),
			  "find_path succeeds across plane");
	t.check((int)corners.size() >= 2, "path has start+end corners");

	// Spawn an agent at one corner and ask it to walk to the other.
	Entity* e = eng->get_level()->spawn_entity();
	e->set_ws_position(glm::vec3(-5.f, 0.f, -5.f));
	auto* agent = e->create_component<NavAgentComponent>();
	agent->set_move_speed(20.f);
	agent->set_arrive_radius(0.5f);
	t.require(agent->request_path_to(glm::vec3(5.f, 0.f, 5.f)),
			  "agent built a path to target");

	// Plane is ~14 metres diagonal at 20 m/s — half a second is plenty, but the integration
	// runner's tick rate is variable so spin a tick budget instead.
	const int max_ticks = 240;
	int ticks = 0;
	while (!agent->has_arrived() && ticks < max_ticks) {
		co_await t.wait_ticks(1);
		ticks++;
	}
	t.check(agent->has_arrived(), "agent reached destination within tick budget");
	glm::vec3 final_pos = e->get_ws_position();
	t.check(glm::length(final_pos - glm::vec3(5.f, 0.f, 5.f)) < 1.0f,
			"agent stopped near target position");

	// Cleanup so we don't leak the manager state into other tests.
	RuntimeNavManager::inst->clear();
}
GAME_TEST("nav/runtime_path", 15.f, test_nav_runtime_path);

// Editor bake pipeline end-to-end:
//   1. Open template.tmap, drop a flat-plane MeshComponent (dynamic model),
//      a NavMeshVolumeComponent that covers it, and a NavMeshSettingsComponent.
//   2. Run `bake_nav` console command — exercises the same code path the editor user hits.
//   3. Verify RuntimeNavManager has a usable navmesh + find_path succeeds.
//   4. `save_baked_nav` and confirm the sidecar file appears on disk.
//   5. Clear the manager, re-load via on_scene_load_nav, verify a path still resolves.
static TestTask test_editor_nav_bake(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "level loaded for editor bake test");

	// some comment
	// another line
	// 
	// asdfasdf
	// 
	// Build a 20 x 20 m flat ground quad as a dynamic model. CCW from +Y so the baker
	// keeps the triangles after the slope check.
	ModelBuilder b;
	uint16_t v0 = b.add_vertex({-10.f, 0.f, -10.f}, {0.f, 0.f}, {0.f, 1.f, 0.f});
	uint16_t v1 = b.add_vertex({ 10.f, 0.f, -10.f}, {1.f, 0.f}, {0.f, 1.f, 0.f});
	uint16_t v2 = b.add_vertex({ 10.f, 0.f,  10.f}, {1.f, 1.f}, {0.f, 1.f, 0.f});
	uint16_t v3 = b.add_vertex({-10.f, 0.f,  10.f}, {0.f, 1.f}, {0.f, 1.f, 0.f});
	b.add_triangle(v0, v2, v1);
	b.add_triangle(v0, v3, v2);

	DynamicModelUniquePtr ground(g_modelMgr.create_dynamic_model(b, "test_nav_ground"));
	t.require(ground != nullptr, "dynamic ground model created");

	// Plane mesh entity.
	Entity* mesh_entity = eng->get_level()->spawn_entity();
	auto* mc = mesh_entity->create_component<MeshComponent>();
	mc->set_model(ground.get());
	t.check(mc->get_nav_static(), "nav_static defaults true on new MeshComponent");

	// Volume covering the whole plane. Volume bounds = owner ws transform applied to a unit cube,
	// so scaling the entity to 40 x 8 x 40 covers a 20m-half ground plane with vertical headroom.
	Entity* vol_entity = eng->get_level()->spawn_entity();
	vol_entity->set_ws_scale(glm::vec3(40.f, 8.f, 40.f));
	vol_entity->create_component<NavMeshVolumeComponent>();

	// Settings — defaults are sane; bump up walkable area for the small plane.
	Entity* settings_entity = eng->get_level()->spawn_entity();
	auto* settings = settings_entity->create_component<NavMeshSettingsComponent>();
	t.require(settings != nullptr, "settings component created");

	// Clean up any prior sidecar so we know the save step actually wrote.
	const std::string sidecar = "eng/template.navmesh";
	FileSys::delete_game_file(sidecar);
	t.require(RuntimeNavManager::inst != nullptr, "manager singleton present");
	RuntimeNavManager::inst->clear();

	// Bake via the same console command the editor user types.
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "bake_nav");
	co_await t.wait_ticks(1);
	t.require(RuntimeNavManager::inst->has_navmesh(), "navmesh produced by bake_nav");

	// Query the freshly baked mesh.
	std::vector<glm::vec3> corners;
	t.check(RuntimeNavManager::inst->find_path({-5,0,-5}, {5,0,5}, corners),
			"find_path succeeds against baked plane");
	t.check((int)corners.size() >= 2, "path has start+end corners");

	// Save the sidecar and verify it landed.
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "save_baked_nav");
	co_await t.wait_ticks(1);
	t.check(FileSys::does_file_exist(sidecar.c_str(), FileSys::GAME_DIR),
			"<map>.navmesh sidecar written to disk");

	// Round-trip the load path — drop the live mesh, reload from the file we just wrote.
	RuntimeNavManager::inst->clear();
	t.require(!RuntimeNavManager::inst->has_navmesh(), "manager cleared before reload");
	LevelNavUtil::on_scene_load_nav("eng/template.tmap");
	t.check(RuntimeNavManager::inst->has_navmesh(), "navmesh reloaded from sidecar");

	corners.clear();
	t.check(RuntimeNavManager::inst->find_path({-5,0,-5}, {5,0,5}, corners),
			"find_path succeeds after sidecar reload");

	// Cleanup so this test doesn't pollute other tests' state or leave files behind.
	RuntimeNavManager::inst->clear();
	FileSys::delete_game_file(sidecar);
}
EDITOR_TEST("nav/editor_bake_roundtrip", 30.f, test_editor_nav_bake);
