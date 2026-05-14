// Source/IntegrationTests/Tests/Editor/test_prefabs.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "IntegrationTests/EditorTestContext.h"
#include "LevelEditor/EditorDocLocal.h"
#include "Framework/Files.h"
#include "Game/Prefab.h"
#include "Game/Components/PrefabAssetComponent.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"
#include "Level.h"
#include <glm/gtc/matrix_transform.hpp>

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

// Inherited entities (children spawned by a PrefabAssetComponent) carry dont_serialize_or_edit
// and must be protected from RemoveEntitiesCommand. The prefab root itself is editable; only its
// deserialized children are inherited. Regression guard for the can_delete_this_object gate
// described in LevelEditor/AGENTS.md.
static TestTask test_inherited_entity_not_deletable(TestContext& t) {
	const std::string prefab_path = "_test_inherited_protect.tprefab";
	const char* prefab_path_cstr = prefab_path.c_str();
	FileSys::delete_game_file(prefab_path_cstr);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	// Build a one-entity prefab and write it to disk
	EntityPtr seed = editor->spawn_entity();
	seed->set_editor_name("InheritedChild");
	seed->set_ls_position({3.0f, 0.0f, 0.0f});
	seed->create_component<MeshComponent>();
	co_await t.wait_ticks(1);

	SerializedSceneFile prefab_ser;
	try {
		prefab_ser = NewSerialization::serialize_to_text("inh_test_prefab", {seed.get()}, false, prefab_path.c_str());
	}
	catch (const std::exception&) {
		t.require(false, "serialize prefab failed");
		co_return;
	}
	t.require(PrefabFile::save_text(prefab_path_cstr, prefab_ser.text), "prefab saved to disk");

	seed->destroy();
	co_await t.wait_ticks(1);

	int count_before = t.editor().entity_count();

	// Spawn the prefab through the undoable command path
	auto* instantiate = new InstantiatePrefabCommand(*editor, prefab_path, glm::mat4(1.0f));
	editor->command_mgr->add_command(instantiate);
	co_await t.wait_ticks(2);

	t.check(t.editor().entity_count() > count_before, "prefab spawn added entities to level");

	// Find the prefab root — the entity carrying PrefabAssetComponent
	Entity* prefab_root = nullptr;
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* ent = obj->cast_to<Entity>()) {
			if (ent->get_component<PrefabAssetComponent>()) {
				prefab_root = ent;
				break;
			}
		}
	}
	t.require(prefab_root != nullptr, "prefab root entity present");

	t.check(!editor->is_this_object_inherited(prefab_root), "prefab root itself is editable");

	const auto& children = prefab_root->get_children();
	t.require(!children.empty(), "prefab root has at least one inherited child");

	Entity* inherited_child = children.front();
	t.require(inherited_child != nullptr, "inherited child non-null");

	t.check(inherited_child->dont_serialize_or_edit, "inherited child has dont_serialize_or_edit set");
	t.check(editor->is_this_object_inherited(inherited_child), "is_this_object_inherited true for prefab child");
	t.check(!editor->can_delete_this_object(inherited_child), "can_delete_this_object false for prefab child");

	// Direct construction: command must report invalid for an inherited-only input
	EntityPtr child_ptr = inherited_child->get_self_ptr();
	auto* remove_direct = new RemoveEntitiesCommand(*editor, std::vector<EntityPtr>{child_ptr});
	t.check(!remove_direct->is_valid(), "RemoveEntitiesCommand on inherited child is_valid()==false");
	delete remove_direct;

	// Queue an invalid removal through the manager; the child must still be alive after the flush
	auto* remove_queued = new RemoveEntitiesCommand(*editor, std::vector<EntityPtr>{child_ptr});
	editor->command_mgr->add_command(remove_queued);
	co_await t.wait_ticks(2);

	t.check(child_ptr.get() != nullptr, "inherited child still alive after queued remove attempt");

	FileSys::delete_game_file(prefab_path_cstr);
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/inherited_entity_not_deletable", 20.f, test_inherited_entity_not_deletable);

