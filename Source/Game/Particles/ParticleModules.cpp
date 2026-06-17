#include "ParticleModules.h"

static std::string enum_str(const EnumTypeInfo& info, int64_t val) {
	auto* pair = info.find_for_value(val);
	return pair ? pair->name : "";
}
static int64_t enum_val(const EnumTypeInfo& info, const std::string& s) {
	auto* pair = info.find_for_name(s.c_str());
	return pair ? pair->value : 0;
}

// --- MainModule ---

void to_json(nlohmann::json& j, const MainModule& m)
{
	j["duration"] = m.duration;
	j["looping"] = m.looping;
	j["start_lifetime"] = m.start_lifetime;
	j["start_speed"] = m.start_speed;
	j["start_size"] = m.start_size;
	j["start_rotation"] = m.start_rotation;
	j["start_color"] = m.start_color;
	j["gravity_modifier"] = m.gravity_modifier;
	j["simulation_space"] = enum_str(EnumTrait<SimulationSpace>::StaticEnumType, (int64_t)m.simulation_space);
	j["max_particles"] = m.max_particles;
	j["play_on_awake"] = m.play_on_awake;
}

void from_json(const nlohmann::json& j, MainModule& m)
{
	m.duration = j.value("duration", 5.f);
	m.looping = j.value("looping", true);
	if (j.contains("start_lifetime")) j["start_lifetime"].get_to(m.start_lifetime);
	if (j.contains("start_speed")) j["start_speed"].get_to(m.start_speed);
	if (j.contains("start_size")) j["start_size"].get_to(m.start_size);
	if (j.contains("start_rotation")) j["start_rotation"].get_to(m.start_rotation);
	if (j.contains("start_color")) j["start_color"].get_to(m.start_color);
	if (j.contains("gravity_modifier")) j["gravity_modifier"].get_to(m.gravity_modifier);
	if (j.contains("simulation_space"))
		m.simulation_space = (SimulationSpace)enum_val(
			EnumTrait<SimulationSpace>::StaticEnumType, j["simulation_space"].get<std::string>());
	m.max_particles = j.value("max_particles", 1000);
	m.play_on_awake = j.value("play_on_awake", true);
}

// --- EmissionModule ---

void to_json(nlohmann::json& j, const EmissionBurst& b)
{
	j["time"] = b.time;
	j["count"] = b.count;
	j["cycles"] = b.cycles;
	j["interval"] = b.interval;
}

void from_json(const nlohmann::json& j, EmissionBurst& b)
{
	b.time = j.value("time", 0.f);
	if (j.contains("count")) j["count"].get_to(b.count);
	b.cycles = j.value("cycles", 1);
	b.interval = j.value("interval", 0.01f);
}

void to_json(nlohmann::json& j, const EmissionModule& m)
{
	j["enabled"] = m.enabled;
	j["rate_over_time"] = m.rate_over_time;
	auto& ba = j["bursts"] = nlohmann::json::array();
	for (auto& b : m.bursts) {
		nlohmann::json bj;
		to_json(bj, b);
		ba.push_back(bj);
	}
}

void from_json(const nlohmann::json& j, EmissionModule& m)
{
	m.enabled = j.value("enabled", true);
	if (j.contains("rate_over_time")) j["rate_over_time"].get_to(m.rate_over_time);
	m.bursts.clear();
	if (j.contains("bursts")) {
		for (auto& bj : j["bursts"]) {
			EmissionBurst b;
			from_json(bj, b);
			m.bursts.push_back(b);
		}
	}
}

// --- ShapeModule ---

void to_json(nlohmann::json& j, const ShapeModule& m)
{
	j["enabled"] = m.enabled;
	j["shape"] = enum_str(EnumTrait<ParticleShapeType>::StaticEnumType, (int64_t)m.shape);
	j["radius"] = m.radius;
	j["angle"] = m.angle;
	j["arc"] = m.arc;
	j["radius_thickness"] = m.radius_thickness;
	j["position_offset"] = {m.position_offset.x, m.position_offset.y, m.position_offset.z};
	j["rotation_offset"] = {m.rotation_offset.x, m.rotation_offset.y, m.rotation_offset.z};
	j["scale_offset"] = {m.scale_offset.x, m.scale_offset.y, m.scale_offset.z};
}

