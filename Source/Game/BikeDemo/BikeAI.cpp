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

	// ---- Lookahead point on the racing line ----
	// Steers toward the precomputed racing-line offset (inside corners) rather than
	// the road centre. Boid separation forces still move riders off the ideal line
	// when the pack demands it.
	const float lookahead_dist = lookahead_dist_base + speed * lookahead_dist_per_ms;
	const glm::vec3 lookahead_pt = course->racing_line_lookahead(my_bike->course_dist_m, lookahead_dist);
	dbg_lookahead_pt = lookahead_pt;

	// ---- PID steering ----
	// Error: signed lateral angle toward the lookahead point in bike-local space.
	// We project (lookahead - bike_pos) onto bike_right to get a signed lateral error.
	// Normalising by distance prevents blow-up when the point is very close.
	const glm::vec3 to_target   = lookahead_pt - my_bike->get_ws_position();
	const float     dist_to_tgt = glm::length(to_target);
	const float     steer_err   = (dist_to_tgt > 0.01f)
	                              ? glm::dot(glm::normalize(to_target), my_bike->terrain_forward_dir)
	                              : 0.f;

	// We actually want the lateral component (dot with right), not the forward component.
	// Re-derive: right = cross(forward, world_up) in bike frame.
	// bike_right is not stored explicitly, so reconstruct from bike_direction.
	const glm::vec3 bike_right = glm::normalize(
		glm::cross(my_bike->bike_direction, glm::vec3(0, 1, 0)));
	const float lateral_err = (dist_to_tgt > 0.01f)
	                          ? glm::dot(glm::normalize(to_target), bike_right)
	                          : 0.f;

	// PID
	steer_integral += lateral_err * dt;
	steer_integral  = glm::clamp(steer_integral, -2.f, 2.f);  // anti-windup
	const float d_err   = (dt > 1e-6f) ? (lateral_err - prev_steer_err) / dt : 0.f;
	const float steer_out = steer_kp * lateral_err
	                      + steer_ki * steer_integral
	                      + steer_kd * d_err;
	prev_steer_err = lateral_err;

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
	ci.brake_amount  = 0.f;
	ci.power         = power_out;
	dbg_power_final  = power_out;

	my_bike->update_tick(ci);
}
