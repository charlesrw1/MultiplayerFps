// Source/IntegrationTests/Tests/Editor/test_parenting.cpp
// Entity parenting (prefab-mode feature): serialization round-trip, ParentToCommand undo/redo,
// mode gating, malformed-input validation, and cycle safety.
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/EditorTestContext.h"
#include "GameEnginePublic.h"
#include "LevelEditor/EditorDocLocal.h"
#include "LevelEditor/Commands.h"
#include "LevelSerialization/SerializeNew.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/GroupComponent.h"
#include "Game/Prefab.h"
#include "Framework/Files.h"
#include "Framework/StringName.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static Entity* find_named(UnserializedSceneFile& f, const char* name) {
	for (auto o : f.all_obj_vec)
		if (auto e = o->cast_to<Entity>())
			if (e->get_editor_name() == name)
				return e;
	return nullptr;
}

// Minimal prefab body used to drop the editor into prefab-edit mode.
static const char* kScratchPrefab = "!json\n{\"objs\":[{\"__typename\":\"MeshComponent\",\"editor_name\":\"seed\"}]}";

// Finds a live entity in the currently loaded level by editor name (post-insert).
static Entity* find_in_level(const char* name) {
	for (auto o : eng->get_level()->get_all_objects())
		if (auto e = o->cast_to<Entity>())
			if (e->get_editor_name() == name)
				return e;
	return nullptr;
}

