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
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Game/Components/RagdollJointComponent.h"
#include "Game/Components/RagdollPhysicsBodyComponent.h"
#include "Game/Components/RagdollSetupComponent.h"
#include "Animation/SkeletonData.h"
#include "Render/Model.h"
#include "Framework/Files.h"
#include "Game/Prefab.h"
#include <string>
#include <vector>

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

// Repro for "preview ragdoll bodies aren't jointed to each other" -- exercises
// RagdollSetupComponent::preview_ragdoll() end-to-end against a real skeletal model and checks
// that the spawned child body actually got an AdvancedJointComponent targeting the spawned parent
// body (not just that both bodies exist).
static TestTask test_ragdoll_setup_preview_wires_joint(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Model* m = Model::load("characters/testcharacter/testcharacter.cmdl");
	t.require(m != nullptr && m->get_skel() != nullptr, "test model has a skeleton");
	MSkeleton* skel = m->get_skel();

	// Find any bone with a parent (skip the root) so we have a guaranteed real parent/child pair,
	// independent of this model's exact bone-naming convention.
	const auto& bones = skel->get_all_bones();
	int child_idx = -1, parent_idx = -1;
	for (int i = 0; i < (int)bones.size(); i++) {
		int p = skel->get_bone_parent(i);
		if (p >= 0) {
			child_idx = i;
			parent_idx = p;
			break;
		}
	}
	t.require(child_idx != -1, "model has at least one parent/child bone pair");
	StringName parent_bone_name = bones[parent_idx].name;
	StringName child_bone_name = bones[child_idx].name;

	Entity* setup_ent = editor->spawn_entity();
	setup_ent->set_editor_name("RSW_Setup");
	auto* setup = setup_ent->create_component<RagdollSetupComponent>();
	setup->model = m;
	setup->ensure_rig_mesh();
	co_await t.wait_ticks(1);

	Entity* parent_scaffold = editor->spawn_entity();
	parent_scaffold->set_editor_name("RSW_ParentScaffold");
	parent_scaffold->parent_to(setup_ent);
	parent_scaffold->set_parent_bone(parent_bone_name);
	parent_scaffold->create_component<RagdollPhysicsBodyComponent>();
	// no RagdollJointComponent -- this is the ragdoll root

	Entity* child_scaffold = editor->spawn_entity();
	child_scaffold->set_editor_name("RSW_ChildScaffold");
	child_scaffold->parent_to(setup_ent);
	child_scaffold->set_parent_bone(child_bone_name);
	child_scaffold->create_component<RagdollPhysicsBodyComponent>();
	child_scaffold->create_component<RagdollJointComponent>();
	co_await t.wait_ticks(1);

	setup->preview_ragdoll();
	co_await t.wait_ticks(2);
	t.require(setup->is_previewing(), "preview_ragdoll actually started a preview");

	// The spawned bodies are free/unparented entities -- find them by CapsuleComponent + which
	// bone they ended up bone-parented to isn't set (spawn_body doesn't bone-parent the free
	// body), so instead find them via bone-parent recorded on RagdollComponent by scanning all
	// CapsuleComponents in the level for ones with a matching AdvancedJointComponent target.
	CapsuleComponent* child_capsule = nullptr;
	CapsuleComponent* parent_capsule = nullptr;
	int capsule_count = 0;
	for (auto o : eng->get_level()->get_all_objects()) {
		Entity* e = o->cast_to<Entity>();
		if (!e)
			continue;
		auto* cap = e->get_component<CapsuleComponent>();
		if (!cap)
			continue;
		capsule_count++;
		if (e->get_component<AdvancedJointComponent>())
			child_capsule = cap;
		else
			parent_capsule = cap;
	}
	t.check(capsule_count == 2, "exactly two capsule bodies were spawned (no accidental mirroring)");
	t.require(child_capsule != nullptr, "the child body has an AdvancedJointComponent");
	t.require(parent_capsule != nullptr, "the parent (root) body exists with no joint of its own");

	auto* joint = child_capsule->get_owner()->get_component<AdvancedJointComponent>();
	t.require(joint != nullptr, "child body's AdvancedJointComponent exists");
	t.check(joint->get_target() == parent_capsule->get_owner(),
			"child joint's target is the parent body's entity");
	t.check(joint->get_joint() != nullptr, "the underlying PhysX PxD6Joint was actually created");

	// Physically verify the constraint: shove the child body away from the parent and confirm the
	// joint's translation lock pulls it back close to the parent instead of letting it drift
	// away like an unconstrained body would.
	glm::vec3 parent_pos_before = parent_capsule->get_owner()->get_ws_position();
	glm::vec3 child_pos_before = child_capsule->get_owner()->get_ws_position();
	float initial_separation = glm::length(child_pos_before - parent_pos_before);
	child_capsule->apply_impulse(child_pos_before, glm::vec3(0.f, 0.f, 50.f));
	co_await t.wait_seconds(0.5);
	glm::vec3 parent_pos_after = parent_capsule->get_owner()->get_ws_position();
	glm::vec3 child_pos_after = child_capsule->get_owner()->get_ws_position();
	float final_separation = glm::length(child_pos_after - parent_pos_after);
	t.check(fabsf(final_separation - initial_separation) < 0.2f,
			"child stayed close to parent after an impulse (joint constraint held)");

	co_return;
}

