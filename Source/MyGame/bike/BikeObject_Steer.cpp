#include "BikeHeaders.h"
#include "BikeObject_Local.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "imgui.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

// ============================================================
// Steering / visual tuning vars
// Defined non-static so BikeObject.cpp and BikeObject_Physics.cpp can extern them.
// ============================================================

float steer_max_deg         = 45.f;
float steer_max_deg_hi      = 4.f;
float steer_ref_speed       = 2.5f;
float steer_speed_power     = 2.0f;
float lean_max_deg          = 32.f;
float bar_scale_lo_steer    = 1.5f;
float bar_scale_hi_steer    = 1.0f;
float bar_visual_lean_min   = 1.5f;
float bar_lean_fade_lo      = 0.0f;
float bar_lean_fade_hi      = 1.f;

// Single low-pass on the raw steer input — no build/release asymmetry, no
// wobble/crosswind/bump-steer noise layers. Same path for player and AI.
static float steer_smoothing_tau = 0.12f;  // s

static BikeObject* s_steer_debug = nullptr;  // set each tick for debug menu

// Cosmetic-only max fork/handlebar deflection (front wheel visual + terrain
// raycast probe offset). Never affects heading — see tick_steer.
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
// ci.steer never rotates bike_direction — heading is always the track
// tangent, set by tick_transform's rail movement (see [[bike/bikeai#Rail
// movement]]). It only low-passes into current_steer, which drives the
// cosmetic fork/handlebar twist (BikeObject::tick_transform).
//
// Visual lean/roll is separate and NOT driven by ci.steer: it banks into
// corners based on the track's own curvature at the bike's current rail
// position (central-difference on wp.forward's yaw, same technique as
// BikeCourse::min_turn_radius_ahead, just signed). This is emergent from
// actually following the road, so it's correct in every mode — including a
// hard force-onto-racing-line snap (BikeAIParams::force_racing_line), where
// ci.lateral_shift/ci.steer stay near zero but the bike still corners.
// Mirrors real bike lean physics: bank angle ~ atan(v^2 * curvature / g).
// ------------------------------------------------------------
void BikeObject::tick_steer(const ControlInput& ci, float dt)
{
	ASSERT(dt > 0.f);
	s_steer_debug = this;

	steer_input_raw = ci.steer;
	current_steer   = damp_dt_independent(steer_input_raw, current_steer, steer_smoothing_tau, dt);

	float target_roll = 0.f;
	if (course && course->is_built && speed > 0.1f) {
		// Matches BikeCourse::min_turn_radius_ahead's 6m averaging window, but
		// signed (curvature > 0 = track curves toward +wp.right ahead).
		static constexpr float CURVATURE_HALF_M = 3.f;
		const glm::vec3 fwd_back = course->sample(course_dist_m - CURVATURE_HALF_M).forward;
		const glm::vec3 fwd_fwd  = course->sample(course_dist_m + CURVATURE_HALF_M).forward;
		const float yaw_back = std::atan2(fwd_back.x, fwd_back.z);
		const float yaw_fwd  = std::atan2(fwd_fwd.x, fwd_fwd.z);
		float dyaw = yaw_fwd - yaw_back;
		if (dyaw >  glm::pi<float>()) dyaw -= 2.f * glm::pi<float>();
		if (dyaw < -glm::pi<float>()) dyaw += 2.f * glm::pi<float>();
		const float curvature = dyaw / (2.f * CURVATURE_HALF_M);  // signed, rad/m

		const float centripetal_accel = speed * speed * glm::abs(curvature);
		const float lean_speed_scale  = glm::smoothstep(0.f, 8.f, speed);
		const float lean_uncapped     = atanf(centripetal_accel / BIKE_GRAVITY) * lean_speed_scale;
		// Sign derived from the old heading-integration formula (turn_rate =
		// -d(yaw)/dt, lean sign = sign(turn_rate)): negate curvature's sign.
		target_roll = -glm::sign(curvature) * glm::min(lean_uncapped, glm::radians(lean_max_deg));
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
		ImGui::DragFloat("steer_max_deg",       &steer_max_deg,       0.5f, 5.f,  45.f);
		ImGui::DragFloat("steer_max_deg_hi",    &steer_max_deg_hi,    0.5f, 0.5f, 15.f);
		ImGui::DragFloat("steer_ref_speed",     &steer_ref_speed,     0.1f, 1.f,  15.f);
		ImGui::DragFloat("steer_speed_power",   &steer_speed_power,   0.1f, 0.5f,  5.f);
		if (s_steer_debug)
			ImGui::Text("current max fork angle: %.1f deg  (speed %.1f m/s)",
			            glm::degrees(compute_max_steer_rad(s_steer_debug->speed)), s_steer_debug->speed);
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
