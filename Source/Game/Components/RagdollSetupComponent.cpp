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
#include "Framework/MathLib.h"
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

// Mirrors a WORLD/mesh-space rotation by reflecting each of its 3 local axis directions
// individually across the mirror plane (normal n), then restoring a proper right-handed
// orientation by re-negating whichever axis was most aligned with n (the limb's own
// twist/long axis, for a typical bone). This is deliberately NOT the same as conjugating the
// whole matrix by the reflection (reflect*R*reflect): that form is correct for mirroring a
// standalone rigid body's pose, but it silently negates the rotation ANGLE for any DOF whose
// swing axis is perpendicular to the mirror normal -- exactly the case for a limb's bend axes
// -- which is why joints mirrored that way visibly bent the wrong way (e.g. a right elbow
// flexing forward mirrored to a left elbow flexing backward) and fought PhysX's constraint
// solver (the "spazzing"): the mirrored anchor/target frames came out skewed off the joint's
// own local X/Y/Z convention instead of staying axis-aligned. Reflecting axes individually and
// only fixing up the twist-like axis keeps that axis's own rotation sense intact and keeps the
// other two axes cleanly axis-aligned, matching what "mirror image" actually looks like.
glm::mat3 ragdoll_mirror_rotation(const glm::mat3& rot, const glm::vec3& n) {
	glm::mat3 reflect3 = glm::mat3(1.f) - 2.f * glm::outerProduct(n, n);
	glm::vec3 col0 = reflect3 * rot[0];
	glm::vec3 col1 = reflect3 * rot[1];
	glm::vec3 col2 = reflect3 * rot[2];
	float d0 = fabsf(glm::dot(rot[0], n));
	float d1 = fabsf(glm::dot(rot[1], n));
	float d2 = fabsf(glm::dot(rot[2], n));
	if (d0 >= d1 && d0 >= d2)
		col0 = -col0;
	else if (d1 >= d2)
		col1 = -col1;
	else
		col2 = -col2;
	return glm::mat3(col0, col1, col2);
}

// Mirrors a bone-local offset (a position+rotation expressed relative to bone R's own bind
// frame -- e.g. a scaffolding entity's get_ls_transform(), or a joint anchor relative to its
// body) onto bone L's bind frame. Reflects through the actual mirror plane of THIS skeleton --
// the perpendicular bisector of the R/L bones' bind-pose mesh-space positions -- rather than
// assuming a fixed world axis (e.g. "X is left/right"), which doesn't hold for every
// skeleton/export convention and was the reason plain reuse of the right-side offset on the
// left side never lined up right. Position mirrors by plain point-reflection (unambiguous);
// rotation mirrors via ragdoll_mirror_rotation (see its comment for why that's not the same as
// reflecting the position).
glm::mat4 ragdoll_mirror_bone_offset(const glm::mat4& ls_offset, const glm::mat4& pose_r, const glm::mat4& pose_l) {
	glm::vec3 pr(pose_r[3]);
	glm::vec3 pl(pose_l[3]);
	glm::vec3 n = pr - pl;
	float len = glm::length(n);
	if (len < 1e-6f)
		return ls_offset; // R/L bones coincide in bind pose -- nothing sane to mirror against
	n /= len;
	glm::vec3 mid = (pr + pl) * 0.5f;

	glm::mat4 world_r = pose_r * ls_offset;
	glm::vec3 world_r_pos(world_r[3]);
	glm::vec3 world_l_pos = world_r_pos - 2.f * glm::dot(world_r_pos - mid, n) * n;
	glm::mat3 world_l_rot = ragdoll_mirror_rotation(glm::mat3(world_r), n);

	glm::mat4 world_l(world_l_rot);
	world_l[3] = glm::vec4(world_l_pos, 1.f);
	return glm::inverse(pose_l) * world_l;
}
} // namespace

