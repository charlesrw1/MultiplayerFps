// Source/IntegrationTests/Tests/Editor/test_lua_component.cpp
//
// Editor-mode integration tests for Lua-defined components:
//   1. Place a Lua component on an entity, edit a field via reflection (the same
//      path the property grid uses), save the level, reload, verify it survives.
//   2. Hot-reload the Lua class with a renamed + removed + added field set; verify
//      the live instance's shadow buffer was migrated (overlapping kept, missing
//      dropped, new fields use template defaults).
//   3. Confirm PropertyGrid synthesizes a row per Lua-backed field.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/EditorTestContext.h"
#include "GameEnginePublic.h"
#include "GameEngineLocal.h"
#include "LevelEditor/EditorDocLocal.h"
#include "LevelEditor/EdPropertyGrid.h"
#include "LevelEditor/SelectionState.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Files.h"
#include "Framework/PropertyEd.h"
#include "Framework/FnFactory.h"
#include "LevelEditor/PropertyEditors.h"
#include "Scripting/ScriptManager.h"
#include "Level.h"
#include <string>

namespace {

// Defines two Lua component classes (initial + reload variant) used across tests.
// Loaded via ScriptManager::reload_from_content so we don't have to drop a .lua
// file under Data/scripts/ during the test.
constexpr const char* kLuaSrcInitial =
	"---@class TestLuaComp : Component\n"
	"TestLuaComp = {\n"
	"  ---@type number\n"
	"  hp = 50,\n"
	"  ---@type boolean\n"
	"  alive = true,\n"
	"  ---@type string\n"
	"  display = \"\",\n"
	"}\n"
	"function TestLuaComp:start() end\n";

// 'alive' removed, 'speed' added, 'hp' + 'display' kept (so they must migrate).
constexpr const char* kLuaSrcReloaded =
	"---@class TestLuaComp : Component\n"
	"TestLuaComp = {\n"
	"  ---@type number\n"
	"  hp = 99,\n"
	"  ---@type string\n"
	"  display = \"new_default\",\n"
	"  ---@type number\n"
	"  speed = 7,\n"
	"}\n"
	"function TestLuaComp:start() end\n";

void load_initial_lua_class() {
	ScriptManager::inst->reload_from_content(kLuaSrcInitial, "test_lua_component_initial");
	ScriptManager::inst->check_for_reload();
}

// Returns the registered LuaClassTypeInfo* for TestLuaComp, or nullptr.
const ClassTypeInfo* find_test_lua_comp() {
	return ClassBase::find_class("TestLuaComp");
}

// Convenience: gets the editor doc and spawns one fresh entity into it.
Entity* spawn_entity_in_editor() {
	auto* doc = static_cast<EditorDoc*>(eng->get_tool());
	return doc->spawn_entity();
}

Component* attach_test_lua_component(Entity* e, const ClassTypeInfo* ti) {
	auto* doc = static_cast<EditorDoc*>(eng->get_tool());
	return doc->attach_component(ti, e);
}

// Look up a single PROP_LUA_BACKED prop by name on the given instance's type.
const PropertyInfo* find_field_comp(const ClassTypeInfo* ti, const char* name) {
	if (!ti || !ti->props)
		return nullptr;
	for (int i = 0; i < ti->props->count; ++i) {
		auto& pi = ti->props->list[i];
		if ((pi.flags & PROP_LUA_BACKED) && std::string(pi.name) == name)
			return &pi;
	}
	return nullptr;
}

} // namespace