EDITOR_TEST("ragdoll/setup_preview_wires_joint", 30.f, test_ragdoll_setup_preview_wires_joint);

// Actual root cause of the "found N roots" report: the user authors RagdollJointComponent on a
// SEPARATE sibling entity from RagdollPhysicsBodyComponent (both bone-parented to the same bone),
// not on the same entity -- Pass A used to look up the joint via
// child->get_component<RagdollJointComponent>() on the body's own entity only, so it always came
// back null for this (valid, intended) authoring pattern.
static TestTask test_ragdoll_setup_preview_joint_on_separate_entity(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Model* m = Model::load("characters/testcharacter/testcharacter.cmdl");
	t.require(m != nullptr && m->get_skel() != nullptr, "test model has a skeleton");
	MSkeleton* skel = m->get_skel();
	const auto& bones = skel->get_all_bones();
	int child_idx = -1, parent_idx = -1;
	for (int i = 0; i < (int)bones.size(); i++) {
		int p = skel->get_bone_parent(i);
		if (p >= 0) {
			child_idx = i;
			parent_idx = p;
			break;
		}
	}
	t.require(child_idx != -1, "model has at least one parent/child bone pair");
	StringName parent_bone_name = bones[parent_idx].name;
	StringName child_bone_name = bones[child_idx].name;

	Entity* setup_ent = editor->spawn_entity();
	setup_ent->set_editor_name("RSWS_Setup");
	auto* setup = setup_ent->create_component<RagdollSetupComponent>();
	setup->model = m;
	setup->ensure_rig_mesh();
	co_await t.wait_ticks(1);

	Entity* parent_body_scaffold = editor->spawn_entity();
	parent_body_scaffold->set_editor_name("RSWS_ParentBody");
	parent_body_scaffold->parent_to(setup_ent);
	parent_body_scaffold->set_parent_bone(parent_bone_name);
	parent_body_scaffold->create_component<RagdollPhysicsBodyComponent>();
	// no joint anywhere for the parent bone -- it's the root

	Entity* child_body_scaffold = editor->spawn_entity();
	child_body_scaffold->set_editor_name("RSWS_ChildBody");
	child_body_scaffold->parent_to(setup_ent);
	child_body_scaffold->set_parent_bone(child_bone_name);
	child_body_scaffold->create_component<RagdollPhysicsBodyComponent>();

	// The joint lives on a THIRD, separate sibling entity, bone-parented to the SAME bone as
	// child_body_scaffold, but is not itself the body's entity.
	Entity* child_joint_scaffold = editor->spawn_entity();
	child_joint_scaffold->set_editor_name("RSWS_ChildJoint");
	child_joint_scaffold->parent_to(setup_ent);
	child_joint_scaffold->set_parent_bone(child_bone_name);
	child_joint_scaffold->create_component<RagdollJointComponent>();
	co_await t.wait_ticks(1);

	setup->preview_ragdoll();
	co_await t.wait_ticks(2);
	t.require(setup->is_previewing(), "preview_ragdoll started");

	CapsuleComponent* child_capsule = nullptr;
	CapsuleComponent* parent_capsule = nullptr;
	int capsule_count = 0;
	for (auto o : eng->get_level()->get_all_objects()) {
		Entity* e = o->cast_to<Entity>();
		auto* cap = e ? e->get_component<CapsuleComponent>() : nullptr;
		if (!cap)
			continue;
		capsule_count++;
		if (e->get_component<AdvancedJointComponent>())
			child_capsule = cap;
		else
			parent_capsule = cap;
	}
	t.check(capsule_count == 2, "exactly two capsule bodies were spawned");
	t.require(child_capsule != nullptr,
			  "the child body resolved its joint even though it was authored on a separate entity");
	t.require(parent_capsule != nullptr, "the parent (root) body exists with no joint");
	t.check(child_capsule->get_owner()->get_component<AdvancedJointComponent>()->get_target() ==
				parent_capsule->get_owner(),
			"child joint's target is the parent body's entity");

	co_return;
}

