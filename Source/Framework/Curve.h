#pragma once
#include <vector>
#include "Framework/Handle.h"
#include "Framework/CurveEditorImgui.h"
#include "Framework/StringName.h"
class FileWriter;
class FileReader;
class BakedCurve
{
public:
	void bake_from(const std::vector<EditingCurve>& curves, float max_time, float min_y, float frames_per_second);
	float evaluate_named(StringName name, float time);
	float evaluate(float time);
	void write_to(FileWriter& out);
	void read_from(FileReader& in);

	struct track {
		StringName name;
		int keyframe_start = 0;
		// if keyframe_start < 0, then track only has 1 keyframe
		float min_val=0.0;
	};
	int total_keyframes = 0;
	float total_length = 0.0;
	std::vector<track> tracks;
	std::vector<float> floats;
};