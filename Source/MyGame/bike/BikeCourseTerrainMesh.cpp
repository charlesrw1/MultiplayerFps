// BikeCourseTerrainMesh.cpp
// Builds the visible procedural terrain for the Hilly course: a flat grid
// spanning terrain_size_m x terrain_size_m, centered on the origin (matching
// where build_hilly_circuit places the road), heights/normals sampled from
// bike_hilly_height. Mirrors BikeCourseMesh.cpp's road-ribbon pattern.
#include "BikeHeaders.h"
#include "BikeCourseHilly.h"
#include "Render/Model.h"
#include "Render/ModelManager.h"
#include "Game/GameplayStatic.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Render/MaterialPublic.h"

namespace {
// 16-bit indices cap the builder at 65535 unique vertices -- clamp grid
// resolution so (segs+1)^2 always stays comfortably under that.
constexpr int MAX_TERRAIN_SEGS = 250;  // (250+1)^2 = 63001 verts

// World-space metres per texture tile -- independent of grid resolution so the
// grass texture doesn't restretch when terrain_grid_step_m changes.
constexpr float TERRAIN_UV_TILE_M = 8.f;

std::shared_ptr<MaterialInstance> get_terrain_material() {
	// MaterialInstance is an asset-database-owned object (never deleted directly); wrap it in a
	// non-owning shared_ptr (no-op deleter) purely to satisfy ModelBuilder::begin_submesh's signature.
	static std::shared_ptr<MaterialInstance> mat(MaterialInstance::load("groundgrass_01.mi"),
	                                              [](MaterialInstance*) {});
	return mat;
}
}

void BikeGameApplication::build_terrain_mesh()
{
	if (course_variant != BikeHardcodedCourseKind::Hilly) {
		if (terrain_mesh_component)
			terrain_mesh_component->set_model(nullptr);
		terrain_mesh.reset();
		return;
	}

	const BikeHillyParams& hp = g_hilly_params;
	const float size = glm::max(1.f, hp.terrain_size_m);
	int segs = (int)glm::round(size / glm::max(0.5f, hp.terrain_grid_step_m));
	segs = glm::clamp(segs, 4, MAX_TERRAIN_SEGS);
	const int   verts_per_side = segs + 1;
	const float step           = size / (float)segs;
	const float half           = size * 0.5f;
	// Central-difference epsilon for normals -- independent of grid step so
	// normal quality doesn't degrade at coarse resolutions.
	const float normal_eps = 0.5f;

	ModelBuilder builder;
	builder.begin_submesh(get_terrain_material());

	std::vector<uint16_t> idx(verts_per_side * verts_per_side);
	for (int i = 0; i < verts_per_side; ++i) {
		const float x = -half + (float)i * step;
		for (int j = 0; j < verts_per_side; ++j) {
			const float z = -half + (float)j * step;
			const float y = bike_hilly_terrain_height(x, z);

			const float h_x1 = bike_hilly_terrain_height(x + normal_eps, z);
			const float h_x0 = bike_hilly_terrain_height(x - normal_eps, z);
			const float h_z1 = bike_hilly_terrain_height(x, z + normal_eps);
			const float h_z0 = bike_hilly_terrain_height(x, z - normal_eps);
			const glm::vec3 normal = glm::normalize(glm::vec3(
				(h_x0 - h_x1) / (2.f * normal_eps),
				1.f,
				(h_z0 - h_z1) / (2.f * normal_eps)));

			const glm::vec2 uv(x / TERRAIN_UV_TILE_M, z / TERRAIN_UV_TILE_M);
			idx[i * verts_per_side + j] = builder.add_vertex({ x, y, z }, uv, normal);
		}
	}

	// Winding: (A,B,C,D) = (i,j)->(i,j+1)->(i+1,j+1)->(i+1,j) faces +Y (verified
	// via cross(B-A,C-A) and cross(C-A,D-A) both pointing up for this order).
	for (int i = 0; i < segs; ++i) {
		for (int j = 0; j < segs; ++j) {
			const uint16_t a = idx[i * verts_per_side + j];
			const uint16_t b = idx[i * verts_per_side + (j + 1)];
			const uint16_t c = idx[(i + 1) * verts_per_side + (j + 1)];
			const uint16_t d = idx[(i + 1) * verts_per_side + j];
			builder.add_quad(a, b, c, d);
		}
	}

	if (!terrain_mesh)
		terrain_mesh.reset(g_modelMgr.create_dynamic_model(builder, "bike_hilly_terrain"));
	else
		g_modelMgr.refresh_dynamic_model(terrain_mesh.get(), builder);

	if (!terrain_mesh_entity) {
		terrain_mesh_entity    = GameplayStatic::spawn_entity();
		terrain_mesh_component = terrain_mesh_entity->create_component<MeshComponent>();
	}

	// Force a resync even though the Model* pointer is unchanged after a refresh --
	// set_model() only re-syncs render data when the pointer differs.
	terrain_mesh_component->set_model(nullptr);
	terrain_mesh_component->set_model(terrain_mesh.get());
}
