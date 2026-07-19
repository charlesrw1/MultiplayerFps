// BikeCourseMesh.cpp
// Builds the visible road ribbon mesh from BikeGameApplication::course.waypoints
// and displays it through a MeshComponent, so hardcoded/road-network courses
// alike get a real drawn road instead of only debug lines.
#include "BikeHeaders.h"
#include "Render/Model.h"
#include "Render/ModelManager.h"
#include "Game/GameplayStatic.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Render/MaterialPublic.h"

namespace {
std::shared_ptr<MaterialInstance> get_road_material() {
	// MaterialInstance is an asset-database-owned object (never deleted directly); wrap it in a
	// non-owning shared_ptr (no-op deleter) purely to satisfy ModelBuilder::begin_submesh's signature.
	static std::shared_ptr<MaterialInstance> mat(MaterialInstance::load("materials/asphalt14/asphalt14.mi"),
	                                              [](MaterialInstance*) {});
	return mat;
}
}

BikeGameApplication::~BikeGameApplication()
{
	if (road_mesh_entity)
		road_mesh_entity->destroy();
}

void BikeGameApplication::build_road_mesh()
{
	if (!course.is_built || course.waypoints.size() < 2) {
		if (road_mesh_component)
			road_mesh_component->set_model(nullptr);
		road_mesh.reset();
		return;
	}

	ModelBuilder builder;
	builder.begin_submesh(get_road_material());
	const glm::vec3 up(0.f, 1.f, 0.f);

	const int n = (int)course.waypoints.size();
	std::vector<uint16_t> left(n), right(n);
	for (int i = 0; i < n; ++i) {
		const BikeWaypoint& wp = course.waypoints[i];
		const float tile_len = wp.road_half_width * 2.f > 0.001f ? wp.road_half_width * 2.f : 1.f;
		const float v = wp.dist_from_start / tile_len;
		const glm::vec3 l = wp.position - wp.right * wp.road_half_width;
		const glm::vec3 r = wp.position + wp.right * wp.road_half_width;
		left[i]  = builder.add_vertex(l, { 0.f, v }, up);
		right[i] = builder.add_vertex(r, { 1.f, v }, up);
	}

	const int num_segs = course.is_loop ? n : n - 1;
	for (int i = 0; i < num_segs; ++i) {
		const int j = (i + 1) % n;
		builder.add_quad(left[i], right[i], right[j], left[j]);
	}

	if (!road_mesh)
		road_mesh.reset(g_modelMgr.create_dynamic_model(builder, "bike_road"));
	else
		g_modelMgr.refresh_dynamic_model(road_mesh.get(), builder);

	if (!road_mesh_entity) {
		road_mesh_entity    = GameplayStatic::spawn_entity();
		road_mesh_component = road_mesh_entity->create_component<MeshComponent>();
	}

	// Force a resync even though the Model* pointer is unchanged after a refresh —
	// set_model() only re-syncs render data when the pointer differs.
	road_mesh_component->set_model(nullptr);
	road_mesh_component->set_model(road_mesh.get());
}
