#include "BikeHeaders.h"
#include "Framework/MathLib.h"
#include "Debug.h"
#include "GameEnginePublic.h"
#include <glm/gtx/vector_angle.hpp>

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

	// ---- Simple lookahead racing-line follower ----
	// Cap lookahead by corner radius so the target stays inside the bend,
	// giving the AI early turn-in without overshooting.
	const float corner_scan_m = glm::max(corner_look_m, speed * 0.8f);
	const float raw_min_r     = course->min_turn_radius_ahead(my_bike->course_dist_m, corner_scan_m);
	const float min_r         = glm::max(raw_min_r, 3.f);
	dbg_min_r = min_r;

	const float look_base      = lookahead_dist_base + speed * lookahead_dist_per_ms;
	const float lookahead_dist = look_base;// glm::min(look_base, 2.5f * min_r);
	const glm::vec3 lookahead_pt = course->racing_line_lookahead(my_bike->course_dist_m, lookahead_dist);
	dbg_lookahead_pt   = lookahead_pt;
	dbg_lookahead_dist = lookahead_dist;

	const glm::vec3 up         = glm::vec3(0, 1, 0);
	const glm::vec3 bike_right = glm::normalize(glm::cross(my_bike->bike_direction, up));
	const glm::vec3 to_target  = lookahead_pt - my_bike->get_ws_position();
	const float dist_to_tgt    = glm::length(to_target);

	// Actual signed angle to lookahead point (atan2 vs sin — no saturation before 90°)
	const float angle_to_target = (dist_to_tgt > 0.01f)
	    ? glm::atan(glm::dot(glm::normalize(to_target), bike_right),
	                glm::dot(glm::normalize(to_target), my_bike->bike_direction))
	    : 0.f;

	// Heading alignment: damps oscillation — reduce steer when already pointing at the line
	const BikeWaypoint cur_wp  = course->sample(my_bike->course_dist_m);
	const glm::vec3 path_right = glm::normalize(glm::cross(cur_wp.forward, up));
	const float heading_err    = glm::dot(my_bike->bike_direction, path_right);

	const float steer_out = glm::clamp(angle_to_target * steer_k - heading_err * 0.5f, -1.f, 1.f);

	// ---- Corner braking ----
	// v_max = sqrt(corner_speed_k * g * R * traction); corner_speed_k ≈ 1/traction_lean_comp.
	const float v_max        = glm::sqrt(corner_speed_k * 9.81f * min_r * my_bike->surface_traction);
	const float brake_amount = 0.f;// (speed > v_max)
	                           //? glm::clamp((speed - v_max) / 3.f, 0.f, 0.8f) : 0.f;
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

	// ---- Record training data ----
	if (g_nn_recorder.enabled) {
		const BikeNNFeatures feat = BikeNNFeatures::extract(my_bike, course);
		g_nn_recorder.try_record(feat, steer_out);
	}

	// ---- Fill ControlInput ----
	// Boid steer forces blended on top of PID path steer.
	// Priority (highest first): separation > cohesion (paceline) > alignment (heading match).
	const float boid_steer = my_bike->boid_separation_steer
	                       + my_bike->boid_cohesion_steer
	                       + my_bike->boid_align_steer;
	dbg_steer_pre_boids = steer_out;
	BikeObject::ControlInput ci;
	const float steer_after_boids = glm::clamp(steer_out, -1.f, 1.f);
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
