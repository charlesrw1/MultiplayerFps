#include "BikeHeaders.h"
#include "Framework/MathLib.h"
#include "Debug.h"
#include "GameEnginePublic.h"

// Tunable constants defined in BikeApplication.cpp, exposed here for AI use.
extern float AI_GAP_TARGET;
extern float AI_GAP_KP;
extern float AI_GAP_KD;
extern float AI_GAP_MAX_PULL;
extern float AI_GAP_MAX_PUSH;

// ============================================================
// BikeAI::evaluate
// ============================================================

void BikeAI::evaluate(BikeObject* my_bike)
{
	if (!course || !course->is_built) return;

	const float dt    = eng->get_dt();
	const float speed = my_bike->speed;

	// ---- Adaptive lookahead racing-line follower ----
	//
	// Core insight: a fixed long lookahead wraps around sharp corners (the target is
	// already past the apex), causing late turn-in and overshoot. Instead, cap the
	// lookahead by the upcoming corner radius so the AI commits to entry earlier.
	//
	// On a straight (large min_r): full speed-scaled lookahead distance.
	// In a tight corner (small min_r): lookahead shrinks to ~0.4*R so the target
	// stays on the current arc and the AI follows the racing line cleanly.

	// 1. Sample curvature ahead (short window) to get corner radius.
	//    Clamp min_r to avoid path-noise spikes from junction kinks.
	const float corner_scan_m = glm::max(corner_look_m, speed * 0.8f);
	const float raw_min_r     = course->min_turn_radius_ahead(my_bike->course_dist_m, corner_scan_m);
	const float min_r         = glm::max(raw_min_r, 3.f);  // noise floor: ignore kinks < 3 m radius

	// 2. Lookahead distance: speed-based baseline, capped only by very tight corners.
	// The corner cap uses 2.5*R so the lookahead points to the corner EXIT, not
	// to a point still on the arc (0.45*R was only 1.35m on a 3m fillet — far too close).
	const float look_base   = lookahead_dist_base + speed * lookahead_dist_per_ms;
	const float look_corner = 2.5f * min_r;
	const float look_raw    = glm::min(look_base, look_corner);

	// Low-pass filter: min_turn_radius_ahead samples at 2m steps over 4m-spaced
	// waypoints, so as the bike moves the scan-window boundary crosses junctions
	// and min_r jumps.  Smoothing removes this high-frequency noise so the
	// lookahead point glides rather than bobbing.
	// Seed on first frame (smooth_lookahead_dist starts at 0).
	if (smooth_lookahead_dist < 0.1f) smooth_lookahead_dist = look_raw;
	smooth_lookahead_dist = damp_dt_independent(look_raw, smooth_lookahead_dist, 0.1f, dt);
	const float lookahead_dist = smooth_lookahead_dist;

	const glm::vec3 lookahead_pt = course->racing_line_lookahead(my_bike->course_dist_m, lookahead_dist);
	dbg_lookahead_pt  = lookahead_pt;
	dbg_lookahead_dist = lookahead_dist;
	dbg_min_r          = min_r;

	// 3. Lateral steering error toward the racing-line lookahead point.
	const glm::vec3 bike_right = glm::normalize(glm::cross(my_bike->bike_direction, glm::vec3(0, 1, 0)));
	const glm::vec3 to_target  = lookahead_pt - my_bike->get_ws_position();
	const float dist_to_tgt    = glm::length(to_target);
	const float lateral_err    = (dist_to_tgt > 0.01f)
	                             ? glm::dot(glm::normalize(to_target), bike_right) : 0.f;

	// 4. Stanley lateral correction on top: corrects position error at the current point.
	//    Dividing by speed keeps this term finite; clamping safe_speed prevents blow-up at rest.
	const BikeWaypoint cur_wp = course->sample(my_bike->course_dist_m);
	const float lat_err       = my_bike->lateral_pos - cur_wp.racing_line_lateral;
	const float safe_speed    = glm::max(speed, 2.f);  // higher floor than before — avoids saturation at low speed
	const float stanley_corr  = glm::atan(stanley_k * lat_err / safe_speed);

	// 5. Curvature feedforward: compare path heading near vs far within the lookahead window.
	//    Gives the AI anticipatory steering — it starts turning before lateral drift accumulates.
	//    signed curvature +ve = right-hand bend, so steer right to match.
	const float ff_near_d = glm::min(lookahead_dist * 0.5f, 5.f);
	const float ff_far_d  = lookahead_dist;
	const BikeWaypoint wp_ff_near = course->sample(my_bike->course_dist_m + ff_near_d);
	const BikeWaypoint wp_ff_far  = course->sample(my_bike->course_dist_m + ff_far_d);
	const float yaw_near = std::atan2(wp_ff_near.forward.x, wp_ff_near.forward.z);
	const float yaw_far  = std::atan2(wp_ff_far.forward.x, wp_ff_far.forward.z);
	float dyaw = yaw_far - yaw_near;
	if (dyaw >  glm::pi<float>()) dyaw -= 2.f * glm::pi<float>();
	if (dyaw < -glm::pi<float>()) dyaw += 2.f * glm::pi<float>();
	const float signed_curvature = dyaw / glm::max(ff_far_d - ff_near_d, 0.1f);  // rad/m
	const float curvature_ff     = curvature_ff_k * signed_curvature;

	const float steer_out = glm::clamp(lateral_err - stanley_corr + curvature_ff, -1.f, 1.f);

	// Store path-following breakdown for debug overlay
	dbg_lat_err      = lat_err;
	dbg_stanley_corr = stanley_corr;
	dbg_curvature_ff = curvature_ff;

	// ---- Corner braking ----
	// v_max = sqrt(corner_speed_k * g * R * traction); corner_speed_k ≈ 1/traction_lean_comp.
	const float v_max        = glm::sqrt(corner_speed_k * 9.81f * min_r * my_bike->surface_traction);
	const float brake_amount = (speed > v_max)
	                           ? glm::clamp((speed - v_max) / 3.f, 0.f, 0.8f) : 0.f;
	dbg_v_max        = v_max;
	dbg_brake_amount = brake_amount;

	// ---- Power (smoothed toward target, boid alignment + draft-seek added on top) ----
	actual_power_command = damp_dt_independent(
		target_power_watts, actual_power_command, POWER_SLEW, dt);

	float power_out = actual_power_command;
	dbg_power_base = actual_power_command;

	// While peeling or drifting back, suppress boid power nudges and gap-following so
	// the rider actually slows down and drifts through the pack.
	const bool in_paceline_exit = (paceline_state == PacelineState::Peeling ||
	                               paceline_state == PacelineState::DriftingBack);

	// Speed-alignment nudge: match rider-ahead speed (chain propagation from update_boids)
	if (!in_paceline_exit) {
		power_out += my_bike->boid_align_power_nudge;
		dbg_power_align_nudge = my_bike->boid_align_power_nudge;
	} else {
		dbg_power_align_nudge = 0.f;
	}

	// Longitudinal separation: yield power when side-by-side with another rider
	power_out += my_bike->boid_long_sep_power;

	// Gap following PD using rider_ahead (sticky locking handled in update_gaps).
	// KP: gap_err (too far = +, too close = -).
	// KD: relative speed = rider_ahead->speed - my_speed (gap growing = +, closing = -).
	// Suppressed while peeling/drifting — the rider needs to LOSE ground, not close it.
	float gap_bonus = 0.f;
	if (my_bike->rider_ahead && !in_paceline_exit) {
		const float gap_ahead = my_bike->gap_to_ahead_m;
		if (gap_ahead < AI_GAP_TARGET + 30.f) {
			const float gap_err  = gap_ahead - AI_GAP_TARGET;
			const float gap_rate = my_bike->rider_ahead->speed - speed;
			gap_bonus = glm::clamp(gap_err * AI_GAP_KP + gap_rate * AI_GAP_KD,
			                       -AI_GAP_MAX_PUSH, AI_GAP_MAX_PULL);
			power_out += gap_bonus;
		}
	}
	dbg_power_seek_bonus = gap_bonus;

	// ---- Fill ControlInput ----
	// Boid steer forces blended on top of PID path steer.
	// Priority (highest first): separation > cohesion (paceline) > alignment (heading match).
	const float boid_steer = my_bike->boid_separation_steer
	                       + my_bike->boid_cohesion_steer
	                       + my_bike->boid_align_steer;
	dbg_steer_pre_boids = steer_out;
	BikeObject::ControlInput ci;
	const float steer_after_boids = glm::clamp(steer_out + boid_steer, -1.f, 1.f);
	dbg_steer_pre_hard = steer_after_boids;

	// Hard steer cutoff: if a neighbour is inside the exclusion zone, the boid update
	// has narrowed hard_steer_min/max to block any steer that would close the gap further.
	// This is the last-resort override — soft separation should have handled it already.
	ci.steer        = glm::clamp(steer_after_boids, my_bike->hard_steer_min, my_bike->hard_steer_max);
	dbg_steer_final  = ci.steer;
	// Braking suppresses power output — don't add gap/boid bonuses while braking hard.
	ci.brake_amount  = brake_amount;
	ci.power         = (brake_amount > 0.1f) ? 0.f : power_out;
	dbg_power_final  = power_out;

	my_bike->update_tick(ci);
}
