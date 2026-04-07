#include "BikeHeaders.h"

#include "Game/GameplayStatic.h"

// Defined in BikeWind.cpp
extern glm::vec3 wind_direction;
extern float     wind_speed;
extern float     wind_gust_factor;
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "Physics/Physics2.h"
#include "imgui.h"
#include <cfloat>

// ============================================================
// Steering / physics tuning vars
// ============================================================

static float steer_speed_gate_lo   = 6.2f;   // m/s — speed_factor = 0 below this
static float steer_speed_gate_hi   = 18.f;   // m/s — speed_factor = 1 above this
static float steer_lean_threshold  = 0.55f;  // stick fraction at which inertia starts
static float steer_lean_range      = 0.35f;  // stick range over which inertia ramps to full
static float steer_build_lo        = 0.152f; // build smoothing at low speed (snappy)
static float steer_build_hi        = 0.185f; // build smoothing at high speed (gradual)
static float steer_release_lo      = 0.000f; // release smoothing when not committed
static float steer_release_hi      = 0.500f; // release smoothing when deeply committed
static float steer_max_deg         = 45.f;   // max physical steer angle (degrees)
static float steer_min_radius      = 1.5f;   // minimum turn radius (m)
static float steer_radius_coeff    = 0.09f;  // speed² coefficient for min radius
static float bike_gear_shift_cooldown = 3.f;

// ============================================================
// BikeObject
// ============================================================

void BikeObject::start()
{
	set_ticking(true);
	auto* model = get_owner()->create_component<MeshComponent>();
	model->set_model(Model::load("cube1m.cmdl"));
	bike_direction = glm::vec3(0.f, 0.f, 1.f);
}

void BikeObject::update()
{
	GameplayStatic::reset_debug_text_height();
	if (input)
		input->evaluate(this);
}

