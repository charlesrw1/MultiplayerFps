#pragma once

#include "imgui.h"
#include "Framework/Util.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>

#include "EnumDefReflection.h"
#include "ReflectionProp.h"
#include "ReflectionMacros.h"
#include "ArrayReflection.h"

enum class CurvePointType
{
	Linear,	// linear interp
	Constant,	// no interp
	SplitTangents,	// 2 handle free tangents
	Aligned,		// free tangents but they are kept aligned
};
ENUM_HEADER(CurvePointType);

class CurvePoint
{
public:

	float value = 0.0;
	float time = 0.0;
	glm::vec2 tangent0 = glm::vec2(-1, 0);
	glm::vec2 tangent1 = glm::vec2(1, 0);
	CurvePointType type = CurvePointType::Linear;
};

struct EditingCurve
{
	static const PropertyInfoList* get_props() {
		static StdVectorCallback<CurvePoint> vecdef_points(get_cruve_point_prop());
		START_PROPS(EditingCurve)
			REG_STDSTRING(name,PROP_SERIALIZE),
			REG_INT(color,PROP_SERIALIZE,""),/* reg it as a uint*/
			REG_STDVECTOR(points,PROP_SERIALIZE)
		END_PROPS(EditingCurve)
	}

	static const PropertyInfoList* get_cruve_point_prop() {
		static PropertyInfo info[] = {
			make_struct_property("_value",0/* 0 offset */,PROP_DEFAULT, "CurvePointSerialize")
		};
		static PropertyInfoList list = { info,1,"_CurvePoint" };
		return &list;
	}

	std::vector<CurvePoint> points;
	std::string name = "";
	bool visible = true;
	Color32 color = COLOR_PINK;

	uint32_t curve_id = 0;
};
class CurveEditorImgui
{
public:
	CurveEditorImgui() {}

	void draw();
	// Max value on the X axis (min X axis is always 0)
	float max_x_value = 35.0;

	// Max/min values on the Y axis
	float min_y_value = 0.0;
	float max_y_value = 1.0;

	// current position of the scrubber, in X units
	float current_time = 0.0;
	bool set_scrubber_this_frame = false;

	// grid snap settings
	bool snap_scrubber_to_grid = false;
	bool enable_grid_snapping_x = true;
	bool enable_grid_snapping_y = true;
	ImVec2 grid_snap_size = ImVec2(1, 0.1);

	const std::vector<EditingCurve>& get_curve_array() const { return curves; }
	void add_curve(EditingCurve curve) {
		curve.curve_id = current_id_value++;
		curves.push_back(curve);
	}
	void clear_all() {
		curves.clear();
		set_selected_curve(-1);
		current_id_value = 0;
		dragging_point = false;
		dragged_point_index = -1;
		dragged_point_type = 0;
		point_index_for_popup = -1;
	}
private:
	void draw_editor_space();

	std::vector<EditingCurve> curves;

	// Scale is what gets changed with zooming, modifies base_scale
	ImVec2 scale = ImVec2(1, 15);
	// Base scale is constant, defines a screenspace -> grid unit factor
	const ImVec2 base_scale =ImVec2(1.0/32,-1.0/30);
	// Offset, defined in grid space
	ImVec2 grid_offset=ImVec2(0,1);

	// used internally for convenience
	ImVec2 BASE_SCREENPOS;	// base screenpos corner of graph content region
	ImVec2 WINDOW_SIZE;	// not actually the window size :P, its the content region that the graph draws into in the imgui table

	// state stuff
	int selected_curve = -1;
	// these are all indexing into selected_curve
	bool dragging_point = false;
	int dragged_point_index = -1;
	int dragged_point_type = 0;   // 0 = point, 1=tangent0,2=tangent1
	int point_index_for_popup = -1;

	bool dragging_scrubber = false;

	ImVec2 clickpos{};

	bool started_pan = false;
	ImVec2 pan_start = {};

	// used to identify curves internally
	uint32_t current_id_value = 0;

	void set_selected_curve(int index) {
		if (selected_curve != index) {
			// remove any state from setting points
			dragging_point = false;
			dragged_point_index = -1;
			dragged_point_type = 0;
			point_index_for_popup = -1;

			selected_curve = index;
		}
	}
	ImVec2 grid_to_screenspace(ImVec2 grid) const;
	ImVec2 screenspace_to_grid(ImVec2 screen) const;

	void clamp_point_to_grid(ImVec2& gridspace) {
		if (enable_grid_snapping_x) {
			gridspace.x = std::round(gridspace.x / grid_snap_size.x) * grid_snap_size.x;
		}
		if (enable_grid_snapping_y) {
			gridspace.y = std::round(gridspace.y / grid_snap_size.y) * grid_snap_size.y;
		}
		if (gridspace.x < 0)
			gridspace.x = 0;
		if (gridspace.x > max_x_value)
			gridspace.x = max_x_value;
		if (gridspace.y < min_y_value) gridspace.y = min_y_value;
		if (gridspace.y > max_y_value) gridspace.y = max_y_value;
	}
};

