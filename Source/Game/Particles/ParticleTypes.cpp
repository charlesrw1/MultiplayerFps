#include "ParticleTypes.h"
#include "Framework/MathLib.h"
#include <glm/glm.hpp>

glm::vec2 bezier_evaluate(float t, const glm::vec2 p0, const glm::vec2& p1,
                           const glm::vec2& p2, const glm::vec2& p3);

float evaluate_editing_curve(const EditingCurve& curve, float t)
{
	auto& points = curve.points;
	if (points.empty())
		return 0.f;
	if (points.size() == 1)
		return points[0].value;

	if (t <= points.front().time)
		return points.front().value;
	if (t >= points.back().time)
		return points.back().value;

	int seg = 0;
	for (int i = 0; i < (int)points.size() - 1; i++) {
		if (t >= points[i].time && t <= points[i + 1].time) {
			seg = i;
			break;
		}
	}

	auto& p0 = points[seg];
	auto& p1 = points[seg + 1];
	float seg_t = (p1.time - p0.time) > 1e-6f
		? (t - p0.time) / (p1.time - p0.time)
		: 0.f;

	if (p0.type == CurvePointType::Constant) {
		return p0.value;
	}
	else if (p0.type == CurvePointType::Linear) {
		return glm::mix(p0.value, p1.value, seg_t);
	}
	else {
		glm::vec2 bp0 = {p0.time, p0.value};
		glm::vec2 bp1 = bp0 + p0.tangent1;
		glm::vec2 bp3 = {p1.time, p1.value};
		glm::vec2 bp2 = bp3 + p1.tangent0;
		auto result = bezier_evaluate(seg_t, bp0, bp1, bp2, bp3);
		return result.y;
	}
}

float MinMaxCurve::evaluate(float normalized_t, float random01) const
{
	switch (mode) {
	case MinMaxCurveMode::Constant:
		return constant_min;
	case MinMaxCurveMode::RandomBetweenConstants:
		return glm::mix(constant_min, constant_max, random01);
	case MinMaxCurveMode::Curve:
		return evaluate_editing_curve(curve0, normalized_t) * curve_scalar;
	case MinMaxCurveMode::RandomBetweenCurves: {
		float v0 = evaluate_editing_curve(curve0, normalized_t);
		float v1 = evaluate_editing_curve(curve1, normalized_t);
		return glm::mix(v0, v1, random01) * curve_scalar;
	}
	default:
		return constant_min;
	}
}

// --- EditingCurve JSON ---

void to_json(nlohmann::json& j, const CurvePoint& p)
{
	j = nlohmann::json{
		{"value", p.value},
		{"time", p.time},
		{"tangent0", {p.tangent0.x, p.tangent0.y}},
		{"tangent1", {p.tangent1.x, p.tangent1.y}},
		{"type", (int)p.type}
	};
}

void from_json(const nlohmann::json& j, CurvePoint& p)
{
	p.value = j.value("value", 0.f);
	p.time = j.value("time", 0.f);
	if (j.contains("tangent0")) {
		auto& t = j["tangent0"];
		p.tangent0 = {t[0].get<float>(), t[1].get<float>()};
	}
	if (j.contains("tangent1")) {
		auto& t = j["tangent1"];
		p.tangent1 = {t[0].get<float>(), t[1].get<float>()};
	}
	p.type = (CurvePointType)j.value("type", 0);
}

void to_json(nlohmann::json& j, const EditingCurve& c)
{
	j = nlohmann::json{
		{"name", c.name},
		{"color", c.color.to_uint()},
		{"points", nlohmann::json::array()}
	};
	for (auto& pt : c.points) {
		nlohmann::json pj;
		to_json(pj, pt);
		j["points"].push_back(pj);
	}
}

void from_json(const nlohmann::json& j, EditingCurve& c)
{
	c.name = j.value("name", std::string(""));
	if (j.contains("color"))
		c.color = Color32(j["color"].get<uint32_t>());
	c.points.clear();
	if (j.contains("points")) {
		for (auto& pj : j["points"]) {
			CurvePoint pt;
			from_json(pj, pt);
			c.points.push_back(pt);
		}
	}
}

// --- MinMaxCurve JSON ---

