#include "BikeHeaders.h"
#include "BikeObject_Local.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "imgui.h"

// ============================================================
// Steering / visual tuning vars
// Defined non-static so BikeObject.cpp and BikeObject_Physics.cpp can extern them.
// ============================================================

float steer_max_deg         = 45.f;
float steer_max_deg_hi      = 4.f;
float steer_ref_speed       = 2.5f;
float steer_speed_power     = 2.0f;
float steer_min_radius      = 1.5f;
float steer_radius_coeff    = 0.11f;
float lean_max_deg          = 32.f;
float bar_scale_lo_steer    = 1.5f;
float bar_scale_hi_steer    = 1.0f;
float bar_visual_lean_min   = 1.5f;
float bar_lean_fade_lo      = 0.0f;
float bar_lean_fade_hi      = 1.f;

// Single low-pass on the raw steer input — no build/release asymmetry, no
// wobble/crosswind/bump-steer noise layers. Same path for player and AI.
static float steer_smoothing_tau = 0.12f;  // s
static float max_turn_rate_dps   = 220.f;  // deg/s hard ceiling regardless of the speed-based cap

static BikeObject* s_steer_debug = nullptr;  // set each tick for debug menu

float compute_max_steer_rad(float speed)
{
	ASSERT(speed >= 0.f);
	const float safe_speed = glm::max(speed, steer_ref_speed);
	const float t          = glm::pow(steer_ref_speed / safe_speed, steer_speed_power);
	return glm::radians(steer_max_deg_hi + (steer_max_deg - steer_max_deg_hi) * t);
}

// ------------------------------------------------------------
// BikeObject::tick_steer
//
// Resolves ci.steer (heading command, [-1,1]) into current_steer via a
// single low-pass, then a speed-scaled turn-rate cap yaws bike_direction.
// Visual lean is derived from the resulting turn rate. Sign convention:
// positive steer = turn LEFT. See [[bike/sign_conventions]]
// ------------------------------------------------------------
void BikeObject::tick_steer(const ControlInput& ci, float dt)
{
	ASSERT(dt > 0.f);
	s_steer_debug = this;

	steer_input_raw = ci.steer;
	current_steer   = damp_dt_independent(steer_input_raw, current_steer, steer_smoothing_tau, dt);

	// Speed-scaled turn-rate cap — tighter turns only above a minimum radius,
	// applied directly to turn rate.
	const float min_turn_r = glm::max(steer_min_radius, speed * speed * steer_radius_coeff);
	const float speed_cap  = (min_turn_r > 0.01f) ? (speed / min_turn_r) : 0.f;
	const float max_rate   = glm::min(speed_cap, glm::radians(max_turn_rate_dps));

	turn_rate = current_steer * max_rate;
	const float angle = -turn_rate * dt;
	bike_direction = glm::normalize(glm::mat3(glm::rotate(glm::mat4(1.f), angle, glm::vec3(0, 1, 0))) * bike_direction);

	// Visual lean, derived from the resulting turn rate.
	float target_roll = 0.f;
	if (glm::abs(turn_rate) > 0.001f && speed > 0.1f) {
		const float turn_r             = speed / glm::max(glm::abs(turn_rate), 0.001f);
		const float centripetal_accel  = (speed * speed) / turn_r;
		const float lean_speed_scale   = glm::smoothstep(0.f, 8.f, speed);
		const float lean_uncapped      = atanf(centripetal_accel / BIKE_GRAVITY) * lean_speed_scale;
		target_roll = glm::sign(current_steer) * glm::min(lean_uncapped, glm::radians(lean_max_deg));
	}
	current_roll = damp_dt_independent(target_roll, current_roll, 0.01f, dt);
}

// ============================================================
// Debug menu: Steering
// ============================================================

static void bike_steer_debug()
{
	ImGui::SeparatorText("Steering");
	{
		ImGui::DragFloat("steer_smoothing_tau", &steer_smoothing_tau, 0.005f, 0.01f, 1.f);
		ImGui::DragFloat("max_turn_rate_dps",   &max_turn_rate_dps,   1.f,    10.f, 720.f);
		ImGui::DragFloat("steer_max_deg",       &steer_max_deg,       0.5f, 5.f,  45.f);
		ImGui::DragFloat("steer_max_deg_hi",    &steer_max_deg_hi,    0.5f, 0.5f, 15.f);
		ImGui::DragFloat("steer_ref_speed",     &steer_ref_speed,     0.1f, 1.f,  15.f);
		ImGui::DragFloat("steer_speed_power",   &steer_speed_power,   0.1f, 0.5f,  5.f);
		if (s_steer_debug)
			ImGui::Text("current max steer: %.1f deg  (speed %.1f m/s)",
			            glm::degrees(compute_max_steer_rad(s_steer_debug->speed)), s_steer_debug->speed);
		ImGui::DragFloat("steer_radius_coeff", &steer_radius_coeff, 0.005f, 0.f, 0.2f);
		ImGui::DragFloat("lean_max_deg",       &lean_max_deg,       0.5f,   5.f, 50.f);
		ImGui::SeparatorText("Handlebar Visual");
		ImGui::DragFloat("bar_scale_lo_steer",  &bar_scale_lo_steer,  0.1f, 0.5f, 12.f);
		ImGui::DragFloat("bar_scale_hi_steer",  &bar_scale_hi_steer,  0.1f, 0.1f,  6.f);
		ImGui::DragFloat("bar_visual_lean_min", &bar_visual_lean_min, 0.02f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_lo",    &bar_lean_fade_lo,    0.01f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_hi",    &bar_lean_fade_hi,    0.01f, 0.f,  1.f);
	}
}
ADD_TO_DEBUG_MENU(bike_steer_debug);