// Round-trips a parent+child+grandchild chain through serialize -> unserialize -> INSERT-into-level
// and checks that parent links, local transforms, is_top_level, and parent_bone are all preserved.
// Critically this drives the insert path (the same one Level::start/prefab-load use), which is where
// the "unparented on entry" assert lives and where parenting is actually established. A test that
// only checks unserialize output would miss that entirely.
static TestTask test_parenting_serialize_roundtrip(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Entity* parent = editor->spawn_entity();
	parent->set_editor_name("RT_P");
	parent->create_component<MeshComponent>();
	parent->set_ls_position({5.f, 0.f, 0.f});

	Entity* child = editor->spawn_entity();
	child->set_editor_name("RT_C");
	child->create_component<MeshComponent>();
	child->parent_to(parent);
	child->set_ls_position({1.f, 2.f, 3.f});
	child->set_parent_bone(NAME("spine_01"));

	Entity* grandchild = editor->spawn_entity();
	grandchild->set_editor_name("RT_G");
	grandchild->create_component<MeshComponent>();
	grandchild->parent_to(child);
	grandchild->set_ls_position({0.f, 4.f, 0.f});
	grandchild->set_is_top_level(true);

	co_await t.wait_ticks(1);

	std::vector<Entity*> ents = {parent, child, grandchild};
	std::string text =
		NewSerialization::serialize_to_text("test_parent", ents, false, nullptr, nullptr, /*hierarchy*/ true).text;
	t.check(text.find("__parent") != std::string::npos, "parent link written to text");
	t.check(text.find("__parent_bone") != std::string::npos, "parent bone written to text");
	t.check(text.find("__is_top_level") != std::string::npos, "top-level flag written to text");

	// Destroy the authored originals so the round-tripped copies are the only RT_* in the level
	// (find_in_level matches by name; duplicates would make the assertions meaningless).
	parent->destroy(); // cascades to child + grandchild
	co_await t.wait_ticks(1);

	// Unserialize leaves entities flat (the insert contract); the hierarchy is recorded, not applied.
	UnserializedSceneFile un = NewSerialization::unserialize_from_text("test_parent", text, false);
	t.check(!un.hierarchy.empty(), "hierarchy links captured, not applied during unserialize");
	for (auto o : un.all_obj_vec)
		if (auto e = o->cast_to<Entity>())
			t.require(e->get_parent() == nullptr, "entities are unparented straight out of unserialize");

	// Insert into the live level: this is where parenting is established (and where the crash was).
	editor->insert_unserialized_into_scene(un);
	co_await t.wait_ticks(1);

	Entity* rp = find_in_level("RT_P");
	Entity* rc = find_in_level("RT_C");
	Entity* rg = find_in_level("RT_G");
	t.require(rp && rc && rg, "all three entities present in level after insert");

	t.check(rc->get_parent() == rp, "child reparented to parent after insert");
	t.check(rg->get_parent() == rc, "grandchild reparented to child after insert");
	t.check(glm::length(rc->get_ls_position() - glm::vec3(1.f, 2.f, 3.f)) < 0.001f, "child local pos preserved");
	t.check(glm::length(rg->get_ls_position() - glm::vec3(0.f, 4.f, 0.f)) < 0.001f, "grandchild local pos preserved");
	t.check(rc->has_parent_bone() && rc->get_parent_bone() == NAME("spine_01"), "parent bone preserved");
	t.check(rg->get_is_top_level(), "is_top_level preserved");

	rp->destroy(); // cascades to child + grandchild
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/parenting_serialize_roundtrip", 20.f, test_parenting_serialize_roundtrip);

// Regression for the load-path assert: saving a prefab that contains parenting and then OPENING it
// fresh must not trip "get_parent() == nullptr" in insert_unserialized_entities_into_level_internal.
// This reproduces the exact crash (open animman.tprefab) via a synthetic prefab.
static TestTask test_parenting_prefab_load(TestContext& t) {
	const std::string pp = "_parenting_load.tprefab";
	FileSys::delete_game_file(pp.c_str());

	// Build a parent+child hierarchy in prefab mode and save it with hierarchy serialization.
	PrefabFile::save_text(pp, kScratchPrefab);
	co_await t.wait_ticks(1);
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + pp).c_str());
	co_await t.wait_ticks(5);
	{
		EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
		t.require(editor && editor->is_editing_prefab(), "in prefab mode to author");
		Entity* parent = editor->spawn_entity();
		parent->set_editor_name("LOAD_P");
		parent->create_component<MeshComponent>();
		Entity* child = editor->spawn_entity();
		child->set_editor_name("LOAD_C");
		child->create_component<MeshComponent>();
		child->parent_to(parent);

		std::vector<Entity*> ents = {parent, child};
		std::string text =
			NewSerialization::serialize_to_text("author", ents, false, pp.c_str(), nullptr, /*hierarchy*/ true).text;
		PrefabFile::save_text(pp, text.c_str());
		co_await t.wait_ticks(1);
	}

	// Re-open the prefab from scratch. This goes through Level::start -> insert, the crashing path.
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + pp).c_str());
	co_await t.wait_ticks(5);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor && editor->is_editing_prefab(), "prefab reopened without assert/crash");

	Entity* rp = find_in_level("LOAD_P");
	Entity* rc = find_in_level("LOAD_C");
	t.require(rp && rc, "both entities loaded from prefab");
	t.check(rc->get_parent() == rp, "child parented to parent after prefab load");

	FileSys::delete_game_file(pp.c_str());
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/parenting_prefab_load", 25.f, test_parenting_prefab_load);

