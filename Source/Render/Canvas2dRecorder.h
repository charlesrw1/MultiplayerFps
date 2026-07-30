#pragma once
#include <vector>
#include "glm/glm.hpp"
#include "Framework/Util.h"
#include "Framework/Rect2d.h"
#include "Framework/MeshBuilder.h"
#include "Render/Canvas2dTypes.h"
#include "Render/Canvas2dGpuMesh.h"

class IGraphicsTexture;

// Per-frame recording state + incremental batching, parallel in role to the old
// RenderWindow but folding draws directly into Canvas2dBatch ranges instead of a flat
// draw-cmd list. Owned as private static state inside Canvas2d (see design doc) --
// there is no separate Canvas2dSystem singleton.
class Canvas2dRecorder
{
public:
	// Resets all recording state to frame-start defaults (no target/viewport/view/scissor
	// carried over from the previous frame) and clears the closed-batch list + transient
	// arena. Caller (Canvas2d::update()) still has to set_target/set_viewport to the
	// actual frame defaults right after -- this only guarantees nothing leaks forward.
	void begin_frame();

	void set_target(IGraphicsTexture* new_target, glm::ivec2 new_target_size);
	void set_clear(bool clear_color, Color32 color, bool clear_depth);
	void set_viewport(Rect2d new_viewport);
	void set_view_matrix(glm::mat4 view);
	void set_scissor(bool enabled, Rect2d rect);
	void push_scissor(Rect2d rect);
	void pop_scissor();

	IGraphicsTexture* get_target() const { return target; }
	glm::ivec2 get_target_size() const { return target_size; }
	Rect2d get_viewport() const { return viewport; }
	glm::mat4 get_view_proj() const;

	// Records geometry already appended to `source` as the index range
	// [index_start, index_start+index_count). transform_matters must be false for the
	// transient arena (its transform is already baked into vertex positions at record
	// time) and true for a persistent Canvas2dVertexArray draw (where transform is a
	// real per-draw GPU uniform, so it has to match for two calls to batch together).
	void submit(Canvas2dGpuMesh* source, int index_start, int index_count, const DrawSettings& settings,
				const glm::mat4& transform, bool transform_matters);

	MeshBuilder& get_transient_arena() { return transient_arena; }
	Canvas2dGpuMesh& get_transient_gpu_mesh() { return transient_gpu_mesh; }
	std::vector<Canvas2dBatch>& get_batches() { return batches; }

private:
	IGraphicsTexture* target = nullptr;
	glm::ivec2 target_size{0, 0};
	Rect2d viewport{};
	glm::mat4 view_matrix{1.f};
	bool scissor_enabled = false;
	Rect2d scissor{};
	bool wants_clear_color = false;
	Color32 pending_clear_color = COLOR_BLACK;
	bool wants_clear_depth = false;
	uint64_t state_version = 0;

	std::vector<Rect2d> scissor_stack;
	std::vector<Canvas2dBatch> batches;
	MeshBuilder transient_arena;
	Canvas2dGpuMesh transient_gpu_mesh;
};
