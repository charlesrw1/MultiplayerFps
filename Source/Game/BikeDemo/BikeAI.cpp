#include "BikeHeaders.h"
#include "Framework/MathLib.h"
#include "Debug.h"
#include "GameEnginePublic.h"

// ============================================================
// BikeAI::evaluate
// ============================================================

void BikeAI::evaluate(BikeObject* my_bike)
{
	if (!course || !course->is_built) return;

	const float dt    = eng->get_dt();
	const float speed = my_bike->speed;

	// ---- Lookahead point on the spline ----
	const float lookahead_dist = lookahead_dist_base + speed * lookahead_dist_per_ms;
	const glm::vec3 lookahead_pt = course->lookahead(my_bike->course_dist_m, lookahead_dist);
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

	// ---- Power (smoothed toward target) ----
	actual_power_command = damp_dt_independent(
		target_power_watts, actual_power_command, POWER_SLEW, dt);

	// ---- Fill ControlInput ----
	BikeObject::ControlInput ci;
	ci.steer        = glm::clamp(steer_out, -1.f, 1.f);
	ci.brake_amount = 0.f;
	ci.power        = actual_power_command;

	my_bike->update_tick(ci);
}
