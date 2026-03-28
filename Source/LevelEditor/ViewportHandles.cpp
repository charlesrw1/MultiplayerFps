#include "ViewportHandles.h"
#include "Debug.h"
#include "Game/GameplayStatic.h"
#include "imgui.h"
#include "Input/InputSystem.h"

inline VHResult EViewportHandles::point_handle(int64_t id, glm::vec3& inout_position) {
	auto& item = items[id];
	item.pos = inout_position;
	if (item.mytype == ActiveItem::NEWLY_MADE) {
	}
	item.was_wanted_this_frame = true;
	return {};
}

VHResult EViewportHandles::box_handles(int64_t id, glm::mat4& box_center, glm::vec3& boxextents) {
	auto& item = items[id];
	if (dragging_state.item != id) {
		item.transform = box_center;
		item.boxextents = boxextents;
	}
	if (item.mytype == ActiveItem::NEWLY_MADE) {
	}
	item.was_wanted_this_frame = true;
	if (dragging_state.item == id) {
		box_center = item.newtransform;
		return VHResult::Changing;
	}

	return VHResult::Unchanged;
}

void extract_columns(glm::mat4 m, glm::vec3& right, glm::vec3& up, glm::vec3& forward) {
	right = glm::normalize(glm::vec3(m[0][0], m[1][0], m[2][0]));
	up = glm::normalize(glm::vec3(m[0][1], m[1][1], m[2][1]));
	forward = glm::normalize(glm::vec3(m[0][2], m[1][2], m[2][2]));
}

void draw_matrix(glm::mat4 m) {
	glm::vec3 o = m[3];

	Debug::add_line(o, o + vec3(m[0]), COLOR_RED, 0);
	Debug::add_line(o, o + vec3(m[1]), COLOR_GREEN, 0);
	Debug::add_line(o, o + vec3(m[2]), COLOR_BLUE, 0);
	GameplayStatic::debug_text(string_format("%f %f %f\n", o.x, o.y, o.z));
}

float a = 0.0;
float b = -1;

