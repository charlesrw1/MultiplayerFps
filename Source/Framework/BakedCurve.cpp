// BakedCurve: serialization and baking from EditingCurve data.
// See: Framework/Curve.h for struct definition.

#include "Framework/Curve.h"
#include "BinaryReadWrite.h"
#include "CurveEditorImgui.h"
#include <algorithm>
#include <cassert>
#include <glm/glm.hpp>

// Forward declaration of bezier helper defined in CurveEditorImgui.cpp
glm::vec2 bezier_evaluate(float t, const glm::vec2 p0, const glm::vec2& p1,
                           const glm::vec2& p2, const glm::vec2& p3);

class ClassBase;
#include "AddClassToFactory.h"

void BakedCurve::write_to(FileWriter& out)
{
	ASSERT(total_keyframes >= 0);
	out.write_int32(total_keyframes);
	out.write_float(total_length);
}

void BakedCurve::read_from(FileReader& in)
{
	ASSERT(true); // placeholder; reading not yet implemented
}

void BakedCurve::bake_from(const std::vector<EditingCurve>& curves, float max_time,
                            float min_y, float frames_per_second)
{
	ASSERT(max_time > 0.0f && frames_per_second > 0.0f);

	this->total_length    = max_time;
	this->total_keyframes = (int)(max_time * frames_per_second);

	const float step = 1.0f / frames_per_second;

	for (int curve_idx = 0; curve_idx < (int)curves.size(); curve_idx++) {
		auto& curve  = curves[curve_idx];
		auto& points = curve.points;

		struct outstruct
		{
			float time     = 0;
			float value    = 0;
			bool  constant = false;
		};

		std::vector<outstruct> outsorted;

		// Sample each segment
		for (int pointidx = 0; pointidx < (int)points.size() - 1; pointidx++) {
			auto& point = points[pointidx];

			if (point.type == CurvePointType::Constant) {
				const float interval = points[pointidx + 1].time - point.time;
				for (float t = 0.0f; t < interval; t += step)
					outsorted.push_back({point.time + t, point.value, true});

			} else if (point.type == CurvePointType::Linear) {
				const float interval = points[pointidx + 1].time - point.time;
				const float slope    = (points[pointidx + 1].value - point.value) / interval;
				for (float t = 0.0f; t < interval; t += step)
					outsorted.push_back({point.time + t, point.value + slope * t});

			} else if (point.type == CurvePointType::SplitTangents ||
			           point.type == CurvePointType::Aligned) {
				const int BEZIER_CURVE_SUBDIV = 30;
				glm::vec2 pointsout[BEZIER_CURVE_SUBDIV];
				glm::vec2 p0 = {point.time, point.value};
				glm::vec2 p1 = p0 + point.tangent1;
				glm::vec2 p3 = {points[pointidx + 1].time, points[pointidx + 1].value};
				glm::vec2 p2 = p3 + points[pointidx + 1].tangent0;
				for (int j = 0; j < BEZIER_CURVE_SUBDIV - 1; j++) {
					float t    = j / (float(BEZIER_CURVE_SUBDIV) - 1.0f);
					pointsout[j] = bezier_evaluate(t, p0, p1, p2, p3);
					outsorted.push_back({pointsout[j].x, pointsout[j].y});
				}
			}
		}

		// Flat extensions before the first and after the last point
		if (!points.empty()) {
			auto& pointfront = points.front();
			float interval   = pointfront.time - 0.0f;
			for (float t = 0.0f; t < interval; t += step)
				outsorted.push_back({0.0f + t, pointfront.value});

			auto& pointback = points.back();
			interval        = max_time - pointback.time;
			for (float t = 0.0f; t < interval; t += step)
				outsorted.push_back({pointback.time + t, pointback.value});
		}

		// Sort and resample to a uniform keyframe grid
		std::vector<outstruct> outfinal;
		outfinal.resize(total_keyframes);
		std::sort(outsorted.begin(), outsorted.end(),
				  [](const auto& p1, const auto& p2) { return p1.time < p2.time; });

		for (int i = 0; i < total_keyframes; i++) {
			const float time = i * frames_per_second;
			auto find = std::lower_bound(outsorted.begin(), outsorted.end(), time,
										 [](const outstruct& p1, float t) { return p1.time < t; });
			if (find == outsorted.end()) {
				outfinal[i] = outsorted.front();
			} else {
				const int index = (int)std::distance(outsorted.begin(), find);
				if (index > 0 && !find->constant) {
					float this_time = find->time;
					float prev_time = outsorted[index - 1].time;
					assert(prev_time < this_time);
					float INTERP      = (time - prev_time) / (this_time - prev_time);
					float interped_val = glm::mix(outsorted[index - 1].value, find->value, INTERP);
					outfinal[i]       = {time, interped_val, false};
				} else {
					outfinal[i] = *find;
				}
			}
		}
	}
}
