#include "BikeHeaders.h"
#include "BikeObject_Local.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "Physics/Physics2.h"
#include "imgui.h"

// Wind state accessed via g_wind (defined in BikeWind.cpp)

// ============================================================
// Physics-only constants (not shared with other BikeObject files)
// ============================================================
static constexpr float BIKE_CDA         = 0.3f;
static constexpr float BIKE_AIR_DENSITY = 1.225f;
static constexpr float BIKE_ROLL_RESIST = 0.004f;
static           float BIKE_MAX_BRAKE   = 1.8f;

// Traction tuning
static float traction_mu           = 1.0f;   // dry-road tire µ
static float traction_lean_comp    = 0.20f;  // bicycle lean handles most cornering
static float traction_build_rate   = 2.0f;   // slide builds at (slip_ratio-1) × this per second
static float traction_recover_rate = 1.5f;   // slide recovers at this per second

// Bump speed / power loss
static float bump_speed_thresh  = 0.003f;  // minimum total_bump before speed is shed
static float bump_speed_loss    = 0.06f;   // m/s lost per unit excess bump (direct KE loss)
static float bump_rolling_scale = 12.f;    // extra resistive force (N) per unit bump

// Pedal stroke
static float stroke_amp = 1.0f;  // 0=constant power, 1=full realistic lurch

static BikeObject* s_physics_debug = nullptr;  // set each tick for debug menu

static float compute_max_steer_rad_phys(float speed)
{
	ASSERT(speed >= 0.f);
	const float safe_speed = glm::max(speed, steer_ref_speed);
	const float t          = glm::pow(steer_ref_speed / safe_speed, steer_speed_power);
	return glm::radians(steer_max_deg_hi + (steer_max_deg - steer_max_deg_hi) * t);
}

// ------------------------------------------------------------
// BikeObject::tick_physics
//
// Applies propulsive, aerodynamic, rolling, braking and slope forces to
// advance `speed`. Also handles traction / rear-skid and corner-overspeed
// crash detection.
// ------------------------------------------------------------
void BikeObject::tick_physics(ControlInput& ci, float dt)
{
	ASSERT(dt > 0.f);
	s_physics_debug = this;

	const float safe_speed = glm::max(speed, 0.3f);

	// Pedal stroke: use heavily smoothed speed (tau~2s) to advance phase so
	// per-stroke speed fluctuations don't collapse the feedback loop.
	stroke_speed_smooth = damp_dt_independent(speed, stroke_speed_smooth, 0.368f, dt);
	const float gear_ratio_now = (float)gear.front_cogs[gear.current_low_gear]
	                           / (float)gear.back_cogs[gear.current_high_gear];
	const float stable_cadence = stroke_speed_smooth / glm::max(gear_ratio_now * BIKE_WHEEL_CIRC, 0.01f);
	stroke_phase += stable_cadence * glm::two_pi<float>() * dt;
	if (stroke_phase > glm::two_pi<float>()) stroke_phase -= glm::two_pi<float>();

	float effective_power = ci.power;
	if (stable_cadence > 0.1f) {
		const float envelope = glm::max(0.f, glm::sin(stroke_phase * 2.f));
		effective_power = ci.power * glm::mix(1.f, glm::pi<float>() * envelope, stroke_amp);
	}

	// Forces
	const float wind_along_bike  = get_wind_along_bike();
	const float apparent_speed   = speed - wind_along_bike;
	const float drag             = 0.5f * BIKE_AIR_DENSITY * (BIKE_CDA * draft_factor) * apparent_speed * glm::abs(apparent_speed);
	const float rolling          = (speed > 0.05f) ? (BIKE_ROLL_RESIST * BIKE_MASS * BIKE_GRAVITY) : 0.f;
	const float normal_force_brk = BIKE_MASS * BIKE_GRAVITY * glm::cos(terrain_gradient);
	const float kinetic_friction = traction_mu * 0.7f * normal_force_brk;
	const float braking          = glm::mix(ci.brake_amount * BIKE_MASS * BIKE_MAX_BRAKE, kinetic_friction, rear_skid);
	const float slope_force      = BIKE_MASS * BIKE_GRAVITY * glm::sin(terrain_gradient);
	const float drive_force      = effective_power / safe_speed;

	// Crash: decay speed, timer
	if (is_crashed) {
		crash_timer -= dt;
		speed = damp_dt_independent(0.f, speed, 0.015f, dt);
		if (crash_timer <= 0.f) { is_crashed = false; crash_timer = 0.f; corner_warn_timer = 0.f; }
	}

	// Crack decals raise effective rolling resistance and shed kinetic energy
	const float excess_bump     = glm::max(0.f, crack_impulse - bump_speed_thresh);
	const float bump_resistance = crack_impulse * bump_rolling_scale;  // extra resistive N

	const float net_force = is_crashed ? 0.f : (drive_force - drag - rolling - bump_resistance - braking - slope_force);
	speed = glm::max(0.f, speed + (net_force / BIKE_MASS) * dt);

	// Direct KE loss: energy absorbed by frame/body on sharp bumps
	if (excess_bump > 0.f)
		speed = glm::max(0.f, speed - excess_bump * bump_speed_loss);

	// Traction / slip
	{
		const float max_grip = surface_traction * traction_mu * normal_force_brk;

		// Rear skid from heavy braking
		const float brake_demanded = ci.brake_amount * BIKE_MASS * BIKE_MAX_BRAKE;
		const float long_slip = (max_grip > 1.f) ? brake_demanded / max_grip : 0.f;
		if (long_slip > 1.f)
			rear_skid = glm::min(1.f, rear_skid + (long_slip - 1.f) * traction_build_rate * dt);
		else
			rear_skid = glm::max(0.f, rear_skid - traction_recover_rate * dt);
		bump_impulse = glm::max(bump_impulse, rear_skid * 0.4f);

		// Corner overspeed → crash
		const float wheelbase     = BIKE_WHEELBASE;
		const float min_turn_r    = glm::max(steer_min_radius, speed * speed * steer_radius_coeff);
		const float max_steer_rad = compute_max_steer_rad_phys(speed);
		const float steer_angle   = current_steer * max_steer_rad;
		if (!is_crashed && glm::abs(steer_angle) > 0.001f && speed > 3.f) {
			const float turn_r      = glm::max(wheelbase / glm::abs(glm::tan(steer_angle)), min_turn_r);
			const float centripetal = BIKE_MASS * speed * speed / turn_r * traction_lean_comp;
			if (centripetal > max_grip) {
				corner_warn_timer += dt;
				if (corner_warn_timer >= 0.15f) { is_crashed = true; crash_timer = 3.f; speed *= 0.1f; }
			} else {
				corner_warn_timer = glm::max(0.f, corner_warn_timer - dt * 3.f);
			}
		}
	}
}