BikeObject::ControlInput BikeObject::update_tick(ControlInput ci)
{
	const float dt = eng->get_dt();

	// --- Physics constants ---
	const float mass           = 83.f;    // kg  (rider ~75 + bike ~8)
	const float wheel_circ     = 2.1f;    // m   (700c wheel ~2.1m circumference)
	const float CdA            = 0.3f;    // m^2 (drag area, hoods position)
	const float air_density    = 1.225f;  // kg/m^3
	const float roll_resist    = 0.004f;  // dimensionless rolling resistance coeff
	const float gravity        = 9.81f;
	const float max_brake_decel = 7.f;   // m/s^2 at full brake lever

	// --- Stamina tick ---
	{
		StaminaState& s = stamina;
		const RiderStats& r = rider;

		// Effective FTP: glycogen_factor 1.0 (fresh) → 0.55 (cooked)
		const float glycogen_factor = 0.55f + 0.45f * s.glycogen;
		s.effective_ftp = r.base_ftp * glycogen_factor;

		// Power ceiling: scales down as W' depletes
		const float w_frac = s.w_prime / r.w_prime_max;
		s.power_ceiling = s.effective_ftp + (r.sprint_watts - s.effective_ftp) * w_frac;

		// Clamp requested power to physiological ceiling
		s.actual_power = glm::min(ci.power, glm::max(0.f, s.power_ceiling));
		ci.power = s.actual_power;

		const float recovery_percentage = 0.85;
		// W' drain (watts above FTP cost W')
		if (s.actual_power > s.effective_ftp) {
			s.w_prime -= (s.actual_power - s.effective_ftp) * dt;
			s.w_prime  = glm::max(0.f, s.w_prime);
		} else if (s.actual_power < s.effective_ftp * recovery_percentage) {
			// Recovery in zone 2 and below (~22 J/s at full zone 1 effort)
			const float zone2_factor = 1.f - s.actual_power / (s.effective_ftp * recovery_percentage);
			s.w_prime += 22.f * zone2_factor * dt;
			s.w_prime  = glm::min(s.w_prime, r.w_prime_max);
		}

		// Glycogen drain — disproportionately faster above FTP
		// At FTP (ratio=1), depletes ~0.35 over 70min.  Sprint (ratio~3) is 5x faster.
		if (s.actual_power > 0.f) {
			const float ratio = s.actual_power / s.effective_ftp;
			s.glycogen -= 0.0000833f * glm::pow(ratio, 1.5f) * dt;
			s.glycogen  = glm::max(0.f, s.glycogen);
		}

		// Lactate / O2-debt: accumulates from efforts above FTP, decays tau~5min (300s).
		// This raises the HR floor during recovery from repeated hard efforts.
		if (s.actual_power > s.effective_ftp)
			s.lactate += (s.actual_power - s.effective_ftp) * dt;
		s.lactate = damp_dt_independent(0.f, s.lactate, 0.9967f, dt);  // exp(-1/300) ≈ 0.9967
		s.lactate = glm::min(s.lactate, 20000.f);
		// Scale: ~0.002 bpm/J → 10kJ debt ≈ 20bpm floor elevation
		const float lactate_hr = s.lactate * 0.002f;

		// HR target from power fraction (0 = rest, 1.0 ~ at FTP, >1 above FTP)
		const float power_frac = glm::clamp(s.actual_power / (s.effective_ftp * 1.15f), 0.f, 1.f);
		const float hr_target_from_power = r.hr_rest + (r.hr_max - r.hr_rest) * power_frac;

		// Cardiac drift: rises with glycogen depletion, recovers at rest (long-term)
		const float drift_target = (1.f - s.glycogen) * 18.f;  // max ~18 bpm drift
		// tau ~120s: smoothing = exp(-1/120) ≈ 0.9917
		s.hr_drift = damp_dt_independent(drift_target, s.hr_drift, 0.9917f, dt);

		// HR floor from lactate — HR cannot fall below this even at 0W
		// drift is added once after the max(), so it applies to both paths
		const float hr_floor = r.hr_rest + lactate_hr;
		const float hr_target = glm::max(hr_target_from_power, hr_floor) + s.hr_drift;

		// Asymmetric smoothing: rise tau=45s, fall tau=90s
		// damp_dt_independent(a, b, smoothing, dt) = mix(a, b, smoothing^dt)
		// smoothing = exp(-1/tau)
		const float tau_coeff = (s.hr_current < hr_target) ? exp(-1/30.0) : exp(-1/60.0); // exp(-1/45), exp(-1/90)
		s.hr_current = damp_dt_independent(hr_target, s.hr_current, tau_coeff, dt);
		s.hr_current = glm::clamp(s.hr_current, r.hr_rest, r.hr_max + 5.f);

		// Heart pulse phase — increments at HR rate (bpm → rad/s)
		s.hr_pulse_phase += (s.hr_current / 60.f) * 6.2832f * dt;
		if (s.hr_pulse_phase > 6.2832f) s.hr_pulse_phase -= 6.2832f;
	}

	// --- Drive force:  P = F*v  =>  F = P/v ---
	// Clamp min speed to avoid division by zero on standing start
	const float safe_speed  = glm::max(speed, 0.3f);
	const float drive_force = ci.power / safe_speed;

	// --- Wind: project onto bike heading to get headwind/tailwind component ---
	// Positive = tailwind (reduces drag), negative = headwind (increases drag).
	// Effective wind speed includes the Perlin gust factor (mirrors BikeWind.cpp).
	const float eff_wind_speed   = wind_speed * (1.f + wind_gust_factor * 1.5f);
	const float wind_along_bike = get_wind_along_bike();

	// Apparent airspeed: speed of air relative to the rider.
	// Using v*|v| preserves sign so a strong tailwind can reduce (or negate) drag.
	const float apparent_speed   = speed - wind_along_bike;
	const float drag    = 0.5f * air_density * CdA * apparent_speed * glm::abs(apparent_speed);
	const float rolling = (speed > 0.05f) ? (roll_resist * mass * gravity) : 0.f;
	const float braking = ci.brake_amount * mass * max_brake_decel;
	const float slope_force = mass * gravity * glm::sin(terrain_gradient);

	const float net_force = drive_force - drag - rolling - braking - slope_force;
	speed += (net_force / mass) * dt;
	speed = glm::max(0.f, speed);

	// --- Committed steer (lean-in + inertia) ---
	const float speed_range  = glm::max(steer_speed_gate_hi - steer_speed_gate_lo, 0.001f);
	const float speed_factor = glm::clamp((speed - steer_speed_gate_lo) / speed_range, 0.f, 1.f);
	const float lean_depth   = glm::clamp((glm::abs(current_steer) - steer_lean_threshold) / glm::max(steer_lean_range, 0.001f), 0.f, 1.f);
	const float inertia      = speed_factor * lean_depth;

	const float build_smoothing   = glm::mix(steer_build_lo,   steer_build_hi,   speed_factor);
	const float release_smoothing = glm::mix(steer_release_lo, steer_release_hi, inertia);

	if (glm::abs(ci.steer) > glm::abs(current_steer)) {
		current_steer = damp_dt_independent(ci.steer, current_steer, build_smoothing, dt);
	} else {
		current_steer = damp_dt_independent(ci.steer, current_steer, release_smoothing, dt);
	}
	GameplayStatic::debug_text(string_format("wind_along_bike = %+.2f m/s (eff %.1f)", wind_along_bike, eff_wind_speed));
	GameplayStatic::debug_text(string_format("apparent_speed = %.2f m/s", apparent_speed));
	GameplayStatic::debug_text(string_format("speed_factor = %.3f", speed_factor));
	GameplayStatic::debug_text(string_format("lean_depth = %.3f", lean_depth));
	GameplayStatic::debug_text(string_format("inertia = %.3f", inertia));
	GameplayStatic::debug_text(string_format("steer = %.3f", current_steer));

	// --- Steering (bicycle geometry) ---
	const float wheelbase       = 1.0f;
	const float max_steer_rad   = glm::radians(45.f);
	const float min_turn_radius = glm::max(1.5f, speed * speed * 0.09f);

	const float steer_angle = current_steer * max_steer_rad;
	float turn_rate = 0.f;
	if (glm::abs(steer_angle) > 0.001f) {
		const float radius = glm::max(wheelbase / glm::abs(glm::tan(steer_angle)), min_turn_radius);
		turn_rate = (speed / radius) * glm::sign(current_steer);
	}

	if (glm::abs(turn_rate) > 0.0001f) {
		const float angle = -turn_rate * dt;
		const glm::mat3 rot = glm::mat3(glm::rotate(glm::mat4(1.f), angle, glm::vec3(0, 1, 0)));
		bike_direction = glm::normalize(rot * bike_direction);
	}

	// --- Visual lean (roll into turns) ---
	const float lean_speed_scale = glm::clamp(speed / 15.f, 0.f, 1.f);
	const float target_roll = current_steer * lean_speed_scale * 0.30f;
	current_roll = damp_dt_independent(target_roll, current_roll, 0.02f, dt);

	// --- Auto gear selection (target ~90 RPM = 1.5 rev/s) ---
	speed_smoothed = damp_dt_independent(speed, speed_smoothed, 0.04f, dt);
	gear_shift_cooldown = glm::max(0.f, gear_shift_cooldown - dt);
	just_shifted = false;
	const float target_cadence_rps = 1.5f;
	if (gear_shift_cooldown == 0.f) {
		int best_gear = gear.current_high_gear;
		float best_diff = FLT_MAX;
		for (int i = 0; i < (int)gear.back_cogs.size(); i++) {
			const float ratio = (float)gear.front_cogs[gear.current_low_gear] / (float)gear.back_cogs[i];
			const float cad   = speed_smoothed / (ratio * wheel_circ);
			const float diff  = glm::abs(cad - target_cadence_rps);
			if (diff < best_diff) { best_diff = diff; best_gear = i; }
		}
		const float cur_ratio       = (float)gear.front_cogs[gear.current_low_gear] / (float)gear.back_cogs[gear.current_high_gear];
		const float cur_cadence_rpm = (speed_smoothed > 0.1f) ? (speed_smoothed / (cur_ratio * wheel_circ)) * 60.f : 0.f;
		const bool outside_range    = cur_cadence_rpm < 80.f || cur_cadence_rpm > 95.f;
		if (best_gear != gear.current_high_gear && outside_range) {
			gear.current_high_gear = best_gear;
			gear_shift_cooldown = bike_gear_shift_cooldown;
			just_shifted = true;
		}
	}
	const float gear_ratio = (float)gear.front_cogs[gear.current_low_gear]
	                       / (float)gear.back_cogs[gear.current_high_gear];
	cadence = (speed > 0.1f) ? (speed / (gear_ratio * wheel_circ)) : 0.f; // rev/s

	// --- Move entity (flat XZ, terrain will correct Y) ---
	glm::vec3 pos = get_owner()->get_ws_position();
	pos += bike_direction * speed * dt;

	// --- Terrain raycasts (front + rear wheel) ---
	const float wheel_half     = 0.55f;
	const float ray_up_offset  = 1.5f;
	const float ray_reach      = 4.0f;
	const float seat_height    = 0.3f;

	const glm::vec3 front_org = pos + bike_direction * wheel_half + glm::vec3(0, ray_up_offset, 0);
	const glm::vec3 rear_org  = pos - bike_direction * wheel_half + glm::vec3(0, ray_up_offset, 0);
	const glm::vec3 down      = glm::vec3(0, -(ray_up_offset + ray_reach), 0);

	world_query_result front_res, rear_res;
	const bool front_hit = g_physics.trace_ray(front_res, front_org, front_org + down, nullptr, UINT32_MAX);
	const bool rear_hit  = g_physics.trace_ray(rear_res,  rear_org,  rear_org  + down, nullptr, UINT32_MAX);

	glm::vec3 terrain_forward = bike_direction;

	if (front_hit && rear_hit) {
		const glm::vec3 slope_vec = front_res.hit_pos - rear_res.hit_pos;
		const float horiz = glm::length(glm::vec2(slope_vec.x, slope_vec.z));
		terrain_gradient  = atan2f(slope_vec.y, horiz);
		terrain_forward   = glm::normalize(slope_vec);
		pos.y = (front_res.hit_pos.y + rear_res.hit_pos.y) * 0.5f + seat_height;
	} else if (rear_hit) {
		pos.y = rear_res.hit_pos.y + seat_height;
		terrain_gradient = 0.f;
	} else if (front_hit) {
		pos.y = front_res.hit_pos.y + seat_height;
		terrain_gradient = 0.f;
	} else {
		terrain_gradient = 0.f;
	}

	// --- Bump detection ---
	prev_gradient = damp_dt_independent(terrain_gradient, prev_gradient, 0.08f, dt);
	const float gradient_delta = glm::abs(terrain_gradient - prev_gradient);
	bump_impulse = gradient_delta * speed;

	// --- Orientation: steer direction (flat) + terrain pitch + roll ---
	const glm::vec3 right        = glm::normalize(glm::cross(bike_direction, glm::vec3(0, 1, 0)));
	const glm::vec3 terrain_up   = glm::normalize(glm::cross(right, terrain_forward));
	const glm::mat3 basis(right, terrain_up, -terrain_forward);
	const glm::quat base_orient  = glm::quat(basis);
	const glm::quat roll_quat    = glm::angleAxis(current_roll, terrain_forward);
	const glm::quat orient       = roll_quat * base_orient;

	get_owner()->set_ws_position_rotation(pos, orient);

	return ci;
}