void from_json(const nlohmann::json& j, ShapeModule& m)
{
	m.enabled = j.value("enabled", true);
	if (j.contains("shape"))
		m.shape = (ParticleShapeType)enum_val(
			EnumTrait<ParticleShapeType>::StaticEnumType, j["shape"].get<std::string>());
	m.radius = j.value("radius", 1.f);
	m.angle = j.value("angle", 25.f);
	m.arc = j.value("arc", 360.f);
	m.radius_thickness = j.value("radius_thickness", 1.f);
	if (j.contains("position_offset")) {
		auto& v = j["position_offset"];
		m.position_offset = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
	}
	if (j.contains("rotation_offset")) {
		auto& v = j["rotation_offset"];
		m.rotation_offset = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
	}
	if (j.contains("scale_offset")) {
		auto& v = j["scale_offset"];
		m.scale_offset = {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
	}
}

// --- VelocityOverLifetimeModule ---

void to_json(nlohmann::json& j, const VelocityOverLifetimeModule& m)
{
	j["enabled"] = m.enabled;
	j["x"] = m.x;
	j["y"] = m.y;
	j["z"] = m.z;
	j["space"] = enum_str(EnumTrait<SimulationSpace>::StaticEnumType, (int64_t)m.space);
	j["orbital_x"] = m.orbital_x;
	j["orbital_y"] = m.orbital_y;
	j["orbital_z"] = m.orbital_z;
	j["radial"] = m.radial;
}

void from_json(const nlohmann::json& j, VelocityOverLifetimeModule& m)
{
	m.enabled = j.value("enabled", false);
	if (j.contains("x")) j["x"].get_to(m.x);
	if (j.contains("y")) j["y"].get_to(m.y);
	if (j.contains("z")) j["z"].get_to(m.z);
	if (j.contains("space"))
		m.space = (SimulationSpace)enum_val(
			EnumTrait<SimulationSpace>::StaticEnumType, j["space"].get<std::string>());
	if (j.contains("orbital_x")) j["orbital_x"].get_to(m.orbital_x);
	if (j.contains("orbital_y")) j["orbital_y"].get_to(m.orbital_y);
	if (j.contains("orbital_z")) j["orbital_z"].get_to(m.orbital_z);
	if (j.contains("radial")) j["radial"].get_to(m.radial);
}

// --- ColorOverLifetimeModule ---

void to_json(nlohmann::json& j, const ColorOverLifetimeModule& m)
{
	j["enabled"] = m.enabled;
	j["color"] = m.color;
}

void from_json(const nlohmann::json& j, ColorOverLifetimeModule& m)
{
	m.enabled = j.value("enabled", false);
	if (j.contains("color")) j["color"].get_to(m.color);
}

// --- SizeOverLifetimeModule ---

void to_json(nlohmann::json& j, const SizeOverLifetimeModule& m)
{
	j["enabled"] = m.enabled;
	j["size"] = m.size;
	j["separate_axes"] = m.separate_axes;
	if (m.separate_axes) {
		j["x"] = m.x;
		j["y"] = m.y;
		j["z"] = m.z;
	}
}

void from_json(const nlohmann::json& j, SizeOverLifetimeModule& m)
{
	m.enabled = j.value("enabled", false);
	if (j.contains("size")) j["size"].get_to(m.size);
	m.separate_axes = j.value("separate_axes", false);
	if (j.contains("x")) j["x"].get_to(m.x);
	if (j.contains("y")) j["y"].get_to(m.y);
	if (j.contains("z")) j["z"].get_to(m.z);
}

// --- RotationOverLifetimeModule ---

void to_json(nlohmann::json& j, const RotationOverLifetimeModule& m)
{
	j["enabled"] = m.enabled;
	j["angular_velocity"] = m.angular_velocity;
	j["separate_axes"] = m.separate_axes;
}

void from_json(const nlohmann::json& j, RotationOverLifetimeModule& m)
{
	m.enabled = j.value("enabled", false);
	if (j.contains("angular_velocity")) j["angular_velocity"].get_to(m.angular_velocity);
	m.separate_axes = j.value("separate_axes", false);
}

// --- TextureSheetModule ---

void to_json(nlohmann::json& j, const TextureSheetModule& m)
{
	j["enabled"] = m.enabled;
	j["tiles_x"] = m.tiles_x;
	j["tiles_y"] = m.tiles_y;
	j["animation"] = enum_str(EnumTrait<TextureSheetAnimation>::StaticEnumType, (int64_t)m.animation);
	j["frame_over_time"] = m.frame_over_time;
	j["start_frame"] = m.start_frame;
	j["cycles"] = m.cycles;
}

void from_json(const nlohmann::json& j, TextureSheetModule& m)
{
	m.enabled = j.value("enabled", false);
	m.tiles_x = j.value("tiles_x", 1);
	m.tiles_y = j.value("tiles_y", 1);
	if (j.contains("animation"))
		m.animation = (TextureSheetAnimation)enum_val(
			EnumTrait<TextureSheetAnimation>::StaticEnumType, j["animation"].get<std::string>());
	if (j.contains("frame_over_time")) j["frame_over_time"].get_to(m.frame_over_time);
	if (j.contains("start_frame")) j["start_frame"].get_to(m.start_frame);
	m.cycles = j.value("cycles", 1);
}

// --- NoiseModule ---

void to_json(nlohmann::json& j, const NoiseModule& m)
{
	j["enabled"] = m.enabled;
	j["strength"] = m.strength;
	j["frequency"] = m.frequency;
	j["scroll_speed"] = m.scroll_speed;
	j["octaves"] = m.octaves;
	j["damping"] = m.damping;
	j["quality"] = enum_str(EnumTrait<NoiseQuality>::StaticEnumType, (int64_t)m.quality);
}

void from_json(const nlohmann::json& j, NoiseModule& m)
{
	m.enabled = j.value("enabled", false);
	if (j.contains("strength")) j["strength"].get_to(m.strength);
	m.frequency = j.value("frequency", 0.5f);
	if (j.contains("scroll_speed")) j["scroll_speed"].get_to(m.scroll_speed);
	m.octaves = j.value("octaves", 1);
	m.damping = j.value("damping", true);
	if (j.contains("quality"))
		m.quality = (NoiseQuality)enum_val(
			EnumTrait<NoiseQuality>::StaticEnumType, j["quality"].get<std::string>());
}

// --- RendererModule ---

void to_json(nlohmann::json& j, const RendererModule& m)
{
	j["enabled"] = m.enabled;
	j["material"] = m.material.get() ? m.material.get()->get_name() : "";
	j["render_mode"] = enum_str(EnumTrait<ParticleRenderMode>::StaticEnumType, (int64_t)m.render_mode);
	j["sort_mode"] = enum_str(EnumTrait<ParticleSortMode>::StaticEnumType, (int64_t)m.sort_mode);
	j["speed_scale"] = m.speed_scale;
	j["length_scale"] = m.length_scale;
}

void from_json(const nlohmann::json& j, RendererModule& m)
{
	m.enabled = j.value("enabled", true);
	std::string mat_path = j.value("material", std::string(""));
	if (!mat_path.empty())
		m.material = g_assets.find<MaterialInstance>(mat_path);
	if (j.contains("render_mode"))
		m.render_mode = (ParticleRenderMode)enum_val(
			EnumTrait<ParticleRenderMode>::StaticEnumType, j["render_mode"].get<std::string>());
	if (j.contains("sort_mode"))
		m.sort_mode = (ParticleSortMode)enum_val(
			EnumTrait<ParticleSortMode>::StaticEnumType, j["sort_mode"].get<std::string>());
	m.speed_scale = j.value("speed_scale", 0.f);
	m.length_scale = j.value("length_scale", 2.f);
}
