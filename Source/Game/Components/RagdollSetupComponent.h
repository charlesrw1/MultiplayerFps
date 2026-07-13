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

	void start() final;
	void stop() final;
	void editor_start() final;
#ifdef EDITOR_BUILD
	void on_inspector_imgui() final;
	void editor_on_change_property() override;
#endif

	// Ensures this component's own owner entity has a MeshComponent(model) + bind-pose animator.
	// The MeshComponent is dont_serialize_or_edit=true (always re-derived from `model`, matching
	// the Component::editor_set_model idiom) -- but the owner entity itself, this component, and
	// any scaffolding children placed under the owner are all normally serialized.
	void ensure_rig_mesh();

	// "Preview Ragdoll" button: tears down any previous preview set and rebuilds a fresh,
	// simulated ragdoll from the current scaffolding (mirroring right-side bones to left) on a
	// separate transient entity. The authoring entity (this component's owner) is never simulated.
	void preview_ragdoll();

private:
	obj<Entity> preview_mesh_entity; // transient, dont_serialize_or_edit=true
	obj<RagdollComponent> preview_ragdoll_comp;
	std::vector<obj<Entity>> preview_body_entities; // free/unparented, dont_serialize_or_edit=true

	void teardown_preview();
};
