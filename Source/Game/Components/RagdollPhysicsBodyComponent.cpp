#include "RagdollPhysicsBodyComponent.h"
#include "RagdollGizmoMesh.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/MeshbuilderComponent.h"
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
	// Alpha-blended (ragdollGhost.mm) -- unlike the joint gizmo, a capsule is convex/watertight
	// (no coincident overlapping surfaces like the cone's cap+lateral wall), so translucency
	// doesn't hit the same draw-order sorting artifacts.
	static std::shared_ptr<MaterialInstance> mat(MaterialInstance::load("eng/ragdollGhost.mm"),
												  [](MaterialInstance*) {});
	return mat;
}
} // namespace

void RagdollPhysicsBodyComponent::editor_start() {
	rebuild_gizmo_mesh();
}

void RagdollPhysicsBodyComponent::stop() {
	// Destroy the gizmo entity (and its MeshComponent/MeshBuilderComponent) FIRST so the renderer
	// drops their proxies through the normal entity-teardown path, THEN free the underlying
	// dynamic Model -- never the other way around (a freed-but-still-referenced Model crashes the
	// next scene draw).
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
	Entity* g = gizmo_entity.get();

	// Matches CapsuleComponent::add_actor_shapes() / PxCapsuleGeometry exactly: `height_offset` is
	// the capsule's CENTER (not its base), and `height` is the TOTAL capsule length including both
	// hemisphere caps -- so the cylindrical segment (p0/p1, the sphere centers the caps bulge out
	// from) is only `height - 2*radius` long, split evenly above/below center.
	glm::vec3 center(0.f, height_offset, 0.f);
	float half_cyl = glm::max(height * 0.5f - radius, 0.f);
	glm::vec3 p0 = center - glm::vec3(0.f, half_cyl, 0.f);
	glm::vec3 p1 = center + glm::vec3(0.f, half_cyl, 0.f);

	// Solid, alpha-blended capsule.
	ModelBuilder mb;
	mb.begin_submesh(get_ragdoll_body_gizmo_material());
	ragdoll_append_capsule_solid(mb, p0, p1, radius);
	if (!gizmo_model)
		gizmo_model.reset(g_modelMgr.create_dynamic_model(mb, "ragdoll_body_gizmo"));
	else
		g_modelMgr.refresh_dynamic_model(gizmo_model.get(), mb);

	auto* mc = g->get_component<MeshComponent>();
	if (!mc)
		mc = g->create_component<MeshComponent>();
	mc->set_model(gizmo_model.get());
	mc->set_casts_shadows(false);

	// Depth-tested wireframe outline over the solid, Unity-collider-gizmo style.
	auto* mbc = g->get_component<MeshBuilderComponent>();
	if (!mbc)
		mbc = g->create_component<MeshBuilderComponent>();
	mbc->use_background_color = false;
	mbc->depth_tested = true;
	mbc->use_transform = true;
	mbc->mb.Begin();
	ragdoll_append_capsule_lines(mbc->mb, p0, p1, radius, COLOR_WHITE);
	mbc->mb.End();
	mbc->sync_render_data();
}
#endif
