#include "BikeHeaders.h"
#include "Framework/MathLib.h"
#include "Debug.h"
#include "GameEnginePublic.h"
#include <glm/gtx/vector_angle.hpp>

// Rotate a vector around the world Y axis (CCW positive when viewed from above,
// matching glm::rotate(mat4, angle, vec3(0,1,0))).
static glm::vec3 rotate_y(glm::vec3 v, float angle_rad) {
	const float c = glm::cos(angle_rad);
	const float s = glm::sin(angle_rad);
	return glm::vec3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
}

// Project a flat arc (constant turn_rate, constant speed) to get world position after t seconds.
// turn_rate sign convention: positive = turning right (matches BikeObject::tick_transform).
static glm::vec3 arc_predict(glm::vec3 pos, glm::vec3 dir, float speed, float turn_rate, float t)
{
	if (glm::abs(turn_rate) < 0.001f)
		return pos + dir * (speed * t);

	const float R      = speed / glm::abs(turn_rate);
	const glm::vec3 up = glm::vec3(0, 1, 0);
	const glm::vec3 right  = glm::normalize(glm::cross(dir, up));
	const glm::vec3 center = pos + right * (R * glm::sign(turn_rate));
	const float sweep      = -turn_rate * t;
	return center + rotate_y(pos - center, sweep);
}

// ============================================================
// BikeAI::evaluate
// ============================================================

