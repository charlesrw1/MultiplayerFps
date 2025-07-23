#pragma once
#include "PhysicsComponents.h"

// To make ragdolls work:
// On your entity:
//		needs a MeshComponent with a skeletal mesh and an Animator
//		needs a RagdollComponent
// 
// The ragdoll component also has to be initialized with multiple PhysicsBodies and Joints. These PhysicsBodies themselves *arent* parented to bones, they are "free" entities. 
// but they are linked to a bone and can also have a local space transform to the bone
// 
// so to make a ragdoll (in Lua):
// local e = GameplayStatic.spawn_entity()
// local meshcomponent = e:create_component(MeshComponent)
// meshcomponent:set_model(Model.load("animated_model.cmdl"))
// set_animator_tree(meshcomponent) -- implment this function, see example code
// 
// local rotate_sideways = lMath.from_euler({x=math.pi*0.5})
// local spine = GampleStatic.spawn_entity()
// local sb = spine:create_component(CapsuleComponent)
// spine:set_ws_rotation(rotate_sideways)
// local spine1 = GameplayStatic.spawn_entity()
// spine1:set_ws_rotation(rotate_sideways)
// local s1b = spine1:create_component(CapsuleComponent)
// local joint = spine:create_component(AdvancedJointComponent)
// joint:set_target(spine1)
// 
// local ragdoll = e:create_component(RagdollComponent)
// ragdoll:add_body("spine",spine)
// ragdoll:add_body("spine1",spine1)
// 
// ragdoll:enable()

// now clearly thats a lot of writing for just 2 bones. youll want to make some functions to make the process of adding the bodies/joints easier.


#include "MeshComponent.h"

class RagdollComponent : public Component {
public:
	CLASS_BODY(RagdollComponent);
	void start() final;
	void stop() final;

	REF void enable();
	REF void disable();

	void on_pre_get_bones();

	int get_num_bodies() const {
		return bodies.size();
	}
	int get_body_index(int i) {
		return bodies.at(i).bone_index;
	}
	glm::mat4 get_body_bone_transform(int i);

	// when adding bodies, it expects it to have a local space transform set
	REF void add_body(StringName parented_bone, PhysicsBody* body);
	REF void add_root_body(StringName parented_bone, PhysicsBody* body);
private:
	struct RagdollBody {
		glm::vec3 bindPosePos;
		glm::quat bindPoseRot;
		glm::mat4 invBindPose;
		int bone_index = -1;
		obj<PhysicsBody> ptr;
	};
	obj<MeshComponent> meshComponent;
	std::vector<RagdollBody> bodies;
	int root_body_index = -1;
	glm::mat4 inv_root_body = glm::mat4(1);
};