EDITOR_TEST("ragdoll/setup_preview_joint_on_separate_entity", 30.f,
			test_ragdoll_setup_preview_joint_on_separate_entity);

// User report: a knee-style hinge (twist Locked, one swing Locked, the other swing Limited)
// still visibly twists and swings laterally in PhysX -- i.e. the "Locked" axes aren't actually
// rigid. Physically test each of the 3 rotational axes independently: spin the child body with a
// strong angular velocity around each of its local X/Y/Z axes in turn and measure how much the
// child-to-parent relative orientation actually changed after a short simulation window. Locked
// axes should barely move; the one Limited axis should move a lot.
static TestTask test_ragdoll_setup_locked_axes_are_rigid(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Model* m = Model::load("characters/testcharacter/testcharacter.cmdl");
	t.require(m != nullptr && m->get_skel() != nullptr, "test model has a skeleton");
	MSkeleton* skel = m->get_skel();
	const auto& bones = skel->get_all_bones();
	int child_idx = -1, parent_idx = -1;
	for (int i = 0; i < (int)bones.size(); i++) {
		int p = skel->get_bone_parent(i);
		if (p >= 0) {
			child_idx = i;
			parent_idx = p;
			break;
		}
	}
	t.require(child_idx != -1, "model has at least one parent/child bone pair");
	StringName parent_bone_name = bones[parent_idx].name;
	StringName child_bone_name = bones[child_idx].name;

	Entity* setup_ent = editor->spawn_entity();
	setup_ent->set_editor_name("RSWL_Setup");
	auto* setup = setup_ent->create_component<RagdollSetupComponent>();
	setup->model = m;
	setup->ensure_rig_mesh();
	co_await t.wait_ticks(1);

	Entity* parent_body = editor->spawn_entity();
	parent_body->set_editor_name("RSWL_ParentBody");
	parent_body->parent_to(setup_ent);
	parent_body->set_parent_bone(parent_bone_name);
	parent_body->create_component<RagdollPhysicsBodyComponent>();

	Entity* child_body = editor->spawn_entity();
	child_body->set_editor_name("RSWL_ChildBody");
	child_body->parent_to(setup_ent);
	child_body->set_parent_bone(child_bone_name);
	child_body->create_component<RagdollPhysicsBodyComponent>();

	Entity* child_joint = editor->spawn_entity();
	child_joint->set_editor_name("RSWL_ChildJoint");
	child_joint->parent_to(setup_ent);
	child_joint->set_parent_bone(child_bone_name);
	auto* joint_scaffold = child_joint->create_component<RagdollJointComponent>();
	// Knee-style hinge: twist and swing1 locked, only swing2 open.
	joint_scaffold->ang_x_motion = JM::Locked;
	joint_scaffold->ang_y_motion = JM::Locked;
	joint_scaffold->ang_z_motion = JM::Limited;
	joint_scaffold->swing2_limit = 0.8f;
	co_await t.wait_ticks(1);

	// For each test, torque is applied nominally "along" one local axis, but a working hinge
	// naturally REDIRECTS any torque into rotation around its one free (open) axis -- like
	// pushing a door off-hinge-axis still swings it around the hinge pin. So total rotation ANGLE
	// alone can't tell "moved because the locked axis leaked" apart from "moved because the
	// torque got correctly redirected into the free axis". Decompose the delta rotation's AXIS
	// (expressed in the child's own pre-torque local frame, since anchor is identity here) and
	// check its alignment with each of local X/Y/Z to see which physical axis actually rotated.
	float axis_deg[3] = {0, 0, 0};
	glm::vec3 axis_alignment[3] = {glm::vec3(0), glm::vec3(0), glm::vec3(0)}; // |dot| with local X/Y/Z
	glm::vec3 local_axes[3] = {glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)};
	for (int axis_i = 0; axis_i < 3; axis_i++) {
		setup->preview_ragdoll();
		co_await t.wait_ticks(2);

		CapsuleComponent* child_capsule = nullptr;
		CapsuleComponent* parent_capsule = nullptr;
		for (auto o : eng->get_level()->get_all_objects()) {
			Entity* e = o->cast_to<Entity>();
			auto* cap = e ? e->get_component<CapsuleComponent>() : nullptr;
			if (!cap)
				continue;
			if (e->get_component<AdvancedJointComponent>())
				child_capsule = cap;
			else
				parent_capsule = cap;
		}
		t.require(child_capsule != nullptr && parent_capsule != nullptr, "bodies exist");

		glm::quat rel_before = glm::inverse(parent_capsule->get_owner()->get_ws_rotation()) *
								child_capsule->get_owner()->get_ws_rotation();

		glm::vec3 world_axis = child_capsule->get_owner()->get_ws_rotation() * local_axes[axis_i];
		child_capsule->set_angular_velocity(world_axis * 15.f);
		co_await t.wait_seconds(0.3);

		glm::quat rel_after = glm::inverse(parent_capsule->get_owner()->get_ws_rotation()) *
							   child_capsule->get_owner()->get_ws_rotation();
		glm::quat delta = glm::inverse(rel_before) * rel_after;
		axis_deg[axis_i] = glm::degrees(glm::angle(delta));
		glm::vec3 delta_axis = glm::axis(delta);
		axis_alignment[axis_i] = glm::vec3(fabsf(glm::dot(delta_axis, glm::vec3(1, 0, 0))),
											fabsf(glm::dot(delta_axis, glm::vec3(0, 1, 0))),
											fabsf(glm::dot(delta_axis, glm::vec3(0, 0, 1))));
	}

	t.check(axis_deg[0] < 10.f, ("locked twist axis stayed rigid under torque (moved " +
								 std::to_string(axis_deg[0]) + " degrees)")
									.c_str());
	// A working hinge REDIRECTS torque applied off its free axis into rotation around the free
	// axis (like pushing a door off-hinge-axis still swings it around the hinge pin) -- so total
	// angle alone can't distinguish "the locked axis leaked" from "the torque got correctly
	// redirected into the one open axis". Check the actual rotation AXIS instead: for the
	// Y-torque test (swing1 locked, swing2 open), Y itself should barely be present in the
	// resulting rotation axis, even though the total angle can be large (redirected into Z).
	t.check(axis_alignment[1].y < 0.3f,
			("locked swing1 (Y) barely present in the resulting rotation axis -- true lock, not a "
			 "leak (alignment x=" +
			 std::to_string(axis_alignment[1].x) + " y=" + std::to_string(axis_alignment[1].y) + " z=" +
			 std::to_string(axis_alignment[1].z) + ", total angle=" + std::to_string(axis_deg[1]) + " -- "
			 "redirected into the open swing2/Z axis, which is correct hinge behavior)")
				.c_str());
	t.check(axis_deg[2] > 15.f, ("limited swing2 axis actually moved under torque (moved " +
								 std::to_string(axis_deg[2]) + " degrees)")
									.c_str());

	co_return;
}

