#include "RagdollSetupComponent.h"
#include "RagdollComponent.h"
#include "RagdollPhysicsBodyComponent.h"
#include "RagdollJointComponent.h"
#include "RagdollUtil.h"
#include "Game/Entity.h"
#include "Level.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "GameEnginePublic.h"
#include "Framework/Log.h"
#include <unordered_map>

#ifdef EDITOR_BUILD
#include "imgui.h"
#endif

void RagdollSetupComponent::ensure_rig_mesh() {
	if (!model.get())
		return;
	Entity* owner = get_owner();
	auto* mesh = owner->get_component<MeshComponent>();
	if (!mesh) {
		mesh = owner->create_component<MeshComponent>();
		// Always re-derived from `model` -- matches the Component::editor_set_model idiom for a
		// component-owned display mesh. The owner entity itself (and any scaffolding children
		// placed under it) still serialize normally.
		mesh->dont_serialize_or_edit = true;
	}
	if (mesh->get_model() != model.get())
		mesh->set_model(model.get());

	// Minimal one-node anim graph: just evaluates the bind pose, matching RagdollComponent's
	// requirement of valid global bone matrices, and giving RagdollJointComponent's DOF-preview
	// something to swap out temporarily. This entity is the AUTHORING display only -- it never
	// gets a RagdollComponent and is never simulated; only the transient preview entity is.
	if (!mesh->get_animator()) {
		agBuilder builder;
		auto bind = builder.alloc<agBindPose>();
		builder.set_root(bind);
		mesh->create_animator(&builder);
	}
}

void RagdollSetupComponent::start() {
	ensure_rig_mesh();
}

void RagdollSetupComponent::stop() {
	// The rig MeshComponent lives directly on this component's owner entity -- ordinary
	// entity/component teardown handles it. Only the free/unparented preview bodies (and the
	// transient preview entity) need explicit cleanup here.
	teardown_preview();
}

void RagdollSetupComponent::editor_start() { start(); }

void RagdollSetupComponent::teardown_preview() {
	for (auto& e : preview_body_entities) {
		if (e.get())
			e->destroy();
	}
	preview_body_entities.clear();
	if (preview_mesh_entity.get())
		preview_mesh_entity->destroy();
	preview_mesh_entity = obj<Entity>();
	preview_ragdoll_comp = obj<RagdollComponent>();
}

namespace {
struct AuthoredBone {
	Entity* scaffold_entity = nullptr;
	RagdollPhysicsBodyComponent* body = nullptr;
	RagdollJointComponent* joint = nullptr; // null => this bone is the ragdoll root
	int bone_index = -1;
	StringName parent_bone; // resolved by walking the skeleton hierarchy; null => root/unresolved
};
} // namespace