class SequencerImgui;
class SequencerEditorItem
{
public:
	virtual std::string get_name() { return "placeholder"; }
	bool instant_item = false;
	float time_start = 0;
	float time_end = 0;
	Color32 color=COLOR_CYAN;

	int track_index = 0;

	friend class SequencerImgui;
};


class SequencerImgui
{
public:
	void draw();
	virtual void context_menu_callback() {}

	void clear_all() {
		selectedEntry = -1;
		is_dragging_selected = false;
		items.clear();
	}
	std::vector<std::unique_ptr<SequencerEditorItem>>& get_item_array() { return items; }

	// current position of the scrubber, in X units
	float current_time = 0.0;

	bool set_scrubber_this_frame = false;

	int get_selected_index() const { return selectedEntry; }

	void add_item_direct(SequencerEditorItem* item) {
		items.push_back(std::unique_ptr<SequencerEditorItem>(item));
	}

	void add_item_from_menu(SequencerEditorItem* item) {
		auto mousepos = ImGui::GetMousePos();
		auto gridspace = screenspace_to_grid(mousepos);

		clamp_point_to_grid(gridspace);

		if (moving_right_side) {
			item->time_end = gridspace.x;
		}
		else {
			item->time_start = gridspace.x;
		}
		if (!item->instant_item) {
			if (item->time_end <= item->time_start)
				item->time_end = item->time_start + 1.0;
		}

		// find track index
		mousepos.y -= BASE_SCREENPOS.y;
		mousepos.y -= 20.0;
		int track = std::floor(mousepos.y / 20.0);
		item->track_index = track;
		if (item->track_index < 0)item->track_index = 0;
		if (item->track_index > 4)item->track_index = 4;
		items.push_back(std::unique_ptr<SequencerEditorItem>(item));
	}

	// Max value on the X axis (min X axis is always 0)
	float max_x_value = 35.0;


private:
	std::vector<std::unique_ptr<SequencerEditorItem>> items;
	bool draw_items();


	bool is_dragging_selected = false;
	int selectedEntry = -1;
	bool moving_right_side = false;

	int number_of_event_rows() const {
		int max = 0;
		for (int i = 0; i < items.size(); i++)
			if (items[i]->track_index > max)
				max = items[i]->track_index;
		return max + 1;
	}
	int number_of_event_rows_exclude(int ex) const {
		int max = 0;
		for (int i = 0; i < items.size(); i++)
			if (items[i]->track_index > max && i != ex)
				max = items[i]->track_index;
		return max + 1;
	}

	// Copied from curve editor :P

	// Max/min values on the Y axis
	float min_y_value = 0.0;
	float max_y_value = 1.0;


	// grid snap settings
	bool snap_scrubber_to_grid = false;
	bool enable_grid_snapping_x = true;
	bool enable_grid_snapping_y = true;
	ImVec2 grid_snap_size = ImVec2(1, 0.1);


	// Scale is what gets changed with zooming, modifies base_scale
	ImVec2 scale = ImVec2(1, 1);
	// Base scale is constant, defines a screenspace -> grid unit factor
	const ImVec2 base_scale = ImVec2(1.0 / 32, -1.0 / 30);
	// Offset, defined in grid space
	ImVec2 grid_offset = ImVec2(0, 0);

	// used internally for convenience
	ImVec2 BASE_SCREENPOS;	// base screenpos corner of graph content region
	ImVec2 WINDOW_SIZE;	// not actually the window size :P, its the content region that the graph draws into in the imgui table

	bool dragging_scrubber = false;

	ImVec2 clickpos{};

	bool started_pan = false;
	ImVec2 pan_start = {};

	ImVec2 grid_to_screenspace(ImVec2 grid) const;
	ImVec2 screenspace_to_grid(ImVec2 screen) const;

	void clamp_point_to_grid(ImVec2& gridspace) {
		if (enable_grid_snapping_x) {
			gridspace.x = std::round(gridspace.x / grid_snap_size.x) * grid_snap_size.x;
		}
		if (enable_grid_snapping_y) {
			gridspace.y = std::round(gridspace.y / grid_snap_size.y) * grid_snap_size.y;
		}
		if (gridspace.x < 0)
			gridspace.x = 0;
		if (gridspace.x > max_x_value)
			gridspace.x = max_x_value;
		if (gridspace.y < min_y_value) gridspace.y = min_y_value;
		if (gridspace.y > max_y_value) gridspace.y = max_y_value;
	}
};
