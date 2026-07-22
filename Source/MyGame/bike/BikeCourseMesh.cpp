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
#include "Game/Components/MeshbuilderComponent.h"
#include "Render/MaterialPublic.h"

namespace {
// Waypoint height is a gameplay reference (rider projection/physics), not a render
// height -- lift the visible road mesh a bit further above it so it doesn't z-fight
// with or sink below the terrain. Mirrors RoadNetworkComponent's ROAD_GROUND_OFFSET.
constexpr float ROAD_MESH_Y_OFFSET = 0.01f;
// Racing line sits on top of the road surface itself, so it needs its own further
// epsilon above ROAD_MESH_Y_OFFSET to avoid z-fighting with the road mesh.
constexpr float RACING_LINE_Y_OFFSET = ROAD_MESH_Y_OFFSET + 0.01f;

std::shared_ptr<MaterialInstance> get_road_material() {
	// MaterialInstance is an asset-database-owned object (never deleted directly); wrap it in a
	// non-owning shared_ptr (no-op deleter) purely to satisfy ModelBuilder::begin_submesh's signature.
	static std::shared_ptr<MaterialInstance> mat(MaterialInstance::load("materials/asphalt14/asphalt14.mi"),
	                                              [](MaterialInstance*) {});
	return mat;
}
}

// Note: road_mesh_entity is intentionally NOT destroyed here. By the time
// Application destructors run, GameEngineLocal::cleanup has already called
// stop_game(), which tears down the Level (and every entity in it, including
// this one) -- eng->get_level() is null/stale at this point, so Entity::destroy()
// would dereference it. road_mesh (the GPU model) still frees itself safely via
// its own destructor, since the renderer isn't torn down until after Application.
BikeGameApplication::~BikeGameApplication() = default;

void BikeGameApplication::build_road_mesh()
{
	if (!course.is_built || course.waypoints.size() < 2) {
		if (road_mesh_component)
			road_mesh_component->set_model(nullptr);
		road_mesh.reset();
		return;
	}

	// Divides the tile length so the asphalt texture repeats more densely across
	// the road surface instead of stretching one tile over the full width/length.
	static constexpr float UV_TILE_SCALE = 1.8f;

	ModelBuilder builder;
	builder.begin_submesh(get_road_material());
	const glm::vec3 up(0.f, 1.f, 0.f);
	const glm::vec3 y_offset(0.f, ROAD_MESH_Y_OFFSET, 0.f);

	const int n = (int)course.waypoints.size();
	std::vector<uint16_t> left(n), right(n);
	for (int i = 0; i < n; ++i) {
		const BikeWaypoint& wp = course.waypoints[i];
		const float tile_len = wp.road_half_width * 2.f > 0.001f ? wp.road_half_width * 2.f : 1.f;
		const float v = wp.dist_from_start / tile_len * UV_TILE_SCALE;
		const glm::vec3 l = wp.position - wp.right * wp.road_half_width + y_offset;
		const glm::vec3 r = wp.position + wp.right * wp.road_half_width + y_offset;
		left[i]  = builder.add_vertex(l, { 0.f, v }, up);
		right[i] = builder.add_vertex(r, { UV_TILE_SCALE, v }, up);
	}

	const int num_segs = course.is_loop ? n : n - 1;
	for (int i = 0; i < num_segs; ++i) {
		const int j = (i + 1) % n;
		// wp.right (cross(WORLD_UP, fwd)) is the opposite handedness from the
		// cross(dir, up) convention RoadNetworkComponent::push_road_strip uses, so
		// left/right must be swapped here to keep the strip's front face up.
		builder.add_quad(right[i], left[i], left[j], right[j]);
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

void BikeGameApplication::set_draw_racing_line(bool show)
{
	draw_racing_line_debug = show;

	if (!racing_line_entity) {
		if (!show) return;  // nothing built yet, nothing to hide
		racing_line_entity = GameplayStatic::spawn_entity();
		racing_line_mb     = racing_line_entity->create_component<MeshBuilderComponent>();
		racing_line_mb->use_transform = false;  // vertices are pushed in world space already
		racing_line_mb->depth_tested  = true;
	}

	racing_line_mb->mb.Begin();
	if (show && course.is_built && course.waypoints.size() >= 2) {
		const glm::vec3 y_offset(0.f, RACING_LINE_Y_OFFSET, 0.f);
		const int n        = (int)course.waypoints.size();
		const int num_segs = course.is_loop ? n : n - 1;
		for (int i = 0; i < num_segs; ++i) {
			const int j = (i + 1) % n;
			racing_line_mb->mb.PushLine(course.waypoints[i].racing_line_pos + y_offset,
			                            course.waypoints[j].racing_line_pos + y_offset,
			                            Color32(0xff, 0x99, 0x00, 0xff));
		}
	}
	racing_line_mb->mb.End();
	racing_line_mb->sync_render_data();
}
