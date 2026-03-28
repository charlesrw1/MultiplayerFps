// Source/IntegrationTests/Tests/Editor/test_serialize.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "IntegrationTests/EditorTestContext.h"

// Serialize round-trip: load a level, save it, reload, check entity count matches
static TestTask test_serialize_round_trip(TestContext& t) {
	eng->load_level("Data/demo_level_1.tmap");
	t.require(eng->get_level() != nullptr, "level loaded in editor mode");

	int count_before = t.editor().entity_count();
	t.require(count_before > 0, "level has entities before save");

	t.editor().save_level("TestFiles/temp_serialize_test.tmap");
	co_await t.wait_ticks(1);

	eng->load_level("TestFiles/temp_serialize_test.tmap");
	t.require(eng->get_level() != nullptr, "reloaded level is non-null");

	int count_after = t.editor().entity_count();
	t.check(count_after == count_before, ("entity count after round-trip: expected " + std::to_string(count_before) +
										  " got " + std::to_string(count_after))
											 .c_str());
}
EDITOR_TEST("editor/serialize_round_trip", 15.f, test_serialize_round_trip);

// Undo with no commands queued should not crash or change entity count
static TestTask test_undo_noop(TestContext& t) {
	eng->load_level("Data/demo_level_1.tmap");
	int count_before = t.editor().entity_count();

	t.editor().undo();
	co_await t.wait_ticks(1);

	int count_after = t.editor().entity_count();
	t.check(count_after == count_before, "undo with no commands does not change entity count");
}
EDITOR_TEST("editor/undo_noop", 10.f, test_undo_noop);
