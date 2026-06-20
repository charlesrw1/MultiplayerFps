#include "ViewportHandles.h"
#include "Debug.h"
#include "Game/GameplayStatic.h"
#include "imgui.h"
#include "Input/InputSystem.h"
#include "EditorInputs.h"
#include "EditorDocLocal.h"

extern ConfigVar ed_has_snap;
extern ConfigVar ed_translation_snap;
extern ConfigVar ed_scale_snap;

static float snap_value(float x, float snap) {
	return glm::round(x / snap) * snap;
}

VHResult EViewportHandles::point_handle(int64_t id, glm::vec3& inout_position) {
	auto& item = items[id];
	item.was_wanted_this_frame = true;
	return {};
}

VHResult EViewportHandles::box_handles(int64_t id, glm::mat4& box_center, glm::vec3& boxextents,
									   BoxHandleMode mode) {
	auto& item = items[id];
	item.mode = mode;

	if (item.just_finished) {
		item.just_finished = false;
		return VHResult::Finished;
	}

	if (dragging_state.item != id) {
		item.transform = box_center;
		item.boxextents = boxextents;
	}
	item.was_wanted_this_frame = true;

	if (dragging_state.item == id) {
		box_center = item.newtransform;
		boxextents = item.boxextents;
		return VHResult::Changing;
	}

	return VHResult::Unchanged;
}

// ---------------------------------------------------------------------------
// Face handle helpers (6 handles: 2 per axis, at face centers)
// ---------------------------------------------------------------------------

glm::vec3 EViewportHandles::ActiveItem::get_position_for_face_handle(int idx, bool use_new) const {
	glm::vec3 local(0.5f);
	int axis = idx / 2;
	local[axis] = (idx & 1) ? 0.0f : 1.0f;

	const auto& t = use_new ? newtransform : transform;
	return glm::vec3(t * glm::vec4(local * boxextents, 1.0f));
}

glm::vec3 EViewportHandles::ActiveItem::get_normal_for_face_handle(int idx) const {
	glm::vec3 local(0.0f);
	int axis = idx / 2;
	local[axis] = (idx & 1) ? -1.0f : 1.0f;
	return glm::normalize(glm::mat3(transform) * local);
}

// ---------------------------------------------------------------------------
// Edge handle helpers (12 handles: 4 per parallel-axis group)
// Edge i: parallel_axis = i/4, the two perp axes vary by corner bits (i%4)
// ---------------------------------------------------------------------------

glm::vec3 EViewportHandles::ActiveItem::get_position_for_edge_handle(int idx, bool use_new) const {
	ASSERT(idx >= 0 && idx < 12);
	int parallel_axis = idx / 4;
	int corner = idx % 4;

	int perp0 = (parallel_axis + 1) % 3;
	int perp1 = (parallel_axis + 2) % 3;

	glm::vec3 local(0.0f);
	local[parallel_axis] = 0.5f;
	local[perp0] = (corner & 1) ? 1.0f : 0.0f;
	local[perp1] = (corner & 2) ? 1.0f : 0.0f;

	const auto& t = use_new ? newtransform : transform;
	return glm::vec3(t * glm::vec4(local * boxextents, 1.0f));
}

glm::vec3 EViewportHandles::ActiveItem::get_normal_for_edge_handle(int idx) const {
	ASSERT(idx >= 0 && idx < 12);
	int parallel_axis = idx / 4;
	int corner = idx % 4;

	int perp0 = (parallel_axis + 1) % 3;
	int perp1 = (parallel_axis + 2) % 3;

	glm::vec3 local(0.0f);
	local[perp0] = (corner & 1) ? 1.0f : -1.0f;
	local[perp1] = (corner & 2) ? 1.0f : -1.0f;

	return glm::normalize(glm::mat3(transform) * local);
}

// ---------------------------------------------------------------------------
// Constraint plane for dragging
// ---------------------------------------------------------------------------

static bool compute_drag_plane(const glm::vec3& drag_axis, const glm::vec3& handle_pos,
							   const glm::vec3& view_dir, glm::vec3& out_plane_normal) {
	glm::vec3 perp = glm::cross(view_dir, drag_axis);
	float perp_len = glm::length(perp);
	if (perp_len < 0.001f) {
		out_plane_normal = drag_axis;
		return true;
	}
	out_plane_normal = glm::normalize(glm::cross(drag_axis, perp));
	return true;
}

