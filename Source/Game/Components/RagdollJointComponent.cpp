#include "RagdollJointComponent.h"
#include "RagdollGizmoMesh.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Render/ModelManager.h"
#include "Render/MaterialPublic.h"
#include "Framework/Util.h"
#include "Framework/Log.h"
#include "GameEnginePublic.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"

#ifdef EDITOR_BUILD
#include "imgui.h"

namespace {
bool is_dof_open(JM m) { return m == JM::Limited || m == JM::Free; }

std::shared_ptr<MaterialInstance> get_ragdoll_joint_gizmo_material() {
	// MaterialInstance is an asset-database-owned object (never deleted directly); wrap it in a
	// non-owning shared_ptr (no-op deleter) purely to satisfy ModelBuilder::begin_submesh's signature.
	static std::shared_ptr<MaterialInstance> mat(MaterialInstance::load("eng/ragdollJointGizmo.mm"),
												  [](MaterialInstance*) {});
	return mat;
}

constexpr float GIZMO_LENGTH = 0.15f;
constexpr float GIZMO_TWIST_RADIUS = 0.06f;
constexpr float GIZMO_THICKNESS = 0.01f;
constexpr float PREVIEW_PING_PONG_SPEED = 1.5f;  // rad/s (in the sin() phase, not the swept angle)
constexpr float PREVIEW_RIM_SWEEP_SPEED = 1.2f;  // rad/s around the cone rim

StringName preview_rot_var() { return StringName("vRagdollJointPreviewRot"); }
} // namespace

void RagdollJointComponent::editor_start() {
	rebuild_gizmo_mesh();
}

void RagdollJointComponent::stop() {
	// Destroy the gizmo entities (and their MeshComponents) FIRST so the renderer drops their
	// proxies through the normal entity-teardown path, THEN free the underlying dynamic Models --
	// never the other way around (a freed-but-still-referenced Model crashes the next scene draw;
	// see the identical ordering in the old RagdollConfigComponent::stop()/ghost_model teardown).
	if (gizmo_entity.get())
		gizmo_entity->destroy();
	gizmo_entity = obj<Entity>();
	swing_model.reset();
	twist_model.reset();
}

void RagdollJointComponent::editor_on_change_property() {
	rebuild_gizmo_mesh();
}

void RagdollJointComponent::on_inspector_imgui() {
	bool changed = false;
	changed |= ImGui::DragFloat("Twist Min", &twist_limit_min, 0.01f, -3.14f, 3.14f);
	changed |= ImGui::DragFloat("Twist Max", &twist_limit_max, 0.01f, -3.14f, 3.14f);
	changed |= ImGui::DragFloat("Swing1 Limit (Y)", &swing1_limit, 0.01f, 0.f, 3.14f);
	changed |= ImGui::DragFloat("Swing2 Limit (Z)", &swing2_limit, 0.01f, 0.f, 3.14f);
	changed |= ImGui::DragFloat("Damping", &damping, 0.01f, 0.f, 100.f);
	changed |= ImGui::DragFloat("Stiffness", &stiffness, 0.01f, 0.f, 100.f);
	if (changed)
		rebuild_gizmo_mesh();

	ImGui::Separator();
	const bool is_twist_open = is_dof_open(ang_x_motion);
	if (!is_twist_open)
		ImGui::BeginDisabled();
	if (preview_mode == PreviewMode::Twist) {
		if (ImGui::Button("Stop Preview Twist"))
			stop_preview();
	} else if (ImGui::Button("Preview Twist"))
		start_preview_twist();
	if (!is_twist_open)
		ImGui::EndDisabled();

	const bool is_swing_open = is_dof_open(ang_y_motion) || is_dof_open(ang_z_motion);
	if (!is_swing_open)
		ImGui::BeginDisabled();
	if (preview_mode == PreviewMode::Swing) {
		if (ImGui::Button("Stop Preview Swing"))
			stop_preview();
	} else if (ImGui::Button("Preview Swing"))
		start_preview_swing();
	if (!is_swing_open)
		ImGui::EndDisabled();
}

