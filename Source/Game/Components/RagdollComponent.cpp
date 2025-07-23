#include "RagdollComponent.h"
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Animation/Runtime/Animation.h"
#include "Game/Entity.h"
void RagdollComponent::start()
{
	MeshComponent* c = get_owner()->get_component<MeshComponent>();
	if (!c||!c->get_animator()) {
		sys_print(Error, "RagdollComponent::start: no meshcomponent or animator present\n");
		return;
	}
	this->meshComponent = c;
}
#include "Animation/SkeletonData.h"
void RagdollComponent::enable()
{
	MeshComponent* mesh = meshComponent.get();
	ASSERT(mesh);
	AnimatorObject* animator = mesh->get_animator();
	ASSERT(animator);
	// animator->set_update_owner_position_to_root(true);
	const glm::mat4& this_ws = get_owner()->get_ws_transform();
	// this shit sucks, just trying to get it working
	InlineVec<PhysicsJointComponent*,32> joints;
	for (auto& b : bodies) {
		auto phys = b.ptr.get();
		if (!phys || phys->is_a<AdvancedJointComponent>())
			continue;
		auto ls = phys->get_owner()->get_ls_transform();
		auto& poseMatrix = animator->get_model().get_skel()->get_all_bones().at(b.bone_index).posematrix;
		phys->get_owner()->set_ws_transform(poseMatrix * ls);
		auto joint = phys->get_owner()->get_component<PhysicsJointComponent>();
		if(joint)
			joints.push_back(joint);
	}
	for (int i = 0; i < joints.size(); i++)
		joints[i]->refresh_joint();

	for (auto& b : bodies) {
		auto phys = b.ptr.get();
		if (!phys || phys->is_a<AdvancedJointComponent>())
			continue;
		auto ls = compose_transform(b.bindPosePos, b.bindPoseRot, glm::vec3(1));
		phys->enable_with_initial_transforms(
			this_ws * animator->get_last_global_bonemats().at(b.bone_index) * ls,
			this_ws * animator->get_global_bonemats().at(b.bone_index) * ls,
			eng->get_dt());

	}

	animator->set_ragdoll(this);
}
void RagdollComponent::stop() {

}

void RagdollComponent::disable()
{
	
}


void RagdollComponent::on_pre_get_bones()
{
	if (root_body_index != -1) {
		auto& b = bodies.at(root_body_index);
		PhysicsBody* body = b.ptr.get();
		if (body) {
			glm::mat4 root_transform = body->get_ws_transform() * b.invBindPose;
			get_owner()->set_ws_transform(root_transform);
			inv_root_body = glm::inverse(root_transform);
			return;
		}
	}
	inv_root_body = glm::inverse(get_owner()->get_ws_transform());
}
glm::mat4 RagdollComponent::get_body_bone_transform(int i) {

	PhysicsBody* b = bodies.at(i).ptr.get();
	if (!b) {
		return glm::mat4(1);	// ?
	}
	const glm::mat4& transformBody =b->get_owner()->get_ws_transform();
	if (i == root_body_index) {
		return glm::mat4(1);
	}
	else {
		return inv_root_body * transformBody * bodies.at(i).invBindPose;
	}
}
void RagdollComponent::add_body(StringName parented_bone, PhysicsBody* body)
{
	assert(body);
	MeshComponent* c = this->meshComponent.get();
	if (!c) 
		return;
	int idx = c->get_index_of_bone(parented_bone);
	if (idx == -1) {
		sys_print(Error, "RagdollComponent::add_body: unknown bone %s\n", parented_bone.get_c_str());
		return;
	}
	body->set_is_enable(false);

	RagdollBody b;
	b.bindPosePos = body->get_owner()->get_ls_position();
	b.bindPoseRot = body->get_owner()->get_ls_rotation();
	b.bone_index = idx;
	b.invBindPose = glm::inverse(body->get_owner()->get_ls_transform());
	b.ptr = body;
	bodies.push_back(b);
}

REF void RagdollComponent::add_root_body(StringName parented_bone, PhysicsBody* body)
{
	add_body(parented_bone, body);
	assert(!bodies.empty());
	root_body_index = (int)bodies.size() - 1;
}