EDITOR_TEST("ragdoll/setup_locked_axes_are_rigid", 30.f, test_ragdoll_setup_locked_axes_are_rigid);

// User report: a joint authored on its own scaffold entity (rotated relative to the body, e.g. to
// aim its wedge gizmo along a limb whose body capsule needed a different correction rotation) comes
// out physically hinging on a diagonal axis in PhysX instead of the axis the gizmo actually shows.
// Same physical torque-probe technique as test_ragdoll_setup_locked_axes_are_rigid, but this time
// the joint scaffold is rotated 90 degrees about world Y relative to the (identity-rotation) body,
// so the joint's local Z (its one open swing axis) should manifest as WORLD +X, not local/world Z.
static TestTask test_ragdoll_setup_joint_rotation_offset_from_body(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Model* m = Model::load("characters/testcharacter/testcharacter.cmdl");
	t.require(m != nullptr && m->get_skel() != nullptr, "test model has a skeleton");
	MSkeleton* skel = m->get_skel();
	const auto& bones = skel->get_all_bones();
	// Deepest bone (longest ancestor chain), not just the first parent/child pair -- the user's
	// real repro is on calf_r/thigh_r, several bones deep from the skeleton root.
	int child_idx = -1, deepest_depth = -1;
	for (int i = 0; i < (int)bones.size(); i++) {
		int depth = 0;
		for (int p = skel->get_bone_parent(i); p >= 0; p = skel->get_bone_parent(p))
			depth++;
		if (depth > deepest_depth) {
			deepest_depth = depth;
			child_idx = i;
		}
	}
	t.require(child_idx != -1 && deepest_depth >= 1, "model has at least one parent/child bone pair");
	int parent_idx = skel->get_bone_parent(child_idx);
	StringName parent_bone_name = bones[parent_idx].name;
	StringName child_bone_name = bones[child_idx].name;

	Entity* setup_ent = editor->spawn_entity();
	setup_ent->set_editor_name("RSWO_Setup");
	auto* setup = setup_ent->create_component<RagdollSetupComponent>();
	setup->model = m;
	setup->ensure_rig_mesh();
	co_await t.wait_ticks(1);

	Entity* parent_body = editor->spawn_entity();
	parent_body->set_editor_name("RSWO_ParentBody");
	parent_body->parent_to(setup_ent);
	parent_body->set_parent_bone(parent_bone_name);
	parent_body->create_component<RagdollPhysicsBodyComponent>();

	Entity* child_body = editor->spawn_entity();
	child_body->set_editor_name("RSWO_ChildBody");
	child_body->parent_to(setup_ent);
	child_body->set_parent_bone(child_bone_name);
	child_body->create_component<RagdollPhysicsBodyComponent>();
	// Body kept at identity rotation -- the joint scaffold below is authored with its own,
	// different rotation, exactly the "separate sibling entity with its own orientation" pattern
	// called out in RagdollSetupComponent.cpp's Pass C comment.

	Entity* child_joint = editor->spawn_entity();
	child_joint->set_editor_name("RSWO_ChildJoint");
	child_joint->parent_to(setup_ent);
	child_joint->set_parent_bone(child_bone_name);
	child_joint->set_ls_rotation(glm::angleAxis(HALFPI, glm::vec3(0, 1, 0)));
	auto* joint_scaffold = child_joint->create_component<RagdollJointComponent>();
	// Twist and swing1 locked, only swing2 (the joint's own local Z) open.
	joint_scaffold->ang_x_motion = JM::Locked;
	joint_scaffold->ang_y_motion = JM::Locked;
	joint_scaffold->ang_z_motion = JM::Limited;
	joint_scaffold->swing2_limit = 0.8f;
	co_await t.wait_ticks(1);

	setup->preview_ragdoll();
	co_await t.wait_ticks(2);
	t.require(setup->is_previewing(), "preview_ragdoll actually started a preview");

	CapsuleComponent* child_capsule = nullptr;
	CapsuleComponent* parent_capsule = nullptr;
	for (auto o : eng->get_level()->get_all_objects()) {
		Entity* e = o->cast_to<Entity>();
		auto* cap = e ? e->get_component<CapsuleComponent>() : nullptr;
		if (!cap)
			continue;
		if (e->get_component<AdvancedJointComponent>())
			child_capsule = cap;
		else
			parent_capsule = cap;
	}
	t.require(child_capsule != nullptr && parent_capsule != nullptr, "bodies exist");

	// The joint scaffold is rotated 90 degrees about Y relative to the (identity) body, so its local
	// Z axis (the one open swing DOF) points along world +X: R_y(90) * (0,0,1) = (1,0,0).
	glm::vec3 expected_open_axis(1, 0, 0);
	glm::vec3 expected_locked_axis(0, 0, 1); // the joint's local X (twist) is still world Z

	glm::quat rel_before_open = glm::inverse(parent_capsule->get_owner()->get_ws_rotation()) *
								 child_capsule->get_owner()->get_ws_rotation();
	child_capsule->set_angular_velocity(expected_open_axis * 15.f);
	co_await t.wait_seconds(0.3);
	glm::quat rel_after_open = glm::inverse(parent_capsule->get_owner()->get_ws_rotation()) *
							   child_capsule->get_owner()->get_ws_rotation();
	glm::quat delta_open = glm::inverse(rel_before_open) * rel_after_open;
	float open_deg = glm::degrees(glm::angle(delta_open));
	t.check(open_deg > 15.f,
			("torque about the joint's rotated open axis (world +X) actually moved the joint (moved " +
			 std::to_string(open_deg) + " degrees) -- if this fails, the joint frame ignored the scaffold's "
			 "own rotation and is still hinging on an un-rotated axis")
				.c_str());

	// Reset and probe the (still-locked) twist axis, which stays at world Z regardless of the
	// scaffold rotation (rotating 90 about Y maps local X -> world Z: R_y(90) * (1,0,0) = (0,0,-1),
	// still axis-aligned with Z).
	setup->preview_ragdoll();
	co_await t.wait_ticks(2);
	child_capsule = nullptr;
	parent_capsule = nullptr;
	for (auto o : eng->get_level()->get_all_objects()) {
		Entity* e = o->cast_to<Entity>();
		auto* cap = e ? e->get_component<CapsuleComponent>() : nullptr;
		if (!cap)
			continue;
		if (e->get_component<AdvancedJointComponent>())
			child_capsule = cap;
		else
			parent_capsule = cap;
	}
	t.require(child_capsule != nullptr && parent_capsule != nullptr, "bodies exist after restart");
	glm::quat rel_before_locked = glm::inverse(parent_capsule->get_owner()->get_ws_rotation()) *
								   child_capsule->get_owner()->get_ws_rotation();
	child_capsule->set_angular_velocity(expected_locked_axis * 15.f);
	co_await t.wait_seconds(0.3);
	glm::quat rel_after_locked = glm::inverse(parent_capsule->get_owner()->get_ws_rotation()) *
								 child_capsule->get_owner()->get_ws_rotation();
	glm::quat delta_locked = glm::inverse(rel_before_locked) * rel_after_locked;
	float locked_deg = glm::degrees(glm::angle(delta_locked));
	t.check(locked_deg < 10.f, ("torque about the joint's twist axis (world Z) stayed rigid (moved " +
								std::to_string(locked_deg) + " degrees)")
								   .c_str());

	co_return;
}