void RagdollJointComponent::rebuild_gizmo_mesh() {
	Entity* owner = get_owner();
	if (!owner)
		return;
	if (!gizmo_entity.get()) {
		Entity* g = owner->create_child_entity();
		g->dont_serialize_or_edit = true;
		gizmo_entity = g;
	}
	Entity* g = gizmo_entity.get();

	// Local-axis convention: +X = twist axis, Y/Z = swing1/swing2 (matches AdvancedJointComponent
	// with an identity joint anchor).
	const glm::vec3 twist_axis(1.f, 0.f, 0.f);

	const bool swing_2dof = is_dof_open(ang_y_motion) && is_dof_open(ang_z_motion);
	const bool swing_1dof_y = is_dof_open(ang_y_motion) && !is_dof_open(ang_z_motion);
	const bool swing_1dof_z = is_dof_open(ang_z_motion) && !is_dof_open(ang_y_motion);

	ModelBuilder swing_mb;
	bool has_swing = false;
	if (swing_2dof) {
		swing_mb.begin_submesh(get_ragdoll_joint_gizmo_material());
		ragdoll_append_cone_solid(swing_mb, glm::vec3(0.f), twist_axis, swing1_limit, swing2_limit, GIZMO_LENGTH);
		has_swing = true;
	} else if (swing_1dof_y || swing_1dof_z) {
		// Single-axis swing (hinge): the LOCKED swing axis is the hinge's rotation axis; the
		// sweep angle comes from the OTHER (open) axis's limit. Bone rests along +X.
		swing_mb.begin_submesh(get_ragdoll_joint_gizmo_material());
		glm::vec3 hinge_axis = swing_1dof_y ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
		float limit = swing_1dof_y ? swing1_limit : swing2_limit;
		ragdoll_append_wedge_solid(swing_mb, glm::vec3(0.f), hinge_axis, twist_axis, -limit, limit, GIZMO_LENGTH,
								   GIZMO_THICKNESS);
		has_swing = true;
	}
	ModelBuilder twist_mb;
	bool has_twist = is_dof_open(ang_x_motion);
	if (has_twist) {
		twist_mb.begin_submesh(get_ragdoll_joint_gizmo_material());
		float min_rad = (ang_x_motion == JM::Free) ? 0.f : twist_limit_min;
		float max_rad = (ang_x_motion == JM::Free) ? TWOPI : twist_limit_max;
		ragdoll_append_wedge_solid(twist_mb, glm::vec3(0.f), twist_axis, glm::vec3(0, 1, 0), min_rad, max_rad,
								   GIZMO_TWIST_RADIUS, GIZMO_THICKNESS);
	}

	// Both gizmo pieces render through submeshes of a single MeshComponent isn't possible across
	// two separate dynamic Models, so use two child mesh entities under the gizmo entity. When a
	// DOF closes, destroy its child mesh entity FIRST (dropping the renderer's proxy through the
	// normal entity-teardown path) and only THEN free the now-unreferenced dynamic Model --
	// freeing it first leaves a dangling Model* live in a render proxy for the rest of the frame
	// and crashes the next scene draw (same ordering as RagdollConfigComponent's ghost teardown).
	auto ensure_child_mesh = [&](const char* name, Model* model) -> void {
		Entity* child = nullptr;
		for (Entity* c : g->get_children()) {
			if (c->get_editor_name() == name) {
				child = c;
				break;
			}
		}
		if (!model) {
			if (child)
				child->destroy();
			return;
		}
		if (!child) {
			child = g->create_child_entity();
			child->dont_serialize_or_edit = true;
			child->set_editor_name(name);
		}
		auto* mc = child->get_component<MeshComponent>();
		if (!mc)
			mc = child->create_component<MeshComponent>();
		mc->set_model(model);
		mc->set_casts_shadows(false);
	};

	if (has_swing) {
		if (!swing_model)
			swing_model.reset(g_modelMgr.create_dynamic_model(swing_mb, "ragdoll_joint_swing_gizmo"));
		else
			g_modelMgr.refresh_dynamic_model(swing_model.get(), swing_mb);
		ensure_child_mesh("swing_gizmo", swing_model.get());
	} else {
		ensure_child_mesh("swing_gizmo", nullptr);
		swing_model.reset();
	}

	if (has_twist) {
		if (!twist_model)
			twist_model.reset(g_modelMgr.create_dynamic_model(twist_mb, "ragdoll_joint_twist_gizmo"));
		else
			g_modelMgr.refresh_dynamic_model(twist_model.get(), twist_mb);
		ensure_child_mesh("twist_gizmo", twist_model.get());
	} else {
		ensure_child_mesh("twist_gizmo", nullptr);
		twist_model.reset();
	}
}

MeshComponent* RagdollJointComponent::get_rig_mesh() const {
	Entity* owner = get_owner();
	if (!owner || !owner->get_parent())
		return nullptr;
	return owner->get_parent()->get_component<MeshComponent>();
}

static void start_preview_common(RagdollJointComponent* self, MeshComponent* mesh) {
	agBuilder builder;
	auto bind = builder.alloc<agBindPose>();
	auto mb = builder.alloc<agModifyBone>();
	mb->input = bind;
	mb->boneName = self->get_owner()->get_parent_bone();
	mb->rotation = ModifyBoneType::Bonespace;
	mb->rotationVal = preview_rot_var();
	mb->alpha = 1.f;
	builder.set_root(mb);
	mesh->create_animator(&builder);
	// Seed a default value immediately -- the graph can evaluate (AnimatorObject::update) before
	// this component's own update() first runs and sets a real value, and an unset variable
	// throws inside agGetPoseCtx::get_quat_var (see AnimGraphTester.cpp's identical seeding for
	// the same reason).
	mesh->get_animator()->set_quat_variable(preview_rot_var(), glm::quat(1.f, 0.f, 0.f, 0.f));
}

