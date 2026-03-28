#pragma once
#include "IInputReciever.h"
#include "ImGuizmo.h"
#include <glm/glm.hpp>
class ManipulateTransformTool : public IInputReciever
{
public:
	ManipulateTransformTool(EditorDoc& ed_doc);
	void update();
	bool is_hovered();
	bool is_using();
	void check_input();
	void on_focused_tick() final;
	string get_name() final { return "manipulate tool"; }

	void stop_using_custom() {
		if (is_using_for_custom) {
			eng->log_to_fullscreen_gui(Info, "Stopped Using Custom Manipulate Tool");
		}
		is_using_for_custom = false;
		custom_user_key = nullptr;
		update_pivot_and_cached();
	}
	void set_start_using_custom(void* key, glm::mat4 transform_to_edit) {
		if (!is_using_for_custom) {
			eng->log_to_fullscreen_gui(Info, "Using Custom Manipulate Tool");
		}

		world_space_of_selected.clear();
		current_transform_of_group = transform_to_edit;
		is_using_for_custom = true;
		custom_user_key = key;
	}
	bool is_using_key_for_custom(void* key) { return key == custom_user_key; }
	glm::mat4 get_custom_transform() { return current_transform_of_group; }
	bool get_is_using_for_custom() const { return is_using_for_custom; }
	void set_force_axis_mask(int i) {
		reset_group_to_pre_transform();
		axis_mask = i;
	}

	void set_force_gizmo_on(bool b) {
		force_gizmo_on = b;
		axis_mask = 0xff;
	}
	void set_force_op(ImGuizmo::OPERATION op) { force_operation = op; }
	bool get_force_gizmo_on() const { return force_gizmo_on; }
	void reset_group_to_pre_transform();

	ImGuizmo::OPERATION get_operation_type() const { return operation_mask; }
	void set_operation_type(ImGuizmo::OPERATION op) { operation_mask = op; }
	void set_mode(ImGuizmo::MODE m) { mode = m; }
	ImGuizmo::MODE get_mode() const { return mode; }
	void update_pivot_and_cached();

private:
	bool force_gizmo_on = false;

	void on_close();
	void on_open();
	void on_component_deleted(Component* ec);
	void on_entity_changes();
	void on_selection_changed();
	void on_prop_change();

	void on_selected_tarnsform_change(uint64_t);

	void begin_drag();
	void end_drag();

	enum StateEnum
	{
		IDLE,
		SELECTED,
		MANIPULATING_OBJS,
	} state = IDLE;

	int axis_mask = 0xff;
	ImGuizmo::OPERATION force_operation = {};
	ImGuizmo::OPERATION operation_mask = ImGuizmo::OPERATION::TRANSLATE;
	ImGuizmo::MODE mode = ImGuizmo::MODE::WORLD;
	bool has_any_changed = false;

	void* custom_user_key = nullptr;
	bool is_using_for_custom = false;

	std::unordered_map<uint64_t, glm::mat4> world_space_of_selected; // pre transform, ie transform of them is

	glm::mat4 current_transform_of_group = glm::mat4(1.0);
	glm::mat4 pivot_transform = glm::mat4(1.f);

	EditorDoc& ed_doc;
};