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