// 1. Place + edit + save + reload: the field value lands in the shadow buffer,
//    is serialized through the standard reflection backend, and is restored on load.
static TestTask test_place_edit_save_reload(TestContext& t) {
	load_initial_lua_class();

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "blank editor level loaded");

	auto* ti = find_test_lua_comp();
	t.require(ti != nullptr, "TestLuaComp registered after reload_from_content");
	t.require(ti->props != nullptr, "TestLuaComp synthesized a PropertyInfoList");
	t.check(ti->props->count == 3, "TestLuaComp exposes 3 fields (hp, alive, display)");

	Entity* e = spawn_entity_in_editor();
	t.require(e != nullptr, "spawned entity");

	Component* comp = attach_test_lua_component(e, ti);
	t.require(comp != nullptr, "attached TestLuaComp to entity");
	t.check(comp->get_lua_field_shadow() == nullptr, "shadow null before first property access");

	// Template defaults must be present in the shadow once lazily created.
	auto* hp_pi = find_field_comp(ti, "hp");
	auto* alive_pi = find_field_comp(ti, "alive");
	auto* display_pi = find_field_comp(ti, "display");
	t.require(hp_pi && alive_pi && display_pi, "all three reflected fields found");
	t.check(hp_pi->get_float(comp) == 50.f, "hp defaulted to template value 50");
	t.check(comp->get_lua_field_shadow() != nullptr, "shadow lazily created on first get_ptr");
	t.check(alive_pi->get_int(comp) == 1, "alive defaulted to true");

	// Edit through the same accessors the property grid uses.
	hp_pi->set_float(comp, 1234.5f);
	*(std::string*)display_pi->get_ptr(comp) = "edited_value";
	alive_pi->set_int(comp, 0);
	co_await t.wait_ticks(1);

	t.check(hp_pi->get_float(comp) == 1234.5f, "edit landed in shadow buffer (hp)");

	// Save + reopen. The reflection-driven serializer must round-trip Lua-backed fields.
	const char* kPath = "_temp_lua_component.tmap";
	FileSys::delete_game_file(kPath);
	t.editor().save_level(kPath);
	co_await t.wait_ticks(2);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + kPath).c_str());
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "reloaded level non-null");

	// Locate the reloaded component (the entity is the only one with a Lua component).
	Component* reloaded = nullptr;
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* c = obj ? obj->cast_to<Component>() : nullptr) {
			if (&c->get_type() == ti) {
				reloaded = c;
				break;
			}
		}
	}
	t.require(reloaded != nullptr, "found round-tripped TestLuaComp after reload");

	t.check(hp_pi->get_float(reloaded) == 1234.5f, "hp survived save/reload");
	t.check(alive_pi->get_int(reloaded) == 0, "alive survived save/reload");
	auto* reloaded_display = (std::string*)display_pi->get_ptr(reloaded);
	t.check(*reloaded_display == "edited_value", "display survived save/reload");

	FileSys::delete_game_file(kPath);
}
EDITOR_TEST("editor/lua_component_place_edit_save_reload", 30.f, test_place_edit_save_reload);

// 2. Hot-reload merge: live instance's overlapping fields are preserved by name,
//    removed fields dropped, new fields seeded with new template defaults.
static TestTask test_hotreload_field_migration(TestContext& t) {
	load_initial_lua_class();

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);

	auto* ti = find_test_lua_comp();
	t.require(ti != nullptr, "TestLuaComp registered");

	Entity* e = spawn_entity_in_editor();
	Component* comp = attach_test_lua_component(e, ti);
	t.require(comp != nullptr, "Lua component attached");

	// Set distinctive values on overlapping + soon-to-be-removed fields.
	auto* hp_pi_old = find_field_comp(ti, "hp");
	auto* alive_pi_old = find_field_comp(ti, "alive");
	auto* display_pi_old = find_field_comp(ti, "display");
	t.require(hp_pi_old && alive_pi_old && display_pi_old, "pre-reload fields present");
	hp_pi_old->set_float(comp, 777.f);
	*(std::string*)display_pi_old->get_ptr(comp) = "before_reload";
	alive_pi_old->set_int(comp, 1);

	// Snapshot the shadow buffer pointer; the reload-merge path reallocates so the
	// pointer is expected to change.
	uint8_t* old_shadow = comp->get_lua_field_shadow();
	t.require(old_shadow != nullptr, "old shadow allocated");

	// Push the reloaded source. check_for_reload triggers the 4-phase merge.
	ScriptManager::inst->reload_from_content(kLuaSrcReloaded, "test_lua_component_reloaded");
	ScriptManager::inst->check_for_reload();
	co_await t.wait_ticks(1);

	// Type pointer is stable across reload (same LuaClassTypeInfo*) — only the
	// synthesized PropertyInfoList changes.
	t.check(find_test_lua_comp() == ti, "LuaClassTypeInfo* stable across reload");
	t.require(ti->props != nullptr, "post-reload props non-null");
	t.check(ti->props->count == 3, "post-reload has 3 fields (hp, display, speed)");

	auto* hp_pi_new = find_field_comp(ti, "hp");
	auto* display_pi_new = find_field_comp(ti, "display");
	auto* speed_pi_new = find_field_comp(ti, "speed");
	auto* alive_pi_new = find_field_comp(ti, "alive"); // removed in new layout
	t.check(hp_pi_new != nullptr, "hp still reflected after reload");
	t.check(display_pi_new != nullptr, "display still reflected after reload");
	t.check(speed_pi_new != nullptr, "speed reflected after reload");
	t.check(alive_pi_new == nullptr, "alive removed from reflection after reload");

	// Shadow must have been reallocated (sizes likely differ; pointer should not collide).
	t.check(comp->get_lua_field_shadow() != nullptr, "new shadow allocated after reload");

	// Migration: overlapping fields keep their old values.
	t.check(hp_pi_new->get_float(comp) == 777.f, "hp preserved across reload merge");
	t.check(*(std::string*)display_pi_new->get_ptr(comp) == "before_reload",
			"display preserved across reload merge");
	// Added field uses NEW class template default.
	t.check(speed_pi_new->get_float(comp) == 7.f, "speed seeded from new template default");
}
EDITOR_TEST("editor/lua_component_hotreload_field_migration", 30.f, test_hotreload_field_migration);