void RagdollSetupComponent::preview_ragdoll() {
	teardown_preview();
	if (!model.get() || !model.get()->get_skel()) {
		sys_print(Error, "RagdollSetupComponent::preview_ragdoll: no model / skeleton set\n");
		return;
	}
	ensure_rig_mesh();
	MSkeleton* skel = model.get()->get_skel();

	// Pass A -- collect authored (right-side or center only) scaffolding bones. Scaffolding
	// entities are direct children of this component's OWNER entity (which itself carries the
	// rig MeshComponent -- bone-parenting resolves one level deep).
	std::unordered_map<uint64_t, AuthoredBone> authored;
	for (Entity* child : get_owner()->get_children()) {
		auto* body = child->get_component<RagdollPhysicsBodyComponent>();
		if (!body)
			continue;
		StringName bone_name = child->get_parent_bone();
		std::string lower = ragdoll_str_to_lower(bone_name.get_c_str());
		if (ragdoll_is_left_side(lower)) {
			sys_print(Error,
					  "RagdollSetupComponent: left-side scaffolding bone '%s' ignored -- author right/center bones "
					  "only, mirroring generates the left side\n",
					  bone_name.get_c_str());
			continue;
		}
		int idx = skel->get_bone_index(bone_name);
		if (idx == -1) {
			sys_print(Error, "RagdollSetupComponent: scaffolding entity bone-parented to unknown bone '%s'\n",
					  bone_name.get_c_str());
			continue;
		}
		AuthoredBone ab;
		ab.scaffold_entity = child;
		ab.body = body;
		ab.joint = child->get_component<RagdollJointComponent>();
		ab.bone_index = idx;
		authored[bone_name.get_hash()] = ab;
	}
	if (authored.empty()) {
		sys_print(Error, "RagdollSetupComponent::preview_ragdoll: no authored RagdollPhysicsBodyComponent "
						 "scaffolding found under the rig entity\n");
		return;
	}

	// Pass A.5 -- resolve each joint's implicit parent by walking up the skeleton's actual bone
	// hierarchy to the nearest ancestor bone that is also authored (same walk RagdollConfigComponent's
	// old heuristic generator used, just against hand-placed scaffolding instead of a classified[] array).
	int root_count = 0;
	for (auto& [hash, ab] : authored) {
		if (!ab.joint) {
			root_count++;
			continue;
		}
		int p = skel->get_bone_parent(ab.bone_index);
		StringName found;
		while (p >= 0) {
			StringName pname = skel->get_all_bones().at(p).name;
			if (authored.count(pname.get_hash())) {
				found = pname;
				break;
			}
			p = skel->get_bone_parent(p);
		}
		if (found.is_null()) {
			sys_print(Warning,
					  "RagdollSetupComponent: bone '%s' has a RagdollJointComponent but no authored ancestor bone "
					  "was found -- treating as an unconnected extra root\n",
					  ab.scaffold_entity->get_parent_bone().get_c_str());
			root_count++;
		}
		ab.parent_bone = found;
	}
	if (root_count != 1) {
		sys_print(Warning, "RagdollSetupComponent: expected exactly one ragdoll root (a bone with no "
						   "RagdollJointComponent), found %d\n",
				  root_count);
	}

	// Spawn a transient preview mesh + RagdollComponent to hold the simulated bodies.
	preview_mesh_entity = eng->get_level()->spawn_entity();
	Entity* pm = preview_mesh_entity.get();
	pm->dont_serialize_or_edit = true;
	pm->set_ws_transform(get_owner()->get_ws_transform());
	auto* pm_mesh = pm->create_component<MeshComponent>();
	pm_mesh->set_model(model.get());
	agBuilder bind_builder;
	auto bind_node = bind_builder.alloc<agBindPose>();
	bind_builder.set_root(bind_node);
	pm_mesh->create_animator(&bind_builder);
	auto* rag = pm->create_component<RagdollComponent>();
	preview_ragdoll_comp = rag;

	// Pass B -- mirrored spawn. bone-name-hash -> spawned free body entity, one map per side (right/center
	// as authored, and the mirrored left side for right-side bones; center bones only ever populate orig).
	std::unordered_map<uint64_t, Entity*> orig_side_map, mirror_side_map;
	auto spawn_body = [&](const AuthoredBone& ab, const std::string& side_bone_name, bool is_root) -> Entity* {
		Entity* body_entity = eng->get_level()->spawn_entity();
		body_entity->dont_serialize_or_edit = true;
		// Free/unparented entity -- RagdollComponent::add_body ASSERTs this. Copy the scaffolding
		// entity's own bone-relative local transform directly: both it and this new body are (or
		// would be) bone-parented to equivalent-side bones, and get_ls_transform() is parent-
		// independent local data, so the same offset is correct for either side.
		body_entity->set_ws_transform(ab.scaffold_entity->get_ls_transform());
		auto* capsule = body_entity->create_component<CapsuleComponent>();
		capsule->set_data(ab.body->height, ab.body->radius, ab.body->height_offset);
		capsule->set_body_type(BodyType::Dynamic);
		preview_body_entities.push_back(body_entity);
		StringName side_name(side_bone_name.c_str());
		if (is_root)
			rag->add_root_body(side_name, capsule);
		else
			rag->add_body(side_name, capsule);
		return body_entity;
	};
	for (auto& [hash, ab] : authored) {
		StringName own_bone = ab.scaffold_entity->get_parent_bone();
		std::string own_lower = ragdoll_str_to_lower(own_bone.get_c_str());
		bool is_right = ragdoll_is_right_side(own_lower);
		bool is_root = (ab.joint == nullptr);

		orig_side_map[hash] = spawn_body(ab, own_bone.get_c_str(), is_root);

		if (is_right) {
			std::string mirror_name = ragdoll_mirror_bone_name(own_bone.get_c_str());
			if (skel->get_bone_index(StringName(mirror_name.c_str())) == -1) {
				sys_print(Warning,
						  "RagdollSetupComponent: mirrored bone '%s' not found in skeleton -- skipping left side "
						  "for '%s'\n",
						  mirror_name.c_str(), own_bone.get_c_str());
			} else {
				mirror_side_map[hash] = spawn_body(ab, mirror_name, is_root);
			}
		}
	}

	// Pass C -- wire joints, now that every body (both sides) exists. A separate pass so parent
	// lookups never race spawn order.
	auto resolve_parent_body = [&](uint64_t parent_hash, bool for_mirror_side) -> Entity* {
		auto pit = authored.find(parent_hash);
		if (pit == authored.end())
			return nullptr;
		bool parent_is_right =
			ragdoll_is_right_side(ragdoll_str_to_lower(pit->second.scaffold_entity->get_parent_bone().get_c_str()));
		if (for_mirror_side && parent_is_right) {
			auto mit = mirror_side_map.find(parent_hash);
			return mit != mirror_side_map.end() ? mit->second : nullptr;
		}
		auto oit = orig_side_map.find(parent_hash);
		return oit != orig_side_map.end() ? oit->second : nullptr;
	};
	for (auto& [hash, ab] : authored) {
		if (!ab.joint || ab.parent_bone.is_null())
			continue;
		uint64_t parent_hash = ab.parent_bone.get_hash();

		auto wire_one = [&](Entity* body_entity, bool for_mirror_side) {
			if (!body_entity)
				return;
			Entity* parent_entity = resolve_parent_body(parent_hash, for_mirror_side);
			if (!parent_entity) {
				sys_print(Error, "RagdollSetupComponent: could not resolve parent body for joint on '%s'\n",
						  ab.scaffold_entity->get_parent_bone().get_c_str());
				return;
			}
			auto* joint = body_entity->create_component<AdvancedJointComponent>();
			joint->set_translate_joint_motion(JM::Locked, JM::Locked, JM::Locked);
			joint->set_rotation_joint_motion(ab.joint->ang_x_motion, ab.joint->ang_y_motion, ab.joint->ang_z_motion);
			joint->set_twist_vars(ab.joint->twist_limit_min, ab.joint->twist_limit_max, ab.joint->damping,
								   ab.joint->stiffness);
			joint->set_cone_vars(ab.joint->swing1_limit, ab.joint->swing2_limit, ab.joint->damping,
								  ab.joint->stiffness);
			joint->set_joint_anchor(glm::vec3(0.f), glm::quat(1, 0, 0, 0), 0);
			joint->set_target(parent_entity);
		};
		auto oit = orig_side_map.find(hash);
		wire_one(oit != orig_side_map.end() ? oit->second : nullptr, false);
		auto mit = mirror_side_map.find(hash);
		if (mit != mirror_side_map.end())
			wire_one(mit->second, true);
	}

	rag->enable();
}

#ifdef EDITOR_BUILD
void RagdollSetupComponent::editor_on_change_property() {
	ensure_rig_mesh();
}

void RagdollSetupComponent::on_inspector_imgui() {
	if (ImGui::Button("Preview Ragdoll"))
		preview_ragdoll();
}
#endif
