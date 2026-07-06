#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Animation/Runtime/SpringBones.h"
#include <vector>

class MeshComponent;

// One manually-authored spring bone. Not tied to a skeleton bone index -- defined by a rest-pose
// local transform relative to its parent, which is one of:
//   - another spring bone already added to this manager (parentName names it, parent_is_skeleton_bone false)
//   - a bone on meshComponent's skeleton (parentName names it, parent_is_skeleton_bone true)
//   - the owning entity's root (parentName empty)
// Parents must be added before their children (same requirement as SpringBones's skeleton chains).
struct SpringBoneManualDef
{
	StringName name;
	StringName parentName;
	bool parent_is_skeleton_bone = false;
	glm::vec3 localPos = glm::vec3(0.f);
	glm::quat localRot = glm::quat(1.f, 0.f, 0.f, 0.f);
	SpringBoneParams params;
};

struct SpringBoneManualState
{
	SpringBoneState sim; // position/velocity worked on by integrate_spring_bone
	glm::mat4 worldTransform = glm::mat4(1.f); // last simulated result; read by children and attachments
};

// An entity ad-hoc "parented" to a spring bone. This is NOT a real Entity parent/child link --
// see RagdollComponent's parentless-body invariant for why driving a moving parent transform onto
// a child every frame is fragile (on_changed_transform propagation, velocity resets, etc). Instead
// this manager pushes `child`'s ws_transform directly every late_update, and destroys `child` when
// this component's owner entity is destroyed.
struct SpringBoneAttachment
{
	obj<Entity> child;
	int boneIndex = -1;
	glm::mat4 localOffset = glm::mat4(1.f); // child's rest transform relative to the bone's world transform
};

// Spring bone simulation as a component instead of baked into AnimatorObject: bones are manually
// defined (not looked up on a skeleton) and simulated after the animator has produced this frame's
// pose, so a manual bone can be parented to an animated skeleton bone without lagging a frame.
// Also supports ad-hoc "parenting" arbitrary entities to a spring bone (see SpringBoneAttachment).
class SpringBoneManagerComponent : public Component
{
public:
	CLASS_BODY(SpringBoneManagerComponent);
	SpringBoneManagerComponent() { set_call_init_in_editor(true); }

	void start() final;
	void stop() final;

	REF void set_mesh_component(MeshComponent* mc) { meshComponent = mc; }

	// name must be unique within this manager. See SpringBoneManualDef for parent rules.
	void add_spring_bone(StringName name, StringName parentName, bool parent_is_skeleton_bone,
						  glm::vec3 localPos, glm::quat localRot, const SpringBoneParams& params);

	REF void parent_entity_to_spring_bone(StringName boneName, Entity* child);
	REF void unparent_entity(Entity* child);

	// Invoked by GameAnimationMgr after all AnimatorObjects have updated this frame -- must run
	// after animation so parent_is_skeleton_bone lookups see this frame's pose, not last frame's.
	void late_update(float dt);

private:
	int find_def_index(StringName name) const;
	glm::mat4 get_bone_parent_world_transform(const SpringBoneManualDef& def) const;

	obj<MeshComponent> meshComponent;
	std::vector<SpringBoneManualDef> defs;
	std::vector<SpringBoneManualState> states;
	std::vector<SpringBoneAttachment> attachments;
};