// 3. PropertyGrid surfaces Lua-backed fields as rows. The grid walks the class's
//    super chain and pushes one PropertyInfoList per level — we expect TestLuaComp's
//    own list (3 fields) to appear in addition to whatever Component inherits.
#ifdef EDITOR_BUILD
static TestTask test_property_grid_rows(TestContext& t) {
	load_initial_lua_class();

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);

	auto* ti = find_test_lua_comp();
	t.require(ti != nullptr, "TestLuaComp registered");

	Entity* e = spawn_entity_in_editor();
	Component* comp = attach_test_lua_component(e, ti);
	t.require(comp != nullptr, "Lua component attached");

	// Drive the property grid via a freshly-built factory mirroring the editor's setup
	// (grid_factory is private on EditorDoc; rebuilding here avoids touching access).
	FnFactory<IPropertyEditor> factory;
	PropertyFactoryUtil::register_basic(factory);
	PropertyGrid grid(factory);
	grid.add_class_to_grid(comp);

	// At minimum, one GroupRow is added per non-empty PropertyInfoList up the super
	// chain. The TestLuaComp list itself contributes our 3 fields as child rows.
	t.check(!grid.rows.empty(), "grid populated rows from add_class_to_grid");

	// Find the GroupRow whose proplist matches our synthesized lua_props_list.
	bool found_our_group = false;
	int our_field_rows = 0;
	for (auto& row : grid.rows) {
		auto* group = dynamic_cast<GroupRow*>(row.get());
		if (!group || group->proplist != ti->props)
			continue;
		found_our_group = true;
		our_field_rows = (int)group->child_rows.size();
		break;
	}
	t.check(found_our_group, "PropertyGrid contains a GroupRow keyed on TestLuaComp's PropertyInfoList");
	t.check(our_field_rows == 3, "GroupRow has one child row per Lua-backed field");
}
EDITOR_TEST("editor/lua_component_property_grid_rows", 30.f, test_property_grid_rows);