void masdfasdf() {
	ImGui::InputFloat("a", &a);
	ImGui::InputFloat("b", &b);
}
ADD_TO_DEBUG_MENU(masdfasdf);
void EViewportHandles::tick() {
	GameplayStatic::reset_debug_text_height();

	// delete items not updated
	// state = not_dragging, dragging (1 item selected), just stopped dragging (update transform, return selection)
	// for all handles, draw
	// do mouse selection if not_dragging and mouse input not stolen

	bool is_dragging_item = has_item_being_dragged();

	// if dragging:
	//		if mouse released: stop dragging
	//		else: update dragging object from manip

	const bool mouse_clicked = Input::was_mouse_pressed(0) && doc.inputs.can_use_mouse_click();

	// I do this weird thing because Imguizmo acts weird, this is litteraly a hack built on a hacked in feature so
	// yeah....
	if (dragging_state.set_next_frame) {
		dragging_state.set_next_frame = false;

		doc.selection_state->clear_all_selected();
		doc.selection_state->add_entities_to_selection(cached_selection_to_return);
		doc.manipulate->reset_group_to_pre_transform();
		cached_selection_to_return.clear();
	}

	if (is_dragging_item) {
		Entity* hacked = hacked_entity_MFER.get();
		if (!hacked) {
			sys_print(Warning, "no hacked entity viewport handles");
			hacked_entity_MFER = eng->get_level()->spawn_entity();
			hacked_entity_MFER->dont_serialize_or_edit = true;
			hacked_entity_MFER->set_editor_name("___handle_marker");
		}
		else {

			auto pos = hacked->get_ws_position();
			int index = dragging_state.index;
			auto transform = items[dragging_state.item].transform;

			const bool modify_origin = index & 1;
			const auto extents = items[dragging_state.item].boxextents;
			const glm::vec3 localspace = glm::inverse(transform) * glm::vec4(pos, 1.0);
			const auto dir = transform[index / 2];

			glm::vec3 position = glm::vec3(transform[3]);

			glm::vec3 scale;
			scale.x = glm::length(glm::vec3(transform[0]));
			scale.y = glm::length(glm::vec3(transform[1]));
			scale.z = glm::length(glm::vec3(transform[2]));

			glm::mat3 rotation;
			rotation[0] = glm::vec3(transform[0]) / scale.x;
			rotation[1] = glm::vec3(transform[1]) / scale.y;
			rotation[2] = glm::vec3(transform[2]) / scale.z;

			if (modify_origin) {
				position += (localspace[index / 2] * glm::vec3(dir));
				scale[index / 2] -= scale[index / 2] * localspace[index / 2] / extents[index / 2];
			}
			else {
				scale[index / 2] +=
					scale[index / 2] * (localspace[index / 2] - extents[index / 2]) / extents[index / 2];
			}
			glm::mat4 newTransform(1.0f);
			newTransform = glm::translate(newTransform, position);
			newTransform *= glm::mat4(rotation);
			newTransform = glm::scale(newTransform, scale);

			items[dragging_state.item].newtransform = newTransform;
			draw_matrix(transform);
		}
		if (!Input::is_mouse_down(0)) {
			// stop dragging
			doc.manipulate->set_force_gizmo_on(false);
			is_dragging_item = false;
			dragging_state = Dragging();
			doc.manipulate->reset_group_to_pre_transform();

			dragging_state.set_next_frame = true;

			// hacky ...
			doc.inputs.set_focus(nullptr);
		}
		doc.inputs.eat_mouse_click();
	}

	auto delete_usued = [&]() {
		std::vector<int64_t> unused;
		for (auto& [key, value] : items) {
			if (!value.was_wanted_this_frame) {
				unused.push_back(key);
			}
			else
				value.was_wanted_this_frame = false;
		}
		for (auto key : unused) {
			printf("deleting\n");
			items.erase(key);
		}
	};
	delete_usued();

	int64_t want_item_select = -1;
	int want_select_sub = -1;
	Texture* texture = Texture::load("circle.png");
	for (auto& [key, value] : items) {

		for (int i = 0; i < 6; i++) {
			glm::vec3 pos = value.get_position_for_handle(i, key == dragging_state.item);
			auto normal = value.get_normal_for_handle(i);

			if (!doc.ed_cam.get_is_using_ortho() && glm::dot(doc.vs_setup.front, normal) >= 0.0)
				continue;

			glm::vec4 transformed_pos = doc.vs_setup.viewproj * glm::vec4(pos, 1.0);
			transformed_pos = transformed_pos / transformed_pos.w;
			if (transformed_pos.z < 0)
				continue;
			const glm::ivec2 sc = ndc_to_screen_coord(transformed_pos);

			RectangleShape rect;
			const int size = 14;
			rect.rect = Rect2d(sc.x - size / 2, sc.y - size / 2, size, size);
			rect.color = COLOR_WHITE;
			rect.texture = texture;

			UiSystem::inst->window.draw(rect);
			auto mouse_pos = Input::get_mouse_pos() - UiSystem::inst->get_vp_rect().get_pos();
			Rect2d clickrect = Rect2d(sc.x - size * 2, sc.y - size * 2, size * 4, size * 4);
			if (mouse_clicked && clickrect.is_point_inside(mouse_pos)) {
				want_item_select = key;
				want_select_sub = i;
			}
		}
	}

	if (!is_dragging_item && want_item_select != -1) {
		dragging_state.item = want_item_select;
		dragging_state.index = want_select_sub;
		cached_selection_to_return = doc.selection_state->get_selection_as_vector();

		if (!hacked_entity_MFER) {
			hacked_entity_MFER = eng->get_level()->spawn_entity();
			hacked_entity_MFER->dont_serialize_or_edit = true;
			hacked_entity_MFER->set_editor_name("___handle_marker");
		}
		hacked_entity_MFER->set_ws_position(items[want_item_select].get_position_for_handle(want_select_sub, false));
		glm::vec3 p, s;
		glm::quat q;
		decompose_transform(items[want_item_select].transform, p, q, s);
		q = glm::normalize(q);
		hacked_entity_MFER->set_ws_rotation(q);

		doc.selection_state->set_select_only_this(hacked_entity_MFER.get());
		doc.manipulate->reset_group_to_pre_transform();
		doc.manipulate->set_force_gizmo_on(true);
		doc.manipulate->set_force_op(ImGuizmo::OPERATION::TRANSLATE);

		int mask = 0;
		if (want_select_sub / 2 == 0)
			mask = 1;
		if (want_select_sub / 2 == 1)
			mask = 2;
		if (want_select_sub / 2 == 2)
			mask = 4;

		doc.manipulate->set_force_axis_mask(mask);
		doc.manipulate->reset_group_to_pre_transform();

		doc.inputs.eat_mouse_click();
	}
}

inline glm::vec3 EViewportHandles::ActiveItem::get_position_for_handle(int idx, bool use_new) {
	glm::vec3 local(0.5);
	int i = idx / 2;
	local[i] = (idx & 1) ? 0.0 : 1.0;

	auto transform_to_use = (use_new) ? newtransform : transform;

	glm::vec3 transformed = transform * glm::vec4(local * boxextents, 1);
	return transformed;
}

inline glm::vec3 EViewportHandles::ActiveItem::get_normal_for_handle(int idx) {
	glm::vec3 local(0);
	int i = idx / 2;
	local[i] = (idx & 1) ? -1 : 1.0;
	glm::vec3 transformed = glm::transpose(glm::inverse(glm::mat3(transform))) * glm::vec4(local, 1);
	return transformed;
}
