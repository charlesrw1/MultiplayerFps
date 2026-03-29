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

// Test create-and-replace-prefab: spawn entities, select them, create prefab, verify replacement
static TestTask test_editor_prefab_create_and_replace(TestContext& t) {
	const std::string prefab_path = "_tempprefab.tprefab";
	const char* prefab_path_cstr = prefab_path.c_str();

	// Clean up any existing prefab file
	FileSys::delete_game_file(prefab_path_cstr);

	// Open editor in normal mode
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	int count_before = t.editor().entity_count();

	// Create 2 entities using the editor
	EntityPtr e1 = editor->spawn_entity();
	e1->set_editor_name("Test Entity 1");
	e1->set_ls_position({1.0f, 0.0f, 0.0f});
	e1->create_component < MeshComponent>();

	EntityPtr e2 = editor->spawn_entity();
	e2->set_editor_name("Test Entity 2");
	e2->set_ls_position({2.0f, 0.0f, 0.0f});
	e2->create_component < MeshComponent>();


	co_await t.wait_ticks(1);

	int count_with_entities = t.editor().entity_count();
	t.check(count_with_entities == count_before + 4, "entities were added to level");

	// Select both entities using the selection API
	ISelectionApi* sel_api = editor->get_editor_api().selection();
	sel_api->clear_selected();
	sel_api->add_select(e1->get_self_ptr());
	sel_api->add_select(e2->get_self_ptr());

	co_await t.wait_ticks(1);

	// Verify selection
	auto selected = sel_api->get_selected();
	t.require(selected.size() == 2, "both entities selected");

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, ("make-prefab-and-replace " + prefab_path).c_str());
	co_await t.wait_ticks(2);

	t.check(!e1.get() && !e2.get(), "selected ents removed");

	// Verify prefab file was created
	t.check(FileSys::does_file_exist(prefab_path.c_str(), FileSys::GAME_DIR), "file exists on disk");
	std::string prefab_text = PrefabFile::load_text(prefab_path_cstr);
	t.check(!prefab_text.empty(), "prefab file was created");

	// Now open the prefab for editing
	std::string open_cmd = std::string("open-editor ") + prefab_path_cstr;
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, open_cmd.c_str());
	co_await t.wait_ticks(5);

	editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available after opening prefab");
	t.check(editor->is_editing_prefab(), "editor is in prefab mode");

	// remove from disk
	FileSys::delete_game_file(prefab_path_cstr);
	co_await t.wait_ticks(1);
	editor->save();
	co_await t.wait_ticks(10);

	auto load_again = PrefabFile::load_text(prefab_path_cstr);
	t.check(load_again == prefab_text, "prefab same when reloaded");

	// Cleanup
	FileSys::delete_game_file(prefab_path_cstr);

	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/prefab_create_and_replace", 25.f, test_editor_prefab_create_and_replace);

// Test create and edit prefab: write hardcoded prefab, open for editing, add entity, save, reload
static TestTask test_editor_prefab_roundtrip_edit(TestContext& t) {
	const std::string prefab_path = "_hardcoded_prefab.tprefab";
	const char* prefab_path_cstr = prefab_path.c_str();

	// Clean up any existing file
	FileSys::delete_game_file(prefab_path_cstr);

	// Create a hardcoded prefab with one entity in a temporary level
	EntityPtr temp_ent = eng->get_level()->spawn_entity();
	temp_ent->set_editor_name("Prefab Entity 1");
	temp_ent->set_ls_position({5.0f, 0.0f, 0.0f});
	temp_ent->create_component<MeshComponent>();

	SerializedSceneFile serialized;
	try {
		serialized = NewSerialization::serialize_to_text("initial_prefab", {temp_ent}, false);
	}
	catch (const std::exception& e) {
		t.require(false, "Failed to serialize prefab entity");
		co_return;
	}


	// Write hardcoded prefab to disk
	bool save_ok = PrefabFile::save_text(prefab_path_cstr, serialized.text);
	t.require(save_ok, "prefab file saved successfully");

	temp_ent->destroy();

	co_await t.wait_ticks(1);

	// Open prefab for editing
	std::string open_cmd = std::string("open-editor ") + prefab_path_cstr;
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, open_cmd.c_str());
	co_await t.wait_ticks(5);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");
	t.check(editor->is_editing_prefab(), "editor is in prefab mode");

	// Count initial entities
	int initial_count = t.editor().entity_count();
	t.check(initial_count > 0, "prefab loaded with entities");

	// Add a new entity in the prefab editor
	int before_add = t.editor().entity_count();
	EntityPtr new_ent = editor->spawn_entity();
	new_ent->set_editor_name("Prefab Entity 2");
	new_ent->set_ls_position({6.0f, 0.0f, 0.0f});
	new_ent->create_component<MeshComponent>();

	co_await t.wait_ticks(1);

	// Verify count increased
	int after_add_count = t.editor().entity_count();
	t.check(after_add_count > before_add, "entity count increased after adding entity");

	// Save the prefab
	t.editor().save_level(prefab_path_cstr);
	co_await t.wait_ticks(1);

	// Close and reopen the prefab to verify changes persisted
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	// Reopen the prefab
	open_cmd = std::string("open-editor ") + prefab_path_cstr;
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, open_cmd.c_str());
	co_await t.wait_ticks(5);

	editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available after reopening");

	// Verify entities persisted (should be at least 2 now)
	int final_count = t.editor().entity_count();
	t.check(final_count > before_add, "entity count persisted after reload");

	// Cleanup
	FileSys::delete_game_file(prefab_path_cstr);

	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/prefab_roundtrip_edit", 30.f, test_editor_prefab_roundtrip_edit);