void to_json(nlohmann::json& j, const MinMaxCurve& c)
{
	j = nlohmann::json{
		{"mode", [&]() -> std::string { auto* p = EnumTrait<MinMaxCurveMode>::StaticEnumType.find_for_value((int64_t)c.mode); return p ? p->name : ""; }()},
		{"min", c.constant_min},
		{"max", c.constant_max},
		{"curve_scalar", c.curve_scalar},
	};
	if (c.mode == MinMaxCurveMode::Curve || c.mode == MinMaxCurveMode::RandomBetweenCurves) {
		nlohmann::json c0j;
		to_json(c0j, c.curve0);
		j["curve0"] = c0j;
	}
	if (c.mode == MinMaxCurveMode::RandomBetweenCurves) {
		nlohmann::json c1j;
		to_json(c1j, c.curve1);
		j["curve1"] = c1j;
	}
}

void from_json(const nlohmann::json& j, MinMaxCurve& c)
{
	if (j.contains("mode")) {
		auto mode_str = j["mode"].get<std::string>();
		auto* pair = EnumTrait<MinMaxCurveMode>::StaticEnumType.find_for_name(mode_str.c_str());
		c.mode = pair ? (MinMaxCurveMode)pair->value : MinMaxCurveMode::Constant;
	}
	c.constant_min = j.value("min", 0.f);
	c.constant_max = j.value("max", 1.f);
	c.curve_scalar = j.value("curve_scalar", 1.f);
	if (j.contains("curve0"))
		from_json(j["curve0"], c.curve0);
	if (j.contains("curve1"))
		from_json(j["curve1"], c.curve1);
}

// --- Gradient JSON ---

Color32 Gradient::evaluate(float t) const
{
	if (keys.empty())
		return COLOR_WHITE;
	if (keys.size() == 1) {
		auto& k = keys[0];
		uint8_t a = (uint8_t)(k.alpha * 255.f);
		return Color32(k.color.r, k.color.g, k.color.b, a);
	}

	t = glm::clamp(t, 0.f, 1.f);

	if (t <= keys.front().time) {
		auto& k = keys.front();
		return Color32(k.color.r, k.color.g, k.color.b, (uint8_t)(k.alpha * 255.f));
	}
	if (t >= keys.back().time) {
		auto& k = keys.back();
		return Color32(k.color.r, k.color.g, k.color.b, (uint8_t)(k.alpha * 255.f));
	}

	int seg = 0;
	for (int i = 0; i < (int)keys.size() - 1; i++) {
		if (t >= keys[i].time && t <= keys[i + 1].time) {
			seg = i;
			break;
		}
	}

	auto& k0 = keys[seg];
	auto& k1 = keys[seg + 1];
	float frac = (k1.time - k0.time) > 1e-6f
		? (t - k0.time) / (k1.time - k0.time)
		: 0.f;

	uint8_t r = (uint8_t)glm::mix((float)k0.color.r, (float)k1.color.r, frac);
	uint8_t g = (uint8_t)glm::mix((float)k0.color.g, (float)k1.color.g, frac);
	uint8_t b = (uint8_t)glm::mix((float)k0.color.b, (float)k1.color.b, frac);
	float a0 = k0.alpha;
	float a1 = k1.alpha;
	uint8_t a = (uint8_t)(glm::mix(a0, a1, frac) * 255.f);
	return Color32(r, g, b, a);
}

void to_json(nlohmann::json& j, const GradientKey& k)
{
	j = nlohmann::json{
		{"r", k.color.r}, {"g", k.color.g}, {"b", k.color.b},
		{"alpha", k.alpha}, {"time", k.time}
	};
}

void from_json(const nlohmann::json& j, GradientKey& k)
{
	k.color.r = j.value("r", (uint8_t)255);
	k.color.g = j.value("g", (uint8_t)255);
	k.color.b = j.value("b", (uint8_t)255);
	k.color.a = 255;
	k.alpha = j.value("alpha", 1.f);
	k.time = j.value("time", 0.f);
}

void to_json(nlohmann::json& j, const Gradient& g)
{
	j = nlohmann::json::array();
	for (auto& k : g.keys) {
		nlohmann::json kj;
		to_json(kj, k);
		j.push_back(kj);
	}
}

void from_json(const nlohmann::json& j, Gradient& g)
{
	g.keys.clear();
	for (auto& kj : j) {
		GradientKey k;
		from_json(kj, k);
		g.keys.push_back(k);
	}
	g.sort_keys();
}