// Opening a .tprefab and calling save() must write back as .tprefab, never as a sibling .tmap.
// The save path comes from set_document_path + get_save_file_extension(); regression guard for
// the interaction documented in LevelEditor/AGENTS.md.
static TestTask test_prefab_save_extension_correct(TestContext& t) {
	const std::string prefab_path = "_test_save_ext.tprefab";
	const std::string sibling_tmap_path = "_test_save_ext.tmap";
	const char* prefab_path_cstr = prefab_path.c_str();
	const char* sibling_tmap_cstr = sibling_tmap_path.c_str();
	FileSys::delete_game_file(prefab_path_cstr);
	FileSys::delete_game_file(sibling_tmap_cstr);

	// Build the prefab via the editor and write it to disk
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	EntityPtr seed = editor->spawn_entity();
	seed->set_editor_name("SavedPrefabEntity");
	seed->set_ls_position({1.5f, 0.0f, 0.0f});
	seed->create_component<MeshComponent>();
	co_await t.wait_ticks(1);

	SerializedSceneFile prefab_ser;
	try {
		prefab_ser = NewSerialization::serialize_to_text("save_ext_prefab", {seed.get()}, false, prefab_path.c_str());
	}
	catch (const std::exception&) {
		t.require(false, "serialize prefab failed");
		co_return;
	}
	t.require(PrefabFile::save_text(prefab_path_cstr, prefab_ser.text), "prefab saved to disk");

	// Open the prefab — assetName is set, editing_prefab=true
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + prefab_path_cstr).c_str());
	co_await t.wait_ticks(5);

	editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available after opening prefab");
	t.require(editor->is_editing_prefab(), "editor entered prefab mode");
	t.check(std::string(editor->get_save_file_extension()) == "tprefab",
			"get_save_file_extension reports tprefab in prefab mode");

	std::string original_text = PrefabFile::load_text(prefab_path_cstr);
	t.require(!original_text.empty(), "prefab text non-empty on disk before save");

	// Save without touching the path — should write back to the same .tprefab
	bool save_ok = editor->save();
	t.check(save_ok, "save() returns true when path is already known");

	co_await t.wait_ticks(2);

	t.check(FileSys::does_file_exist(prefab_path_cstr, FileSys::GAME_DIR),
			"prefab file still exists at .tprefab path after save");

	t.check(!FileSys::does_file_exist(sibling_tmap_cstr, FileSys::GAME_DIR),
			"no sibling .tmap was created during save");

	std::string reloaded = PrefabFile::load_text(prefab_path_cstr);
	t.check(!reloaded.empty(), "prefab content non-empty after save+reload");

	FileSys::delete_game_file(prefab_path_cstr);
	FileSys::delete_game_file(sibling_tmap_cstr);
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/prefab_save_extension_correct", 25.f, test_prefab_save_extension_correct);

