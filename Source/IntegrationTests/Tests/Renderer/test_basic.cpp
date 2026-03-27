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