// ============================================================
// Debug menu: Physics / Traction / Bump
// ============================================================

static void bike_physics_debug()
{
	ImGui::SeparatorText("Pedal Stroke");
	{
		ImGui::DragFloat("stroke_amp", &stroke_amp, 0.01f, 0.f, 1.f);
		if (s_physics_debug)
			ImGui::Text("phase: %.2f  envelope: %.2f",
				s_physics_debug->stroke_phase,
				glm::max(0.f, glm::sin(s_physics_debug->stroke_phase * 2.f)));
	}
	ImGui::DragFloat("bike_brake", &BIKE_MAX_BRAKE, 0.05f, 0.f, 8.f);
	ImGui::SeparatorText("Bump Speed/Power Loss");
	{
		ImGui::DragFloat("bump_speed_thresh",  &bump_speed_thresh,  0.0005f, 0.f,  0.05f);
		ImGui::DragFloat("bump_speed_loss",    &bump_speed_loss,    0.002f,  0.f,  0.5f);
		ImGui::DragFloat("bump_rolling_scale", &bump_rolling_scale, 0.5f,    0.f,  50.f);
	}
	ImGui::SeparatorText("Traction");
	{
		ImGui::DragFloat("traction_mu",          &traction_mu,          0.01f, 0.5f,  2.f);
		ImGui::DragFloat("traction_lean_comp",   &traction_lean_comp,   0.01f, 0.05f, 1.f);
		ImGui::DragFloat("traction_build_rate",  &traction_build_rate,  0.1f,  0.f,  10.f);
		ImGui::DragFloat("traction_recover_rate",&traction_recover_rate, 0.1f,  0.f,  10.f);
		if (s_physics_debug) {
			ImGui::DragFloat("surface_traction",  &s_physics_debug->surface_traction, 0.01f, 0.f, 1.f);
			ImGui::ProgressBar(s_physics_debug->rear_skid, ImVec2(-1, 0), "rear skid");
			ImGui::ProgressBar(s_physics_debug->corner_warn_timer / 0.15f, ImVec2(-1, 0), "corner warn");
			ImGui::Text("crashed: %s (%.1fs)", s_physics_debug->is_crashed ? "YES" : "no", s_physics_debug->crash_timer);
		}
	}
}
ADD_TO_DEBUG_MENU(bike_physics_debug);
