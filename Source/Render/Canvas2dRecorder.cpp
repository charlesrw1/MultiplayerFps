#include "Render/Canvas2dRecorder.h"
#include <glm/gtc/matrix_transform.hpp>

void Canvas2dRecorder::begin_frame() {
	target = nullptr;
	target_size = glm::ivec2(0, 0);
	viewport = Rect2d();
	view_matrix = glm::mat4(1.f);
	scissor_enabled = false;
	scissor = Rect2d();
	wants_clear_color = false;
	wants_clear_depth = false;
	state_version = 0;
	scissor_stack.clear();
	batches.clear();
	transient_arena.Begin();
}

void Canvas2dRecorder::set_target(Texture* new_target, glm::ivec2 new_target_size) {
	target = new_target;
	target_size = new_target_size;
	state_version++;
}

void Canvas2dRecorder::set_clear(bool clear_color, Color32 color, bool clear_depth) {
	wants_clear_color = clear_color;
	pending_clear_color = color;
	wants_clear_depth = clear_depth;
	state_version++;
}

void Canvas2dRecorder::set_viewport(Rect2d new_viewport) {
	viewport = new_viewport;
	state_version++;
}

void Canvas2dRecorder::set_view_matrix(glm::mat4 view) {
	view_matrix = view;
	state_version++;
}

void Canvas2dRecorder::set_scissor(bool enabled, Rect2d rect) {
	scissor_enabled = enabled;
	scissor = rect;
	state_version++;
}

void Canvas2dRecorder::push_scissor(Rect2d rect) {
	Rect2d prior = scissor_enabled ? scissor : Rect2d(0, 0, target_size.x, target_size.y);
	scissor_stack.push_back(prior);

	// intersect with current
	int16_t x0 = std::max(prior.x, rect.x);
	int16_t y0 = std::max(prior.y, rect.y);
	int16_t x1 = std::min<int16_t>(prior.x + prior.w, rect.x + rect.w);
	int16_t y1 = std::min<int16_t>(prior.y + prior.h, rect.y + rect.h);
	Rect2d result(x0, y0, std::max<int16_t>(0, x1 - x0), std::max<int16_t>(0, y1 - y0));
	set_scissor(true, result);
}

void Canvas2dRecorder::pop_scissor() {
	if (scissor_stack.empty()) {
		set_scissor(false, Rect2d());
		return;
	}
	Rect2d prior = scissor_stack.back();
	scissor_stack.pop_back();
	set_scissor(true, prior);
}

glm::mat4 Canvas2dRecorder::get_view_proj() const {
	glm::mat4 proj = glm::orthoZO(0.f, (float)target_size.x, (float)target_size.y, 0.f, -1.f, 1.f);
	return proj * view_matrix;
}

void Canvas2dRecorder::submit(Canvas2dGpuMesh* source, int index_start, int index_count, const DrawSettings& settings,
							   const glm::mat4& transform, bool transform_matters) {
	if (index_count <= 0)
		return;

	if (!batches.empty()) {
		Canvas2dBatch& back = batches.back();
		bool state_same = back.state_version == state_version;
		bool draw_same = back.source == source && back.texture == settings.texture && back.blend == settings.blend &&
						  back.material == settings.custom_shader && back.depth_test == settings.depth_test;
		bool transform_same = !transform_matters || back.transform == transform;
		bool contiguous = back.index_start + back.index_count == index_start;
		if (state_same && draw_same && transform_same && contiguous) {
			back.index_count += index_count;
			return;
		}
	}

	Canvas2dBatch b;
	b.target = target;
	b.viewport = viewport;
	b.view_proj = get_view_proj();
	b.scissor_enabled = scissor_enabled;
	b.scissor = scissor;
	b.depth_test = settings.depth_test;
	b.blend = settings.blend;
	b.material = settings.custom_shader;
	b.texture = settings.texture;
	b.source = source;
	b.transform = transform;
	b.index_start = index_start;
	b.index_count = index_count;
	b.base_vertex = 0;
	b.wants_clear_color = wants_clear_color;
	b.clear_color = pending_clear_color;
	b.wants_clear_depth = wants_clear_depth;
	b.state_version = state_version;
	batches.push_back(b);
}
