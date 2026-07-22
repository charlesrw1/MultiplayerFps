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

// Roll/lean damping — see tick_steer. Lower = snappier, higher = laggier
// (damp_dt_independent's "smoothing" factor, not seconds). stand_up is
// deliberately laggier than lean_in by default: quick to bank into a corner,
// slower to come back upright, so the exit doesn't read as an instant snap.
float roll_smooth_lean_in   = 0.01f;
float roll_smooth_stand_up  = 0.05f;
// Flips which way the bike banks for a given turn direction — the sign
// convention was carried over algebraically from the old curvature-based
// formula rather than verified visually; flip live here if it leans the
// wrong way instead of editing code.
float roll_dir_sign         = -1.f;

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
// Visual lean/roll is separate and NOT driven by ci.steer: it banks from the
// bike's own actual turn rate (heading_turn_rate, one tick stale — same
// cross-tick convention as every other field in that "written by tick_transform,
// read next tick" group, see BikeHeaders.h), not from the track's lookahead
// curvature. Mirrors real bike lean physics: bank angle ~ atan(v * yaw_rate / g).
//
// This used to be driven directly off track curvature a few metres ahead,
// which put lean a step AHEAD of the bike's own (heading-PID-smoothed, real
// momentum-carrying) turn rate — so the bike stood back up the instant the
// road geometry straightened, even while heading_turn_rate (and therefore
// the actual path) was still unwinding through momentum. Tying roll to
// heading_turn_rate instead means the visual lean can only settle exactly as
// fast as the bike is actually done turning — it carries out of corners
// alongside the real heading momentum instead of snapping upright early.
// Asymmetric damping on top (roll_smooth_lean_in/roll_smooth_stand_up) adds a
// second, purely cosmetic layer of "it's quicker to lean in than to stand
// back up" on top of that.
// ------------------------------------------------------------
void BikeObject::tick_steer(const ControlInput& ci, float dt)
{
	ASSERT(dt > 0.f);
	s_steer_debug = this;

	steer_input_raw = ci.steer;
	current_steer   = damp_dt_independent(steer_input_raw, current_steer, steer_smoothing_tau, dt);

	float target_roll = 0.f;
	if (speed > 0.1f) {
		const float centripetal_accel = speed * glm::abs(heading_turn_rate);  // v*omega == v^2/r
		const float lean_speed_scale  = glm::smoothstep(0.f, 8.f, speed);
		const float lean_uncapped     = atanf(centripetal_accel / BIKE_GRAVITY) * lean_speed_scale;
		target_roll = roll_dir_sign * glm::sign(heading_turn_rate) * glm::min(lean_uncapped, glm::radians(lean_max_deg));
	}
	const bool leaning_in = glm::abs(target_roll) > glm::abs(current_roll);
	current_roll = damp_dt_independent(target_roll, current_roll,
	                                    leaning_in ? roll_smooth_lean_in : roll_smooth_stand_up, dt);
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
		ImGui::DragFloat("roll_smooth_lean_in",  &roll_smooth_lean_in,  0.005f, 0.001f, 0.95f, "%.3f");
		ImGui::DragFloat("roll_smooth_stand_up", &roll_smooth_stand_up, 0.005f, 0.001f, 0.95f, "%.3f");
		ImGui::DragFloat("roll_dir_sign",        &roll_dir_sign,        2.f,   -1.f,   1.f);
		ImGui::SeparatorText("Handlebar Visual");
		ImGui::DragFloat("bar_scale_lo_steer",  &bar_scale_lo_steer,  0.1f, 0.5f, 12.f);
		ImGui::DragFloat("bar_scale_hi_steer",  &bar_scale_hi_steer,  0.1f, 0.1f,  6.f);
		ImGui::DragFloat("bar_visual_lean_min", &bar_visual_lean_min, 0.02f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_lo",    &bar_lean_fade_lo,    0.01f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_hi",    &bar_lean_fade_hi,    0.01f, 0.f,  1.f);
	}
}
ADD_TO_DEBUG_MENU(bike_steer_debug);
