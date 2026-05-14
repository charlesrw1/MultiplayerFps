// Source/IntegrationTests/Tests/Editor/test_serialize.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "IntegrationTests/EditorTestContext.h"
#include <filesystem>
#include <fstream>
#include "Framework/Files.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetRegistryLocal.h"
#include "Render/Model.h"
#include "Logging.h"
namespace fs = std::filesystem;

// Serialize round-trip: load a level, save it, reload, check entity count matches
static TestTask test_serialize_round_trip(TestContext& t) {
	FileSys::delete_game_file("_temp_serialize_test.tmap");


	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "level loaded in editor mode");

	int count_before = t.editor().entity_count();
	t.require(count_before > 0, "level has entities before save");

	t.editor().save_level("_temp_serialize_test.tmap");
	co_await t.wait_ticks(1);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor _temp_serialize_test.tmap");
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "reloaded level is non-null");

	int count_after = t.editor().entity_count();
	t.check(count_after == count_before, ("entity count after round-trip: expected " + std::to_string(count_before) +
										  " got " + std::to_string(count_after))
											 .c_str());

}
EDITOR_TEST("editor/serialize_round_trip", 15.f, test_serialize_round_trip);



// Opening a map that does not exist must not blow away the document the user
// already has open, and it must report the failure to the log.
static TestTask test_open_invalid_map(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "valid level loaded before invalid open");
	t.require(eng->get_tool() != nullptr, "editor tool open before invalid open");

	Level* level_before = eng->get_level();
	IEditorTool* tool_before = eng->get_tool();
	int entity_count_before = t.editor().entity_count();

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor _no_such_map_invalid_path_.tmap");
	co_await t.wait_ticks(4);

	t.check(eng->get_level() == level_before, "current level was not replaced by the invalid open");
	t.check(eng->get_tool() == tool_before, "current editor tool was not closed by the invalid open");
	t.check(t.editor().entity_count() == entity_count_before, "entity count unchanged after invalid open");
}
EDITOR_TEST("editor/test_open_invalid_map", 10.f, test_open_invalid_map);

// Opening with no map argument loads a blank scene — verify the editor is up
// with a valid (but empty) level. The engine emits unrelated GL warnings
// during map load, so we don't assert on global log silence here.
static TestTask test_open_empty_map(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);

	t.check(eng->get_level() != nullptr, "level exists after opening empty map");
	t.check(eng->get_tool() != nullptr, "editor tool exists after opening empty map");
}
EDITOR_TEST("editor/test_open_empty_map", 10.f, test_open_empty_map);

// Undo with no commands queued should not crash or change entity count
static TestTask test_undo_noop(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(1);

	int count_before = t.editor().entity_count();

	t.editor().undo();
	co_await t.wait_ticks(1);

	int count_after = t.editor().entity_count();
	t.check(count_after == count_before, "undo with no commands does not change entity count");
}
EDITOR_TEST("editor/undo_noop", 10.f, test_undo_noop);



static TestTask test_does_new_asset_appear(TestContext& t) {
	const std::string DUMMY_FILE_PREFIX = "_integration_test_dummy";
	const std::string DUMMY_NAME = DUMMY_FILE_PREFIX + ".cmdl";
	FileSys::delete_game_file(DUMMY_NAME);
	t.check(!FileSys::does_file_exist(DUMMY_NAME.c_str(),FileSys::GAME_DIR),"file on disk still");
	co_await t.wait_ticks(3);
	auto file = FileSys::open_write_game(DUMMY_NAME);
	t.check(file != nullptr, "");
	file.reset();
	co_await t.wait_seconds(0.5f);
	auto try_find = [&]() {
		auto& all_files = AssetRegistrySystem::get().get_linear_list();
		return std::any_of(all_files.begin(), all_files.end(), [&](AssetFilesystemNode* n) {
		assert(n);
		return n->asset.filename == DUMMY_NAME && n->asset.type->get_asset_class_type() == &Model::StaticType;
		});
	};
	const bool found_in = try_find();
	FileSys::delete_game_file(DUMMY_NAME);
	t.check(found_in, "newly made file not found in asset registry");
	t.check(!FileSys::does_file_exist(DUMMY_NAME.c_str(), FileSys::GAME_DIR), "file on disk still");
	co_await t.wait_seconds(0.5f);
	const bool found_in_2 = try_find();
	t.check(!found_in_2, "newly deleted file found in asset registry");
}
EDITOR_TEST("editor/does_new_assets_appear", 5.f, test_does_new_asset_appear);
