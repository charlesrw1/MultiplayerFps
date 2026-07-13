// Source/IntegrationTests/Tests/Editor/test_ragdoll_joint_gizmo.cpp
// Repro for a crash reported when placing a RagdollJointComponent and selecting it in the
// editor: the gizmo's dynamic Models were getting freed while a render proxy still referenced
// them (see RagdollJointComponent::stop()/rebuild_gizmo_mesh() ordering).
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/EditorTestContext.h"
#include "GameEnginePublic.h"
#include "LevelEditor/EditorDocLocal.h"
#include "LevelEditor/IEditorApi.h"
#include "LevelEditor/Commands.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/RagdollJointComponent.h"
#include <string>

static TestTask test_ragdoll_joint_select_no_crash(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	// Mimic the real authoring shape: a rig entity with a MeshComponent, and a scaffolding
	// child (the joint's own bone owner) carrying the RagdollJointComponent.
	Entity* rig = editor->spawn_entity();
	rig->set_editor_name("RJ_Rig");
	rig->create_component<MeshComponent>();

	Entity* scaffold = editor->spawn_entity();
	scaffold->set_editor_name("RJ_Scaffold");
	scaffold->parent_to(rig);
	auto* joint = scaffold->create_component<RagdollJointComponent>();
	t.require(joint != nullptr, "RagdollJointComponent created");

	co_await t.wait_ticks(2);

	ISelectionApi* sel_api = editor->get_editor_api().selection();
	sel_api->clear_selected();
	sel_api->add_select(scaffold->get_self_ptr());

	// Several ticks: editor_on_draw_gizmos_selected() / on_inspector_imgui() / property-grid
	// draw all run every frame while selected -- this is where the reported crash happened.
	co_await t.wait_ticks(10);
	t.check(true, "survived selecting a freshly-placed RagdollJointComponent");

	// Flip a DOF closed (2-DOF cone -> locked), the has_swing/has_twist "true->false" gizmo
	// transition that frees one of the dynamic models mid-edit.
	joint->ang_y_motion = JM::Locked;
	joint->ang_z_motion = JM::Locked;
	joint->editor_on_change_property();
	co_await t.wait_ticks(5);
	t.check(true, "survived closing a DOF while selected");

	joint->ang_x_motion = JM::Locked;
	joint->editor_on_change_property();
	co_await t.wait_ticks(5);
	t.check(true, "survived closing the last DOF (twist) while selected");

	// Deselect, then destroy the component's owner while it was the active selection.
	sel_api->clear_selected();
	co_await t.wait_ticks(2);
	scaffold->destroy();
	co_await t.wait_ticks(5);
	t.check(true, "survived destroying the scaffolding entity");

	co_return;
}

EDITOR_TEST("ragdoll/joint_component_select_no_crash", 30.f, test_ragdoll_joint_select_no_crash);

// Closer to a real ragdoll rig: several joint components (mimicking hand-authoring a full
// skeleton), created via EditorDoc::attach_component (same call CreateComponentCommand::execute
// uses), clicked through one at a time (select A, then select B without deselecting first, then
// select C), and finally a save+reload of the level.
static TestTask test_ragdoll_joint_multi_select_and_reload(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Entity* rig = editor->spawn_entity();
	rig->set_editor_name("RJM_Rig");
	rig->create_component<MeshComponent>();

	const char* names[] = {"RJM_Spine", "RJM_ArmR", "RJM_ArmL", "RJM_LegR", "RJM_LegL"};
	Entity* scaffolds[5] = {};
	for (int i = 0; i < 5; i++) {
		Entity* e = editor->spawn_entity();
		e->set_editor_name(names[i]);
		e->parent_to(rig);
		Component* c = editor->attach_component(&RagdollJointComponent::StaticType, e);
		t.require(c != nullptr, "joint component attached");
		scaffolds[i] = e;
		co_await t.wait_ticks(2);
	}

	ISelectionApi* sel_api = editor->get_editor_api().selection();
	for (int i = 0; i < 5; i++) {
		sel_api->clear_selected();
		sel_api->add_select(scaffolds[i]->get_self_ptr());
		co_await t.wait_ticks(3);
	}
	// Click straight from one selection to another without an intervening clear_selected(),
	// matching a normal click-to-click editor interaction.
	for (int i = 4; i >= 0; i--) {
		sel_api->add_select(scaffolds[i]->get_self_ptr());
		co_await t.wait_ticks(3);
	}
	t.check(true, "survived cycling selection across multiple joint components");

	const char* save_path = "TestFiles/ragdoll_joint_multi_select.tmap";
	t.editor().save_level(save_path);
	co_await t.wait_ticks(2);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, (std::string("open-editor ") + save_path).c_str());
	co_await t.wait_ticks(6);
	t.check(true, "survived save+reload with multiple joint components in the level");

	co_return;
}