// Malformed __parent references must be rejected with SerializeInputError, not silently accepted or
// crash.
static TestTask test_parenting_malformed_input(TestContext& t) {
	co_await t.wait_ticks(1);

	auto expect_throw = [&](const std::string& text, const char* what) {
		bool threw = false;
		try {
			NewSerialization::unserialize_from_text("bad", text, false);
		} catch (const SerializeInputError&) {
			threw = true;
		}
		t.check(threw, what);
	};

	// __parent index out of range.
	expect_throw("!json\n{\"objs\":[{\"__typename\":\"MeshComponent\",\"__parent\":5}]}",
				 "out-of-range __parent throws");
	// __parent pointing at itself.
	expect_throw("!json\n{\"objs\":[{\"__typename\":\"MeshComponent\",\"__parent\":0}]}",
				 "self-referential __parent throws");
	// __parent wrong type.
	expect_throw("!json\n{\"objs\":[{\"__typename\":\"MeshComponent\",\"__parent\":\"x\"}]}",
				 "non-integer __parent throws");

	// A valid backward reference must NOT throw and must record a link (applied later at insert).
	std::string ok = "!json\n{\"objs\":["
					 "{\"__typename\":\"MeshComponent\",\"editor_name\":\"A\"},"
					 "{\"__typename\":\"MeshComponent\",\"editor_name\":\"B\",\"__parent\":0}]}";
	bool threw = false;
	try {
		UnserializedSceneFile un = NewSerialization::unserialize_from_text("ok", ok, false);
		Entity* a = find_named(un, "A");
		Entity* b = find_named(un, "B");
		t.require(a && b, "valid hierarchy unserialized both entities");
		t.check(b->get_parent() == nullptr, "unserialize leaves entities flat (parenting deferred to insert)");
		bool linked = false;
		for (auto& l : un.hierarchy)
			if (l.child == b && l.parent == a)
				linked = true;
		t.check(linked, "valid __parent recorded as a deferred link B->A");
	} catch (const SerializeInputError&) {
		threw = true;
	}
	t.check(!threw, "valid hierarchy does not throw");
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/parenting_malformed_input", 15.f, test_parenting_malformed_input);

// ParentToCommand is prefab-only; in level-edit mode is_valid() must be false.
static TestTask test_parenting_mode_gating(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");
	t.check(!editor->is_editing_prefab(), "level-edit mode");

	Entity* a = editor->spawn_entity();
	a->create_component<MeshComponent>();
	Entity* b = editor->spawn_entity();
	b->create_component<MeshComponent>();

	ParentToCommand cmd(*editor, {b}, a, false, false);
	t.check(!cmd.is_valid(), "parenting command invalid in level-edit mode");

	a->destroy();
	b->destroy();
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/parenting_mode_gating", 15.f, test_parenting_mode_gating);

// ParentToCommand: parent-to-existing preserves world transform and undo restores it exactly.
static TestTask test_parenting_command_undo_redo(TestContext& t) {
	const std::string pp = "_parenting_cmd.tprefab";
	FileSys::delete_game_file(pp.c_str());
	PrefabFile::save_text(pp, kScratchPrefab);
	co_await t.wait_ticks(1);
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + pp).c_str());
	co_await t.wait_ticks(5);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor && editor->is_editing_prefab(), "in prefab mode");

	Entity* parent = editor->spawn_entity();
	parent->create_component<MeshComponent>();
	parent->set_ws_position({5.f, 0.f, 0.f});

	Entity* child = editor->spawn_entity();
	child->create_component<MeshComponent>();
	child->set_ws_position({3.f, 1.f, 0.f});
	const glm::vec3 child_ws = child->get_ws_position();

	ParentToCommand cmd(*editor, {child}, parent, false, false);
	t.check(cmd.is_valid(), "valid in prefab mode");
	cmd.execute();
	t.check(child->get_parent() == parent, "child parented on execute");
	t.check(glm::length(child->get_ws_position() - child_ws) < 0.01f, "world transform preserved on parent");

	cmd.undo();
	t.check(child->get_parent() == nullptr, "child unparented on undo");
	t.check(glm::length(child->get_ws_position() - child_ws) < 0.01f, "world transform preserved on undo");

	cmd.execute(); // redo
	t.check(child->get_parent() == parent, "child re-parented on redo");

	parent->destroy();
	FileSys::delete_game_file(pp.c_str());
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/parenting_command_undo_redo", 20.f, test_parenting_command_undo_redo);