void BikeAI::evaluate(BikeObject* my_bike)
{
	if (!course || !course->is_built) return;

	const BikeAIParams& p = g_ai_params;
	const float dt    = eng->get_dt();
	const float speed = my_bike->speed;

	// ---- Lookahead geometry ----
	const float corner_scan_m = glm::max(p.corner_look_m, speed * 0.8f);
	const float raw_min_r     = course->min_turn_radius_ahead(my_bike->course_dist_m, corner_scan_m);
	const float min_r         = glm::max(raw_min_r, 3.f);
	dbg_min_r = min_r;

	const glm::vec3 up = glm::vec3(0, 1, 0);
	const glm::vec3 bike_right = glm::normalize(glm::cross(my_bike->bike_direction, up));

	// corner_factor — pulls lat_offset toward 0 in tight corners.
	// See [[bike/bikeai#Target position]].
	const float corner_factor = glm::clamp(min_r / p.corner_factor_r_full,
	                                       p.corner_factor_min, 1.f);
	dbg_corner_factor = corner_factor;

	// Wheel-following: wheel != null  →  follower; null  →  leader (racing-line).
	const bool follower = (wheel != nullptr);
	dbg_has_wheel    = follower;

	// Heading reference — leader steers to the course tangent (their own judgement,
	// nobody ahead to follow). Followers steer to the WHEEL's own current heading —
	// magnetism to the rider directly ahead, not the ideal line. If the wheel drifts
	// off the racing line, the follower drifts with them. See [[bike/bikeai#Magnetism]].
	const BikeWaypoint cur_wp  = course->sample(my_bike->course_dist_m);
	const glm::vec3 path_right = glm::normalize(glm::cross(cur_wp.forward, up));
	const glm::vec3 heading_ref_right = follower
	    ? glm::normalize(glm::cross(wheel->bike_direction, up))
	    : path_right;
	const float heading_err = glm::dot(my_bike->bike_direction, heading_ref_right);

	// Lookahead distances (shared by leader + follower; followers need full anticipation
	// or they ride the chord into corners and overshoot).
	const float look_base = p.lookahead_dist_base + speed * p.lookahead_dist_per_ms;
	const float look_anti = look_base * p.anticipation_dist_scale;

	auto angle_to_pt = [&](glm::vec3 pt) {
		const glm::vec3 to = pt - my_bike->get_ws_position();
		const float d = glm::length(to);
		if (d < 0.01f) return 0.f;
		return glm::atan(glm::dot(to / d, bike_right),
		                  glm::dot(to / d, my_bike->bike_direction));
	};

	glm::vec3 near_pt, far_pt;
	float steer_gain = p.steer_k;
	if (follower) {
		// Follower: magnetism dominates. Target points trail the WHEEL's own arc —
		// its current position, and a short-horizon prediction from its live
		// turn_rate — instead of a course-relative lookahead. This is what makes
		// following aggressive and line-agnostic: the follower chases the rider
		// ahead's actual path, not what the racing line says they should be doing.
		const glm::vec3 wheel_pos   = wheel->get_ws_position();
		const glm::vec3 wheel_right = glm::normalize(glm::cross(wheel->bike_direction, up));
		const float target_lat      = lat_offset * corner_factor;  // wheel-frame draft slot
		near_pt = wheel_pos + wheel_right * target_lat;
		far_pt  = arc_predict(wheel_pos, wheel->bike_direction, wheel->speed, wheel->turn_rate,
		                      p.follow_anticipation_t) + wheel_right * target_lat;
		steer_gain = p.follower_steer_k;
	} else {
		// Leader: track the racing line — their own judgement, no magnetism (nobody ahead).
		near_pt = course->racing_line_lookahead(my_bike->course_dist_m, look_base);
		far_pt  = course->racing_line_lookahead(my_bike->course_dist_m, look_anti);
	}
	dbg_lookahead_pt   = near_pt;
	dbg_lookahead_dist = look_base;

	const float steer_near = angle_to_pt(near_pt) * steer_gain - heading_err * 0.5f;
	const float steer_far  = angle_to_pt(far_pt)  * steer_gain - heading_err * 0.5f;
	dbg_steer_near = steer_near;
	dbg_steer_far  = steer_far;
	float steer_out_raw = steer_near + p.anticipation_k * (steer_far - steer_near);

	// Followers: add direct road-frame lateral PD onto the wheel's track on top of
	// the arc-tracking above. Arc-tracking supplies heading; this supplies hard
	// lateral convergence (the PD lookahead above converges slowly when lateral
	// error is small, since atan(err/look_dist) → 0). This is the dominant term —
	// followers are "very very" wheel-magnetized, not racing-line-magnetized.
	// Sign: lat_err > 0 means I'm road-right of the wheel's track → need road-LEFT
	// (positive) steer → steer += +lat_err * k.
	if (follower) {
		const float target_lat_world = wheel->lateral_pos + lat_offset * corner_factor;
		const float lat_err = my_bike->lateral_pos - target_lat_world;
		// D term: damp my own lateral velocity (the wheel's lat_vel cancels out
		// when both are tracking the same line).
		steer_out_raw += lat_err * p.follower_lat_k
		              - my_bike->lateral_vel * p.follower_lat_d_k;
	}

	float steer_out = glm::clamp(steer_out_raw, -1.f, 1.f);

	// ---- Track boundary avoidance (PD) + off-track recovery ----
	// While on-track (within safety margin): additive edge steer — racing-line following stays active.
	// Once past the road edge: override steer entirely and brake to scrub lateral momentum.
	// Overriding prevents the racing-line from fighting recovery and causing oscillation.
	//
	// Sign: positive steer = turn left. lateral_pos > 0 = road-right.
	// corrective_vel > 0 means already moving toward centre.
	float edge_brake = 0.f;
	{
		const float cur_lateral = my_bike->lateral_pos;
		const float road_hw     = course->get_road_half_width(my_bike->course_segment);
		const float safe_edge   = road_hw - p.edge_safety_m;
		dbg_off_track = glm::abs(cur_lateral) > road_hw;

		// Worst lateral: current position or arc prediction, whichever is further out
		float worst_lateral = cur_lateral;
		const glm::vec3 cur_pos = my_bike->get_ws_position();
		const float sample_times[2] = { p.edge_predict_t * 0.5f, p.edge_predict_t };
		for (float t : sample_times) {
			glm::vec3 pred = arc_predict(cur_pos, my_bike->bike_direction, speed, my_bike->turn_rate, t);
			float pred_lat = 0.f;
			int   pred_seg = my_bike->course_segment;
			course->project(pred, &pred_lat, &pred_seg, my_bike->course_dist_m);
			if (glm::abs(pred_lat) > glm::abs(worst_lateral))
				worst_lateral = pred_lat;
		}
		dbg_pred_lateral = worst_lateral;

		// Excess: how far beyond the safe zone (0 if safely inside)
		const float excess = glm::max(glm::abs(worst_lateral) - safe_edge, 0.f);

		float edge_steer = 0.f;
		if (excess > 0.f) {
			// D term: lateral velocity component moving toward centre (positive = closing)
			const float corrective_vel = -glm::sign(worst_lateral) * my_bike->lateral_vel;
			const float pd = excess * p.edge_steer_k - corrective_vel * p.edge_vel_damp;
			edge_steer = glm::sign(worst_lateral) * glm::clamp(pd, 0.f, 1.f);
		}

		const float abs_off_excess = glm::max(glm::abs(cur_lateral) - road_hw, 0.f);
		if (abs_off_excess > 0.f) {
			steer_out = edge_steer;
			edge_brake = glm::clamp(abs_off_excess * p.edge_off_brake_k, 0.f, p.edge_off_brake_max);
		} else {
			steer_out = glm::clamp(steer_out + edge_steer, -1.f, 1.f);
		}

		dbg_edge_steer = edge_steer;
		dbg_edge_brake = edge_brake;
	}

	// ---- Side-by-side avoidance steer (only fires when truly abreast) ----
	// Suppress if it would push toward the predicted edge-danger zone.
	// The edge avoidance above already computed dbg_pred_lateral; if that prediction shows
	// danger, don't let a lateral push from a nearby rider fight the recovery steer.
	// Sign: positive steer = road-left.  steer_toward_edge sign = -sign(pred_lateral).
	// Suppress sep_steer when it has the same sign as steer_toward_edge.
	float sep_steer = my_bike->avoidance_sep_steer;
	{
		const float road_hw   = course->get_road_half_width(my_bike->course_segment);
		const float safe_edge = road_hw - p.edge_safety_m;
		if (glm::abs(dbg_pred_lateral) > safe_edge && sep_steer != 0.f) {
			if (glm::sign(sep_steer) == -glm::sign(dbg_pred_lateral))
				sep_steer = 0.f;
		}
	}
	dbg_avoid_steer = sep_steer;
	steer_out = glm::clamp(steer_out + sep_steer, -1.f, 1.f);

	// ---- Anticipatory braking ----
	const float max_decel = 0.8f * 7.f * my_bike->surface_traction;
	float brake_amount = 0.f;
	dbg_v_max = glm::sqrt(p.corner_speed_k * 9.81f * min_r * my_bike->surface_traction);

	for (int i = 0; i < BRAKE_SCAN_STEPS; ++i) {
		const float d_near   = i * BRAKE_SCAN_STEP_M;
		const float d_mid    = d_near + BRAKE_SCAN_STEP_M * 0.5f;
		const float raw_r    = course->min_turn_radius_ahead(my_bike->course_dist_m + d_near, BRAKE_SCAN_STEP_M);
		const float r        = glm::max(raw_r, 3.f);
		const float v_corner = glm::sqrt(p.corner_speed_k * 9.81f * r * my_bike->surface_traction);
		if (speed > v_corner) {
			const float a_needed = (speed * speed - v_corner * v_corner) / (2.f * d_mid);
			const float frac     = glm::clamp(a_needed / max_decel, 0.f, 0.8f);
			if (frac > brake_amount) {
				brake_amount       = frac;
				dbg_brake_dist_m   = d_mid;
				dbg_v_max          = v_corner;
				dbg_brake_corner_r = r;
			}
		}
	}
	// Collision avoidance brake: raise if the yield system detects a squeeze
	dbg_avoid_brake = my_bike->avoidance_brake;
	brake_amount = glm::max(brake_amount, my_bike->avoidance_brake);
	// Off-track recovery brake — only scrub speed when moving; if stopped, allow power to escape
	if (speed > 1.f)
		brake_amount = glm::max(brake_amount, edge_brake);
	dbg_brake_amount = brake_amount;

	// ---- Power ----
	actual_power_command = damp_dt_independent(target_power_watts, actual_power_command, POWER_SLEW, dt);
	float power_out = actual_power_command + my_bike->boid_long_sep_power;
	dbg_power_base = actual_power_command;

	// ---- Fill ControlInput ----
	dbg_steer_pre_boids = steer_out;
	BikeObject::ControlInput ci;
	const float steer_after_boids = glm::clamp(steer_out, -1.f, 1.f);
	dbg_steer_pre_hard = steer_after_boids;

	// All riders go through the normal handlebar-inertia steer path.
	// Off-track recovery bypasses the priority-yield clamp so the bike can escape the edge.
	ci.steer = dbg_off_track
	    ? steer_after_boids
	    : glm::clamp(steer_after_boids, hard_steer_min, hard_steer_max);
	dbg_steer_final  = ci.steer;
	ci.brake_amount  = brake_amount;
	ci.power         = (brake_amount > 0.1f) ? 0.f : power_out;
	dbg_power_final  = power_out;

	my_bike->update_tick(ci);
}
