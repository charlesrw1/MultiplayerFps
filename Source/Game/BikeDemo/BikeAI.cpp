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

	// ---- Stanley path tracker ----
	// Corrects two errors simultaneously at the current position — no distant lookahead
	// that wraps around corners and causes overshoot.
	//
	// heading_err: cross(path_fwd, bike_dir).y — positive = bike heading right of path → steer left
	// lat_err:     lateral_pos - racing_line_lateral — positive = right of line → steer left
	//
	// steer = -(heading_err + atan(k * lat_err / speed))
	//
	// At high speed the atan term shrinks → mainly heading correction (anticipates the turn).
	// At low speed the atan grows → strong lateral snap to line.

	const BikeWaypoint cur_wp  = course->sample(my_bike->course_dist_m);
	dbg_lookahead_pt = cur_wp.position + cur_wp.right * cur_wp.racing_line_lateral;

	// Heading error: y-component of cross(path_forward, bike_direction)
	const glm::vec3& pf = cur_wp.forward;
	const glm::vec3& bd = my_bike->bike_direction;
	const float heading_err = pf.z * bd.x - pf.x * bd.z;  // sin of signed angle

	// Lateral error: how far right of the racing line we are (metres)
	const float lat_err = my_bike->lateral_pos - cur_wp.racing_line_lateral;

	const float safe_speed = glm::max(speed, 0.5f);
	const float steer_out  = glm::clamp(
		-heading_err - glm::atan(stanley_k * lat_err / safe_speed),
		-1.f, 1.f);

	// ---- Corner braking ----
	// Sample upcoming curvature, compute the max safe speed for the tightest corner found.
	// v_max = sqrt(corner_speed_k * g * R * traction)
	// corner_speed_k ≈ 1/traction_lean_comp (default 5.0) to match crash physics.
	const float look_m = glm::max(corner_look_m, speed * 1.5f);
	const float min_r  = course->min_turn_radius_ahead(my_bike->course_dist_m, look_m);
	const float v_max  = glm::sqrt(corner_speed_k * 9.81f * min_r * my_bike->surface_traction);
	const float brake_amount = (speed > v_max)
		? glm::clamp((speed - v_max) / 3.f, 0.f, 0.8f)
		: 0.f;

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