// ---------------------------------------------------------------------------
// Face drag: 1-axis position+scale change
// ---------------------------------------------------------------------------

void EViewportHandles::tick_face_drag(const glm::vec3& current_hit) {
	ASSERT(dragging_state.item != -1);
	auto& item = items[dragging_state.item];
	int index = dragging_state.index;
	int axis = index / 2;
	bool modify_origin = (index & 1);

	const auto& init_t = dragging_state.initial_transform;
	const auto& init_ext = dragging_state.initial_extents;

	glm::vec3 drag_axis = glm::normalize(glm::vec3(init_t[axis]));
	float delta = glm::dot(current_hit - dragging_state.initial_hit, drag_axis);
	if (ed_has_snap.get_bool()) {
		float snap = ed_translation_snap.get_float();
		float handle_along_axis = glm::dot(dragging_state.initial_handle_pos, drag_axis);
		float snapped_pos = snap_value(handle_along_axis + delta, snap);
		delta = snapped_pos - handle_along_axis;
	}

	dragging_state.display_delta[0] = delta;

	glm::vec3 position = glm::vec3(init_t[3]);
	glm::vec3 scale;
	scale.x = glm::length(glm::vec3(init_t[0]));
	scale.y = glm::length(glm::vec3(init_t[1]));
	scale.z = glm::length(glm::vec3(init_t[2]));

	glm::mat3 rotation;
	rotation[0] = glm::vec3(init_t[0]) / scale.x;
	rotation[1] = glm::vec3(init_t[1]) / scale.y;
	rotation[2] = glm::vec3(init_t[2]) / scale.z;

	float extent_world = init_ext[axis] * scale[axis];
	if (glm::abs(extent_world) < 0.0001f)
		return;

	if (modify_origin) {
		position += delta * drag_axis;
		scale[axis] -= delta / init_ext[axis];
	} else {
		scale[axis] += delta / init_ext[axis];
	}

	float min_world_size = ed_has_snap.get_bool() ? ed_translation_snap.get_float() : 0.001f;
	if (scale[axis] * init_ext[axis] < min_world_size)
		scale[axis] = min_world_size / init_ext[axis];

	glm::mat4 new_t(1.0f);
	new_t = glm::translate(new_t, position);
	new_t *= glm::mat4(rotation);
	new_t = glm::scale(new_t, scale);

	if (glm::any(glm::isnan(position)) || glm::any(glm::isnan(scale))) {
		sys_print(Warning, "box handle face drag produced NaN, cancelling drag");
		item.newtransform = dragging_state.initial_transform;
		item.just_finished = true;
		dragging_state = Dragging();
		return;
	}

	item.newtransform = new_t;
}

// ---------------------------------------------------------------------------
// Edge drag: 2-axis position+scale change
// ---------------------------------------------------------------------------

void EViewportHandles::tick_edge_drag(const glm::vec3& current_hit) {
	ASSERT(dragging_state.item != -1);
	auto& item = items[dragging_state.item];
	int idx = dragging_state.index;
	int parallel_axis = idx / 4;
	int corner = idx % 4;

	int perp0 = (parallel_axis + 1) % 3;
	int perp1 = (parallel_axis + 2) % 3;

	const auto& init_t = dragging_state.initial_transform;
	const auto& init_ext = dragging_state.initial_extents;

	glm::vec3 world_delta = current_hit - dragging_state.initial_hit;

	glm::vec3 position = glm::vec3(init_t[3]);
	glm::vec3 scale;
	scale.x = glm::length(glm::vec3(init_t[0]));
	scale.y = glm::length(glm::vec3(init_t[1]));
	scale.z = glm::length(glm::vec3(init_t[2]));

	glm::mat3 rotation;
	rotation[0] = glm::vec3(init_t[0]) / scale.x;
	rotation[1] = glm::vec3(init_t[1]) / scale.y;
	rotation[2] = glm::vec3(init_t[2]) / scale.z;

	int delta_idx = 0;
	auto apply_axis = [&](int axis, bool is_positive_side) {
		glm::vec3 drag_dir = glm::normalize(glm::vec3(init_t[axis]));
		float delta = glm::dot(world_delta, drag_dir);
		if (ed_has_snap.get_bool()) {
			float snap = ed_translation_snap.get_float();
			float handle_along_axis = glm::dot(dragging_state.initial_handle_pos, drag_dir);
			float snapped_pos = snap_value(handle_along_axis + delta, snap);
			delta = snapped_pos - handle_along_axis;
		}

		if (is_positive_side) {
			scale[axis] += delta / init_ext[axis];
		} else {
			position += delta * drag_dir;
			scale[axis] -= delta / init_ext[axis];
		}

		dragging_state.display_delta[delta_idx++] = delta;
	};

	apply_axis(perp0, (corner & 1) != 0);
	apply_axis(perp1, (corner & 2) != 0);

	float min_world_size = ed_has_snap.get_bool() ? ed_translation_snap.get_float() : 0.001f;
	for (int a : {perp0, perp1}) {
		if (scale[a] * init_ext[a] < min_world_size)
			scale[a] = min_world_size / init_ext[a];
	}

	glm::mat4 new_t(1.0f);
	new_t = glm::translate(new_t, position);
	new_t *= glm::mat4(rotation);
	new_t = glm::scale(new_t, scale);

	if (glm::any(glm::isnan(position)) || glm::any(glm::isnan(scale))) {
		sys_print(Warning, "box handle edge drag produced NaN, cancelling drag");
		item.newtransform = dragging_state.initial_transform;
		item.just_finished = true;
		dragging_state = Dragging();
		return;
	}

	item.newtransform = new_t;
}

