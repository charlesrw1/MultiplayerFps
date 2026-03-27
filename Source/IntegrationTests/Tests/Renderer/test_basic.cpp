// Source/IntegrationTests/Tests/Renderer/test_basic.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"

// Smoke test: engine boots, level is valid, survives one tick
static TestTask test_engine_boots(TestContext& t) {
	t.require(eng->get_level() != nullptr, "level is non-null after boot");
	co_await t.wait_ticks(1);
	t.check(true, "survived one tick");
}
GAME_TEST("game/boot", 5.f, test_engine_boots);

// Load a level and verify entity count is stable across ticks
static TestTask test_level_load(TestContext& t) {
	co_await t.load_level("Data/demo_level_1.tmap");
	t.require(eng->get_level() != nullptr, "level loaded");
	int count = (int)eng->get_level()->get_all_objects().num_used;
	t.check(count > 0, "level has entities");
	co_await t.wait_ticks(2);
	int count2 = (int)eng->get_level()->get_all_objects().num_used;
	t.check(count2 == count, "entity count stable after 2 ticks");
}
GAME_TEST("game/level_load", 10.f, test_level_load);

// Screenshot smoke test — verifies capture pipeline; golden created with --promote
static TestTask test_screenshot_smoke(TestContext& t) {
	co_await t.load_level("Data/demo_level_1.tmap");
	co_await t.wait_ticks(3);
	co_await t.capture_screenshot("smoke_demo_level_1");
}
GAME_TEST("renderer/screenshot_smoke", 15.f, test_screenshot_smoke);

#include "Render/RenderConfigVars.h"

#include "Game/Entity.h"
#include "Game/Components/CameraComponent.h"
static void change_map_and_make_camera(const std::string& path, glm::vec3 pos)
{
	eng->load_level(path);
	auto cc = eng->get_level()->spawn_entity()->create_component<CameraComponent>();
	cc->set_is_enabled(true);
	cc->get_owner()->set_ws_position(pos);
	ASSERT(CameraComponent::get_scene_camera() == cc);
}

// ssr/taa smoke test
static TestTask test_ssr_motion_vec_smoke(TestContext& t) {
	r_taa_enabled.set_bool(true);
	auto pre_size = get_app_window_size();
	change_map_and_make_camera("demo_level_1.tmap", {});

	for (int i = 0; i < 50; i++) {
		set_app_window_size({ pre_size.x + i * 5,pre_size.y });
		co_await t.wait_ticks(1);
	}
	co_await t.capture_screenshot("ssr_smoke");

}
GAME_TEST("renderer/ssr_smoke", 15.f, test_ssr_motion_vec_smoke);
