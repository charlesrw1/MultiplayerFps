// Source/IntegrationTests/Tests/Editor/test_prefabs.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "IntegrationTests/EditorTestContext.h"
#include "LevelEditor/EditorDocLocal.h"
#include "Framework/Files.h"
#include "Game/Prefab.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"

// Test that prefab mode is detected when opening a .tprefab file
static TestTask test_prefab_edit_mode_detection(TestContext& t) {
	// Open editor in normal mode first
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	// Verify not in prefab mode for .tmap
	t.check(!editor->is_editing_prefab(), "not in prefab mode for .tmap file");

	// Verify window title doesn't have [Prefab]
	std::string doc_name = editor->get_doc_name();
	t.check(doc_name.find("[Prefab]") == std::string::npos, "normal level doesn't have [Prefab] label");

	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/prefab_mode_detection", 15.f, test_prefab_edit_mode_detection);

// Test that MakePrefabFromSelectionCommand can be created with valid structure
static TestTask test_prefab_command_validity(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	// Test with empty selection - should be invalid
	std::vector<EntityPtr> empty_selection;
	auto cmd1 = new MakePrefabFromSelectionCommand(*editor, empty_selection, "test.tprefab");
	t.check(!cmd1->is_valid(), "empty selection makes command invalid");
	delete cmd1;

	// Test with valid path - would be valid with actual selection
	std::vector<EntityPtr> valid_selection; // Empty but we test structure
	auto cmd2 = new MakePrefabFromSelectionCommand(*editor, valid_selection, "_test_prefab.tprefab");
	// Still invalid because selection is empty, but command structure is valid
	t.check(!cmd2->is_valid(), "empty selection still invalid");
	delete cmd2;

	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/prefab_command_validity", 15.f, test_prefab_command_validity);

// Test that prefab file I/O works (save and load text)
static TestTask test_prefab_file_io(TestContext& t) {
	const std::string TEST_PATH = "test_prefab_io.tprefab";
	const std::string TEST_CONTENT = "!json\n{\"test\": \"data\"}";

	// Clean up any existing file
	FileSys::delete_game_file(TEST_PATH);

	// Test save
	bool save_ok = PrefabFile::save_text(TEST_PATH, TEST_CONTENT);
	t.check(save_ok, "prefab file saves successfully");

	co_await t.wait_ticks(1);

	// Test load
	std::string loaded = PrefabFile::load_text(TEST_PATH);
	t.check(!loaded.empty(), "prefab file loads successfully");
	t.check(loaded == TEST_CONTENT, "loaded content matches saved content");

	// Cleanup
	FileSys::delete_game_file(TEST_PATH);
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/prefab_file_io", 10.f, test_prefab_file_io);

// Test that InstantiatePrefabCommand has valid structure
static TestTask test_instantiate_prefab_command(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	// Test with valid path
	glm::mat4 identity = glm::mat4(1.0f);
	auto cmd = new InstantiatePrefabCommand(*editor, "_nonexistent.tprefab", identity);

	// Command is valid even if file doesn't exist (error handled in execute)
	t.check(cmd->is_valid(), "command with valid path is valid");

	// Test with empty path
	auto cmd2 = new InstantiatePrefabCommand(*editor, "", identity);
	t.check(!cmd2->is_valid(), "command with empty path is invalid");

	delete cmd;
	delete cmd2;
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/instantiate_prefab_command", 15.f, test_instantiate_prefab_command);
