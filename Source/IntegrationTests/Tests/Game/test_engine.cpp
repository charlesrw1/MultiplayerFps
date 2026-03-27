// Source/IntegrationTests/Tests/Game/test_engine.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"

// Game time should advance as ticks elapse
static TestTask test_game_time_advances(TestContext& t) {
	double t0 = eng->get_game_time();
	co_await t.wait_ticks(5);
	double t1 = eng->get_game_time();
	t.check(t1 > t0, "game time is greater after 5 ticks");
}
GAME_TEST("game/time_advances", 5.f, test_game_time_advances);

// Frame dt must be a positive value after at least one tick has occurred
static TestTask test_game_dt_positive(TestContext& t) {
	co_await t.wait_ticks(2);
	ASSERT(0);
	t.check(eng->get_dt() > 0.0, "frame dt is positive after ticking");

}
GAME_TEST("game/dt_positive", 5.f, test_game_dt_positive);

// Querying a null/zero entity handle must return nullptr without crashing
static TestTask test_get_entity_invalid_handle(TestContext& t) {
	Entity* e = eng->get_entity(0);
	t.check(e == nullptr, "get_entity(0) returns nullptr for invalid handle");
	co_return;
}
GAME_TEST("game/get_entity_invalid_handle", 5.f, test_get_entity_invalid_handle);