// Placing a prefab while editing a prefab must flatten the inner prefab into root-level
// entities of the outer prefab — never write a nested PrefabAssetComponent reference. The
// serializer drops parented entities (SerializeNew.cpp:87), so the legacy spawn path
// silently lost the inner prefab on save. This guards the flatten path in
// InstantiatePrefabCommand::execute().
static TestTask test_editor_prefab_flatten_on_drop(TestContext& t) {
	const std::string inner_path = "_inner_for_flatten.tprefab";
	const std::string outer_path = "_outer_for_flatten.tprefab";
	const char* inner_cstr = inner_path.c_str();
	const char* outer_cstr = outer_path.c_str();
	FileSys::delete_game_file(inner_cstr);
	FileSys::delete_game_file(outer_cstr);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	// Build a 2-entity inner prefab on disk
	EntityPtr inner_a = editor->spawn_entity();
	inner_a->set_editor_name("InnerA");
	inner_a->set_ls_position({0.0f, 0.0f, 0.0f});
	inner_a->create_component<MeshComponent>();

	EntityPtr inner_b = editor->spawn_entity();
	inner_b->set_editor_name("InnerB");
	inner_b->set_ls_position({1.0f, 0.0f, 0.0f});
	inner_b->create_component<MeshComponent>();
	co_await t.wait_ticks(1);

	SerializedSceneFile inner_ser;
	try {
		inner_ser = NewSerialization::serialize_to_text("flatten_inner", {inner_a.get(), inner_b.get()}, false,
													   inner_path.c_str());
	}
	catch (const std::exception&) {
		t.require(false, "serialize inner prefab failed");
		co_return;
	}
	t.require(PrefabFile::save_text(inner_cstr, inner_ser.text), "inner prefab saved");
	inner_a->destroy();
	inner_b->destroy();
	co_await t.wait_ticks(1);

	// Build a 1-entity outer prefab on disk
	EntityPtr outer_seed = editor->spawn_entity();
	outer_seed->set_editor_name("OuterSeed");
	outer_seed->set_ls_position({0.0f, 5.0f, 0.0f});
	outer_seed->create_component<MeshComponent>();
	co_await t.wait_ticks(1);

	SerializedSceneFile outer_ser;
	try {
		outer_ser = NewSerialization::serialize_to_text("flatten_outer", {outer_seed.get()}, false, outer_path.c_str());
	}
	catch (const std::exception&) {
		t.require(false, "serialize outer prefab failed");
		co_return;
	}
	t.require(PrefabFile::save_text(outer_cstr, outer_ser.text), "outer prefab saved");
	outer_seed->destroy();
	co_await t.wait_ticks(1);

	// Open the outer prefab for editing
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + outer_cstr).c_str());
	co_await t.wait_ticks(5);

	editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available after opening outer prefab");
	t.require(editor->is_editing_prefab(), "editor entered prefab mode for outer prefab");

	int count_before = t.editor().entity_count();

	// Drop the inner prefab with a translation so we can verify transform application
	const glm::vec3 drop_offset(10.0f, 0.0f, 0.0f);
	glm::mat4 drop_xform = glm::translate(glm::mat4(1.0f), drop_offset);
	auto* instantiate = new InstantiatePrefabCommand(*editor, inner_path, drop_xform);
	editor->command_mgr->add_command(instantiate);
	co_await t.wait_ticks(2);

	t.check(t.editor().entity_count() > count_before, "flatten added entities to outer prefab");

	// Walk the level: there must be NO PrefabAssetComponent referencing the inner prefab,
	// and the freshly-added entities must be root-level (no parent).
	bool found_nested_ref = false;
	int root_inner_count = 0;
	bool saw_a = false, saw_b = false;
	glm::vec3 a_ws_pos{0.0f};
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* pac = obj->cast_to<PrefabAssetComponent>()) {
			if (pac->prefab_path == inner_path)
				found_nested_ref = true;
		}
		if (auto* ent = obj->cast_to<Entity>()) {
			const std::string& nm = ent->get_editor_name();
			if (nm == "InnerA" || nm == "InnerB") {
				t.check(ent->get_parent() == nullptr, (nm + " is a root-level entity after flatten").c_str());
				root_inner_count++;
				if (nm == "InnerA") {
					saw_a = true;
					a_ws_pos = ent->get_ws_position();
				}
				if (nm == "InnerB")
					saw_b = true;
				t.check(!ent->dont_serialize_or_edit, (nm + " is editable (not inherited)").c_str());
			}
		}
	}
	t.check(!found_nested_ref, "no PrefabAssetComponent reference to inner prefab was created");
	t.check(saw_a && saw_b, "both inner-prefab entities present after flatten");
	t.check(root_inner_count == 2, "exactly two root-level inner entities added");

	// Verify drop_xform was applied: InnerA was at (0,0,0) in inner prefab,
	// after flatten its ws position should be drop_offset.
	if (saw_a) {
		float dx = glm::length(a_ws_pos - drop_offset);
		t.check(dx < 0.01f, "InnerA ws_position matches drop transform");
	}

	// Save the outer prefab and assert the file text does NOT contain the inner reference.
	t.editor().save_level(outer_cstr);
	co_await t.wait_ticks(2);

	std::string saved_text = PrefabFile::load_text(outer_cstr);
	t.require(!saved_text.empty(), "outer prefab saved with content");
	t.check(saved_text.find("PrefabAssetComponent") == std::string::npos,
			"saved outer prefab contains no PrefabAssetComponent");
	t.check(saved_text.find(inner_path) == std::string::npos,
			"saved outer prefab contains no path reference to the inner prefab");
	t.check(saved_text.find("InnerA") != std::string::npos, "flattened InnerA persisted in saved text");
	t.check(saved_text.find("InnerB") != std::string::npos, "flattened InnerB persisted in saved text");

	// Reopen and confirm flattened entities are still there and editable.
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + outer_cstr).c_str());
	co_await t.wait_ticks(5);

	editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available after reopen");
	t.require(editor->is_editing_prefab(), "editor back in prefab mode");

	int reopen_inner_a = 0, reopen_inner_b = 0;
	bool reopen_a_editable = false, reopen_b_editable = false;
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* ent = obj->cast_to<Entity>()) {
			const std::string& nm = ent->get_editor_name();
			if (nm == "InnerA") {
				reopen_inner_a++;
				if (!ent->dont_serialize_or_edit) reopen_a_editable = true;
			} else if (nm == "InnerB") {
				reopen_inner_b++;
				if (!ent->dont_serialize_or_edit) reopen_b_editable = true;
			}
		}
	}
	// Exactly one of each — duplicates would indicate init_for_scene failed to clear the level
	// before restoring prefab content (see EditorDocLocal.cpp:354 template-load fallback).
	t.check(reopen_inner_a == 1, "exactly one InnerA after reload (no duplicate)");
	t.check(reopen_inner_b == 1, "exactly one InnerB after reload (no duplicate)");
	t.check(reopen_a_editable, "reloaded InnerA is editable");
	t.check(reopen_b_editable, "reloaded InnerB is editable");

	FileSys::delete_game_file(inner_cstr);
	FileSys::delete_game_file(outer_cstr);
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/prefab_flatten_on_drop", 40.f, test_editor_prefab_flatten_on_drop);

