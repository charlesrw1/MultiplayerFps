#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Assets/IAsset.h"
#include "Render/Model.h"
#include <vector>

class RagdollComponent;

// Replaces RagdollConfigComponent's heuristic auto-generate approach with explicit hand-authored
// scaffolding. Owns `model` and lazily attaches a MeshComponent (dont_serialize_or_edit=true,
// always re-derived from `model`) + a minimal bind-pose animator DIRECTLY to its own owner entity
// -- no separate "rig entity". The user places RagdollPhysicsBodyComponent/RagdollJointComponent
// scaffolding as CHILDREN of this same owner entity, bone-parented (Entity::set_parent_bone) to
// individual right-side/center skeleton bones (bone-parenting resolves one level deep, so they
// must be direct children of the entity that actually owns the MeshComponent -- see
// Entity::get_parent_transform()). This authoring entity's own MeshComponent always just shows
// the bind pose; it is never wired to a RagdollComponent and never simulates. "Preview Ragdoll"
// spawns a wholly separate, transient (dont_serialize_or_edit) second entity with its own
// MeshComponent, RagdollComponent, and mirrored capsule/joint bodies, and is the only thing that
// ever gets enable()'d.
class RagdollSetupComponent : public Component
{
public:
	CLASS_BODY(RagdollSetupComponent, spawnable);
	RagdollSetupComponent() { set_call_init_in_editor(true); }

	REF AssetPtr<Model> model;

	// When true (default), preview_ragdoll mirrors authored right-side bones to their left-side
	// counterparts as usual. Turn off to preview only the authored (right/center) bones -- useful
	// when debugging a single joint/chain in isolation without the mirrored half cluttering things.
	REF bool mirror_bodies_in_preview = true;

	// When false (default), left-side scaffolding bones are pruned (logged and skipped) since
	// mirroring auto-generates the left side from the authored right/center bones. Set true to
	// author the left side by hand instead -- e.g. for an asymmetric ragdoll -- and have it
	// spawned as-authored rather than pruned or mirror-generated.
	REF bool allow_left_side_authoring = false;

	void start() final;
	void stop() final;
	void editor_start() final;
#ifdef EDITOR_BUILD
	void on_inspector_imgui() final;
	void editor_on_change_property() override;

	// "Enable All Joints" / "Disable All Joints" buttons: sets RagdollJointComponent::enabled on
	// every direct child joint at once, for quickly toggling the whole scaffolding rather than
	// clicking through each joint individually.
	void set_all_joints_enabled(bool enabled);
#endif

	// Ensures this component's own owner entity has a MeshComponent(model) + bind-pose animator.
	// The MeshComponent is dont_serialize_or_edit=true (always re-derived from `model`, matching
	// the Component::editor_set_model idiom) -- but the owner entity itself, this component, and
	// any scaffolding children placed under the owner are all normally serialized.
	void ensure_rig_mesh();

	// "Start Preview" / "Reset Preview" buttons: tears down any previous preview set and rebuilds
	// a fresh, simulated ragdoll from the current scaffolding (mirroring right-side bones to left)
	// on a separate transient entity. The authoring entity (this component's owner) is never
	// simulated. Same call either way -- "reset" is just "start" run again while already running.
	void preview_ragdoll();

	// "End Preview" button: tears down the preview set without rebuilding it.
	void end_preview();

	bool is_previewing() const { return preview_mesh_entity.get() != nullptr; }

	// Builds a fully-wired, simulated ragdoll (mesh + RagdollComponent + mirrored capsule bodies +
	// joints) from this component's authored scaffolding -- get_owner()'s model and its
	// RagdollPhysicsBodyComponent/RagdollJointComponent children -- and spawns it into the
	// currently active Level, already enable()'d. Only reads authoring data off get_owner(); never
	// touches this component's own state, so it's safe to call on a prefab's static (never
	// inserted-into-a-Level) instance -- e.g. a node found via PrefabAsset's search functions -- to
	// spawn a real, permanent ragdoll at runtime. preview_ragdoll() above is just this plus
	// bookkeeping for its own transient teardown.
	REF Entity* create_ragdoll_entity(const glm::mat4& transform, bool create_enabled) const;

private:
	obj<Entity> preview_mesh_entity; // transient, dont_serialize_or_edit=true
	obj<RagdollComponent> preview_ragdoll_comp;
	std::vector<obj<Entity>> preview_body_entities; // free/unparented, dont_serialize_or_edit=true

	void teardown_preview();

	// Shared implementation for create_ragdoll_entity()/preview_ragdoll(). Every spawned free body
	// (and pin_root_to_skeleton's anchor entity, if any) is appended to *out_spawned_bodies when
	// non-null, which is how preview_ragdoll() tracks its own set for teardown_preview() --
	// create_ragdoll_entity() passes nullptr since that ragdoll is meant to persist.
	Entity* build_ragdoll(const glm::mat4& transform, bool enabled, std::vector<obj<Entity>>* out_spawned_bodies) const;
};
