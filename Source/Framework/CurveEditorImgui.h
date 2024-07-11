#pragma once

#include "imgui.h"
#include "Framework/Util.h"
#include "Framework/Curve.h"
#include <vector>
#include <string>

struct EditingCurve
{
	Curve thecurve;
	std::string name = "";
	bool visible = true;
	Color32 color = COLOR_CYAN;
	void* user = nullptr;
};
#include <glm/glm.hpp>
class CurveEditorImgui
{
public:
	void draw();
	void update(float dt);
private:
	float max_time = 1.0;
	float time = 0.0;
	std::vector<EditingCurve> curves;

	EditingCurve  curve;

	float MAX_TIME = 35.0;

	// grid space
	ImVec2 scale = ImVec2(1, 1);
	const ImVec2 base_scale =ImVec2(1.0/32,1.0/30);
	ImVec2 grid_offset=ImVec2(0,0);

	ImVec2 BASE_SCREENPOS;
	ImVec2 WINDOW_SIZE;

	ImVec2 grid_to_screenspace(ImVec2 grid) const;
	ImVec2 screenspace_to_grid(ImVec2 screen) const;
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