void RagdollJointComponent::start_preview_twist() {
	MeshComponent* mesh = get_rig_mesh();
	if (!mesh || !mesh->get_animator()) {
		sys_print(Error, "RagdollJointComponent::start_preview_twist: no rig MeshComponent/animator found "
						 "(is this scaffolding entity a direct child of the rig entity?)\n");
		return;
	}
	if (s_previewing_joint && s_previewing_joint != this)
		s_previewing_joint->stop_preview();
	start_preview_common(this, mesh);
	preview_mode = PreviewMode::Twist;
	preview_t = 0.f;
	s_previewing_joint = this;
	set_ticking(true); // update() only fires while ticking is enabled
}

void RagdollJointComponent::start_preview_swing() {
	MeshComponent* mesh = get_rig_mesh();
	if (!mesh || !mesh->get_animator()) {
		sys_print(Error, "RagdollJointComponent::start_preview_swing: no rig MeshComponent/animator found "
						 "(is this scaffolding entity a direct child of the rig entity?)\n");
		return;
	}
	if (s_previewing_joint && s_previewing_joint != this)
		s_previewing_joint->stop_preview();
	start_preview_common(this, mesh);
	preview_mode = PreviewMode::Swing;
	preview_t = 0.f;
	s_previewing_joint = this;
	set_ticking(true); // update() only fires while ticking is enabled
}

void RagdollJointComponent::stop_preview() {
	if (preview_mode != PreviewMode::None) {
		MeshComponent* mesh = get_rig_mesh();
		if (mesh) {
			agBuilder builder;
			auto bind = builder.alloc<agBindPose>();
			builder.set_root(bind);
			mesh->create_animator(&builder);
		}
	}
	preview_mode = PreviewMode::None;
	if (s_previewing_joint == this)
		s_previewing_joint = nullptr;
	set_ticking(false);
}

glm::quat RagdollJointComponent::compute_preview_rotation() const {
	const glm::vec3 twist_axis(1.f, 0.f, 0.f);
	if (preview_mode == PreviewMode::Twist) {
		float phase = 0.5f + 0.5f * sinf(preview_t * PREVIEW_PING_PONG_SPEED);
		float angle = twist_limit_min + (twist_limit_max - twist_limit_min) * phase;
		return glm::angleAxis(angle, twist_axis);
	}
	if (preview_mode == PreviewMode::Swing) {
		const bool swing_2dof = is_dof_open(ang_y_motion) && is_dof_open(ang_z_motion);
		if (swing_2dof) {
			// Rotate AROUND the limits: continuously sweep the azimuth at the cone's limit
			// boundary, tracing the rim of the allowed cone rather than ping-ponging.
			glm::vec3 tangent, bitangent;
			ragdoll_make_basis(twist_axis, glm::vec3(0, 0, 1), tangent, bitangent);
			float ry, rz;
			ragdoll_cone_radii(swing1_limit, swing2_limit, GIZMO_LENGTH, ry, rz);
			float theta = preview_t * PREVIEW_RIM_SWEEP_SPEED;
			glm::vec3 rim_dir = glm::normalize(twist_axis * GIZMO_LENGTH + tangent * (cosf(theta) * ry) +
												bitangent * (sinf(theta) * rz));
			return glm::rotation(twist_axis, rim_dir);
		}
		// Single-axis swing (hinge): ping-pong between the open axis's limit extremes, same as twist.
		const bool swing_1dof_y = is_dof_open(ang_y_motion);
		glm::vec3 hinge_axis = swing_1dof_y ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
		float limit = swing_1dof_y ? swing1_limit : swing2_limit;
		float phase = 0.5f + 0.5f * sinf(preview_t * PREVIEW_PING_PONG_SPEED);
		float angle = -limit + (2.f * limit) * phase;
		return glm::angleAxis(angle, hinge_axis);
	}
	return glm::quat(1, 0, 0, 0);
}

RagdollJointComponent* RagdollJointComponent::s_previewing_joint = nullptr;

#endif

void RagdollJointComponent::update() {
#ifdef EDITOR_BUILD
	if (preview_mode == PreviewMode::None)
		return;
	preview_t += eng->get_dt();
	MeshComponent* mesh = get_rig_mesh();
	if (mesh && mesh->get_animator())
		mesh->get_animator()->set_quat_variable(preview_rot_var(), compute_preview_rotation());
#endif
}