// 4. Hot-reload while a Lua component is currently SELECTED in the editor's
//    property grid. Repro for a 0xFFFFFFFF crash: the editor's grid_cache keys
//    rebuild on the selected-object handle vector, NOT on the underlying
//    PropertyInfoList. Hot-reload swaps ti->props out from under it, freeing
//    the old PropertyInfo storage; the cached GroupRow / PropertyRow still
//    hold raw `const PropertyInfo*` into that freed memory, so the next
//    imgui_draw() tick dereferences garbage during update().
//
//    Drives the real editor pipeline (selection_state -> on_selection_changed
//    -> EdPropertyGrid::draw via UiSystem::draw_imgui_internal) so the failure
//    mode exactly matches what the user sees. On crash, the SEH handler in
//    Source/Framework/Util.cpp writes a minidump next to the test log
//    (test_editor_output.dmp); inspect with `Scripts/dbg.ps1 <dump> "kP 30"`.
static TestTask test_property_grid_crash_after_hot_reload(TestContext& t) {
	load_initial_lua_class();

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "editor level loaded");

	auto* ti = find_test_lua_comp();
	t.require(ti != nullptr, "TestLuaComp registered");
	t.require(ti->props != nullptr, "TestLuaComp has props before reload");

	auto* doc = static_cast<EditorDoc*>(eng->get_tool());
	t.require(doc != nullptr, "got EditorDoc");

	Entity* e = doc->spawn_entity();
	t.require(e != nullptr, "spawned entity");
	Component* comp = doc->attach_component(ti, e);
	t.require(comp != nullptr, "attached TestLuaComp to entity");

	// Drive the same path the user does: selecting in the outliner fires
	// SelectionState::on_selection_changed, which flips EdPropertyGrid into
	// "needs refresh" mode and then on the next draw rebuilds rows pointing
	// into ti->props.
	doc->selection_state->set_select_only_this(e);

	// Two ticks: enough imgui frames for EdPropertyGrid::draw -> grid_cache
	// to populate rows from the CURRENT props list. Property panel must
	// actually have drawn at least once before reload, otherwise the cached
	// row vector is empty and there's nothing to crash on.
	co_await t.wait_ticks(3);

	// Snapshot — selection still pointing at our entity, grid populated.
	t.require(doc->selection_state->has_only_one_selected(), "entity still selected");

	// Hot-reload: synthesize_lua_props_unchecked replaces ti->props with a
	// freshly-allocated PropertyInfoList; the old storage is freed.
	ScriptManager::inst->reload_from_content(kLuaSrcReloaded, "test_lua_component_reloaded");
	ScriptManager::inst->check_for_reload();
	co_await t.wait_ticks(1);

	t.require(ti->props != nullptr, "TestLuaComp has props after reload");

	// The cached PropertyInfo* in the grid would now dangle if not invalidated.
	// Drive more imgui_draw() frames; with the on_class_reloaded hook in place
	// EdPropertyGrid drops its cache and rebuilds against the new layout. With
	// the bug present, internal_update() derefs freed memory and SEHs.
	co_await t.wait_ticks(5);

	t.check(doc->prop_editor != nullptr, "prop_editor alive");
	t.check(doc->selection_state->has_only_one_selected(), "selection still valid post-reload");
}
EDITOR_TEST("editor/lua_component_property_grid_crash_after_hot_reload", 30.f,
			test_property_grid_crash_after_hot_reload);
#endif

// 5. ~Component on a Lua component with a non-empty std::string field must run
//    the placement-delete (destruct_lua_field), and the LuaClassTypeInfo's
//    live-instance set must drop the pointer (unregister_lua_instance). The
//    follow-up hot-reload would crash iterating a dangling Component* if the
//    unregister never ran.
static TestTask test_destructor_string_dtor_and_unregister(TestContext& t) {
	load_initial_lua_class();

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "editor level loaded");

	auto* ti = find_test_lua_comp();
	t.require(ti != nullptr, "TestLuaComp registered");
	// LuaClassTypeInfo inherits ClassTypeInfo; this is the same cast lua_class_alloc does.
	auto* lti = static_cast<const LuaClassTypeInfo*>(ti);

	Entity* e = spawn_entity_in_editor();
	Component* comp = attach_test_lua_component(e, ti);
	t.require(comp != nullptr, "TestLuaComp attached");

	auto* display_pi = find_field_comp(ti, "display");
	t.require(display_pi != nullptr, "display field present");
	// Force a heap allocation so a missing dtor would be visible to leak detection.
	*(std::string*)display_pi->get_ptr(comp) = "non_empty_payload_to_force_string_alloc";

	t.require(lti->get_live_instances().size() == 1, "exactly one live instance pre-destroy");
	t.require(lti->get_live_instances().count(comp) == 1, "live set contains our component");

	// Trigger destruction; deferred-delete drains by end of frame.
	eng->get_level()->destroy_entity(e);
	co_await t.wait_ticks(2);

	t.check(lti->get_live_instances().empty(),
			"live_instances drained after destroy_entity (unregister_lua_instance ran)");

	// Reload now that the only instance has been destroyed: the snapshot phase
	// must iterate an empty set rather than a stale pointer. Survival = pass.
	ScriptManager::inst->reload_from_content(kLuaSrcReloaded, "test_lua_component_reloaded");
	ScriptManager::inst->check_for_reload();
	co_await t.wait_ticks(1);

	t.check(lti->get_live_instances().empty(),
			"live_instances still empty after post-destroy reload");
}
EDITOR_TEST("editor/lua_component_destructor_runs_string_dtor_and_unregisters", 30.f,
			test_destructor_string_dtor_and_unregister);

