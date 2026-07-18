#pragma once

#include "Game/EntityComponent.h"
#include "Framework/Reflection2.h"
#include "Framework/EnumDefReflection.h"
#include "Framework/Handle.h"
#include "Assets/IAsset.h"
#include <vector>

class Model;
struct Render_Object;

// Disabled: no render objects registered.
// EnabledAnimated: registered and re-positioned every frame by the wave function.
// EnabledStatic: registered once with the wave function evaluated at t=0, then left alone.
// EnabledCompactStatic: same static grid, but through the GPU-driven compact instance
//   path (register_compact_batch + set_compact_instances) instead of per-instance
//   Render_Objects -- the validation testbed for that path.
// EnabledCompactDynamic: compact path with is_dynamic=true, re-uploaded every frame by
//   the wave function -- validates the ping-pong prev buffer / motion vectors.
NEWENUM(RenderStressTestState, uint8_t){
	Disabled,
	EnabledAnimated,
	EnabledStatic,
	EnabledCompactStatic,
	EnabledCompactDynamic,
};

// Debug/perf tool: spawns an NxN grid of a single Model as raw Render_Objects (not
// sibling Components/Entities, to keep per-instance overhead to the renderer only) centered
// on the owner, and displaces each instance's height every frame with a 2D wave so the whole
// grid animates. Use grid_length to dial up instance count (grid_length^2 total meshes) to
// stress the renderer/scene submission path.
class RenderStressTestComponent : public Component
{
public:
	CLASS_BODY(RenderStressTestComponent, spawnable);

	RenderStressTestComponent() { set_call_init_in_editor(true); }

	void start() final;
	void update() final;
	void stop() final;
	void on_sync_render_data() final;
#ifdef EDITOR_BUILD
	void editor_on_change_property() final;
	void on_inspector_imgui() final;
#endif

	REF AssetPtr<Model> model;
	// total instance count is grid_length * grid_length
	REF int grid_length = 10;
	REF float spacing = 2.f;
	REF float wave_height = 1.f;
	REF float wave_frequency = 0.5f;
	REF float wave_speed = 2.f;
	// When set, each cube yaws about its own Y axis by (wave height * rotation_scale)
	// radians, so instances turn as they rise and unwind as they fall -- a rotating wave.
	REF bool wave_rotation = false;
	REF float rotation_scale = 1.f;
	REF RenderStressTestState state = RenderStressTestState::Disabled;

private:
	// register_obj/update_obj/remove_obj may only be called outside the renderer's overlapped
	// period (see Game/AGENTS.md); on_sync_render_data() is the one place that's guaranteed
	// safe, so all grid (re)builds and per-frame transform pushes happen there. start()/update()/
	// the editor callbacks only ever set needs_rebuild and call sync_render_data() to schedule it.
	void rebuild_grid();
	void clear_grid();
	// Compact instance path testbed: (re)registers the batch and pushes the static
	// grid through set_compact_instances. Mutually exclusive with the classic grid.
	void on_sync_compact();

	std::vector<handle<Render_Object>> instances;
	// per-instance world-space (x,z) grid offset from the owner, y is filled in by the wave
	std::vector<glm::vec2> grid_offsets;
	bool needs_rebuild = false;

	static constexpr uint16_t kInvalidBatch = 0xFFFF;
	uint16_t compact_batch_id = kInvalidBatch;
};