// Regression: reparenting a bone-attached child (or clearing its parent) must not leave a stale
// parent_bone behind. execute() overwrites the bone to match the command's intent; undo restores it.
static TestTask test_parenting_clears_stale_bone(TestContext& t) {
	const std::string pp = "_parenting_bone.tprefab";
	FileSys::delete_game_file(pp.c_str());
	PrefabFile::save_text(pp, kScratchPrefab);
	co_await t.wait_ticks(1);
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + pp).c_str());
	co_await t.wait_ticks(5);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor && editor->is_editing_prefab(), "in prefab mode");

	Entity* parent = editor->spawn_entity();
	parent->create_component<MeshComponent>();

	Entity* child = editor->spawn_entity();
	child->create_component<MeshComponent>();
	child->set_parent_bone(NAME("spine_01")); // pretend it was bone-attached earlier

	// Reparent to a plain entity with no bone: the stale "spine_01" must be dropped.
	ParentToCommand reparent(*editor, {child}, parent, false, false);
	t.check(reparent.is_valid(), "reparent valid");
	reparent.execute();
	t.check(!child->has_parent_bone(), "reparent-without-bone clears stale parent_bone");
	reparent.undo();
	t.check(child->has_parent_bone() && child->get_parent_bone() == NAME("spine_01"),
			"undo restores prior parent_bone");

	// Clearing the parent must also drop the bone.
	child->set_parent_bone(NAME("spine_01"));
	ParentToCommand clear(*editor, {child}, nullptr, false, /*clear_parent*/ true);
	clear.execute();
	t.check(!child->has_parent_bone(), "clear-parent drops parent_bone");

	parent->destroy();
	child->destroy();
	FileSys::delete_game_file(pp.c_str());
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/parenting_clears_stale_bone", 20.f, test_parenting_clears_stale_bone);

// "Parent to New Empty" spawns an EmptyComponent group node; undo removes it and unparents.
static TestTask test_parenting_new_empty(TestContext& t) {
	const std::string pp = "_parenting_empty.tprefab";
	FileSys::delete_game_file(pp.c_str());
	PrefabFile::save_text(pp, kScratchPrefab);
	co_await t.wait_ticks(1);
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + pp).c_str());
	co_await t.wait_ticks(5);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor && editor->is_editing_prefab(), "in prefab mode");

	Entity* child = editor->spawn_entity();
	child->create_component<MeshComponent>();
	child->set_ws_position({2.f, 0.f, 0.f});

	ParentToCommand cmd(*editor, {child}, nullptr, /*create_new_parent*/ true, false);
	t.check(cmd.is_valid(), "new-empty command valid");
	cmd.execute();
	Entity* empty = child->get_parent();
	t.require(empty != nullptr, "child got a new empty parent");
	t.check(empty->get_component<GroupComponent>() != nullptr, "new parent carries GroupComponent (serializable)");

	cmd.undo();
	t.check(child->get_parent() == nullptr, "undo unparents child");
	t.check(!empty->get_self_ptr().get(), "undo destroyed the spawned empty");

	child->destroy();
	FileSys::delete_game_file(pp.c_str());
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/parenting_new_empty", 20.f, test_parenting_new_empty);

// Attempting to create a cycle (parent an ancestor under its own descendant) must not crash or hang.
static TestTask test_parenting_cycle_safety(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Entity* a = editor->spawn_entity();
	a->create_component<MeshComponent>();
	Entity* b = editor->spawn_entity();
	b->create_component<MeshComponent>();

	b->parent_to(a);       // b under a
	a->parent_to(b);       // attempt cycle: a under its own child b

	// parent_to's cycle guard must have kept the graph acyclic (no infinite loop reaching here).
	int depth = 0;
	for (Entity* n = a; n; n = n->get_parent()) {
		++depth;
		t.require(depth < 100, "hierarchy is acyclic after cycle attempt");
	}
	t.check(true, "no crash/hang on cycle attempt");

	a->destroy();
	b->destroy();
	co_await t.wait_ticks(1);
}
EDITOR_TEST("editor/parenting_cycle_safety", 15.f, test_parenting_cycle_safety);