// 6. Multiple live instances all get an independent shadow reallocation on reload
//    and each one's overlapping fields are snapshot-restored to their own values.
static TestTask test_multiple_instances_hot_reload_migration(TestContext& t) {
	load_initial_lua_class();

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);

	auto* ti = find_test_lua_comp();
	t.require(ti != nullptr, "TestLuaComp registered");

	auto* hp_pi_old = find_field_comp(ti, "hp");
	auto* display_pi_old = find_field_comp(ti, "display");
	t.require(hp_pi_old && display_pi_old, "pre-reload fields present");

	constexpr int kN = 3;
	Component* comps[kN] = {};
	const float kHp[kN] = { 100.f, 200.f, 300.f };
	const char* kDisp[kN] = { "a", "b", "c" };
	void* old_shadows[kN] = {};

	for (int i = 0; i < kN; ++i) {
		Entity* e = spawn_entity_in_editor();
		t.require(e != nullptr, "spawned entity");
		comps[i] = attach_test_lua_component(e, ti);
		t.require(comps[i] != nullptr, "attached TestLuaComp");
		hp_pi_old->set_float(comps[i], kHp[i]);
		*(std::string*)display_pi_old->get_ptr(comps[i]) = kDisp[i];
		old_shadows[i] = comps[i]->get_lua_field_shadow();
		t.require(old_shadows[i] != nullptr, "pre-reload shadow allocated");
	}

	// Distinct pre-reload shadows for distinct components.
	t.check(old_shadows[0] != old_shadows[1] && old_shadows[1] != old_shadows[2] &&
			old_shadows[0] != old_shadows[2],
			"each instance has its own shadow buffer pre-reload");

	ScriptManager::inst->reload_from_content(kLuaSrcReloaded, "test_lua_component_reloaded");
	ScriptManager::inst->check_for_reload();
	co_await t.wait_ticks(1);

	auto* hp_pi_new = find_field_comp(ti, "hp");
	auto* display_pi_new = find_field_comp(ti, "display");
	auto* speed_pi_new = find_field_comp(ti, "speed");
	t.require(hp_pi_new && display_pi_new && speed_pi_new, "post-reload fields present");

	for (int i = 0; i < kN; ++i) {
		t.check(comps[i]->get_lua_field_shadow() != nullptr,
				"post-reload shadow non-null");
		t.check(hp_pi_new->get_float(comps[i]) == kHp[i],
				"hp preserved per-instance across reload merge");
		t.check(*(std::string*)display_pi_new->get_ptr(comps[i]) == kDisp[i],
				"display preserved per-instance across reload merge");
		t.check(speed_pi_new->get_float(comps[i]) == 7.f,
				"speed seeded from new template default on every instance");
	}

	// Post-reload shadows must still be distinct per-instance (no aliasing).
	void* new_shadows[kN];
	for (int i = 0; i < kN; ++i) new_shadows[i] = comps[i]->get_lua_field_shadow();
	t.check(new_shadows[0] != new_shadows[1] && new_shadows[1] != new_shadows[2] &&
			new_shadows[0] != new_shadows[2],
			"each instance has its own shadow buffer post-reload");
}
EDITOR_TEST("editor/lua_component_multiple_instances_hot_reload_migration", 30.f,
			test_multiple_instances_hot_reload_migration);

