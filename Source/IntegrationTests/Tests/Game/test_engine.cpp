// Source/IntegrationTests/Tests/Game/test_engine.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"


// Frame dt must be a positive value after at least one tick has occurred
static TestTask test_game_dt_positive(TestContext& t) {
	co_await t.wait_ticks(2);
	t.check(eng->get_dt() > 0.0, "frame dt is positive after ticking");
}
GAME_TEST("game/dt_positive", 5.f, test_game_dt_positive);