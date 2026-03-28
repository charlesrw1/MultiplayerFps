#include "ManipulateTransformTool.h"
#include "EditorDocLocal.h"
bool line_plane_intersect(Ray r, glm::vec3 plane, float planed, glm::vec3& intersect) {
	float denom = dot(plane, r.dir);

	if (abs(denom) > 0.00001) { // such a high epsilon to deal with weird issues
		float planedist = dot(plane, r.pos) + planed;
		float time = -planedist / denom;
		intersect = r.pos + r.dir * time;
		return true;
	}
	return false;
}

glm::vec3 project_onto_line(glm::vec3 a, glm::vec3 b, glm::vec3 p) {
	glm::vec3 ap = p - a;
	glm::vec3 ab = b - a;
	return a + dot(ap, ab) / dot(ab, ab) * ab;
}

uint32_t color_to_uint(Color32 c) {
	return c.r | c.g << 8 | c.b << 16 | c.a << 24;
}

bool ManipulateTransformTool::is_hovered() {
	return ImGuizmo::IsOver();
}
bool ManipulateTransformTool::is_using() {
	return ImGuizmo::IsUsing();
}

ManipulateTransformTool::ManipulateTransformTool(EditorDoc& ed) : ed_doc(ed) {
	ed_doc.post_node_changes.add(this, &ManipulateTransformTool::on_entity_changes);
	ed_doc.selection_state->on_selection_changed.add(this, &ManipulateTransformTool::on_selection_changed);
	ed_doc.on_close.add(this, &ManipulateTransformTool::on_close);
	ed_doc.on_start.add(this, &ManipulateTransformTool::on_open);

	ed_doc.selection_state->on_selection_changed.add(this, &ManipulateTransformTool::on_selection_changed);

	// refresh cached data
	ed_doc.prop_editor->on_property_change.add(this, &ManipulateTransformTool::on_prop_change);
}

void ManipulateTransformTool::on_close() {
	state = IDLE;
	world_space_of_selected.clear();
}
void ManipulateTransformTool::on_open() {
	state = IDLE;
	world_space_of_selected.clear();
}
void ManipulateTransformTool::on_component_deleted(Component* ec) {
	stop_using_custom();

	update_pivot_and_cached();
}
void ManipulateTransformTool::on_entity_changes() {
	stop_using_custom();

	update_pivot_and_cached();
}
void ManipulateTransformTool::on_selection_changed() {
	stop_using_custom();

	update_pivot_and_cached();
}
void ManipulateTransformTool::on_prop_change() {

	// no stop_using_custom
	update_pivot_and_cached();
}
void ManipulateTransformTool::reset_group_to_pre_transform() {
	for (auto& pair : world_space_of_selected) {
		EntityPtr e(pair.first);
		if (e.get()) {
			e->set_ws_transform(pair.second);
		}
	}
	update_pivot_and_cached();
}
void ManipulateTransformTool::update_pivot_and_cached() {
	if (get_is_using_for_custom())
		return;

	world_space_of_selected.clear();
	auto& ss = ed_doc.selection_state;
	auto has_parent_in_selection_R = [&](auto&& self, Entity* e) -> bool {
		if (!e->get_parent())
			return false;
		auto& sel = ss->get_selection();
		if (sel.find(e->get_parent()->get_instance_id()) != sel.end())
			return true;
		return self(self, e->get_parent());
	};

	if (ss->has_any_selected()) {
		for (auto ehandle : ss->get_selection()) {
			EntityPtr e(ehandle);
			if (e.get()) {
				const bool should_skip = has_parent_in_selection_R(has_parent_in_selection_R, e.get());
				if (!should_skip)
					world_space_of_selected[e.handle] = (e.get()->get_ws_transform());
			}
		}
	}
	static bool selectFirstOnly = true;
	if (world_space_of_selected.size() == 1 || (!world_space_of_selected.empty() && selectFirstOnly)) {
		pivot_transform = world_space_of_selected.begin()->second;
	}
	else if (world_space_of_selected.size() > 1) {
		glm::vec3 v = glm::vec3(0.f);
		for (auto s : world_space_of_selected) {
			v += glm::vec3(s.second[3]);
		}
		v /= (float)world_space_of_selected.size();
		pivot_transform = glm::translate(glm::mat4(1), v);
	}
	current_transform_of_group = pivot_transform;

	if (world_space_of_selected.size() == 0)
		state = IDLE;
	else
		state = SELECTED;

	// return;
	auto snap_to_value = [](float x, float snap) { return glm::round(x / snap) * snap; };

	glm::vec3 p, s;
	glm::quat q;
	decompose_transform(current_transform_of_group, p, q, s);
	glm::vec3 asEuler = glm::eulerAngles(q);
	// printf(": %f\n", asEuler.x);
	if (ed_has_snap.get_bool()) {
		float translation_snap = ed_translation_snap.get_float();
		p.x = snap_to_value(p.x, translation_snap);
		p.y = snap_to_value(p.y, translation_snap);
		p.z = snap_to_value(p.z, translation_snap);
	}
	current_transform_of_group = glm::translate(glm::mat4(1), p);
	current_transform_of_group = current_transform_of_group * glm::mat4_cast(glm::normalize(q));
	current_transform_of_group = glm::scale(current_transform_of_group, glm::vec3(s));

	glm::vec3 p2, s2;
	glm::quat q2;
	decompose_transform(current_transform_of_group, p2, q2, s2);
	asEuler = glm::eulerAngles(q2);
	// printf(".: %f\n", asEuler.x);
}