// 7. Save a level with a Lua component, then drop one of its fields via hot-reload,
//    then load the saved map: SerializeNew should record an "unknown field" warning
//    in Level::unknown_field_warnings and load the surviving fields correctly.
static TestTask test_save_then_reload_class_drops_field(TestContext& t) {
	load_initial_lua_class();

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);

	auto* ti = find_test_lua_comp();
	t.require(ti != nullptr, "TestLuaComp registered");

	auto* hp_pi_pre = find_field_comp(ti, "hp");
	auto* alive_pi_pre = find_field_comp(ti, "alive");
	auto* display_pi_pre = find_field_comp(ti, "display");
	t.require(hp_pi_pre && alive_pi_pre && display_pi_pre, "initial fields present");

	Entity* e = spawn_entity_in_editor();
	Component* comp = attach_test_lua_component(e, ti);
	t.require(comp != nullptr, "TestLuaComp attached");
	hp_pi_pre->set_float(comp, 555.f);
	alive_pi_pre->set_int(comp, 1);
	*(std::string*)display_pi_pre->get_ptr(comp) = "kept";

	const char* kPath = "_temp_lua_dropfield.tmap";
	FileSys::delete_game_file(kPath);
	t.editor().save_level(kPath);
	co_await t.wait_ticks(2);

	// Hot-reload class: 'alive' dropped, 'speed' added. The pre-save live instance is
	// migrated by the merge but will be torn down when open-editor swaps the level.
	ScriptManager::inst->reload_from_content(kLuaSrcReloaded, "test_lua_component_reloaded");
	ScriptManager::inst->check_for_reload();
	co_await t.wait_ticks(1);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + kPath).c_str());
	co_await t.wait_ticks(4);
	t.require(eng->get_level() != nullptr, "saved level reopened");

	// "Unknown field" warning surfaces in Level::unknown_field_warnings — substring
	// match keeps us decoupled from the exact "<typename>.<field>" format.
	bool found_alive_warning = false;
	for (auto& w : eng->get_level()->unknown_field_warnings) {
		if (w.find("alive") != std::string::npos && w.find("TestLuaComp") != std::string::npos) {
			found_alive_warning = true;
			break;
		}
	}
	t.check(found_alive_warning,
			"Level::unknown_field_warnings contains an entry for TestLuaComp.alive");

	// Surviving fields must load correctly under the new layout.
	auto* hp_pi_new = find_field_comp(ti, "hp");
	auto* display_pi_new = find_field_comp(ti, "display");
	auto* speed_pi_new = find_field_comp(ti, "speed");
	t.require(hp_pi_new && display_pi_new && speed_pi_new, "post-reload fields present");

	Component* reloaded = nullptr;
	for (auto obj : eng->get_level()->get_all_objects()) {
		if (auto* c = obj ? obj->cast_to<Component>() : nullptr) {
			if (&c->get_type() == ti) {
				reloaded = c;
				break;
			}
		}
	}
	t.require(reloaded != nullptr, "found restored TestLuaComp after load");

	t.check(hp_pi_new->get_float(reloaded) == 555.f, "hp survived save + class-drop + load");
	t.check(*(std::string*)display_pi_new->get_ptr(reloaded) == "kept",
			"display survived save + class-drop + load");
	t.check(speed_pi_new->get_float(reloaded) == 7.f,
			"speed got new template default, not garbage from dropped slot");

	FileSys::delete_game_file(kPath);
}
EDITOR_TEST("editor/lua_component_save_then_reload_class_drops_field", 30.f,
			test_save_then_reload_class_drops_field);

// 8. Two distinct Lua classes on the same entity must own disjoint PropertyInfoList
//    storage and shadow buffers; writing one must never perturb the other, and a
//    hot-reload of class A must not touch class B's instance.
constexpr const char* kLuaSrcSecond =
	"---@class TestLuaCompB : Component\n"
	"TestLuaCompB = {\n"
	"  ---@type number\n"
	"  power = 11,\n"
	"  ---@type string\n"
	"  tag = \"\",\n"
	"}\n"
	"function TestLuaCompB:start() end\n";

