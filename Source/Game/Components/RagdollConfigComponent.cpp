#include "RagdollConfigComponent.h"
#include "RagdollComponent.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Game/Components/MeshbuilderComponent.h"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "Render/Model.h"
#include "Render/ModelManager.h"
#include "Render/MaterialPublic.h"
#include "Physics/Physics2.h"
#include "GameEnginePublic.h"
#include "Input/InputSystem.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"
#include <unordered_map>
#include <cctype>

#ifdef EDITOR_BUILD
#include "imgui.h"
#include "IEditorTool.h"
#include "Render/ViewSetup.h"
#include "UI/GUISystemPublic.h"
#endif

// Single source of truth for ragdolls: this drives the real RagdollComponent (the same one
// used for hand-authored Lua ragdolls), not a parallel physics implementation. The preview
// entity always carries a MeshComponent + a minimal one-node (bind pose) animator so the
// model is visible even before Auto-Generate/Start Simulating are ever pressed.

static std::string ragdoll_to_lower(std::string s) {
	for (auto& c : s)
		c = (char)tolower((unsigned char)c);
	return s;
}

static glm::mat4 to_mat4(const glm::mat4x3& m) {
	glm::mat4 out(1.0f);
	out[0] = glm::vec4(m[0], 0.f);
	out[1] = glm::vec4(m[1], 0.f);
	out[2] = glm::vec4(m[2], 0.f);
	out[3] = glm::vec4(m[3], 1.f);
	return out;
}

static std::shared_ptr<MaterialInstance> get_ragdoll_ghost_material() {
	// MaterialInstance is an asset-database-owned object (never deleted directly); wrap it in
	// a non-owning shared_ptr (no-op deleter) purely to satisfy ModelBuilder::begin_submesh's
	// signature.
	static std::shared_ptr<MaterialInstance> mat(MaterialInstance::load("eng/ragdollGhost.mm"),
												  [](MaterialInstance*) {});
	return mat;
}