void ManipulateTransformTool::on_selected_tarnsform_change(uint64_t h) {
	stop_using_custom();

	update_pivot_and_cached();
}

void ManipulateTransformTool::begin_drag() {
	ASSERT(state == SELECTED);
	state = MANIPULATING_OBJS;
}

void ManipulateTransformTool::end_drag() {
	ASSERT(state == MANIPULATING_OBJS);
	if (has_any_changed) {
		auto& arr = ed_doc.selection_state->get_selection();
		ed_doc.command_mgr->add_command(new TransformCommand(ed_doc, arr, world_space_of_selected));
		has_any_changed = false;
	}
	update_pivot_and_cached();
}

void ManipulateTransformTool::update() {
	if (state == IDLE)
		return;

	ImGuizmo::SetImGuiContext(eng->get_imgui_context());
	ImGuizmo::SetDrawlist();
	const auto s_pos = UiSystem::inst->get_vp_rect().get_pos();
	const auto s_sz = UiSystem::inst->get_vp_rect().get_size();
	const bool using_ortho = ed_doc.ed_cam.get_is_using_ortho();
	ImGuizmo::SetRect(s_pos.x, s_pos.y, s_sz.x, s_sz.y);
	ImGuizmo::Enable(true);
	ImGuizmo::SetOrthographic(using_ortho);
	// ImGuizmo::GetStyle().TranslationLineArrowSize = 20.0;
	ImGuizmo::GetStyle().TranslationLineThickness = 6.0;
	ImGuizmo::GetStyle().RotationLineThickness = 6.0;
	ImGuizmo::GetStyle().ScaleLineThickness = 6.0;

	const auto mask_to_use = (force_gizmo_on) ? force_operation : operation_mask;

	glm::vec3 snap(-1);
	if (mask_to_use == ImGuizmo::TRANSLATE && ed_has_snap.get_bool())
		snap = glm::vec3(ed_translation_snap.get_float());
	else if (mask_to_use == ImGuizmo::SCALE && ed_has_snap.get_bool())
		snap = glm::vec3(ed_scale_snap.get_float());
	else if (mask_to_use == ImGuizmo::ROTATE && ed_has_snap.get_bool())
		snap = glm::vec3(ed_rotation_snap.get_float());

	auto get_real_op_mask = [](ImGuizmo::OPERATION op, int axis_mask) -> ImGuizmo::OPERATION {
		using namespace ImGuizmo;
		ImGuizmo::OPERATION out{};
		if (op == ImGuizmo::TRANSLATE) {
			if (axis_mask & 1)
				out = out | OPERATION::TRANSLATE_X;
			if (axis_mask & 2)
				out = out | OPERATION::TRANSLATE_Y;
			if (axis_mask & 4)
				out = out | OPERATION::TRANSLATE_Z;
		}
		else if (op == ImGuizmo::ROTATE) {
			if (axis_mask & 1)
				out = out | OPERATION::ROTATE_X;
			if (axis_mask & 2)
				out = out | OPERATION::ROTATE_Y;
			if (axis_mask & 4)
				out = out | OPERATION::ROTATE_Z;
			if (axis_mask == 0xff)
				out = OPERATION::ROTATE;
		}
		else if (op == ImGuizmo::SCALE) {
			if (axis_mask & 1)
				out = out | OPERATION::SCALE_X;
			if (axis_mask & 2)
				out = out | OPERATION::SCALE_Y;
			if (axis_mask & 4)
				out = out | OPERATION::SCALE_Z;
		}
		return out;
	};

	const auto window_sz = UiSystem::inst->get_vp_rect().get_size();
	const float aratio = (float)window_sz.y / window_sz.x;
	const float* const view = glm::value_ptr(ed_doc.vs_setup.view);
	const glm::mat4 friendly_proj_matrix = ed_doc.ed_cam.make_friendly_imguizmo_matrix();
	const float* const proj = glm::value_ptr(friendly_proj_matrix);
	float* model = glm::value_ptr(current_transform_of_group);
	ImGuizmo::SetOrthographic(using_ortho);
	bool good = ImGuizmo::Manipulate(get_force_gizmo_on(), view, proj, get_real_op_mask(mask_to_use, axis_mask), mode,
		model, nullptr, (snap.x > 0) ? &snap.x : nullptr);

	has_any_changed |= good;

	if (ImGuizmo::IsUsingAny() && state == SELECTED) {
		begin_drag(); // was not being used last frame, but now using
	}
	else if (!ImGuizmo::IsUsingAny() && state == MANIPULATING_OBJS) {
		end_drag(); // was using last frame, but now not using. this also saves off transforms into a undoable command
	}
	if (state == MANIPULATING_OBJS) {
		// save off for visible state (command is sent in end_drag)
		if (!get_is_using_for_custom()) {
			auto& ss = ed_doc.selection_state;
			auto& arr = ss->get_selection();
			for (auto elm : arr) {
				auto find = world_space_of_selected.find(elm);
				if (find == world_space_of_selected.end())
					continue; // this is valid, a parent is in the set already
				glm::mat4 ws = current_transform_of_group * glm::inverse(pivot_transform) * find->second;
				EntityPtr e(elm);
				ASSERT(e.get());
				e.get()->set_ws_transform(ws);
			}
		}
	}

	if (is_using())
		ed_doc.inputs.set_focus(this);
}
void ManipulateTransformTool::check_input() {
	const bool is_keyboard_blocked = UiSystem::inst->blocking_keyboard_inputs();
	if (is_keyboard_blocked || UiSystem::inst->is_game_capturing_mouse() || !ed_doc.selection_state->has_any_selected())
		return;
	if (!UiSystem::inst->is_vp_hovered())
		return;
	// if (UiSystem::inst->blocking_keyboard_inputs())
	//	return;

	if (ed_doc.inputs.get_focused() && ed_doc.inputs.get_focused() != this)
		return;

	if (ed_doc.inputs.can_use_mouse_click() && is_hovered())
		ed_doc.inputs.eat_mouse_click();

	const bool has_shift = Input::is_shift_down();
	if (Input::was_key_pressed(SDL_SCANCODE_R)) {

		reset_group_to_pre_transform();

		force_operation = ImGuizmo::ROTATE;

		set_force_gizmo_on(true);
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_G)) {

		reset_group_to_pre_transform();

		force_operation = ImGuizmo::TRANSLATE;

		set_force_gizmo_on(true);
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_X) && get_force_gizmo_on()) {
		reset_group_to_pre_transform();
		if (has_shift)
			axis_mask = 2 | 4;
		else
			axis_mask = 1;
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_Y) && get_force_gizmo_on()) {
		reset_group_to_pre_transform();
		if (has_shift)
			axis_mask = 1 | 4;
		else
			axis_mask = 2;
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_Z) && get_force_gizmo_on()) {
		reset_group_to_pre_transform();
		if (has_shift)
			axis_mask = 1 | 2;
		else
			axis_mask = 4;
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_S) && !Input::is_ctrl_down()) {
		reset_group_to_pre_transform();
		force_operation = ImGuizmo::SCALE;
		mode = ImGuizmo::LOCAL; // local scaling only
		set_force_gizmo_on(true);
	}
}

void ManipulateTransformTool::on_focused_tick() {

	if (Input::was_mouse_pressed(2)) {
		if (get_force_gizmo_on()) {
			reset_group_to_pre_transform();
			set_force_gizmo_on(false);
			ed_doc.inputs.set_focus(nullptr);
			ed_doc.inputs.eat_mouse_click();
		}
	}
	if (Input::was_mouse_pressed(0)) {
		if (get_force_gizmo_on()) {
			set_force_gizmo_on(false);
		}
	}

	if (!is_using() && !Input::is_mouse_down(0)) { // some mf bs right here
		ed_doc.inputs.set_focus(nullptr);
		ed_doc.inputs.eat_mouse_click();
	}
}