static TestTask test_two_classes_one_entity_no_alias(TestContext& t) {
	load_initial_lua_class();
	ScriptManager::inst->reload_from_content(kLuaSrcSecond, "test_lua_component_b");
	ScriptManager::inst->check_for_reload();

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);

	auto* ti_a = ClassBase::find_class("TestLuaComp");
	auto* ti_b = ClassBase::find_class("TestLuaCompB");
	t.require(ti_a != nullptr && ti_b != nullptr, "both Lua classes registered");
	t.require(ti_a->props && ti_b->props, "both classes have props");
	t.check(ti_a->props != ti_b->props, "PropertyInfoList pointers are distinct");
	t.check(ti_a->props->list != ti_b->props->list, "PropertyInfo storage is distinct");

	Entity* e = spawn_entity_in_editor();
	Component* comp_a = attach_test_lua_component(e, ti_a);
	Component* comp_b = attach_test_lua_component(e, ti_b);
	t.require(comp_a != nullptr && comp_b != nullptr, "both components attached");
	t.check(comp_a->get_lua_field_shadow() == nullptr, "A shadow null before first property access");
	t.check(comp_b->get_lua_field_shadow() == nullptr, "B shadow null before first property access");

	auto* a_hp = find_field_comp(ti_a, "hp");
	auto* a_display = find_field_comp(ti_a, "display");
	auto* b_power = find_field_comp(ti_b, "power");
	auto* b_tag = find_field_comp(ti_b, "tag");
	t.require(a_hp && a_display && b_power && b_tag, "fields resolve on the right class");

	// A's fields must not appear on B and vice versa.
	t.check(find_field_comp(ti_a, "power") == nullptr, "A does not expose B's 'power'");
	t.check(find_field_comp(ti_b, "hp") == nullptr, "B does not expose A's 'hp'");

	a_hp->set_float(comp_a, 42.f);
	*(std::string*)a_display->get_ptr(comp_a) = "A";
	b_power->set_float(comp_b, 99.f);
	*(std::string*)b_tag->get_ptr(comp_b) = "B";

	t.require(comp_a->get_lua_field_shadow() != nullptr, "A shadow created on first property access");
	t.require(comp_b->get_lua_field_shadow() != nullptr, "B shadow created on first property access");
	t.check(comp_a->get_lua_field_shadow() != comp_b->get_lua_field_shadow(),
			"A and B own disjoint shadow buffers");

	t.check(a_hp->get_float(comp_a) == 42.f, "A.hp reads back its own write");
	t.check(*(std::string*)a_display->get_ptr(comp_a) == "A", "A.display reads back");
	t.check(b_power->get_float(comp_b) == 99.f, "B.power reads back its own write");
	t.check(*(std::string*)b_tag->get_ptr(comp_b) == "B", "B.tag reads back");

	void* b_shadow_pre = comp_b->get_lua_field_shadow();

	// Reload only A. B's per-class had_changes flag must stay clear so check_for_reload
	// leaves B's metadata and live instance untouched.
	ScriptManager::inst->reload_from_content(kLuaSrcReloaded, "test_lua_component_reloaded");
	ScriptManager::inst->check_for_reload();
	co_await t.wait_ticks(1);

	t.check(comp_b->get_lua_field_shadow() == b_shadow_pre,
			"B's shadow pointer untouched by A's reload");
	t.check(b_power->get_float(comp_b) == 99.f, "B.power preserved across A's reload");
	t.check(*(std::string*)b_tag->get_ptr(comp_b) == "B", "B.tag preserved across A's reload");

	// A migrated correctly (sanity).
	auto* a_hp_new = find_field_comp(ti_a, "hp");
	auto* a_display_new = find_field_comp(ti_a, "display");
	t.require(a_hp_new && a_display_new, "A's overlapping fields still reflected");
	t.check(a_hp_new->get_float(comp_a) == 42.f, "A.hp preserved across its own reload");
	t.check(*(std::string*)a_display_new->get_ptr(comp_a) == "A",
			"A.display preserved across its own reload");
}
EDITOR_TEST("editor/lua_component_two_classes_one_entity_no_alias", 30.f,
			test_two_classes_one_entity_no_alias);

// 9. Lazy shadow allocation: freshly allocated components have null shadow; first
//    PROP_LUA_BACKED get_ptr call creates it with template defaults; components that
//    are never accessed via get_ptr keep null shadow indefinitely.
static TestTask test_lazy_shadow_allocation(TestContext& t) {
	load_initial_lua_class();

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor");
	co_await t.wait_ticks(4);

	auto* ti = find_test_lua_comp();
	t.require(ti != nullptr, "TestLuaComp registered");

	auto* hp_pi = find_field_comp(ti, "hp");
	auto* alive_pi = find_field_comp(ti, "alive");
	t.require(hp_pi && alive_pi, "fields present");

	Entity* e1 = spawn_entity_in_editor();
	Component* comp = attach_test_lua_component(e1, ti);
	t.require(comp != nullptr, "TestLuaComp attached");

	// No property has been touched — shadow must not exist yet.
	t.require(comp->get_lua_field_shadow() == nullptr,
			"shadow null before any property access");

	// First get_ptr triggers lazy creation; value must match template default.
	float hp = hp_pi->get_float(comp);
	t.check(comp->get_lua_field_shadow() != nullptr,
			"shadow created on first get_ptr call");
	t.check(hp == 50.f, "lazily-created shadow seeded with template default (hp=50)");
	t.check(alive_pi->get_int(comp) == 1,
			"lazily-created shadow seeded with template default (alive=true)");

	// A second component that is never accessed via get_ptr keeps null shadow.
	Entity* e2 = spawn_entity_in_editor();
	Component* comp2 = attach_test_lua_component(e2, ti);
	t.require(comp2 != nullptr, "second TestLuaComp attached");
	t.check(comp2->get_lua_field_shadow() == nullptr,
			"component never touched via get_ptr has no shadow");
}
EDITOR_TEST("editor/lua_component_lazy_shadow_allocation", 30.f, test_lazy_shadow_allocation);
