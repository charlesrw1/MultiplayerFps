// Source/IntegrationTests/Tests/Renderer/test_compact_instances.cpp
//
// Smoke test for the GPU-driven compact instance path (register_compact_batch ->
// build_compact_data -> CullComputeCompact dispatch -> COMPACT_INST master-shader
// permutation -> MDI draw). Drives a RenderStressTestComponent in
// EnabledCompactStatic mode and renders several frames. This is a crash/compile
// smoke, not a golden: a COMPACT_INST shader-compile failure asserts inside
// compile_mat_shader, and any binding/logic fault surfaces while rendering, so
// simply reaching the end means the whole vertical slice ran end-to-end.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/CameraComponent.h"
#include "Game/Components/RenderStressTestComponent.h"
#include "Assets/AssetDatabase.h"
#include "Render/Model.h"
#include "Render/RenderConfigVars.h"

static Entity* find_entity_by_editor_name_ci(const char* name) {
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* e = obj->cast_to<Entity>())
			if (e->get_editor_name() == name) return e;
	}
	return nullptr;
}

static TestTask test_compact_instances_smoke(TestContext& t) {
	r_taa_enabled.set_bool(false);

	eng->load_level("maps/demo_level_1.tmap");
	co_await t.wait_ticks(2);

	Entity* cam_ent = find_entity_by_editor_name_ci("decal_and_shadow_test");
	t.require(cam_ent != nullptr, "camera entity not found in level");

	auto cc = eng->get_level()->spawn_entity()->create_component<CameraComponent>();
	cc->set_is_enabled(true);
	if (cam_ent)
		cc->get_owner()->set_ws_transform(cam_ent->get_ws_transform());

	// Spawn the compact grid at the camera so some instances land in view (exercises
	// a non-zero primCount through the COMPACT_INST draw, not just the cull/compile).
	auto owner = eng->get_level()->spawn_entity();
	if (cam_ent)
		owner->set_ws_position(cam_ent->get_ws_position());
	auto stress = owner->create_component<RenderStressTestComponent>();
	stress->model = g_assets.find<Model>("arrowModel.cmdl");
	stress->grid_length = 8;
	stress->spacing = 2.f;

	// First drive the CLASSIC path so the shared (model,material) slot grows its
	// instance_alloced, then switch the SAME component/model to the compact path.
	// This reproduces the slot-namespace collision that used to assert in
	// register_compact_batch; with compact slots namespaced it must render cleanly.
	stress->state = RenderStressTestState::EnabledStatic;
	stress->sync_render_data();
	co_await t.wait_ticks(3);

	stress->state = RenderStressTestState::EnabledCompactStatic;
	stress->sync_render_data();
	co_await t.wait_ticks(5);

	t.require(true, "classic->compact switch on the same model rendered without crashing");
}
GAME_TEST("renderer/compact_instances_smoke", 60.f, test_compact_instances_smoke);