float BikeObject::get_wind_along_bike() const
{
	const float eff_wind_speed = wind_speed * (1.f + wind_gust_factor * 1.5f);
	const glm::vec3 wdir_norm = glm::length(wind_direction) > 0.001f
		? glm::normalize(wind_direction) : glm::vec3(0.f);
	const float wind_along_bike = glm::dot(wdir_norm, bike_direction) * eff_wind_speed;

	return wind_along_bike;
}

glm::vec3 BikeObject::get_wind_along_bike_vector() const
{
	const float eff_wind_speed = wind_speed * (1.f + wind_gust_factor * 1.5f);


	return wind_direction * eff_wind_speed - bike_direction*speed;
}

// ============================================================
// Debug menu: Bike Physics
// ============================================================

static void bike_physics_debug()
{
	ImGui::SeparatorText("Steering");
	{
#define DAMP_IMGUI(name) ImGui::DragFloat(#name, &name, 0.001f, 0.f, 0.2f)
		DAMP_IMGUI(steer_build_lo);
		DAMP_IMGUI(steer_build_hi);
		DAMP_IMGUI(steer_release_lo);
		DAMP_IMGUI(steer_release_hi);
#undef DAMP_IMGUI
		ImGui::DragFloat("steer_lean_threshold", &steer_lean_threshold, 0.05f);
		ImGui::DragFloat("steer_lean_range",     &steer_lean_range,     0.05f);
		ImGui::DragFloat("steer_speed_gate_lo",  &steer_speed_gate_lo,  0.1f);
		ImGui::DragFloat("steer_speed_gate_hi",  &steer_speed_gate_hi,  0.1f);
	}
	ImGui::SeparatorText("Gear");
	{
		ImGui::InputFloat("shift_cooldown", &bike_gear_shift_cooldown);
	}
}
ADD_TO_DEBUG_MENU(bike_physics_debug);
