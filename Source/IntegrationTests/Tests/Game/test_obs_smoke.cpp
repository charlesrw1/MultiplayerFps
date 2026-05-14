// Source/IntegrationTests/Tests/Game/test_obs_smoke.cpp
//
// Smoke test for ObsGame: spawn the application, build the obstacle course
// + player, drive synthetic input, verify entities and basic state, capture
// a screenshot.
//
// Runs in game_test mode where g_application_class defaults to "Application"
// (a no-op base), so we manually instantiate ObsGameApplication via
// ClassBase::create_class and call start() ourselves.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/ObsGame/ObsGameHeaders.h"
#include "Framework/ClassBase.h"

static TestTask test_obs_smoke(TestContext& t)
{
	// Load a minimal level so eng->get_level() is valid for entity spawn.
	eng->load_level("empty_map.tmap");
	co_await t.wait_ticks(2);
	t.require(eng->get_level() != nullptr, "level loaded");

	// Manually create + initialise our application (the engine's active
	// Application in game_test mode is the no-op base).
	Application* base = ClassBase::create_class<Application>("ObsGameApplication");
	t.require(base != nullptr, "ClassBase::create_class<ObsGameApplication> succeeded");
	auto* app = static_cast<ObsGameApplication*>(base);
	app->start();   // builds level + spawns player + sets instance
	co_await t.wait_ticks(5); // let physics + first ticks run

	// --- Verify spawn ---
	t.require(ObsGameApplication::get() == app, "instance singleton set");
	t.require(app->player != nullptr, "player spawned");
	t.require(app->player->capsule != nullptr, "capsule attached");
	t.require(app->player->left_hand != nullptr,  "left hand spawned");
	t.require(app->player->right_hand != nullptr, "right hand spawned");
	t.require(app->player->left_hand->body  != nullptr, "left hand has body");
	t.require(app->player->right_hand->body != nullptr, "right hand has body");
	t.require(app->camera != nullptr && app->camera->cc != nullptr, "camera + CameraComponent spawned");

	// --- Drive synthetic input: walk forward briefly ---
	app->player->debug_set_use_synthetic_input(true);
	for (int i = 0; i < 60; ++i) {
		app->player->debug_drive(glm::vec3(0.f, 0.f, -1.f), 0.f, 0.f, false);
		co_await t.wait_ticks(1);
	}

	// Player should have moved forward (-Z) from origin.
	const glm::vec3 chest = app->player->get_chest_pos();
	t.check(chest.z < -0.2f, "player walked forward toward obstacle course");

	// --- Try a grab probe: lean toward Box A (z=-3) and engage both triggers ---
	for (int i = 0; i < 60; ++i) {
		app->player->debug_drive(glm::vec3(0.f, 0.f, -1.f), 1.f, 1.f, false);
		co_await t.wait_ticks(1);
	}

	co_await t.capture_screenshot("obs_smoke");
}
GAME_TEST("obsgame/smoke", 30.f, test_obs_smoke);