EDITOR_TEST("ragdoll/setup_joint_rotation_offset_from_body", 30.f,
			test_ragdoll_setup_joint_rotation_offset_from_body);

// User-reported repro: "RagdollSetupComponent: expected exactly one ragdoll root ... found 5" --
// a LONGER contiguous chain (more than one hop) reports far more roots than expected, meaning
// most joints fail to resolve their parent. The single-hop test above passes, so this tests
// whether it's specifically the multi-bone / multi-hop case that breaks.
static TestTask test_ragdoll_setup_preview_multibone_chain(TestContext& t) {
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Model* m = Model::load("characters/testcharacter/testcharacter.cmdl");
	t.require(m != nullptr && m->get_skel() != nullptr, "test model has a skeleton");
	MSkeleton* skel = m->get_skel();
	const auto& bones = skel->get_all_bones();

	// Find the deepest bone (longest ancestor chain), then take up to 5 bones walking up from it
	// (deepest first) to author a contiguous multi-hop chain.
	int deepest_idx = -1, deepest_depth = -1;
	for (int i = 0; i < (int)bones.size(); i++) {
		int depth = 0;
		for (int p = skel->get_bone_parent(i); p >= 0; p = skel->get_bone_parent(p))
			depth++;
		if (depth > deepest_depth) {
			deepest_depth = depth;
			deepest_idx = i;
		}
	}
	t.require(deepest_idx != -1 && deepest_depth >= 3, "model has a chain at least 4 bones deep");

	std::vector<int> chain; // deepest-first
	for (int i = deepest_idx; i >= 0 && (int)chain.size() < 5; i = skel->get_bone_parent(i))
		chain.push_back(i);
	t.require(chain.size() >= 4, "collected at least 4 bones for the chain");

	Entity* setup_ent = editor->spawn_entity();
	setup_ent->set_editor_name("RSWM_Setup");
	auto* setup = setup_ent->create_component<RagdollSetupComponent>();
	setup->model = m;
	setup->ensure_rig_mesh();
	co_await t.wait_ticks(1);

	// Author leaf -> root (chain[0] is the leaf/deepest, chain.back() is the root/topmost); every
	// bone except the root gets a RagdollJointComponent.
	for (size_t i = 0; i < chain.size(); i++) {
		Entity* scaffold = editor->spawn_entity();
		scaffold->set_editor_name(("RSWM_Bone_" + std::to_string(i)).c_str());
		scaffold->parent_to(setup_ent);
		scaffold->set_parent_bone(bones[chain[i]].name);
		scaffold->create_component<RagdollPhysicsBodyComponent>();
		if (i + 1 < chain.size()) // not the root
			scaffold->create_component<RagdollJointComponent>();
	}
	co_await t.wait_ticks(1);

	setup->preview_ragdoll();
	co_await t.wait_ticks(2);
	t.require(setup->is_previewing(), "preview_ragdoll actually started a preview");

	int capsule_count = 0, jointed_count = 0, root_count = 0;
	for (auto o : eng->get_level()->get_all_objects()) {
		Entity* e = o->cast_to<Entity>();
		if (!e || !e->get_component<CapsuleComponent>())
			continue;
		capsule_count++;
		auto* joint = e->get_component<AdvancedJointComponent>();
		if (joint && joint->get_target() != nullptr)
			jointed_count++;
		else
			root_count++;
	}
	t.check(capsule_count == (int)chain.size(), "one capsule body spawned per authored bone");
	t.check(root_count == 1, "exactly one body has no resolved joint (the true root)");
	t.check(jointed_count == (int)chain.size() - 1, "every non-root body resolved a joint to its parent");

	co_return;
}