// ---------------------------------------------------------------------------
// Main tick
// ---------------------------------------------------------------------------

void EViewportHandles::tick(EditorInputs& inputs) {
	bool is_dragging = has_item_being_dragged();
	const bool mouse_clicked = Input::was_mouse_pressed(0) && inputs.can_use_mouse_click();

	// Update drag
	if (is_dragging) {
		if (!Input::is_mouse_down(0)) {
			auto& item = items[dragging_state.item];
			item.just_finished = true;
			dragging_state = Dragging();
			inputs.set_focus(nullptr);
			is_dragging = false;
		} else {
			auto mouse_pos = Input::get_mouse_pos();
			Ray ray = doc.unproject_mouse_to_ray(mouse_pos.x, mouse_pos.y);
			glm::vec3 hit;
			if (ray_plane_intersect(ray, dragging_state.plane_normal, dragging_state.plane_point, hit)) {
				auto& item = items[dragging_state.item];
				if (item.mode == BoxHandleMode::Face)
					tick_face_drag(hit);
				else
					tick_edge_drag(hit);
			}
			draw_drag_info_text();
		}
		inputs.eat_mouse_click();
	}

	// Delete items not updated this frame
	{
		std::vector<int64_t> unused;
		for (auto& [key, value] : items) {
			if (!value.was_wanted_this_frame)
				unused.push_back(key);
			else
				value.was_wanted_this_frame = false;
		}
		for (auto key : unused)
			items.erase(key);
	}

	// Collect visible handles, find hover/click candidates, then draw
	int64_t want_item_select = -1;
	int want_select_sub = -1;
	int64_t hover_item = -1;
	int hover_sub = -1;
	Texture* texture = Texture::load("circle.png");
	auto mpos = Input::get_mouse_pos() - UiSystem::inst->get_vp_rect().get_pos();

	struct HandleDraw {
		int64_t key;
		int index;
		glm::ivec2 sc;
		BoxHandleMode mode;
		bool is_dragged;
	};
	std::vector<HandleDraw> visible_handles;

	for (auto& [key, value] : items) {
		bool is_this_dragging = (key == dragging_state.item);
		int handle_count = (value.mode == BoxHandleMode::Face) ? 6 : 12;

		for (int i = 0; i < handle_count; i++) {
			glm::vec3 pos;
			if (value.mode == BoxHandleMode::Face) {
				pos = value.get_position_for_face_handle(i, is_this_dragging);
				glm::vec3 normal = value.get_normal_for_face_handle(i);
				if (!doc.ed_cam.get_is_using_ortho() && glm::dot(doc.vs_setup.front, normal) >= 0.0f)
					continue;
			} else {
				pos = value.get_position_for_edge_handle(i, is_this_dragging);
				int parallel_axis = i / 4;
				int perp0 = (parallel_axis + 1) % 3;
				int perp1 = (parallel_axis + 2) % 3;
				glm::vec3 axis_a = glm::normalize(glm::vec3(value.transform[perp0]));
				glm::vec3 axis_b = glm::normalize(glm::vec3(value.transform[perp1]));
				const float threshold = 0.85f;
				if (glm::abs(glm::dot(doc.vs_setup.front, axis_a)) > threshold)
					continue;
				if (glm::abs(glm::dot(doc.vs_setup.front, axis_b)) > threshold)
					continue;
			}

			glm::vec4 clip_pos = doc.vs_setup.viewproj * glm::vec4(pos, 1.0f);
			clip_pos /= clip_pos.w;
			if (clip_pos.z < 0.0f)
				continue;

			const glm::ivec2 sc = ndc_to_screen_coord(clip_pos);
			const int size = 14;

			Rect2d hitrect = Rect2d(sc.x - size * 2, sc.y - size * 2, size * 4, size * 4);
			if (!is_dragging && hitrect.is_point_inside(mpos)) {
				hover_item = key;
				hover_sub = i;
				if (mouse_clicked) {
					want_item_select = key;
					want_select_sub = i;
				}
			}

			visible_handles.push_back({key, i, sc, value.mode, is_this_dragging && (i == dragging_state.index)});
		}
	}

	for (auto& h : visible_handles) {
		const int size = 14;
		RectangleShape rect;
		rect.rect = Rect2d(h.sc.x - size / 2, h.sc.y - size / 2, size, size);
		if (h.is_dragged)
			rect.color = Color32(50, 200, 255);
		else if (h.key == hover_item && h.index == hover_sub)
			rect.color = Color32(130, 220, 255);
		else
			rect.color = (h.mode == BoxHandleMode::Edge) ? Color32(255, 220, 80) : COLOR_WHITE;
		rect.texture = texture;
		UiSystem::inst->window.draw(rect);
	}

	// Start drag
	if (!is_dragging && want_item_select != -1) {
		auto& item = items[want_item_select];
		dragging_state.item = want_item_select;
		dragging_state.index = want_select_sub;
		dragging_state.initial_transform = item.transform;
		dragging_state.initial_extents = item.boxextents;

		glm::vec3 handle_pos;
		if (item.mode == BoxHandleMode::Face) {
			handle_pos = item.get_position_for_face_handle(want_select_sub, false);
			int axis = want_select_sub / 2;
			glm::vec3 drag_axis = glm::normalize(glm::vec3(item.transform[axis]));
			compute_drag_plane(drag_axis, handle_pos, doc.vs_setup.front, dragging_state.plane_normal);
		} else {
			handle_pos = item.get_position_for_edge_handle(want_select_sub, false);
			int parallel_axis = want_select_sub / 4;
			glm::vec3 edge_dir = glm::normalize(glm::vec3(item.transform[parallel_axis]));
			dragging_state.plane_normal = edge_dir;
		}

		dragging_state.plane_point = handle_pos;
		dragging_state.initial_handle_pos = handle_pos;

		auto mouse_pos = Input::get_mouse_pos();
		Ray ray = doc.unproject_mouse_to_ray(mouse_pos.x, mouse_pos.y);
		glm::vec3 hit;
		if (ray_plane_intersect(ray, dragging_state.plane_normal, dragging_state.plane_point, hit)) {
			dragging_state.initial_hit = hit;
		} else {
			dragging_state.initial_hit = handle_pos;
		}

		inputs.set_focus(this);
		inputs.eat_mouse_click();
	}
}

void EViewportHandles::draw_drag_info_text() {
	ASSERT(dragging_state.item != -1);
	auto& item = items[dragging_state.item];

	glm::vec3 handle_pos;
	if (item.mode == BoxHandleMode::Face)
		handle_pos = item.get_position_for_face_handle(dragging_state.index, true);
	else
		handle_pos = item.get_position_for_edge_handle(dragging_state.index, true);

	glm::vec4 clip = doc.vs_setup.viewproj * glm::vec4(handle_pos, 1.0f);
	if (clip.w <= 0.0f)
		return;
	clip /= clip.w;
	glm::ivec2 sc = ndc_to_screen_coord(clip);

	std::string label;
	if (item.mode == BoxHandleMode::Face) {
		label = string_format("%+8.3f", dragging_state.display_delta[0]);
	} else {
		label = string_format("%+8.3f  %+8.3f", dragging_state.display_delta[0], dragging_state.display_delta[1]);
	}

	TextShape text;
	text.text = label;
	text.color = COLOR_WHITE;
	text.with_drop_shadow = true;
	text.drop_shadow_ofs = 1;
	text.rect = Rect2d(sc.x + 16, sc.y - 8, 0, 0);
	UiSystem::inst->window.draw(text);
}
