#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Render/DynamicModelPtr.h"

// Editor scaffolding only -- NOT a PhysicsBody subclass, creates no PhysX actor. Lives on a
// child entity bone-parented (Entity::set_parent_bone) directly under the RagdollSetupComponent
// rig entity's MeshComponent. Authors one ragdoll body's capsule shape; RagdollSetupComponent
// reads these fields to spawn a real CapsuleComponent when building the simulated preview.
class RagdollPhysicsBodyComponent : public Component
{
public:
	CLASS_BODY(RagdollPhysicsBodyComponent, spawnable);
	// Needed so stop() actually fires in the editor (Component::deactivate_internal only calls
	// stop() when !is_editor_level() || get_call_init_in_editor()) -- without this, editor_start()
	// builds the gizmo entity/model but nothing ever tears it down before this component's own
	// destructor frees gizmo_model, racing the (separately-owned) gizmo entity's render-proxy
	// teardown and crashing on the next scene draw. See stop().
	RagdollPhysicsBodyComponent() { set_call_init_in_editor(true); }

	REF float height = 0.3f;
	REF float radius = 0.15f;
	REF float height_offset = 0.15f; // matches CapsuleComponent's field semantics

#ifdef EDITOR_BUILD
	void editor_start() final;
	void stop() final;
	void on_inspector_imgui() final;
	void editor_on_change_property() override;
#endif

private:
#ifdef EDITOR_BUILD
	void rebuild_gizmo_mesh();
	obj<Entity> gizmo_entity;
	DynamicModelUniquePtr gizmo_model;
#endif
};
