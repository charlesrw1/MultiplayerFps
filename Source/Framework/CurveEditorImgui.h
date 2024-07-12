#pragma once

#include "imgui.h"
#include "Framework/Util.h"
#include "Framework/Curve.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct EditingCurve
{
	Curve thecurve;
	std::string name = "";
	bool visible = true;
	Color32 color = COLOR_PINK;
};
class CurveEditorImgui
{
public:
	CurveEditorImgui() {
	}
	void draw();
	void update(float dt);
private:
	void draw_editor_space();

	std::vector<EditingCurve> curves;

	// Max value on the X axis (min X axis is always 0)
	float MAX_TIME = 35.0;

	// Max/min values on the Y axis
	float MIN_VALUE = 0.0;
	float MAX_VALUE = 1.0;

	bool enable_grid_snapping_x = true;
	bool enable_grid_snapping_y = true;
	ImVec2 grid_snap_size = ImVec2(1, 0.1);

	// Scale is what gets changed with zooming, modifies base_scale
	ImVec2 scale = ImVec2(1, 1);
	// Base scale is constant, defines a screenspace -> grid unit factor
	const ImVec2 base_scale =ImVec2(1.0/32,1.0/30);
	// Offset, defined in grid space
	ImVec2 grid_offset=ImVec2(0,0);

	// used internally for convenience
	ImVec2 BASE_SCREENPOS;
	ImVec2 WINDOW_SIZE;

	// state stuff
	int selected_curve = -1;
	// these are all indexing into selected_curve
	bool dragging_point = false;
	int dragged_point_index = -1;
	int dragged_point_type = 0;   // 0 = point, 1=tangent0,2=tangent1
	int point_index_for_popup = -1;

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
		if (gridspace.x > MAX_TIME)
			gridspace.x = MAX_TIME;
		if (gridspace.y < MIN_VALUE) gridspace.y = MIN_VALUE;
		if (gridspace.y > MAX_VALUE) gridspace.y = MAX_VALUE;
	}
};

class SequencerImgui;
class SequencerEditorItem
{
public:
	virtual std::string get_name() { return "placeholder"; }
	bool instant_item = false;
	int time_start = 0;
	int time_end = 0;
	Color32 color=COLOR_CYAN;
private:
	int track_index = 0;

	friend class SequencerImgui;
};


class SequencerImgui
{
public:
	void draw();
	virtual void context_menu_callback() {}

	void add_item(SequencerEditorItem* item) {
		items.push_back(std::unique_ptr<SequencerEditorItem>(item));
	}
private:
	int GetFrameMax() const { return frameMax; }
	int GetFrameMin() const { return frameMin; }


	int firstFrame = 0;
	int frameMin = 0;
	int frameMax = 100;
	int currentFrame = 0;

	void draw_header(int first_frame, float width, int* current_frame);
	void draw_scrollbar(int* first_frame, float width, int current_frame);
	void draw_items(float timelie_width);

	float frame_pixel_width_target = 15.f;
	float frame_pixel_width = 15.f;
	bool MovingScrollBar = false;
	bool MovingCurrentFrame = false;
	bool panningView = false;
	ImVec2 panningViewSource;
	int panningViewFrame;

	int selectedEntry = -1;
	int movingEntry = -1;
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

	std::vector<std::unique_ptr<SequencerEditorItem>> items;
};