// Builds an orthonormal (tangent, bitangent) basis perpendicular to dir, with `up_ref` used to
// pick a consistent bitangent direction (falls back to a fixed axis if parallel to dir).
static void make_basis(glm::vec3 dir, glm::vec3 up_ref, glm::vec3& tangent, glm::vec3& bitangent) {
	if (glm::length(glm::cross(up_ref, dir)) < 0.001f)
		up_ref = (glm::abs(dir.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
	tangent = glm::normalize(glm::cross(up_ref, dir));
	bitangent = glm::cross(dir, tangent);
}

static void append_capsule_solid(ModelBuilder& mb, glm::vec3 p0, glm::vec3 p1, float radius) {
	const int segs = 8;
	glm::vec3 axis = p1 - p0;
	float len = glm::length(axis);
	glm::vec3 dir = (len > 0.0001f) ? axis / len : glm::vec3(0, 1, 0);
	glm::vec3 tangent, bitangent;
	make_basis(dir, glm::vec3(0, 0, 1), tangent, bitangent);

	uint16_t ring0[segs], ring1[segs];
	for (int i = 0; i < segs; i++) {
		float t = TWOPI * i / segs;
		glm::vec3 n = tangent * cosf(t) + bitangent * sinf(t);
		ring0[i] = mb.add_vertex(p0 + n * radius, {0, 0}, n);
		ring1[i] = mb.add_vertex(p1 + n * radius, {0, 0}, n);
	}
	uint16_t capc0 = mb.add_vertex(p0, {0, 0}, -dir);
	uint16_t capc1 = mb.add_vertex(p1, {0, 0}, dir);
	for (int i = 0; i < segs; i++) {
		int ni = (i + 1) % segs;
		mb.add_quad(ring0[i], ring1[i], ring1[ni], ring0[ni]);
		mb.add_triangle(capc0, ring0[ni], ring0[i]);
		mb.add_triangle(capc1, ring1[i], ring1[ni]);
	}
}

// Swing limits near +-90 degrees (common for near-hinge joints like elbows/knees) would blow
// up a naive tan(angle)*length cone radius. Clamp the *visualized* angle and cap the resulting
// radius as a multiple of the cone length so the gizmo stays a reasonable size regardless of
// how wide the actual physics limit is.
static void cone_radii(float angle_y, float angle_z, float length, float& out_ry, float& out_rz) {
	float ay = glm::clamp(angle_y, 0.02f, 1.1f);
	float az = glm::clamp(angle_z, 0.02f, 1.1f);
	out_ry = glm::min(tanf(ay) * length, length * 2.2f);
	out_rz = glm::min(tanf(az) * length, length * 2.2f);
}

static void append_cone_solid(ModelBuilder& mb, glm::vec3 apex, glm::vec3 dir, float angle_y, float angle_z,
							   float length) {
	const int segs = 10;
	dir = glm::normalize(dir);
	glm::vec3 tangent, bitangent;
	make_basis(dir, glm::vec3(0, 0, 1), tangent, bitangent);
	float ry, rz;
	cone_radii(angle_y, angle_z, length, ry, rz);

	uint16_t apex_idx = mb.add_vertex(apex, {0, 0}, -dir);
	std::vector<uint16_t> ring(segs);
	glm::vec3 tip = apex + dir * length;
	for (int i = 0; i < segs; i++) {
		float t = TWOPI * i / segs;
		glm::vec3 p = tip + tangent * (cosf(t) * ry) + bitangent * (sinf(t) * rz);
		ring[i] = mb.add_vertex(p, {0, 0}, dir);
	}
	for (int i = 0; i < segs; i++) {
		int ni = (i + 1) % segs;
		mb.add_triangle(apex_idx, ring[i], ring[ni]);
	}
}

// Wireframe outline for the same cone: rim + 4 spokes only (kept sparse -- this is drawn for
// every joint, and a dense wireframe per joint makes the whole ragdoll unreadable).
static void append_cone_lines(MeshBuilder& mb, glm::vec3 apex, glm::vec3 dir, float angle_y, float angle_z,
							   float length, Color32 color) {
	const int segs = 12;
	dir = glm::normalize(dir);
	glm::vec3 tangent, bitangent;
	make_basis(dir, glm::vec3(0, 0, 1), tangent, bitangent);
	float ry, rz;
	cone_radii(angle_y, angle_z, length, ry, rz);
	glm::vec3 tip = apex + dir * length;

	glm::vec3 ring[segs];
	for (int i = 0; i < segs; i++) {
		float t = TWOPI * i / segs;
		ring[i] = tip + tangent * (cosf(t) * ry) + bitangent * (sinf(t) * rz);
	}
	for (int i = 0; i < segs; i++)
		mb.PushLine(ring[i], ring[(i + 1) % segs], color);
	const int spoke_step = segs / 4;
	for (int i = 0; i < segs; i += spoke_step)
		mb.PushLine(apex, ring[i], color);
}

RagdollConfigComponent::RagdollConfigComponent() {
	set_call_init_in_editor(true);

	elbow_limits.swing1_limit = 0.02f;
	elbow_limits.swing2_limit = 1.3f;
	elbow_limits.twist_min = -0.1f;
	elbow_limits.twist_max = 0.1f;

	knee_limits.swing1_limit = 0.02f;
	knee_limits.swing2_limit = 1.4f;
	knee_limits.twist_min = -0.05f;
	knee_limits.twist_max = 0.05f;

	shoulder_limits.swing1_limit = 1.2f;
	shoulder_limits.swing2_limit = 1.2f;
	shoulder_limits.twist_min = -0.8f;
	shoulder_limits.twist_max = 0.8f;

	hip_limits.swing1_limit = 1.0f;
	hip_limits.swing2_limit = 0.6f;
	hip_limits.twist_min = -0.4f;
	hip_limits.twist_max = 0.4f;

	spine_limits.swing1_limit = 0.25f;
	spine_limits.swing2_limit = 0.25f;
	spine_limits.twist_min = -0.3f;
	spine_limits.twist_max = 0.3f;

	neck_limits.swing1_limit = 0.4f;
	neck_limits.swing2_limit = 0.4f;
	neck_limits.twist_min = -0.5f;
	neck_limits.twist_max = 0.5f;

	wrist_limits.swing1_limit = 0.3f;
	wrist_limits.swing2_limit = 0.3f;
	wrist_limits.twist_min = -0.2f;
	wrist_limits.twist_max = 0.2f;

	ankle_limits.swing1_limit = 0.3f;
	ankle_limits.swing2_limit = 0.3f;
	ankle_limits.twist_min = -0.15f;
	ankle_limits.twist_max = 0.15f;
}

void RagdollConfigComponent::start() {
	if (model.get())
		ensure_preview_entity();
}

void RagdollConfigComponent::stop() {
	// Bodies are free entities (not children of preview_entity), so destroy them explicitly.
	for (auto& e : body_entities) {
		if (e.get())
			e->destroy();
	}
	body_entities.clear();
	if (preview_entity.get())
		preview_entity->destroy();
	preview_entity = obj<Entity>();
	ragdoll = obj<RagdollComponent>();
	ghost_entity = obj<Entity>();
	ghost_model.reset();
	bodies_built = false;
	simulating = false;
}

void RagdollConfigComponent::editor_start() { start(); }

#ifdef EDITOR_BUILD
void RagdollConfigComponent::editor_on_change_property() {
	// Show the model as soon as it's assigned in the property grid, not just after
	// Auto-Generate -- fires on every property change on this component, so it's also how a
	// later model swap gets picked up.
	if (!model.get())
		return;
	ensure_preview_entity();
	if (!preview_entity.get())
		return;
	auto* mesh = preview_entity->get_component<MeshComponent>();
	if (mesh && mesh->get_model() != model.get())
		mesh->set_model(model.get());
}
#endif

const JointTuning& RagdollConfigComponent::tuning_for_role(RagdollJointRole role) const {
	switch (role) {
	case RagdollJointRole::Spine: return spine_limits;
	case RagdollJointRole::Neck: return neck_limits;
	case RagdollJointRole::Shoulder: return shoulder_limits;
	case RagdollJointRole::Elbow: return elbow_limits;
	case RagdollJointRole::Wrist: return wrist_limits;
	case RagdollJointRole::Hip: return hip_limits;
	case RagdollJointRole::Knee: return knee_limits;
	case RagdollJointRole::Ankle: return ankle_limits;
	default: return spine_limits;
	}
}

void RagdollConfigComponent::ensure_preview_entity() {
	if (preview_entity.get())
		return;
	Entity* owner = get_owner();
	Entity* p = owner->create_child_entity();
	p->dont_serialize_or_edit = true;
	preview_entity = p;

	auto* mesh = p->create_component<MeshComponent>();
	mesh->set_model(model.get());

	// Minimal one-node anim graph: just evaluates the bind pose. This is the only thing
	// RagdollComponent actually needs (valid global bone matrices to snap bodies to) -- no
	// clip asset required.
	agBuilder builder;
	auto bind = builder.alloc<agBindPose>();
	builder.set_root(bind);
	mesh->create_animator(&builder);
}

void RagdollConfigComponent::generate_from_skeleton() {
	bone_configs.clear();
	bodies_built = false;

	Model* m = model.get();
	if (!m || !m->get_skel()) {
		sys_print(Error, "RagdollConfigComponent::generate_from_skeleton: no model/skeleton set\n");
		return;
	}
	ensure_preview_entity();

	MSkeleton* skel = m->get_skel();
	const auto& bones = skel->get_all_bones();
	const int n = (int)bones.size();

	struct Classified {
		RagdollLimbGroup group = RagdollLimbGroup::None;
		RagdollJointRole role = RagdollJointRole::None;
		bool included = false;
		glm::vec3 pos = glm::vec3(0.f);
		glm::quat rot = glm::quat(1, 0, 0, 0);
	};
	std::vector<Classified> classified(n);

	struct HeuristicEntry {
		const char* substr;
		RagdollLimbGroup group;
		RagdollJointRole role;
		bool sided;
	};
	static const HeuristicEntry table[] = {
		{"pelvis", RagdollLimbGroup::Torso, RagdollJointRole::None, false},
		{"hips", RagdollLimbGroup::Torso, RagdollJointRole::None, false},
		{"spine", RagdollLimbGroup::Torso, RagdollJointRole::Spine, false},
		{"neck", RagdollLimbGroup::Head, RagdollJointRole::Neck, false},
		{"head", RagdollLimbGroup::Head, RagdollJointRole::Neck, false},
		{"upperarm", RagdollLimbGroup::ArmLeft, RagdollJointRole::Shoulder, true},
		{"lowerarm", RagdollLimbGroup::ArmLeft, RagdollJointRole::Elbow, true},
		{"forearm", RagdollLimbGroup::ArmLeft, RagdollJointRole::Elbow, true},
		{"hand", RagdollLimbGroup::ArmLeft, RagdollJointRole::Wrist, true},
		{"thigh", RagdollLimbGroup::LegLeft, RagdollJointRole::Hip, true},
		{"upperleg", RagdollLimbGroup::LegLeft, RagdollJointRole::Hip, true},
		{"calf", RagdollLimbGroup::LegLeft, RagdollJointRole::Knee, true},
		{"lowerleg", RagdollLimbGroup::LegLeft, RagdollJointRole::Knee, true},
		{"shin", RagdollLimbGroup::LegLeft, RagdollJointRole::Knee, true},
		{"foot", RagdollLimbGroup::LegLeft, RagdollJointRole::Ankle, true},
	};

	auto is_right_side = [](const std::string& lower) {
		return lower.find("_r") != std::string::npos || lower.find("right") != std::string::npos;
	};
	auto is_left_side = [](const std::string& lower) {
		return lower.find("_l") != std::string::npos || lower.find("left") != std::string::npos;
	};
	auto mirror_group = [](RagdollLimbGroup g, bool right) -> RagdollLimbGroup {
		if (!right)
			return g;
		if (g == RagdollLimbGroup::ArmLeft)
			return RagdollLimbGroup::ArmRight;
		if (g == RagdollLimbGroup::LegLeft)
			return RagdollLimbGroup::LegRight;
		return g;
	};

	for (int i = 0; i < n; i++) {
		std::string lower = ragdoll_to_lower(bones[i].strname);
		for (auto& e : table) {
			if (lower.find(e.substr) != std::string::npos) {
				bool right = e.sided && is_right_side(lower) && !is_left_side(lower);
				classified[i].group = mirror_group(e.group, right);
				classified[i].role = e.role;
				classified[i].included = true;
				break;
			}
		}
		glm::mat4 m4 = to_mat4(bones[i].posematrix);
		classified[i].pos = glm::vec3(m4[3]);
		classified[i].rot = glm::quat_cast(glm::mat3(m4));
	}

	std::vector<std::vector<int>> children(n);
	for (int i = 0; i < n; i++) {
		int p = skel->get_bone_parent(i);
		if (p >= 0 && p < n)
			children[p].push_back(i);
	}

	auto find_child_pos = [&](int idx) -> glm::vec3 {
		std::vector<int> queue = children[idx];
		for (int qi = 0; qi < (int)queue.size(); qi++) {
			int c = queue[qi];
			if (classified[c].included)
				return classified[c].pos;
			for (int cc : children[c])
				queue.push_back(cc);
		}
		return classified[idx].pos + classified[idx].rot * glm::vec3(0, 1, 0) * 0.15f;
	};
	auto find_included_parent_name = [&](int idx) -> StringName {
		int p = skel->get_bone_parent(idx);
		while (p >= 0) {
			if (classified[p].included)
				return bones[p].name;
			p = skel->get_bone_parent(p);
		}
		return StringName();
	};

	for (int i = 0; i < n; i++) {
		if (!classified[i].included)
			continue;
		RagdollBoneConfig cfg;
		cfg.bone_name = bones[i].name;
		cfg.parent_bone_name = find_included_parent_name(i);
		cfg.is_root = cfg.parent_bone_name.is_null();
		cfg.group = classified[i].group;
		cfg.role = classified[i].role;

		glm::vec3 p0 = classified[i].pos;
		glm::vec3 p1 = find_child_pos(i);
		glm::vec3 seg = p1 - p0;
		float length = glm::length(seg);
		if (length < 0.02f)
			length = 0.15f;

		float radius_frac = 0.22f;
		if (cfg.group == RagdollLimbGroup::Torso)
			radius_frac = 0.35f;
		else if (cfg.group == RagdollLimbGroup::Head)
			radius_frac = 0.4f;

		cfg.capsule_height = length;
		cfg.capsule_radius = length * radius_frac * capsule_radius_scale;
		// Entity sits at the bone's own bind-pose position (p0, the proximal/joint end), not the
		// segment midpoint -- RagdollComponent::add_body/enable record and replay this position as
		// a small local-to-bone offset (matching CreateRagdollSwat's Lua bodies, which never move
		// off the origin). The segment extends distally via the capsule shape's height_offset
		// instead, so entity transform stays bone-local and joint anchors land at (0,0,0).
		cfg.local_offset_pos = p0;
		glm::vec3 dir = (length > 0.0001f) ? seg / length : glm::vec3(0, 1, 0);
		cfg.local_offset_rot = glm::rotation(glm::vec3(0, 1, 0), dir);

		bone_configs.push_back(cfg);
	}

	rebuild_ghost_visual();
}

void RagdollConfigComponent::rebuild_ghost_visual() {
	if (!preview_entity.get() || bone_configs.empty())
		return;

	ModelBuilder solid;
	solid.begin_submesh(get_ragdoll_ghost_material());

	MeshBuilder lines;
	lines.Begin();

	for (auto& b : bone_configs) {
		glm::vec3 dir = b.local_offset_rot * glm::vec3(0, 1, 0);
		glm::vec3 p0 = b.local_offset_pos;
		glm::vec3 p1 = p0 + dir * b.capsule_height;
		// solid mesh already shows the capsule shape; skip the redundant wireframe outline here
		// (drawing both makes the whole ragdoll unreadable) -- only the joint-limit cone gets a
		// line overlay, since that's the part that benefits from an explicit rim/spokes.
		append_capsule_solid(solid, p0, p1, b.capsule_radius);

		if (!b.is_root) {
			const JointTuning& t = tuning_for_role(b.role);
			float cone_len = b.capsule_height * 0.35f;
			append_cone_solid(solid, p0, -dir, t.swing1_limit, t.swing2_limit, cone_len);
			append_cone_lines(lines, p0, -dir, t.swing1_limit, t.swing2_limit, cone_len, COLOR_WHITE);
		}
	}
	lines.End();

	if (!ghost_entity.get()) {
		Entity* g = preview_entity->create_child_entity();
		g->dont_serialize_or_edit = true;
		ghost_entity = g;
	}
	Entity* g = ghost_entity.get();

	if (!ghost_model)
		ghost_model.reset(g_modelMgr.create_dynamic_model(solid, "ragdoll_ghost"));
	else
		g_modelMgr.refresh_dynamic_model(ghost_model.get(), solid);

	auto* mc = g->get_component<MeshComponent>();
	if (!mc)
		mc = g->create_component<MeshComponent>();
	mc->set_model(ghost_model.get());
	mc->set_casts_shadows(false);

#ifdef EDITOR_BUILD
	auto* mbc = g->get_component<MeshBuilderComponent>();
	if (!mbc)
		mbc = g->create_component<MeshBuilderComponent>();
	mbc->use_background_color = false;
	mbc->depth_tested = false;
	mbc->use_transform = true;
	mbc->mb = lines;
	mbc->sync_render_data();
#endif
}

void RagdollConfigComponent::rebuild_bodies() {
	ensure_preview_entity();
	Entity* p = preview_entity.get();
	if (!p || bone_configs.empty())
		return;

	if (ragdoll.get())
		ragdoll->destroy();

	// Bodies are free entities we own explicitly -- tear down the previous set here (they are NOT
	// children of preview_entity, so destroying preview_entity would not reach them).
	for (auto& e : body_entities) {
		if (e.get())
			e->destroy();
	}
	body_entities.clear();

	auto* rag = p->create_component<RagdollComponent>();
	ragdoll = rag;

	std::unordered_map<uint64_t, Entity*> bone_name_to_entity;
	for (auto& cfg : bone_configs) {
		uint8_t group_bit = (uint8_t)(1 << (uint8_t)cfg.group);
		if ((sim_group_mask & group_bit) == 0)
			continue;

		// Spawn a FREE (top-level, unparented) entity for each body. Ragdoll bodies must never be
		// parented to the mesh entity: their transform is driven by physics, and a moving parent would
		// stomp their velocity every frame (RagdollComponent::add_body ASSERTs this). We track them in
		// body_entities for cleanup.
		Entity* body_entity = eng->get_level()->spawn_entity();
		body_entity->dont_serialize_or_edit = true;
		body_entities.push_back(body_entity);
		// The body is a FREE entity, so its local transform IS its world transform. Place it at the
		// world ORIGIN with only the bone's rotation offset -- NOT preview_ws*rotation. add_body() records
		// this entity's *local* transform as bindPosePos/bindPoseRot, and enable() composes it as
		//   this_ws * bone_posematrix(bone-space -> mesh-space) * bindOffset.
		// bindPosePos must therefore be 0 (a bone-space offset), not the preview's world position; baking
		// the world position in here would double-apply it in enable() and scatter the bodies (breaking
		// every joint). This matches the Lua ragdoll bodies, which are spawned at the origin with only a
		// rotation. The segment length lives in the capsule shape's height_offset, and local_offset_pos is
		// used only for the ghost preview mesh.
		body_entity->set_ws_transform(glm::mat4_cast(cfg.local_offset_rot));

		auto* capsule = body_entity->create_component<CapsuleComponent>();
		// entity origin is the bone's own (proximal) position; extend the shape distally by half
		// its height so it spans the bone segment instead of straddling the joint.
		capsule->set_data(cfg.capsule_height, cfg.capsule_radius, cfg.capsule_height * 0.5f);
		capsule->set_is_static(false);
		capsule->set_is_simulating(true);

		bone_name_to_entity[cfg.bone_name.get_hash()] = body_entity;

		if (cfg.is_root) {
			rag->add_root_body(cfg.bone_name, capsule);
		} else {
			rag->add_body(cfg.bone_name, capsule);
			auto it = bone_name_to_entity.find(cfg.parent_bone_name.get_hash());
			if (it != bone_name_to_entity.end()) {
				auto* joint = body_entity->create_component<AdvancedJointComponent>();
				joint->set_translate_joint_motion(JM::Locked, JM::Locked, JM::Locked);
				joint->set_rotation_joint_motion(JM::Limited, JM::Limited, JM::Limited);
				const JointTuning& t = tuning_for_role(cfg.role);
				joint->set_twist_vars(t.twist_min, t.twist_max, t.damping, t.stiffness);
				joint->set_cone_vars(t.swing1_limit, t.swing2_limit, t.damping, t.stiffness);
				// entity origin already is the proximal joint point, so no anchor offset is needed.
				joint->set_joint_anchor(glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), 0);
				joint->set_target(it->second);
			}
		}
	}
	bodies_built = true;
}

void RagdollConfigComponent::start_simulate() {
	if (simulating)
		return;
	if (!bodies_built)
		rebuild_bodies();
	if (!ragdoll.get() || !bodies_built)
		return;

	// No re-parenting needed: the bodies are free entities, so the owner's transform can't drag them.
	// preview_entity stays a child of the owner; enable() reads its (correct, world-placed) transform
	// to position the bodies, and on_pre_get_bones() drives it to follow the root body while simulating.
	ragdoll->enable();
	simulating = true;

	if (ghost_entity.get()) {
		auto* mc = ghost_entity->get_component<MeshComponent>();
		if (mc)
			mc->set_is_visible(false);
	}
}

void RagdollConfigComponent::stop_simulate() {
	if (!simulating)
		return;
	if (ragdoll.get())
		ragdoll->disable();
	if (preview_entity.get()) {
		// on_pre_get_bones() drove preview_entity's transform to the root body while simulating;
		// snap it back onto the owner so the idle preview lines back up. (It stays parented to the
		// owner throughout -- only the bodies are free entities.)
		preview_entity->set_ws_transform(get_owner()->get_ws_transform());
	}
	simulating = false;
	is_dragging = false;
	drag_body = obj<PhysicsBody>();

	if (ghost_entity.get()) {
		auto* mc = ghost_entity->get_component<MeshComponent>();
		if (mc)
			mc->set_is_visible(true);
	}
}

void RagdollConfigComponent::update() {
#ifdef EDITOR_BUILD
	if (eng->is_editor_level() && simulating)
		update_drag_in_editor();
#endif
}

void RagdollConfigComponent::update_drag_in_editor() {
#ifdef EDITOR_BUILD
	if (!get_owner()->get_selected_in_editor()) {
		is_dragging = false;
		drag_body = obj<PhysicsBody>();
		return;
	}
	if (!UiSystem::inst->is_vp_hovered() && !is_dragging) {
		return;
	}

	IEditorTool* tool = eng->get_tool();
	if (!tool)
		return;
	const View_Setup* vs = tool->get_vs();
	if (!vs)
		return;

	const auto vp_rect = UiSystem::inst->get_vp_rect();
	const auto mouse = Input::get_mouse_pos() - vp_rect.get_pos();
	const auto vp_size = vp_rect.get_size();
	if (vp_size.x <= 0 || vp_size.y <= 0)
		return;

	glm::vec2 ndc = glm::vec2((mouse.x / (float)vp_size.x) * 2.f - 1.f, 1.f - (mouse.y / (float)vp_size.y) * 2.f);
	glm::mat4 inv_vp = glm::inverse(vs->viewproj);
	glm::vec4 near_pt = inv_vp * glm::vec4(ndc, -1.f, 1.f);
	glm::vec4 far_pt = inv_vp * glm::vec4(ndc, 1.f, 1.f);
	near_pt /= near_pt.w;
	far_pt /= far_pt.w;
	glm::vec3 ray_origin = glm::vec3(near_pt);
	glm::vec3 ray_dir = glm::normalize(glm::vec3(far_pt - near_pt));

	if (!is_dragging) {
		if (!Input::is_mouse_down(0))
			return;
		world_query_result res;
		if (!g_physics.trace_ray(res, ray_origin, ray_origin + ray_dir * 1000.f, nullptr, UINT32_MAX))
			return;
		if (!res.component || !res.component->is_a<CapsuleComponent>())
			return;
		// Only grab one of our own ragdoll bodies. They are free entities now, so match against
		// body_entities rather than a parent check.
		Entity* hit_owner = res.component->get_owner();
		bool is_ours = false;
		for (auto& e : body_entities) {
			if (e.get() == hit_owner) {
				is_ours = true;
				break;
			}
		}
		if (!hit_owner || !is_ours)
			return;
		is_dragging = true;
		drag_body = res.component;
		drag_depth = glm::dot(res.hit_pos - ray_origin, vs->front);
		return;
	}

	if (!Input::is_mouse_down(0)) {
		is_dragging = false;
		drag_body = obj<PhysicsBody>();
		return;
	}
	PhysicsBody* body = drag_body.get();
	if (!body) {
		is_dragging = false;
		return;
	}
	float facing = glm::dot(ray_dir, vs->front);
	if (facing < 0.0001f)
		facing = 0.0001f;
	glm::vec3 target = ray_origin + ray_dir * (drag_depth / facing);
	glm::vec3 current = body->get_ws_position();
	float dt = (float)eng->get_dt();
	if (dt < 0.0001f)
		dt = 0.0001f;
	body->set_linear_velocity((target - current) / dt);
#endif
}

#ifdef EDITOR_BUILD
class RagdollConfigEditorUi : public IComponentEditorUi
{
public:
	explicit RagdollConfigEditorUi(RagdollConfigComponent* c) : comp(c) {}
	bool draw() override {
		if (!comp->is_simulating()) {
			if (ImGui::Button("Auto-Generate"))
				comp->generate_from_skeleton();
		}

		ImGui::Separator();
		if (!comp->is_simulating()) {
			if (ImGui::Button("Start Simulating"))
				comp->start_simulate();
		} else {
			if (ImGui::Button("Stop Simulating"))
				comp->stop_simulate();
			ImGui::SameLine();
			if (ImGui::Button("Restart Simulating")) {
				comp->stop_simulate();
				comp->start_simulate();
			}
		}
		return false;
	}

private:
	RagdollConfigComponent* comp;
};

std::unique_ptr<IComponentEditorUi> RagdollConfigComponent::create_editor_ui() {
	return std::make_unique<RagdollConfigEditorUi>(this);
}
#endif
