#include "RagdollComponent.h"
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Animation/Runtime/Animation.h"
#include "Game/Entity.h"
void RagdollComponent::start() {
	MeshComponent* c = get_owner()->get_component<MeshComponent>();
	if (!c || !c->get_animator()) {
		sys_print(Error, "RagdollComponent::start: no meshcomponent or animator present\n");
		return;
	}
	this->meshComponent = c;
}
#include "Animation/SkeletonData.h"
void RagdollComponent::enable() {
	MeshComponent* mesh = meshComponent.get();
	ASSERT(mesh);
	AnimatorObject* animator = mesh->get_animator();
	ASSERT(animator);
	// animator->set_update_owner_position_to_root(true);
	const glm::mat4& this_ws = get_owner()->get_ws_transform();
	// this shit sucks, just trying to get it working
	InlineVec<PhysicsJointComponent*, 32> joints;
	for (auto& b : bodies) {
		auto phys = b.ptr.get();
		if (!phys || phys->is_a<AdvancedJointComponent>())
			continue;
		// Use the *stored* bind offset, not the entity's live ls_transform. enable_with_initial_transforms
		// below flips each body to is_top_level, so after one enable/disable cycle (e.g. the config editor's
		// "Restart Simulating") get_ls_transform() returns the body's full world pose instead of the small
		// bone-local rotation offset -- feeding that back through this_ws*poseMatrix*ls collapses the ragdoll.
		// This matches the ls used in the enable_with_initial_transforms loop, so enable() is idempotent.
		auto ls = compose_transform(b.bindPosePos, b.bindPoseRot, glm::vec3(1));
		auto& poseMatrix = animator->get_model().get_skel()->get_all_bones().at(b.bone_index).posematrix;
		// poseMatrix is bone space -> mesh space; this_ws converts mesh space -> world space
		// (matches the this_ws usage a few lines below in enable_with_initial_transforms).
		phys->get_owner()->set_ws_transform(this_ws * (glm::mat4)poseMatrix * ls);
		auto joint = phys->get_owner()->get_component<PhysicsJointComponent>();
		if (joint)
			joints.push_back(joint);
	}
	for (int i = 0; i < joints.size(); i++)
		joints[i]->refresh_joint();

	for (auto& b : bodies) {
		auto phys = b.ptr.get();
		if (!phys || phys->is_a<AdvancedJointComponent>())
			continue;
		// enable_with_initial_transforms ASSERTs has_initialized && !static && simulating.
		// Configure the body to dynamic+simulating before enabling so a misconfigured
		// ragdoll body warns instead of crashing the asserts.
		if (phys->get_body_type() != BodyType::Dynamic) {
			// Expected on every disable()->enable() cycle (e.g. ragdoll config editor's
			// "Restart Simulating"): disable() intentionally leaves bodies kinematic.
			sys_print(Debug, "RagdollComponent::enable: body %s is not dynamic+simulating, forcing\n",
					  phys->get_owner()->get_editor_name().c_str());
			phys->set_body_type(BodyType::Dynamic);
		}
		auto ls = compose_transform(b.bindPosePos, b.bindPoseRot, glm::vec3(1));
		phys->enable_with_initial_transforms(this_ws * animator->get_last_global_bonemats().at(b.bone_index) * ls,
											 this_ws * animator->get_global_bonemats().at(b.bone_index) * ls,
											 eng->get_dt());
	}

	animator->set_ragdoll(this);
}
void RagdollComponent::stop() {
	for (auto& b : bodies) {
		if (b.ptr.get()) {
			b.ptr->destroy(); // remove physics bodies
		}
	}
}

void RagdollComponent::disable() {
	MeshComponent* mesh = meshComponent.get();
	if (!mesh)
		return;
	AnimatorObject* animator = mesh->get_animator();
	if (!animator)
		return;
	const glm::mat4& this_ws = get_owner()->get_ws_transform();
	for (auto& b : bodies) {
		auto phys = b.ptr.get();
		if (!phys || phys->is_a<AdvancedJointComponent>())
			continue;
		// freeze back to kinematic then snap to the current animated pose -- must switch
		// mode before pushing the transform, since dynamic bodies ignore direct Entity
		// transform pushes.
		phys->set_body_type(BodyType::Kinematic);
		auto ls = compose_transform(b.bindPosePos, b.bindPoseRot, glm::vec3(1));
		phys->get_owner()->set_ws_transform(this_ws * animator->get_global_bonemats().at(b.bone_index) * ls);
	}
	animator->set_ragdoll(nullptr);
}

void RagdollComponent::on_pre_get_bones() {
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
		return glm::mat4(1); // ?
	}
	const glm::mat4& transformBody = b->get_owner()->get_ws_transform();
	if (i == root_body_index) {
		return glm::mat4(1);
	} else {
		return inv_root_body * transformBody * bodies.at(i).invBindPose;
	}
}
void RagdollComponent::add_body(StringName parented_bone, PhysicsBody* body) {
	assert(body);
	// INVARIANT: ragdoll physics bodies must be FREE (unparented) entities, never children of the
	// mesh/owner entity. A simulating body's transform is owned by PhysX; if the body's entity has a
	// parent that moves (e.g. the animated mesh entity, whose transform on_pre_get_bones rewrites every
	// frame), that parent move propagates into the child and PhysicsBody::on_changed_transform ->
	// set_transform zeroes the body's velocity every frame -- gravity never accumulates and the ragdoll
	// crawls instead of falling. Keep bodies parentless (see RagdollConfigComponent::rebuild_bodies,
	// which spawns them via level->spawn_entity()) and this whole class of bug cannot happen.
	ASSERT(body->get_owner() && body->get_owner()->get_parent() == nullptr &&
		   "ragdoll physics bodies must not be parented -- they must be free/top-level entities");
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

REF void RagdollComponent::add_root_body(StringName parented_bone, PhysicsBody* body) {
	size_t before = bodies.size();
	add_body(parented_bone, body);
	if (bodies.size() == before) {
		// add_body already logged the reason (unknown bone / no meshcomponent); don't crash
		// callers that pass an unresolvable bone name for the root.
		sys_print(Error, "RagdollComponent::add_root_body: failed to add root body for bone %s\n",
				  parented_bone.get_c_str());
		return;
	}
	root_body_index = (int)bodies.size() - 1;
}

REF PhysicsBody* RagdollComponent::get_physics_body_for_bone(StringName name) {
	MeshComponent* mesh = meshComponent.get();
	if (!mesh) {
		sys_print(Warning, "get_physics_body_for_bone: no mesh\n");
		return nullptr;
	}
	int bone_idx = mesh->get_index_of_bone(name);
	if (bone_idx==-1) {
		sys_print(Warning, "get_physics_body_for_bone: invalid bone %s\n", name.get_c_str());
		return nullptr;
	}
	for (auto& body : bodies) {
		if (body.bone_index == bone_idx) {
			return body.ptr.get();
		}
	}
	sys_print(Warning, "get_physics_body_for_bone: no matching body for bone %s\n",name.get_c_str());
	return nullptr;
}
