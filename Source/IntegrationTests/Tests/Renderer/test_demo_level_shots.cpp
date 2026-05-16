// Source/IntegrationTests/Tests/Renderer/test_demo_level_shots.cpp
//
// Rendering golden screenshots from named camera entities in demo_level_1.tmap.
// Each camera entity in the map is named identically to its screenshot.
// First run with --promote to capture goldens.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/CameraComponent.h"
#include "Render/RenderConfigVars.h"
#include "Framework/Config.h"

extern ConfigVar r_ddgi_halfres;
extern ConfigVar ddgi_test;           // "dt"
extern ConfigVar enable_ssr;          // "r.ssr"
extern ConfigVar debug_sun_shadow;    // "r.debug_csm"
// r_taa_enabled, r_debug_mode already extern'd in RenderConfigVars.h

static Entity* find_entity_by_editor_name(const char* name)
{
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* e = obj->cast_to<Entity>())
			if (e->get_editor_name() == name) return e;
	}
	return nullptr;
}

static CameraComponent* spawn_camera_at_named_entity(TestContext& t, const char* name)
{
	Entity* cam_entity = find_entity_by_editor_name(name);
	t.require(cam_entity != nullptr, "camera entity not found in level");
	if (!cam_entity) return nullptr;

	auto cc = eng->get_level()->spawn_entity()->create_component<CameraComponent>();
	cc->set_is_enabled(true);
	cc->get_owner()->set_ws_transform(cam_entity->get_ws_transform());
	ASSERT(CameraComponent::get_scene_camera() == cc);
	return cc;
}

static TestTask test_demo_level_1_shots(TestContext& t)
{
	// Baseline render config for deterministic captures.
	r_ddgi_halfres.set_bool(false);
	ddgi_test.set_bool(true);
	enable_ssr.set_bool(false);
	r_taa_enabled.set_bool(false);

	eng->load_level("maps/demo_level_1.tmap");
	co_await t.wait_ticks(2);  // give the level a frame to settle.

	struct Shot {
		const char* name;
		int  debug_mode;  // r.debug_mode value during capture (0 = leave alone)
		bool debug_csm;   // r.debug_csm value during capture
		// Opt-in soft-fail band (negative = strict). Used for shots whose pixels
		// genuinely jitter across driver/timing changes without indicating a regression.
		int   warn_channel_delta;
		float warn_diff_fraction;
	};
	const Shot shots[] = {
		// decal+shadow shot drifts ~max_delta=29 on a tiny fraction of pixels — warn instead of fail.
		{ "decal_and_shadow_test",     0,  false,  48, 0.005f },
		{ "inside_shadows_test",       0,  false,  -1, -1.f   },
		{ "occlusion_cull_mode_test",  12, false,  -1, -1.f   },
		{ "debug_csm_test",            0,  true,   -1, -1.f   },
	};

	for (const Shot& s : shots) {
		spawn_camera_at_named_entity(t, s.name);
		r_debug_mode.set_integer(s.debug_mode);
		debug_sun_shadow.set_bool(s.debug_csm);

		co_await t.wait_ticks(2);   // let render state apply.
		co_await t.capture_screenshot(s.name, s.warn_channel_delta, s.warn_diff_fraction);

		// Reset per-shot vars so the next shot starts from baseline.
		r_debug_mode.set_integer(0);
		debug_sun_shadow.set_bool(false);
	}
}
GAME_TEST("renderer/demo_level_1_shots", 60.f, test_demo_level_1_shots);
