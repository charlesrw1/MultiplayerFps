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

NEWENUM(CurvePointType,uint8_t)
{
	Linear,	// linear interp
	Constant,	// no interp
	SplitTangents,	// 2 handle free tangents
	Aligned,		// free tangents but they are kept aligned
};


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

class CurveEditorImgui;

// subclass this to implment whatever custom behavior you want for events
class SequencerEditorItem
{
public:
	virtual ~SequencerEditorItem() {  }
	virtual std::string get_name() { return "placeholder"; }

	bool instant_item = false;	// if true, then event will have 2 handles, one for start and one for end, "duration events"
	float time_start = 0;
	float time_end = 0;
	Color32 color = COLOR_CYAN;
	float y_coord = 0.0;

	friend class CurveEditorImgui;
};

using CurveEditContextMenuCallback = void(*)(CurveEditorImgui*);
class CurveEditorImgui
{
public:
	CurveEditorImgui() {}

	// call this every imgui frame
	void draw();

	std::string window_name = "Curve Editor";

	// this callback is used when right-clicking on canvas when "Events" is selected
	// use this to do ImGui::x() calls to add events, add with add_item_from_menu()
	CurveEditContextMenuCallback callback = nullptr;
	// can use this for whatever, to access in callback
	void* user_ptr = nullptr;

	// if true, show the "Add Row" button to add curves manually
	bool show_add_curve_button = true;

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
	const std::vector<std::unique_ptr<SequencerEditorItem>>& get_event_array() { return events; }

	// add a curve to edit
	void add_curve(EditingCurve curve) {
		curve.curve_id = current_id_value++;
		curves.push_back(curve);
	}

	void clear_all() {
		curves.clear();
		set_selected_curve(-1);
		current_id_value = 0;
		dragged_point_index = -1;
		dragged_point_type = 0;
		dragged_point_index = -1;

		events.clear();
		selected_curve_or_event = -1;
		is_dragging_selected = false;
	}

	// add an event, note that CurveEditor manages memory and will call destructor if event is deleted
	void add_item_from_menu(std::unique_ptr<SequencerEditorItem> item) {
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
		item->y_coord = gridspace.y;
		events.push_back(std::move(item));
	}

	SequencerEditorItem* get_selected_event() const {
		return is_selected_event_valid() ? events.at(selected_curve_or_event).get() : nullptr;
	}
private:
	bool is_curve_selected(int index) const {
		return !selecting_event && index == selected_curve_or_event;
	}
	bool is_event_selected(int index) const {
		return selecting_event && index == selected_curve_or_event;
	}
	bool is_selecting_a_curve() const {
		return !selecting_event && selected_curve_or_event != -1;
	}
	bool is_selected_curve_valid() const {
		return !selecting_event && selected_curve_or_event >= 0 && selected_curve_or_event < curves.size();
	}
	bool is_selected_point_valid() const {
		return is_selected_curve_valid() && dragged_point_index >= 0 && dragged_point_index < curves.at(selected_curve_or_event).points.size();
	}
	bool is_selected_event_valid() const {
		return selecting_event && selected_curve_or_event >= 0 && selected_curve_or_event < events.size();
	}

	void draw_editor_space();

	std::vector<EditingCurve> curves;
	std::vector<std::unique_ptr<SequencerEditorItem>> events;

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
	bool selecting_event = false;
	bool drawing_events = true;
	int selected_curve_or_event = -1;

	// for events
	bool is_dragging_selected = false;
	bool moving_right_side = false;

	// these are all indexing into selected_curve
	int dragged_point_index = -1;
	int dragged_point_type = 0;   // 0 = point, 1=tangent0,2=tangent1

	bool dragging_scrubber = false;

	ImVec2 clickpos{};

	bool started_pan = false;
	ImVec2 pan_start = {};

	// used to identify curves internally
	uint32_t current_id_value = 0;

	void set_selected_curve(int index) {
		if (selecting_event || selected_curve_or_event != index) {
			sys_print(Debug, "set selected curve %d\n", index);

			// remove any state from setting points
			is_dragging_selected = false;
			dragged_point_index = -1;
			dragged_point_type = 0;
			dragged_point_index = -1;

			selected_curve_or_event = index;
			selecting_event = false;
		}
	}
	void set_selected_event(int index, bool dragging_left) {
		sys_print(Debug, "set selected event %d\n",index);
		selecting_event = true;
		is_dragging_selected = true;
		moving_right_side = !dragging_left;
		selected_curve_or_event = index;
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