// Regression guard: flatten-drop-then-undo used to crash inside
// SelectionState::get_selection_as_vector because (a) InstantiatePrefabCommand never
// invoked post_node_changes so stale handles weren't pruned, and (b) the function
// dereferenced eng->get_object(e) without a null-check.
static TestTask test_editor_prefab_drop_then_undo_no_crash(TestContext& t) {
	const std::string inner_path = "_undo_inner.tprefab";
	const std::string outer_path = "_undo_outer.tprefab";
	const char* inner_cstr = inner_path.c_str();
	const char* outer_cstr = outer_path.c_str();
	FileSys::delete_game_file(inner_cstr);
	FileSys::delete_game_file(outer_cstr);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	// Inner prefab (2 entities)
	EntityPtr ia = editor->spawn_entity();
	ia->set_editor_name("InnerA");
	ia->set_ls_position({0.0f, 0.0f, 0.0f});
	ia->create_component<MeshComponent>();
	EntityPtr ib = editor->spawn_entity();
	ib->set_editor_name("InnerB");
	ib->set_ls_position({1.0f, 0.0f, 0.0f});
	ib->create_component<MeshComponent>();
	co_await t.wait_ticks(1);

	SerializedSceneFile inner_ser;
	try {
		inner_ser = NewSerialization::serialize_to_text("undo_inner", {ia.get(), ib.get()}, false, inner_path.c_str());
	} catch (const std::exception&) { t.require(false, "serialize inner"); co_return; }
	t.require(PrefabFile::save_text(inner_cstr, inner_ser.text), "inner saved");
	ia->destroy();
	ib->destroy();
	co_await t.wait_ticks(1);

	// Outer prefab (1 entity)
	EntityPtr seed = editor->spawn_entity();
	seed->set_editor_name("OuterSeed");
	seed->set_ls_position({0.0f, 5.0f, 0.0f});
	seed->create_component<MeshComponent>();
	co_await t.wait_ticks(1);

	SerializedSceneFile outer_ser;
	try {
		outer_ser = NewSerialization::serialize_to_text("undo_outer", {seed.get()}, false, outer_path.c_str());
	} catch (const std::exception&) { t.require(false, "serialize outer"); co_return; }
	t.require(PrefabFile::save_text(outer_cstr, outer_ser.text), "outer saved");
	seed->destroy();
	co_await t.wait_ticks(1);

	// Open outer prefab for editing
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + outer_cstr).c_str());
	co_await t.wait_ticks(5);
	editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor after open");
	t.require(editor->is_editing_prefab(), "editing_prefab");

	// Drop inner prefab → flatten path runs
	auto* instantiate = new InstantiatePrefabCommand(*editor, inner_path, glm::mat4(1.0f));
	editor->command_mgr->add_command(instantiate);
	co_await t.wait_ticks(2);

	// Select one of the flattened entities so undo will leave a stale handle behind.
	ISelectionApi* sel = editor->get_editor_api().selection();
	sel->clear_selected();
	EntityPtr to_select;
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* ent = obj->cast_to<Entity>()) {
			if (ent->get_editor_name() == "InnerA") {
				to_select = ent->get_self_ptr();
				break;
			}
		}
	}
	t.require(to_select.get() != nullptr, "found InnerA to select");
	sel->add_select(to_select);
	co_await t.wait_ticks(1);
	t.require(sel->get_selected().size() == 1, "selection has 1 entry after add_select");

	// Undo the flatten. Before the fix this leaves a stale handle in the selection,
	// and the next get_selected() asserts/null-derefs in get_selection_as_vector.
	editor->command_mgr->undo();
	co_await t.wait_ticks(2);

	// This call is the crash site. If we get here without crashing, defense #1 worked.
	auto selected_after = sel->get_selected();
	t.check(selected_after.size() == 0,
			"selection emptied after undo (post_node_changes triggered validate_selection)");

	// Undo destroyed the flattened entities — verify they're gone.
	int still_present = 0;
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* ent = obj->cast_to<Entity>()) {
			const std::string& nm = ent->get_editor_name();
			if (nm == "InnerA" || nm == "InnerB") still_present++;
		}
	}
	t.check(still_present == 0, "undo destroyed all flattened entities");

	FileSys::delete_game_file(inner_cstr);
	FileSys::delete_game_file(outer_cstr);
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/prefab_drop_then_undo_no_crash", 40.f, test_editor_prefab_drop_then_undo_no_crash);

