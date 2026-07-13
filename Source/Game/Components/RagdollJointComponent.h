#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Render/DynamicModelPtr.h"
#include "Game/Components/PhysicsComponents.h" // JM/JointMotion

// Editor scaffolding only -- NOT a PhysicsJointComponent subclass, creates no PhysX joint. Lives
// on the same scaffolding entity as a sibling RagdollPhysicsBodyComponent (bone-parented under
// the rig entity's MeshComponent). If a bone has no RagdollJointComponent, it's the ragdoll
// root; RagdollSetupComponent resolves the joint's *parent* bone implicitly by walking up the
// skeleton hierarchy to the nearest ancestor bone that also has an authored body (no field here
// names it explicitly).
//
// Local-axis convention (matches AdvancedJointComponent with an identity joint anchor, which is
// what RagdollSetupComponent spawns with): local +X = twist axis, local Y/Z = the two swing axes
// (ang_y_motion/swing1_limit maps to Y, ang_z_motion/swing2_limit maps to Z).
class RagdollJointComponent : public Component
{
public:
	CLASS_BODY(RagdollJointComponent, spawnable);
	// Needed so stop() actually fires in the editor (Component::deactivate_internal only calls
	// stop() when !is_editor_level() || get_call_init_in_editor()) -- without this, editor_start()
	// builds the gizmo entities/models but nothing ever tears them down before this component's
	// own destructor frees swing_model/twist_model, racing the (separately-owned) gizmo entities'
	// render-proxy teardown and crashing on the next scene draw. See stop().
	RagdollJointComponent() { set_call_init_in_editor(true); }

	REF JM ang_x_motion = JM::Limited; // twist
	REF JM ang_y_motion = JM::Limited; // swing1
	REF JM ang_z_motion = JM::Limited; // swing2
	REF float twist_limit_min = -0.3f;
	REF float twist_limit_max = 0.3f;
	REF float swing1_limit = 0.5f;
	REF float swing2_limit = 0.5f;
	REF float damping = 0.f;
	REF float stiffness = 0.f;

#ifdef EDITOR_BUILD
	void editor_start() final;
	void stop() final;
	void on_inspector_imgui() final;
	void editor_on_change_property() override;
#endif
	void update() override;

private:
#ifdef EDITOR_BUILD
	void rebuild_gizmo_mesh();
	void start_preview_twist();
	void start_preview_swing();
	void stop_preview();
	class MeshComponent* get_rig_mesh() const; // get_owner()->get_parent()'s MeshComponent
	glm::quat compute_preview_rotation() const;

	enum class PreviewMode { None, Twist, Swing };
	PreviewMode preview_mode = PreviewMode::None;
	float preview_t = 0.f;
	static RagdollJointComponent* s_previewing_joint;

	obj<Entity> gizmo_entity;
	DynamicModelUniquePtr swing_model; // cone (2-DOF) or wedge (1-DOF); null when both swing axes are Locked
	DynamicModelUniquePtr twist_model; // twist wedge/ring; null unless ang_x_motion is Limited/Free
#endif
};
