#include "SpringBoneManagerComponent.h"
#include "GameAnimationMgr.h"
#include "MeshComponent.h"
#include "Game/Entity.h"
#include "Framework/MathLib.h"
#include "Framework/Util.h"

void SpringBoneManagerComponent::start() {
	GameAnimationMgr::inst->add_to_post_animate_set(*this);
}

void SpringBoneManagerComponent::stop() {
	GameAnimationMgr::inst->remove_from_post_animate_set(*this);
	// Ad-hoc "children" have no real Entity parent link, so nothing else destroys them
	// when this manager's owner dies -- do it here.
	for (auto& att : attachments) {
		Entity* child = att.child.get();
		if (child)
			child->destroy();
	}
	attachments.clear();
}

int SpringBoneManagerComponent::find_def_index(StringName name) const {
	for (int i = 0; i < (int)defs.size(); i++)
		if (defs[i].name == name)
			return i;
	return -1;
}

glm::mat4 SpringBoneManagerComponent::get_bone_parent_world_transform(const SpringBoneManualDef& def) const {
	if (def.parent_is_skeleton_bone) {
		MeshComponent* mc = meshComponent.get();
		ASSERT(mc && "spring bone parented to a skeleton bone requires set_mesh_component() first");
		return mc->get_owner()->get_ws_transform() * mc->get_ls_transform_of_bone(def.parentName);
	}
	if (def.parentName == StringName())
		return get_owner()->get_ws_transform();

	const int parentIdx = find_def_index(def.parentName);
	ASSERT(parentIdx >= 0 && "spring bone parent not found -- parents must be added before their children");
	return states[parentIdx].worldTransform;
}

void SpringBoneManagerComponent::add_spring_bone(StringName name, StringName parentName, bool parent_is_skeleton_bone,
												  glm::vec3 localPos, glm::quat localRot,
												  const SpringBoneParams& params) {
	ASSERT(find_def_index(name) == -1 && "spring bone name already used on this manager");
	ASSERT((parent_is_skeleton_bone || parentName == StringName() || find_def_index(parentName) != -1) &&
		   "spring bone parent must already be added to this manager (or be a skeleton bone / empty for owner root)");

	SpringBoneManualDef def;
	def.name = name;
	def.parentName = parentName;
	def.parent_is_skeleton_bone = parent_is_skeleton_bone;
	def.localPos = localPos;
	def.localRot = localRot;
	def.params = params;

	defs.push_back(def);
	states.emplace_back();
}

void SpringBoneManagerComponent::parent_entity_to_spring_bone(StringName boneName, Entity* child) {
	if (!child)
		return;
	const int idx = find_def_index(boneName);
	if (idx == -1) {
		sys_print(Error, "SpringBoneManagerComponent::parent_entity_to_spring_bone: unknown bone %s\n",
				  boneName.get_c_str());
		return;
	}
	unparent_entity(child); // avoid duplicate attachments if re-parented to a different bone

	SpringBoneAttachment att;
	att.child = child;
	att.boneIndex = idx;
	att.localOffset = glm::inverse(states[idx].worldTransform) * child->get_ws_transform();
	attachments.push_back(att);
}

void SpringBoneManagerComponent::unparent_entity(Entity* child) {
	for (int i = 0; i < (int)attachments.size(); i++) {
		if (attachments[i].child.get() == child) {
			attachments.erase(attachments.begin() + i);
			return;
		}
	}
}

void SpringBoneManagerComponent::late_update(float dt) {
	Entity* owner = get_owner();
	ASSERT(owner);

	for (int i = 0; i < (int)defs.size(); i++) {
		const SpringBoneManualDef& def = defs[i];
		SpringBoneManualState& state = states[i];

		const glm::mat4 parentWorld = get_bone_parent_world_transform(def);
		const glm::mat4 localMat = compose_transform(def.localPos, def.localRot, glm::vec3(1.f));
		const glm::mat4 worldRestMat = parentWorld * localMat;
		const glm::vec3 worldParentPos = glm::vec3(parentWorld[3]);

		state.worldTransform = integrate_spring_bone(dt, def.params, worldRestMat, worldParentPos, state.sim);
	}

	for (auto& att : attachments) {
		Entity* child = att.child.get();
		if (!child)
			continue;
		child->set_ws_transform(states[att.boneIndex].worldTransform * att.localOffset);
	}
}
