#pragma once
#include "ParticleTypes.h"
#include "Render/MaterialPublic.h"
#include "Assets/AssetDatabase.h"
#include <string>
#include <vector>

NEWENUM(SimulationSpace, uint8_t){
	Local,
	World,
};

NEWENUM(ParticleShapeType, uint8_t){
	Sphere,
	Hemisphere,
	Cone,
	Box,
	Circle,
};

NEWENUM(ParticleRenderMode, uint8_t){
	Billboard,
	StretchedBillboard,
	Mesh,
};

NEWENUM(ParticleSortMode, uint8_t){
	None,
	ByDistance,
	OldestFirst,
	YoungestFirst,
};

NEWENUM(TextureSheetAnimation, uint8_t){
	WholeSheet,
	SingleRow,
};

NEWENUM(NoiseQuality, uint8_t){
	Low,
	Medium,
	High,
};

struct MainModule
{
	float duration = 5.f;
	bool looping = true;
	MinMaxCurve start_lifetime;
	MinMaxCurve start_speed;
	MinMaxCurve start_size;
	MinMaxCurve start_rotation;
	Gradient start_color;
	MinMaxCurve gravity_modifier;
	SimulationSpace simulation_space = SimulationSpace::Local;
	int max_particles = 1000;
	bool play_on_awake = true;

	MainModule() {
		start_lifetime.constant_min = 1.f;
		start_speed.constant_min = 5.f;
		start_size.constant_min = 1.f;
		gravity_modifier.constant_min = 0.f;
		start_color.keys.push_back({COLOR_WHITE, 1.f, 0.f});
	}
};

struct EmissionBurst
{
	float time = 0.f;
	MinMaxCurve count;
	int cycles = 1;
	float interval = 0.01f;
};

struct EmissionModule
{
	bool enabled = true;
	MinMaxCurve rate_over_time;
	std::vector<EmissionBurst> bursts;

	EmissionModule() {
		rate_over_time.constant_min = 10.f;
	}
};

struct ShapeModule
{
	bool enabled = true;
	ParticleShapeType shape = ParticleShapeType::Cone;
	float radius = 1.f;
	float angle = 25.f;
	float arc = 360.f;
	float radius_thickness = 1.f;
	glm::vec3 position_offset = glm::vec3(0.f);
	glm::vec3 rotation_offset = glm::vec3(0.f);
	glm::vec3 scale_offset = glm::vec3(1.f);
};

struct VelocityOverLifetimeModule
{
	bool enabled = false;
	MinMaxCurve x, y, z;
	SimulationSpace space = SimulationSpace::Local;
	MinMaxCurve orbital_x, orbital_y, orbital_z;
	MinMaxCurve radial;
};

struct ColorOverLifetimeModule
{
	bool enabled = false;
	Gradient color;
};

struct SizeOverLifetimeModule
{
	bool enabled = false;
	MinMaxCurve size;
	bool separate_axes = false;
	MinMaxCurve x, y, z;

	SizeOverLifetimeModule() {
		size.mode = MinMaxCurveMode::Curve;
		size.curve_scalar = 1.f;
	}
};

struct RotationOverLifetimeModule
{
	bool enabled = false;
	MinMaxCurve angular_velocity;
	bool separate_axes = false;
};

struct TextureSheetModule
{
	bool enabled = false;
	int tiles_x = 1;
	int tiles_y = 1;
	TextureSheetAnimation animation = TextureSheetAnimation::WholeSheet;
	MinMaxCurve frame_over_time;
	MinMaxCurve start_frame;
	int cycles = 1;
};

struct NoiseModule
{
	bool enabled = false;
	MinMaxCurve strength;
	float frequency = 0.5f;
	MinMaxCurve scroll_speed;
	int octaves = 1;
	bool damping = true;
	NoiseQuality quality = NoiseQuality::High;

	NoiseModule() {
		strength.constant_min = 1.f;
	}
};

struct ForceOverLifetimeModule
{
	bool enabled = false;
	MinMaxCurve x, y, z;
	SimulationSpace space = SimulationSpace::Local;
};

struct LimitVelocityOverLifetimeModule
{
	bool enabled = false;
	MinMaxCurve speed;
	float dampen = 1.f;
	bool separate_axes = false;
	MinMaxCurve x, y, z;
	SimulationSpace space = SimulationSpace::Local;
};

struct RendererModule
{
	bool enabled = true;
	AssetPtr<MaterialInstance> material;
	ParticleRenderMode render_mode = ParticleRenderMode::Billboard;
	ParticleSortMode sort_mode = ParticleSortMode::None;
	float speed_scale = 0.f;
	float length_scale = 2.f;
};

// JSON serialization for all modules
void to_json(nlohmann::json& j, const MainModule& m);
void from_json(const nlohmann::json& j, MainModule& m);
void to_json(nlohmann::json& j, const EmissionModule& m);
void from_json(const nlohmann::json& j, EmissionModule& m);
void to_json(nlohmann::json& j, const ShapeModule& m);
void from_json(const nlohmann::json& j, ShapeModule& m);
void to_json(nlohmann::json& j, const VelocityOverLifetimeModule& m);
void from_json(const nlohmann::json& j, VelocityOverLifetimeModule& m);
void to_json(nlohmann::json& j, const ColorOverLifetimeModule& m);
void from_json(const nlohmann::json& j, ColorOverLifetimeModule& m);
void to_json(nlohmann::json& j, const SizeOverLifetimeModule& m);
void from_json(const nlohmann::json& j, SizeOverLifetimeModule& m);
void to_json(nlohmann::json& j, const RotationOverLifetimeModule& m);
void from_json(const nlohmann::json& j, RotationOverLifetimeModule& m);
void to_json(nlohmann::json& j, const TextureSheetModule& m);
void from_json(const nlohmann::json& j, TextureSheetModule& m);
void to_json(nlohmann::json& j, const NoiseModule& m);
void from_json(const nlohmann::json& j, NoiseModule& m);
void to_json(nlohmann::json& j, const ForceOverLifetimeModule& m);
void from_json(const nlohmann::json& j, ForceOverLifetimeModule& m);
void to_json(nlohmann::json& j, const LimitVelocityOverLifetimeModule& m);
void from_json(const nlohmann::json& j, LimitVelocityOverLifetimeModule& m);
void to_json(nlohmann::json& j, const RendererModule& m);
void from_json(const nlohmann::json& j, RendererModule& m);