// Regression guard: opening a .tprefab that contains a PrefabAssetComponent reference
// (legacy/bad data) must auto-flatten the embedded prefab inline and warn, leaving zero
// PrefabAssetComponent instances in the editor. Saving must produce a clean form.
static TestTask test_editor_prefab_auto_flatten_on_load(TestContext& t) {
	const std::string inner_path = "_autoflat_inner.tprefab";
	const std::string outer_path = "_autoflat_outer.tprefab";
	const char* inner_cstr = inner_path.c_str();
	const char* outer_cstr = outer_path.c_str();
	FileSys::delete_game_file(inner_cstr);
	FileSys::delete_game_file(outer_cstr);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	// Build inner prefab (2 named entities)
	EntityPtr na = editor->spawn_entity();
	na->set_editor_name("NestedA");
	na->set_ls_position({0.0f, 0.0f, 0.0f});
	na->create_component<MeshComponent>();
	EntityPtr nb = editor->spawn_entity();
	nb->set_editor_name("NestedB");
	nb->set_ls_position({2.0f, 0.0f, 0.0f});
	nb->create_component<MeshComponent>();
	co_await t.wait_ticks(1);

	SerializedSceneFile inner_ser;
	try {
		inner_ser = NewSerialization::serialize_to_text("autoflat_inner", {na.get(), nb.get()}, false,
													   inner_path.c_str());
	} catch (const std::exception&) { t.require(false, "serialize inner"); co_return; }
	t.require(PrefabFile::save_text(inner_cstr, inner_ser.text), "inner saved");
	na->destroy();
	nb->destroy();
	co_await t.wait_ticks(1);

	// Build a "bad" outer prefab: in scene mode (not prefab edit), InstantiatePrefabCommand
	// creates a real PrefabAssetComponent entity. Serialize that single entity to .tprefab.
	auto* spawn_cmd = new InstantiatePrefabCommand(*editor, inner_path, glm::mat4(1.0f));
	editor->command_mgr->add_command(spawn_cmd);
	co_await t.wait_ticks(2);

	Entity* pac_owner = nullptr;
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* ent = obj->cast_to<Entity>()) {
			if (ent->get_component<PrefabAssetComponent>()) {
				pac_owner = ent;
				break;
			}
		}
	}
	t.require(pac_owner != nullptr, "scene-mode spawn produced a PrefabAssetComponent owner");

	SerializedSceneFile bad_ser;
	try {
		bad_ser = NewSerialization::serialize_to_text("bad_outer", {pac_owner}, false, outer_path.c_str());
	} catch (const std::exception&) { t.require(false, "serialize bad outer"); co_return; }
	t.require(PrefabFile::save_text(outer_cstr, bad_ser.text), "bad outer saved");
	// Confirm the file really contains the nested reference (so we know the test setup is valid)
	std::string bad_text_on_disk = PrefabFile::load_text(outer_cstr);
	t.require(bad_text_on_disk.find("PrefabAssetComponent") != std::string::npos,
			  "bad outer file contains PrefabAssetComponent before auto-flatten");
	pac_owner->destroy();
	co_await t.wait_ticks(1);

	// Open the bad outer in prefab edit mode → auto-flatten should run
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + outer_cstr).c_str());
	co_await t.wait_ticks(5);
	editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor after open");
	t.require(editor->is_editing_prefab(), "editing_prefab");

	// No PrefabAssetComponent must remain in the level
	int pac_count = 0;
	int saw_na = 0, saw_nb = 0;
	bool na_editable_rootlevel = false, nb_editable_rootlevel = false;
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (obj->cast_to<PrefabAssetComponent>()) pac_count++;
		if (auto* ent = obj->cast_to<Entity>()) {
			const std::string& nm = ent->get_editor_name();
			if (nm == "NestedA") {
				saw_na++;
				if (!ent->dont_serialize_or_edit && ent->get_parent() == nullptr)
					na_editable_rootlevel = true;
			} else if (nm == "NestedB") {
				saw_nb++;
				if (!ent->dont_serialize_or_edit && ent->get_parent() == nullptr)
					nb_editable_rootlevel = true;
			}
		}
	}
	t.check(pac_count == 0, "auto-flatten removed all PrefabAssetComponent instances");
	t.check(saw_na >= 1, "NestedA promoted to root after auto-flatten");
	t.check(saw_nb >= 1, "NestedB promoted to root after auto-flatten");
	t.check(na_editable_rootlevel, "NestedA is editable and root-level");
	t.check(nb_editable_rootlevel, "NestedB is editable and root-level");

	// Save the now-clean form and confirm the file no longer references the inner prefab
	t.editor().save_level(outer_cstr);
	co_await t.wait_ticks(2);

	std::string saved = PrefabFile::load_text(outer_cstr);
	t.require(!saved.empty(), "outer saved with content");
	t.check(saved.find("PrefabAssetComponent") == std::string::npos,
			"saved outer no longer references PrefabAssetComponent");
	t.check(saved.find(inner_path) == std::string::npos,
			"saved outer no longer references inner prefab path");
	t.check(saved.find("NestedA") != std::string::npos, "NestedA persisted in saved text");
	t.check(saved.find("NestedB") != std::string::npos, "NestedB persisted in saved text");

	FileSys::delete_game_file(inner_cstr);
	FileSys::delete_game_file(outer_cstr);
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/prefab_auto_flatten_on_load", 40.f, test_editor_prefab_auto_flatten_on_load);

