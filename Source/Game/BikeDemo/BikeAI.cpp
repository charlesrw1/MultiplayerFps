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

	// Boid mode: non-leader riders with active group forces use direct heading from boid_desired_dir.
	// Leaders (and isolated riders with no boid data) fall back to racing-line following.
	const bool in_boid_mode = !is_leader && my_bike->boid_forces_active;
	dbg_is_boid_mode = in_boid_mode;
	my_bike->boid_steer_override = false;

	float steer_out = 0.f;

	if (in_boid_mode) {
		// Convert boid desired heading to a steer_out value.
		// cross_y > 0  →  boid_desired_dir is to the RIGHT of bike_direction
		// positive steer = LEFT, so steer = -(angle to right) * k
		const glm::vec3 want_dir  = my_bike->boid_desired_dir;
		const float cross_y       = glm::dot(glm::cross(my_bike->bike_direction, want_dir), up);
		const float dot_cd        = glm::dot(my_bike->bike_direction, want_dir);
		const float angle_err     = glm::atan(cross_y, dot_cd);  // + = right of current heading
		steer_out = glm::clamp(-angle_err * p.boid_turn_k, -1.f, 1.f);
		dbg_steer_near     = steer_out;
		dbg_steer_far      = steer_out;
		dbg_lookahead_dist = 0.f;
	} else {
		// Racing-line following (leaders and isolated fallback)
		const float look_base        = p.lookahead_dist_base + speed * p.lookahead_dist_per_ms;
		const glm::vec3 lookahead_pt = course->racing_line_lookahead(my_bike->course_dist_m, look_base);
		dbg_lookahead_pt   = lookahead_pt;
		dbg_lookahead_dist = look_base;

		const glm::vec3 bike_right = glm::normalize(glm::cross(my_bike->bike_direction, up));
		const glm::vec3 to_target  = lookahead_pt - my_bike->get_ws_position();
		const float dist_to_tgt    = glm::length(to_target);

		const float angle_to_target = (dist_to_tgt > 0.01f)
		    ? glm::atan(glm::dot(glm::normalize(to_target), bike_right),
		                glm::dot(glm::normalize(to_target), my_bike->bike_direction))
		    : 0.f;

		const BikeWaypoint cur_wp  = course->sample(my_bike->course_dist_m);
		const glm::vec3 path_right = glm::normalize(glm::cross(cur_wp.forward, up));
		const float heading_err    = glm::dot(my_bike->bike_direction, path_right);

		const float steer_near = angle_to_target * p.steer_k - heading_err * 0.5f;

		const glm::vec3 anti_pt  = course->racing_line_lookahead(my_bike->course_dist_m, look_base * p.anticipation_dist_scale);
		const glm::vec3 to_anti  = anti_pt - my_bike->get_ws_position();
		const float dist_to_anti = glm::length(to_anti);
		float steer_far = steer_near;
		if (dist_to_anti > 0.01f) {
			const float angle_anti = glm::atan(glm::dot(glm::normalize(to_anti), bike_right),
			                                    glm::dot(glm::normalize(to_anti), my_bike->bike_direction));
			steer_far = angle_anti * p.steer_k - heading_err * 0.5f;
		}
		dbg_steer_near = steer_near;
		dbg_steer_far  = steer_far;
		steer_out = glm::clamp(steer_near + p.anticipation_k * (steer_far - steer_near), -1.f, 1.f);
	}

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

	// Boid mode uses a direct turn-rate override in tick_steer instead of the handlebar inertia path.
	// Off-track recovery always falls back to normal physics steer so the bike can escape the edge.
	if (in_boid_mode && !dbg_off_track) {
		const float tr = steer_after_boids * p.boid_max_turn_rate;
		my_bike->boid_steer_override     = true;
		my_bike->boid_turn_rate_override = glm::clamp(tr, -p.boid_max_turn_rate, p.boid_max_turn_rate);
		dbg_boid_turn_rate = my_bike->boid_turn_rate_override;
		ci.steer = 0.f;
	} else {
		my_bike->boid_steer_override = false;
		ci.steer = dbg_off_track
		    ? steer_after_boids
		    : glm::clamp(steer_after_boids, hard_steer_min, hard_steer_max);
	}
	dbg_steer_final  = ci.steer;
	ci.brake_amount  = brake_amount;
	ci.power         = (brake_amount > 0.1f) ? 0.f : power_out;
	dbg_power_final  = power_out;

	my_bike->update_tick(ci);
}