EDITOR_TEST("ragdoll/joint_component_multi_select_and_reload", 40.f,
			test_ragdoll_joint_multi_select_and_reload);

// Realistic click order: select the (bare) entity FIRST, exactly like a user selecting it in the
// outliner, THEN add the component to the already-selected entity via attach_component (same
// call the real "Add Component" UI uses) with NO wait_ticks in between -- so on_inspector_imgui()
// / editor_on_draw_gizmos_selected() for the just-created component can run on the very first
// tick after creation, before this test does anything else.
static TestTask test_ragdoll_joint_select_then_add(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Entity* rig = editor->spawn_entity();
	rig->set_editor_name("RJS_Rig");
	rig->create_component<MeshComponent>();

	Entity* scaffold = editor->spawn_entity();
	scaffold->set_editor_name("RJS_Scaffold");
	scaffold->parent_to(rig);
	co_await t.wait_ticks(1);

	ISelectionApi* sel_api = editor->get_editor_api().selection();
	sel_api->clear_selected();
	sel_api->add_select(scaffold->get_self_ptr());
	// select, THEN immediately add the component to the already-selected entity, no wait between
	Component* c = editor->attach_component(&RagdollJointComponent::StaticType, scaffold);
	t.require(c != nullptr, "joint component attached to a pre-selected entity");

	co_await t.wait_ticks(15);
	t.check(true, "survived selecting-then-adding a RagdollJointComponent");

	co_return;
}

EDITOR_TEST("ragdoll/joint_component_select_then_add", 30.f, test_ragdoll_joint_select_then_add);

// Simulates rapidly dragging a DOF slider through Locked<->Limited transitions many times within
// a single tick (ImGui::DragFloat/enum combos fire on_inspector_imgui -> editor_on_change_property
// on every value change, not just release) -- the has_swing/has_twist "true<->false" gizmo
// rebuild path, hit repeatedly and rapidly rather than the single clean transition the earlier
// tests exercise.
static TestTask test_ragdoll_joint_rapid_dof_toggle(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Entity* rig = editor->spawn_entity();
	rig->set_editor_name("RJT_Rig");
	rig->create_component<MeshComponent>();

	Entity* scaffold = editor->spawn_entity();
	scaffold->set_editor_name("RJT_Scaffold");
	scaffold->parent_to(rig);
	auto* joint = scaffold->create_component<RagdollJointComponent>();
	t.require(joint != nullptr, "RagdollJointComponent created");

	ISelectionApi* sel_api = editor->get_editor_api().selection();
	sel_api->clear_selected();
	sel_api->add_select(scaffold->get_self_ptr());
	co_await t.wait_ticks(2);

	// All within a tight loop, no wait_ticks between iterations -- worst-case rapid toggling,
	// touching swing (2-DOF <-> 1-DOF <-> closed) and twist independently and together.
	for (int i = 0; i < 60; i++) {
		joint->ang_y_motion = (i % 2 == 0) ? JM::Locked : JM::Limited;
		joint->ang_z_motion = (i % 3 == 0) ? JM::Locked : JM::Limited;
		joint->ang_x_motion = (i % 4 == 0) ? JM::Locked : JM::Limited;
		joint->editor_on_change_property();
		if (i % 10 == 0)
			co_await t.wait_ticks(1); // occasionally let a frame draw mid-storm
	}
	co_await t.wait_ticks(10);
	t.check(true, "survived rapid repeated DOF toggling while selected");

	co_return;
}

EDITOR_TEST("ragdoll/joint_component_rapid_dof_toggle", 30.f, test_ragdoll_joint_rapid_dof_toggle);