// Regression guard: make-prefab-and-replace must be refused in prefab edit mode — otherwise
// it would spawn a PrefabAssetComponent reference inside a prefab, the exact data shape the
// editor forbids (drop-flatten, load-flatten, etc. all exist to keep prefabs free of nested
// PAC references).
static TestTask test_editor_make_prefab_replace_refused_in_prefab_mode(TestContext& t) {
	const std::string seed_path = "_mpr_seed.tprefab";
	const std::string target_path = "_mpr_refused.tprefab";
	const char* seed_cstr = seed_path.c_str();
	const char* target_cstr = target_path.c_str();
	FileSys::delete_game_file(seed_cstr);
	FileSys::delete_game_file(target_cstr);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);

	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	// Build a seed .tprefab with one entity so we have something to open in prefab mode.
	EntityPtr seed = editor->spawn_entity();
	seed->set_editor_name("SeedEntity");
	seed->set_ls_position({0.0f, 0.0f, 0.0f});
	seed->create_component<MeshComponent>();
	co_await t.wait_ticks(1);

	SerializedSceneFile seed_ser;
	try {
		seed_ser = NewSerialization::serialize_to_text("mpr_seed", {seed.get()}, false, seed_path.c_str());
	} catch (const std::exception&) { t.require(false, "serialize seed"); co_return; }
	t.require(PrefabFile::save_text(seed_cstr, seed_ser.text), "seed prefab saved");
	seed->destroy();
	co_await t.wait_ticks(1);

	// Open in prefab edit mode
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + seed_cstr).c_str());
	co_await t.wait_ticks(5);
	editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor after open");
	t.require(editor->is_editing_prefab(), "editing_prefab");

	// Add an entity inside the prefab and select it — typical "I want to extract this into a prefab" gesture.
	EntityPtr victim = editor->spawn_entity();
	victim->set_editor_name("VictimEntity");
	victim->set_ls_position({3.0f, 0.0f, 0.0f});
	victim->create_component<MeshComponent>();
	co_await t.wait_ticks(1);

	// Direct is_valid() check — the gate.
	std::vector<EntityPtr> sel{victim};
	auto* cmd = new MakePrefabAndReplaceCommand(*editor, sel, target_path);
	t.check(!cmd->is_valid(), "MakePrefabAndReplaceCommand is_valid() == false in prefab edit mode");
	delete cmd;

	// Queue through the manager — execute path must not run, victim must survive, no file must be created.
	int count_before = t.editor().entity_count();
	auto* queued = new MakePrefabAndReplaceCommand(*editor, sel, target_path);
	editor->command_mgr->add_command(queued);
	co_await t.wait_ticks(2);

	t.check(victim.get() != nullptr, "selected entity not destroyed by refused command");
	t.check(t.editor().entity_count() == count_before, "entity count unchanged (no PAC spawn)");
	t.check(!FileSys::does_file_exist(target_cstr, FileSys::GAME_DIR),
			"refused command did not write the target .tprefab to disk");

	// And confirm no PrefabAssetComponent was injected into the level.
	int pac_count = 0;
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (obj->cast_to<PrefabAssetComponent>()) pac_count++;
	}
	t.check(pac_count == 0, "no PrefabAssetComponent created in prefab-edit level");

	FileSys::delete_game_file(seed_cstr);
	FileSys::delete_game_file(target_cstr);
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/make_prefab_replace_refused_in_prefab_mode", 30.f,
			test_editor_make_prefab_replace_refused_in_prefab_mode);
