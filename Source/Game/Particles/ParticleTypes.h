#pragma once
#include "Framework/CurveEditorImgui.h"
#include "Framework/Util.h"
#include "Framework/EnumDefReflection.h"
#include <json.hpp>
#include <vector>
#include <algorithm>

NEWENUM(MinMaxCurveMode, uint8_t){
	Constant,
	RandomBetweenConstants,
	Curve,
	RandomBetweenCurves,
};

struct MinMaxCurve
{
	MinMaxCurveMode mode = MinMaxCurveMode::Constant;
	float constant_min = 0.f;
	float constant_max = 1.f;
	float curve_scalar = 1.f;
	EditingCurve curve0;
	EditingCurve curve1;

	float evaluate(float normalized_t, float random01) const;
};

void to_json(nlohmann::json& j, const MinMaxCurve& c);
void from_json(const nlohmann::json& j, MinMaxCurve& c);

struct GradientKey
{
	Color32 color = COLOR_WHITE;
	float alpha = 1.f;
	float time = 0.f;
};

struct Gradient
{
	std::vector<GradientKey> keys;

	Color32 evaluate(float t) const;

	void sort_keys() {
		std::sort(keys.begin(), keys.end(),
			[](const GradientKey& a, const GradientKey& b) { return a.time < b.time; });
	}
};

void to_json(nlohmann::json& j, const Gradient& g);
void from_json(const nlohmann::json& j, Gradient& g);

float evaluate_editing_curve(const EditingCurve& curve, float t);

void to_json(nlohmann::json& j, const EditingCurve& c);
void from_json(const nlohmann::json& j, EditingCurve& c);
