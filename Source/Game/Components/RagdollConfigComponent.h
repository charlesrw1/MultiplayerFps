#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Assets/IAsset.h"
#include "Framework/StructReflection.h"
#include "Framework/StringName.h"
#include "Framework/EnumDefReflection.h"
#include "Render/DynamicModelPtr.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <vector>

// Editor-only ragdoll authoring tool. Builds a full ragdoll (capsules + AdvancedJointComponent
// joints, wired into a single RagdollComponent -- the one and only ragdoll runtime, no parallel
// implementation here) from a skeleton using a bone-name heuristic, so nothing per-bone has to
// be hand authored. The mesh + animator + RagdollComponent live on a "preview_entity" child
// (dont_serialize_or_edit) that stays parented to this entity. The capsule/joint bodies, however,
// are FREE top-level entities (never parented) -- a simulating body's transform is physics-owned and
// must not be dragged by a moving parent (RagdollComponent::add_body ASSERTs this). See body_entities.

class Model;
class Entity;
class MeshBuilderComponent;
class RagdollComponent;
class PhysicsBody;

NEWENUM(RagdollLimbGroup, uint8_t){
	None,
	Torso,
	Head,
	ArmLeft,
	ArmRight,
	LegLeft,
	LegRight,
};

// which JointTuning entry a bone's joint should pull its limits from
enum class RagdollJointRole : uint8_t
{
	None,
	Spine,
	Neck,
	Shoulder,
	Elbow,
	Wrist,
	Hip,
	Knee,
	Ankle,
};

struct JointTuning
{
	STRUCT_BODY();
	REF float swing1_limit = 0.5f; // radians
	REF float swing2_limit = 0.5f;
	REF float twist_min = -0.3f;
	REF float twist_max = 0.3f;
	REF float damping = 0.f;
	REF float stiffness = 0.f;
};

class RagdollConfigComponent : public Component
{
public:
	CLASS_BODY(RagdollConfigComponent, spawnable);

	RagdollConfigComponent();

	REF AssetPtr<Model> model;

	// per-joint-type tuning, seeded with a reasonable humanoid default, hand-tuned from there
	REF JointTuning spine_limits;
	REF JointTuning neck_limits;
	REF JointTuning shoulder_limits;
	REF JointTuning elbow_limits;
	REF JointTuning wrist_limits;
	REF JointTuning hip_limits;
	REF JointTuning knee_limits;
	REF JointTuning ankle_limits;

	REF float capsule_radius_scale = 1.f;

	// bitmask over RagdollLimbGroup (1 << group), drives which bones join the ragdoll.
	// Custom property editor: LevelEditor/PropertyEditors.cpp RagdollGroupMaskEditor.
	REFLECT(type = RagdollGroupMask)
	uint8_t sim_group_mask = 0xFF;

	void start() final;
	void stop() final;
	void editor_start() final;

#ifdef EDITOR_BUILD
	std::unique_ptr<IComponentEditorUi> create_editor_ui() final;
	void editor_on_change_property() override; // shows/updates the model as soon as it's assigned
#endif

	void update() override;

	bool is_simulating() const { return simulating; }
	void generate_from_skeleton();
	void start_simulate();
	void stop_simulate();

private:
	struct RagdollBoneConfig
	{
		StringName bone_name;
		StringName parent_bone_name; // empty => root
		float capsule_radius = 0.15f;
		float capsule_height = 0.3f;
		glm::vec3 local_offset_pos = glm::vec3(0.f);
		glm::quat local_offset_rot = glm::quat(1, 0, 0, 0);
		RagdollLimbGroup group = RagdollLimbGroup::None;
		RagdollJointRole role = RagdollJointRole::None;
		bool is_root = false;
	};

	const JointTuning& tuning_for_role(RagdollJointRole role) const;

	void ensure_preview_entity();  // always-present visual entity: MeshComponent + 1-node bind-pose animator
	void rebuild_bodies();		   // (re)spawns capsule/joint entities + RagdollComponent from bone_configs
	void rebuild_ghost_visual();   // solid additive capsule/cone preview mesh, shown only while idle

	// drag-while-simulating: raycast-pick a spawned capsule under the mouse and drive it
	// toward a view-plane target using set_linear_velocity. Only active while simulating and
	// this component's entity is selected in the editor.
	void update_drag_in_editor();

	std::vector<RagdollBoneConfig> bone_configs; // rebuilt from `model`'s skeleton + tuning fields, never serialized

	obj<Entity> preview_entity;	 // always exists once model is set: MeshComponent + animator + ragdoll
	obj<RagdollComponent> ragdoll;   // lives on preview_entity
	// The capsule/joint bodies are FREE (unparented) top-level entities, NOT children of preview_entity
	// -- a simulating body's transform is owned by physics and must not be dragged by a moving parent
	// (RagdollComponent::add_body ASSERTs this). We own their lifetime explicitly via this list.
	std::vector<obj<Entity>> body_entities;
	obj<Entity> ghost_entity;		 // child of preview_entity: solid ghost capsule/cone visualization
	DynamicModelUniquePtr ghost_model;

	bool bodies_built = false;
	bool simulating = false;

	// drag state
	bool is_dragging = false;
	obj<PhysicsBody> drag_body;
	float drag_depth = 0.f;
};
