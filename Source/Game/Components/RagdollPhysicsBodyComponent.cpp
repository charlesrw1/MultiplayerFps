#include "RagdollPhysicsBodyComponent.h"
#include "RagdollGizmoMesh.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Render/ModelManager.h"
#include "Render/MaterialPublic.h"
#include "Framework/Util.h"
#include <vector>
#include <cmath>

#ifdef EDITOR_BUILD
#include "imgui.h"

namespace {
std::shared_ptr<MaterialInstance> get_ragdoll_body_gizmo_material() {
	// MaterialInstance is an asset-database-owned object (never deleted directly); wrap it in a
	// non-owning shared_ptr (no-op deleter) purely to satisfy ModelBuilder::begin_submesh's signature.
	static std::shared_ptr<MaterialInstance> mat(MaterialInstance::load("eng/ragdollGhost.mm"),
												  [](MaterialInstance*) {});
	return mat;
}

// Solid capsule for the gizmo -- doesn't need to be geometrically exact, just readable.
void ragdoll_body_append_capsule_solid(ModelBuilder& mb, glm::vec3 p0, glm::vec3 p1, float radius) {
	const int segs = 10;
	glm::vec3 axis = p1 - p0;
	float len = glm::length(axis);
	glm::vec3 dir = (len > 0.0001f) ? axis / len : glm::vec3(0, 1, 0);
	glm::vec3 tangent, bitangent;
	ragdoll_make_basis(dir, glm::vec3(0, 0, 1), tangent, bitangent);

	std::vector<uint16_t> ring0(segs), ring1(segs);
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
} // namespace

void RagdollPhysicsBodyComponent::editor_start() {
	rebuild_gizmo_mesh();
}

void RagdollPhysicsBodyComponent::stop() {
	// Destroy the gizmo entity (and its MeshComponent) FIRST so the renderer drops its proxy
	// through the normal entity-teardown path, THEN free the underlying dynamic Model -- never
	// the other way around (a freed-but-still-referenced Model crashes the next scene draw).
	if (gizmo_entity.get())
		gizmo_entity->destroy();
	gizmo_entity = obj<Entity>();
	gizmo_model.reset();
}

void RagdollPhysicsBodyComponent::editor_on_change_property() {
	rebuild_gizmo_mesh();
}

void RagdollPhysicsBodyComponent::on_inspector_imgui() {
	bool changed = false;
	changed |= ImGui::DragFloat("Height", &height, 0.01f, 0.01f, 5.f);
	changed |= ImGui::DragFloat("Radius", &radius, 0.005f, 0.01f, 2.f);
	changed |= ImGui::DragFloat("Height Offset", &height_offset, 0.01f, -5.f, 5.f);
	if (changed)
		rebuild_gizmo_mesh();
}

void RagdollPhysicsBodyComponent::rebuild_gizmo_mesh() {
	Entity* owner = get_owner();
	if (!owner)
		return;
	if (!gizmo_entity.get()) {
		Entity* g = owner->create_child_entity();
		g->dont_serialize_or_edit = true;
		gizmo_entity = g;
	}

	ModelBuilder mb;
	mb.begin_submesh(get_ragdoll_body_gizmo_material());
	glm::vec3 p0(0.f, height_offset, 0.f);
	glm::vec3 p1 = p0 + glm::vec3(0.f, height, 0.f);
	ragdoll_body_append_capsule_solid(mb, p0, p1, radius);

	if (!gizmo_model)
		gizmo_model.reset(g_modelMgr.create_dynamic_model(mb, "ragdoll_body_gizmo"));
	else
		g_modelMgr.refresh_dynamic_model(gizmo_model.get(), mb);

	Entity* g = gizmo_entity.get();
	auto* mc = g->get_component<MeshComponent>();
	if (!mc)
		mc = g->create_component<MeshComponent>();
	mc->set_model(gizmo_model.get());
	mc->set_casts_shadows(false);
}
#endif
