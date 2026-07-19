#pragma once
#include "Game/EntityComponent.h"
#include "Animation/Runtime/SpringBones.h"

// Editor authoring scaffolding for one manually-authored spring bone -- pure data, no preview logic
// of its own (that lives on SpringBonePreviewComponent). Placed as a child entity either directly
// under the rig entity (the spring bone chain root, bone-parented via Entity::set_parent_bone to a
// skeleton bone) or under another SpringBoneSetupComponent entity (a chain continuation). See
// build_spring_bones_from_setup (SpringBoneSetupUtil.h) for how a hierarchy of these is turned into
// a live SpringBoneManagerComponent.
class SpringBoneSetupComponent : public Component
{
public:
	CLASS_BODY(SpringBoneSetupComponent, spawnable);

	REF float yaw_stiffness = 100.f;
	REF float yaw_damping = 8.f;
	REF float pitch_stiffness = 100.f;
	REF float pitch_damping = 8.f;
	REF float along_stiffness = 100.f;
	REF float along_damping = 8.f;
	REF bool allow_length_flex = false;
	REF float gravity = 0.f;
	REF glm::vec3 gravityDir = glm::vec3(0.f, -1.f, 0.f);

	SpringBoneParams get_params() const;
};