Entity* RagdollSetupComponent::build_ragdoll(const glm::mat4& transform, bool enabled, std::vector<obj<Entity>>* out_spawned_bodies) const {
	CPU_FUNCTION();
	if (!model.get() || !model.get()->get_skel()) {
		sys_print(Error, "RagdollSetupComponent::create_ragdoll_entity: no model / skeleton set\n");
		return nullptr;
	}
	MSkeleton* skel = model.get()->get_skel();

	// Pass A -- collect authored (right-side or center only) scaffolding bones. Scaffolding
	// entities are direct children of this component's OWNER entity (which itself carries the
	// rig MeshComponent -- bone-parenting resolves one level deep). RagdollPhysicsBodyComponent
	// and RagdollJointComponent are NOT required to be on the same entity -- a joint may be
	// authored on a separate sibling entity that's bone-parented to the same bone as the body, so
	// resolve joints by bone name across ALL children first, not just the body's own entity.
	std::unordered_map<uint64_t, RagdollJointComponent*> joints_by_bone;
	for (Entity* child : get_owner()->get_children()) {
		if (auto* joint = child->get_component<RagdollJointComponent>())
			joints_by_bone[child->get_parent_bone().get_hash()] = joint;
	}

	std::unordered_map<uint64_t, AuthoredBone> authored;
	for (Entity* child : get_owner()->get_children()) {
		auto* body = child->get_component<RagdollPhysicsBodyComponent>();
		if (!body)
			continue;
		StringName bone_name = child->get_parent_bone();
		std::string lower = ragdoll_str_to_lower(bone_name.get_c_str());
		if (!allow_left_side_authoring && ragdoll_is_left_side(lower)) {
			sys_print(Error,
					  "RagdollSetupComponent: left-side scaffolding bone '%s' ignored -- author right/center bones "
					  "only, mirroring generates the left side (set allow_left_side_authoring to author it by hand)\n",
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
		auto jit = joints_by_bone.find(bone_name.get_hash());
		ab.joint = (jit != joints_by_bone.end()) ? jit->second : nullptr;
		ab.bone_index = idx;
		authored[bone_name.get_hash()] = ab;
	}
	if (authored.empty()) {
		sys_print(Error, "RagdollSetupComponent::create_ragdoll_entity: no authored RagdollPhysicsBodyComponent "
						 "scaffolding found under the rig entity\n");
		return nullptr;
	}

	// Pass A.6 -- prune any bone whose own joint is disabled (RagdollJointComponent::enabled ==
	// false), or that descends from one, by walking each bone's ancestors through the skeleton's
	// actual bone hierarchy (same walk Pass A.5 uses below) and dropping it from `authored` if
	// any authored bone along that path -- including itself -- has a disabled joint. This is what
	// makes disabling a joint cut off that whole limb/chain instead of just leaving a dangling,
	// unconnected body: descendants would otherwise still spawn as free-falling bodies with no
	// path back to the rest of the ragdoll.
	for (auto it = authored.begin(); it != authored.end();) {
		bool pruned = false;
		for (int idx = it->second.bone_index; idx >= 0; idx = skel->get_bone_parent(idx)) {
			StringName bname = skel->get_all_bones().at(idx).name;
			auto ait = authored.find(bname.get_hash());
			if (ait != authored.end() && ait->second.joint && !ait->second.joint->enabled) {
				pruned = true;
				break;
			}
		}
		if (pruned) {
			sys_print(Info,
					  "RagdollSetupComponent: bone '%s' pruned from preview -- a disabled joint on it or an "
					  "ancestor bone\n",
					  it->second.scaffold_entity->get_parent_bone().get_c_str());
			it = authored.erase(it);
		} else {
			++it;
		}
	}
	if (authored.empty()) {
		sys_print(Error, "RagdollSetupComponent::create_ragdoll_entity: every authored bone was pruned by a disabled "
						 "joint\n");
		return nullptr;
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

	// Single-joint isolation debugging: a bone with a RagdollJointComponent but no authored
	// ancestor (the "unconnected extra root" case just above) normally ends up as a free-falling
	// dynamic body with no joint wired at all -- there's nothing authored to connect it to. If it's
	// the ONLY joint in the whole scaffolding, that's really someone testing one joint in isolation
	// (e.g. a single head bone carrying both the body and the joint), so instead pin that joint
	// rigidly to its own skeleton bind-pose position rather than leaving it unwired.
	int authored_joint_count = 0;
	int unconnected_joint_count = 0;
	uint64_t pinned_bone_hash = 0;
	for (auto& [hash, ab] : authored) {
		if (!ab.joint)
			continue;
		authored_joint_count++;
		if (ab.parent_bone.is_null()) {
			unconnected_joint_count++;
			pinned_bone_hash = hash;
		}
	}
	bool pin_root_to_skeleton = (authored_joint_count == 1 && unconnected_joint_count == 1);
	if (pin_root_to_skeleton)
		sys_print(Info, "RagdollSetupComponent: single-joint scaffolding detected -- pinning its joint rigidly to "
						"its skeleton bind-pose position instead of leaving it unconnected\n");

	// Spawn the mesh + RagdollComponent entity that will hold the simulated bodies.
	Entity* pm = eng->get_level()->spawn_entity();
	pm->dont_serialize_or_edit = true;
	pm->set_ws_transform(transform);
	auto* pm_mesh = pm->create_component<MeshComponent>();
	pm_mesh->set_model(model.get());
	agBuilder bind_builder;
	auto bind_node = bind_builder.alloc<agBindPose>();
	bind_builder.set_root(bind_node);
	pm_mesh->create_animator(&bind_builder);
	auto* rag = pm->create_component<RagdollComponent>();

	// Pass B -- mirrored spawn. bone-name-hash -> spawned free body entity, one map per side (right/center
	// as authored, and the mirrored left side for right-side bones; center bones only ever populate orig).
	// Looks up the mirror bone's index and returns both bones' bind-pose (bone->mesh space)
	// matrices, or false if `own_bone` has no side suffix or its mirror isn't in this skeleton.
	auto find_mirror_bone_poses = [&](int own_bone_index, StringName own_bone, glm::mat4& out_pose_r,
									   glm::mat4& out_pose_l) -> bool {
		int mirror_bone_index = skel->get_bone_index(StringName(ragdoll_mirror_bone_name(own_bone.get_c_str()).c_str()));
		if (mirror_bone_index == -1)
			return false;
		out_pose_r = (glm::mat4)skel->get_all_bones().at(own_bone_index).posematrix;
		out_pose_l = (glm::mat4)skel->get_all_bones().at(mirror_bone_index).posematrix;
		return true;
	};

	std::unordered_map<uint64_t, Entity*> orig_side_map, mirror_side_map;
	auto spawn_body = [&](const AuthoredBone& ab, const std::string& side_bone_name, const glm::mat4& ls_offset,
						   bool is_root) -> Entity* {
		Entity* body_entity = eng->get_level()->spawn_entity();
		body_entity->dont_serialize_or_edit = true;
		// Free/unparented entity -- RagdollComponent::add_body ASSERTs this. `ls_offset` is the
		// bone-relative local transform to use -- the scaffolding entity's own get_ls_transform()
		// as-authored for the original side, or that offset mirrored onto the opposite bone's
		// bind frame for the mirror side (see ragdoll_mirror_bone_offset).
		body_entity->set_ws_transform(ls_offset);
		auto* capsule = body_entity->create_component<CapsuleComponent>();
		capsule->set_data(ab.body->height, ab.body->radius, ab.body->height_offset);
		capsule->set_body_type(BodyType::Dynamic);
		if (out_spawned_bodies)
			out_spawned_bodies->push_back(body_entity);
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

		orig_side_map[hash] = spawn_body(ab, own_bone.get_c_str(), ab.scaffold_entity->get_ls_transform(), is_root);

		if (is_right && mirror_bodies_in_preview) {
			std::string mirror_name = ragdoll_mirror_bone_name(own_bone.get_c_str());
			if (allow_left_side_authoring && authored.count(StringName(mirror_name.c_str()).get_hash())) {
				// Hand-authored left-side bone already claims this bone name -- let its own pass
				// through `authored` spawn it as-authored instead of overwriting it with an
				// auto-mirrored body.
				continue;
			}
			glm::mat4 pose_r, pose_l;
			if (!find_mirror_bone_poses(ab.bone_index, own_bone, pose_r, pose_l)) {
				sys_print(Warning,
						  "RagdollSetupComponent: mirrored bone '%s' not found in skeleton -- skipping left side "
						  "for '%s'\n",
						  mirror_name.c_str(), own_bone.get_c_str());
			} else {
				glm::mat4 mirrored_ls =
					ragdoll_mirror_bone_offset(ab.scaffold_entity->get_ls_transform(), pose_r, pose_l);
				mirror_side_map[hash] = spawn_body(ab, mirror_name, mirrored_ls, is_root);
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
		bool is_pinned = pin_root_to_skeleton && hash == pinned_bone_hash;
		if (!ab.joint || (ab.parent_bone.is_null() && !is_pinned))
			continue;
		uint64_t parent_hash = is_pinned ? 0 : ab.parent_bone.get_hash();

		glm::quat body_rot = ab.scaffold_entity->get_ls_rotation();
		glm::quat anchor_q = glm::inverse(body_rot);
		glm::vec3 anchor_p = glm::inverse(body_rot) *
							  (ab.joint->get_owner()->get_ls_position() - ab.scaffold_entity->get_ls_position());
		glm::quat joint_rot = ab.joint->get_owner()->get_ls_rotation();

		// Mirrored variants of the three quantities above, for the mirror-side body's own joint
		// (only computed when this bone actually spawned one). Same derivation as anchor_q/
		// anchor_p/joint_rot, just fed the body/joint local frames reflected onto the mirror
		// bone (ragdoll_mirror_bone_offset) instead of the as-authored right-side ones -- reusing
		// the right side's numbers verbatim here was the actual mirroring bug: it happened to
		// work only when a bone's bind pose was numerically symmetric about the origin.
		glm::quat anchor_q_mirror = anchor_q;
		glm::vec3 anchor_p_mirror = anchor_p;
		glm::quat joint_rot_mirror = joint_rot;
		if (mirror_side_map.count(hash)) {
			StringName own_bone = ab.scaffold_entity->get_parent_bone();
			glm::mat4 pose_r, pose_l;
			if (find_mirror_bone_poses(ab.bone_index, own_bone, pose_r, pose_l)) {
				glm::mat4 body_ls = compose_transform(ab.scaffold_entity->get_ls_position(), body_rot, glm::vec3(1.f));
				glm::mat4 joint_ls = compose_transform(ab.joint->get_owner()->get_ls_position(),
														ab.joint->get_owner()->get_ls_rotation(), glm::vec3(1.f));
				glm::mat4 mirrored_body_ls = ragdoll_mirror_bone_offset(body_ls, pose_r, pose_l);
				glm::mat4 mirrored_joint_ls = ragdoll_mirror_bone_offset(joint_ls, pose_r, pose_l);
				glm::vec3 mbody_p, mjoint_p, scale_unused;
				glm::quat mbody_rot;
				decompose_transform(mirrored_body_ls, mbody_p, mbody_rot, scale_unused);
				decompose_transform(mirrored_joint_ls, mjoint_p, joint_rot_mirror, scale_unused);
				anchor_q_mirror = glm::inverse(mbody_rot);
				anchor_p_mirror = glm::inverse(mbody_rot) * (mjoint_p - mbody_p);
			}
		}

		auto wire_one = [&](Entity* body_entity, bool for_mirror_side) {
			if (!body_entity)
				return;
			Entity* parent_entity;
			if (is_pinned) {
				// Bare anchor entity, deliberately with no PhysicsBody and never added to `rag` --
				// its transform is never touched again after this. PhysicsJointComponent::init_joint
				// treats a target with no PhysicsBody as "anchor to world frame" (PhysicsJoints.cpp,
				// make_joint_shared's b==nullptr branch), so the joint locks rigidly to this position
				// instead of connecting to another authored ancestor body.
				Entity* anchor = eng->get_level()->spawn_entity();
				anchor->dont_serialize_or_edit = true;
				anchor->set_ws_transform(body_entity->get_ws_transform());
				if (out_spawned_bodies)
					out_spawned_bodies->push_back(anchor);
				parent_entity = anchor;
			} else {
				parent_entity = resolve_parent_body(parent_hash, for_mirror_side);
			}
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
			if (for_mirror_side) {
				joint->set_joint_anchor(anchor_p_mirror, anchor_q_mirror, 0);
				joint->set_target_anchor(glm::vec3(0.f), joint_rot_mirror);
			} else {
				joint->set_joint_anchor(anchor_p, anchor_q, 0);
				joint->set_target_anchor(glm::vec3(0.f), joint_rot);
			}
			joint->set_target(parent_entity);
		};
		auto oit = orig_side_map.find(hash);
		wire_one(oit != orig_side_map.end() ? oit->second : nullptr, false);
		auto mit = mirror_side_map.find(hash);
		if (mit != mirror_side_map.end())
			wire_one(mit->second, true);
	}
	if (enabled)
		rag->enable();
	return pm;
}

Entity* RagdollSetupComponent::create_ragdoll_entity(const glm::mat4& transform, bool create_enabled) const {
	auto* entity = build_ragdoll(transform, create_enabled, nullptr);
	return entity;
}

void RagdollSetupComponent::preview_ragdoll() {
	teardown_preview();
	ensure_rig_mesh();
	preview_mesh_entity = build_ragdoll(get_owner()->get_ws_transform(), true, &preview_body_entities);
	if (preview_mesh_entity.get())
		preview_ragdoll_comp = preview_mesh_entity->get_component<RagdollComponent>();
}

void RagdollSetupComponent::end_preview() {
	teardown_preview();
}

#ifdef EDITOR_BUILD
void RagdollSetupComponent::editor_on_change_property() {
	ensure_rig_mesh();
}

void RagdollSetupComponent::on_inspector_imgui() {
	if (!is_previewing()) {
		if (ImGui::Button("Start Preview"))
			preview_ragdoll();
	} else {
		if (ImGui::Button("End Preview"))
			end_preview();
		ImGui::SameLine();
		if (ImGui::Button("Reset Preview"))
			preview_ragdoll();
	}

	if (ImGui::Button("Enable All Joints"))
		set_all_joints_enabled(true);
	ImGui::SameLine();
	if (ImGui::Button("Disable All Joints"))
		set_all_joints_enabled(false);
}

void RagdollSetupComponent::set_all_joints_enabled(bool enabled) {
	for (Entity* child : get_owner()->get_children()) {
		if (auto* joint = child->get_component<RagdollJointComponent>())
			joint->enabled = enabled;
	}
}
#endif