EDITOR_TEST("ragdoll/setup_preview_multibone_chain", 30.f, test_ragdoll_setup_preview_multibone_chain);

// User's actual repro is inside a .tprefab (animman_physics.tprefab), not a plain .tmap level --
// the plain-level chain test above passes, so this specifically tests whether PREFAB EDIT MODE
// is the differentiator (e.g. bone-parenting or skeleton resolution behaving differently there).
static TestTask test_ragdoll_setup_preview_multibone_chain_in_prefab(TestContext& t) {
	const std::string prefab_path = "_ragdoll_setup_chain_test.tprefab";
	FileSys::delete_game_file(prefab_path.c_str());

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "open-editor eng/template.tmap");
	co_await t.wait_ticks(4);
	EditorDoc* editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr, "editor available");

	Model* m = Model::load("characters/testcharacter/testcharacter.cmdl");
	t.require(m != nullptr && m->get_skel() != nullptr, "test model has a skeleton");
	MSkeleton* skel = m->get_skel();
	const auto& bones = skel->get_all_bones();

	int deepest_idx = -1, deepest_depth = -1;
	for (int i = 0; i < (int)bones.size(); i++) {
		int depth = 0;
		for (int p = skel->get_bone_parent(i); p >= 0; p = skel->get_bone_parent(p))
			depth++;
		if (depth > deepest_depth) {
			deepest_depth = depth;
			deepest_idx = i;
		}
	}
	t.require(deepest_idx != -1 && deepest_depth >= 3, "model has a chain at least 4 bones deep");
	std::vector<int> chain;
	for (int i = deepest_idx; i >= 0 && (int)chain.size() < 5; i = skel->get_bone_parent(i))
		chain.push_back(i);
	t.require(chain.size() >= 4, "collected at least 4 bones for the chain");

	// Build the same rig as the plain-level test, but this time convert it into a real .tprefab
	// (matching test_prefabs.cpp's make-prefab-and-replace flow) and reopen it for editing there.
	Entity* setup_ent = editor->spawn_entity();
	setup_ent->set_editor_name("RSWP_Setup");
	auto* setup = setup_ent->create_component<RagdollSetupComponent>();
	setup->model = m;
	setup->ensure_rig_mesh();
	co_await t.wait_ticks(1);

	std::vector<Entity*> scaffolds;
	for (size_t i = 0; i < chain.size(); i++) {
		Entity* scaffold = editor->spawn_entity();
		scaffold->set_editor_name(("RSWP_Bone_" + std::to_string(i)).c_str());
		scaffold->parent_to(setup_ent);
		scaffold->set_parent_bone(bones[chain[i]].name);
		scaffold->create_component<RagdollPhysicsBodyComponent>();
		if (i + 1 < chain.size())
			scaffold->create_component<RagdollJointComponent>();
		scaffolds.push_back(scaffold);
	}
	co_await t.wait_ticks(1);

	// MakePrefabAndReplaceCommand serializes exactly the given selection -- it does NOT recurse
	// into children on its own (confirmed: the real "Make Prefab Using..." menu item just passes
	// selection_state->get_selection_as_vector() straight through, EditorDocViewport.cpp:787-792).
	// So the full hierarchy must be explicitly selected, matching a real user selecting every node
	// (or a recursive-select-children outliner action) before making the prefab.
	ISelectionApi* sel_api = editor->get_editor_api().selection();
	sel_api->clear_selected();
	sel_api->add_select(setup_ent->get_self_ptr());
	for (Entity* s : scaffolds)
		sel_api->add_select(s->get_self_ptr());
	co_await t.wait_ticks(1);

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, ("make-prefab-and-replace " + prefab_path).c_str());
	co_await t.wait_ticks(2);
	t.require(FileSys::does_file_exist(prefab_path.c_str(), FileSys::GAME_DIR), "prefab file was created");

	// Diagnostic: is RagdollJointComponent even present in the SERIALIZED text, or does it fail to
	// write in the first place (as opposed to failing to read back)?
	std::string saved_text = PrefabFile::load_text(prefab_path.c_str());
	size_t joint_occurrences = 0;
	for (size_t pos = saved_text.find("RagdollJointComponent"); pos != std::string::npos;
		 pos = saved_text.find("RagdollJointComponent", pos + 1))
		joint_occurrences++;
	t.check(joint_occurrences == chain.size() - 1,
			("RagdollJointComponent appears in the SAVED prefab text the expected number of times "
			 "(found " + std::to_string(joint_occurrences) + ", expected " + std::to_string(chain.size() - 1) + ")")
				.c_str());
	{
		size_t body_occurrences = 0;
		for (size_t pos = saved_text.find("RagdollPhysicsBodyComponent"); pos != std::string::npos;
			 pos = saved_text.find("RagdollPhysicsBodyComponent", pos + 1))
			body_occurrences++;
		t.check(body_occurrences == chain.size(),
				("RagdollPhysicsBodyComponent occurrences in saved text (found " +
				 std::to_string(body_occurrences) + ", expected " + std::to_string(chain.size()) + ")")
					.c_str());
	}

	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, ("open-editor " + prefab_path).c_str());
	co_await t.wait_ticks(5);
	editor = static_cast<EditorDoc*>(eng->get_tool());
	t.require(editor != nullptr && editor->is_editing_prefab(), "editor is now in prefab edit mode");

	// Find the RagdollSetupComponent instance inside the reopened prefab.
	RagdollSetupComponent* setup2 = nullptr;
	for (auto o : eng->get_level()->get_all_objects()) {
		Entity* e = o->cast_to<Entity>();
		if (e && e->get_component<RagdollSetupComponent>()) {
			setup2 = e->get_component<RagdollSetupComponent>();
			break;
		}
	}
	t.require(setup2 != nullptr, "RagdollSetupComponent found inside the reopened prefab");

	// Diagnostic: did the scaffold entities survive the round-trip at all, and if so, are they
	// parented to setup2's owner (Entity::get_parent()) as direct children?
	int scaffold_entities_found = 0, scaffold_children_of_setup = 0;
	for (auto o : eng->get_level()->get_all_objects()) {
		Entity* e = o->cast_to<Entity>();
		if (!e || e->get_editor_name().rfind("RSWP_Bone_", 0) != 0)
			continue;
		scaffold_entities_found++;
		if (e->get_parent() == setup2->get_owner())
			scaffold_children_of_setup++;
	}
	t.check(scaffold_entities_found == (int)chain.size(),
			"all scaffold entities survived the save/reload round-trip by name");
	t.check(scaffold_children_of_setup == (int)chain.size(),
			"all scaffold entities are still parented (Entity::get_parent()) to the setup entity");
	t.check((int)setup2->get_owner()->get_children().size() == (int)chain.size(),
			"setup entity's own get_children() count matches the authored chain size");

	// Diagnostic: did RagdollJointComponent/RagdollPhysicsBodyComponent themselves survive the
	// round-trip on each scaffold entity (as opposed to the entities/hierarchy surviving but the
	// COMPONENTS on them being dropped or failing to reattach)?
	int scaffold_with_body = 0, scaffold_with_joint = 0;
	for (Entity* c : setup2->get_owner()->get_children()) {
		if (c->get_component<RagdollPhysicsBodyComponent>())
			scaffold_with_body++;
		if (c->get_component<RagdollJointComponent>())
			scaffold_with_joint++;
	}
	t.check(scaffold_with_body == (int)chain.size(),
			"every scaffold child still has its RagdollPhysicsBodyComponent after reload");
	t.check(scaffold_with_joint == (int)chain.size() - 1,
			"every non-root scaffold child still has its RagdollJointComponent after reload");

	setup2->preview_ragdoll();
	co_await t.wait_ticks(2);
	t.require(setup2->is_previewing(), "preview_ragdoll started inside prefab edit mode");

	int capsule_count = 0, jointed_count = 0, root_count = 0;
	for (auto o : eng->get_level()->get_all_objects()) {
		Entity* e = o->cast_to<Entity>();
		if (!e || !e->get_component<CapsuleComponent>())
			continue;
		capsule_count++;
		auto* joint = e->get_component<AdvancedJointComponent>();
		if (joint && joint->get_target() != nullptr)
			jointed_count++;
		else
			root_count++;
	}
	t.check(capsule_count == (int)chain.size(), "one capsule body spawned per authored bone (prefab mode)");
	t.check(root_count == 1, "exactly one root body in prefab mode (this is the user's reported failure: found N)");
	t.check(jointed_count == (int)chain.size() - 1, "every non-root body resolved a joint in prefab mode");

	co_return;
}

EDITOR_TEST("ragdoll/setup_preview_multibone_chain_in_prefab", 30.f,
			test_ragdoll_setup_preview_multibone_chain_in_prefab);
