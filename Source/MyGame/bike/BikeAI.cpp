#include "BikeHeaders.h"
#include "Framework/MathLib.h"
#include "Debug.h"
#include "GameEnginePublic.h"
#include <glm/gtx/vector_angle.hpp>
#include <vector>
#include <cfloat>

// BikeGameApplication::all_riders is the sense source for the hemisphere scan
// below. It is NOT sorted-order-relative — every neighbor relationship here
// (ahead/behind/beside) is computed from real world positions / course_dist_m /
// lateral_pos, never from array index. See [[bike/bikeai#Ground rule]].
extern BikeGameApplication* g_bike_app;

// ============================================================
// BikeAI::evaluate — the one magnetism layer.
//
// Heading (ci.steer) is driven purely by a PID tracking the racing line —
// it never reflects pack position. Lateral repositioning for pack behavior
// (cohesion/separation/draft/line-formation) is a separate desired-offset
// target converted into ci.lateral_shift, a direct sideways translation
// independent of heading (see BikeObject::tick_transform). This is what lets
// a rider tuck in or dodge without having to physically re-aim the bike.
// See [[bike/bikeai]].
// ============================================================

void BikeAI::evaluate(BikeObject* my_bike)
{
	if (!course || !course->is_built) return;

	const BikeAIParams& p = g_ai_params;
	const float dt    = eng->get_dt();
	const float speed = my_bike->speed;
	const glm::vec3 up = glm::vec3(0, 1, 0);
	const glm::vec3 my_pos     = my_bike->get_ws_position();
	const glm::vec3 bike_right = glm::normalize(glm::cross(my_bike->bike_direction, up));

	auto angle_to_pt = [&](glm::vec3 pt) {
		const glm::vec3 to = pt - my_pos;
		const float d = glm::length(to);
		if (d < 0.01f) return 0.f;
		return glm::atan(glm::dot(to / d, bike_right),
		                  glm::dot(to / d, my_bike->bike_direction));
	};

	// ---- Corner lookahead (shared by braking + speed target) ----
	const float corner_scan_m = glm::max(p.corner_look_m, speed * 0.8f);
	const float raw_min_r     = course->min_turn_radius_ahead(my_bike->course_dist_m, corner_scan_m);
	const float min_r         = glm::max(raw_min_r, 3.f);
	dbg_min_r = min_r;

	// ============================================================
	// 1. Sense — forward-hemisphere neighbor scan. Real geometry only:
	//    world-space direction/distance for the cone test, course_dist_m/
	//    lateral_pos for longitudinal/lateral relationships. Never array index.
	// ============================================================
	struct Neighbor { BikeObject* rider; float dist; float long_gap; float lat_gap; };
	std::vector<Neighbor> neighbors;
	if (g_bike_app) {
		const float cos_half_angle = glm::cos(glm::radians(p.sense_half_angle_deg));
		for (BikeObject* other : g_bike_app->all_riders) {
			if (other == my_bike) continue;
			const glm::vec3 to_other = other->get_ws_position() - my_pos;
			const float dist = glm::length(to_other);
			if (dist < 0.01f || dist > p.sense_radius_m) continue;
			if (glm::dot(my_bike->bike_direction, to_other / dist) < cos_half_angle) continue;
			neighbors.push_back({ other,
			                       dist,
			                       other->course_dist_m - my_bike->course_dist_m,  // +ve = ahead
			                       other->lateral_pos    - my_bike->lateral_pos }); // +ve = neighbor road-right
		}
	}
	dbg_num_neighbors = (int)neighbors.size();

	// ============================================================
	// 2. Magnetism — desired lateral offset (delta from current lateral_pos).
	// ============================================================
	float target_offset_delta = 0.f;

	// Separation: push away from any neighbor closer than separation_dist_m
	// (real 3D distance), weighted by inverse falloff.
	float separation_term = 0.f;
	for (const Neighbor& n : neighbors) {
		if (n.dist >= p.separation_dist_m) continue;
		const float weight = 1.f - n.dist / p.separation_dist_m;
		separation_term -= glm::sign(n.lat_gap) * weight * p.separation_k;
	}
	target_offset_delta += separation_term;

	// Nearest forward neighbor — drives both draft and line-formation.
	const Neighbor* nearest_ahead = nullptr;
	for (const Neighbor& n : neighbors) {
		if (n.long_gap <= 0.f) continue;
		if (!nearest_ahead || n.long_gap < nearest_ahead->long_gap)
			nearest_ahead = &n;
	}

	float draft_term = 0.f;
	if (nearest_ahead && nearest_ahead->long_gap < p.draft_dist_m) {
		// Pull toward zero lateral offset from the neighbor's line (tuck in behind).
		draft_term = -nearest_ahead->lat_gap * p.draft_k;
	}
	target_offset_delta += draft_term;

	// Line formation: smaller, always-on pull toward alignment with the nearest
	// forward neighbor (any range within the sense hemisphere), so riders settle
	// into a single-file line even before they're close enough to draft.
	float lineform_term = 0.f;
	if (nearest_ahead)
		lineform_term = -nearest_ahead->lat_gap * p.lineformation_k;
	target_offset_delta += lineform_term;

	// Cohesion: only when isolated (nearest neighbor farther than the trigger
	// distance) — pull toward the lateral centroid of sensed neighbors so a
	// rider doesn't drift away from the group when nobody is close enough for
	// draft/line-formation to matter.
	float cohesion_term = 0.f;
	if (!neighbors.empty()) {
		float nearest_dist = FLT_MAX;
		for (const Neighbor& n : neighbors) nearest_dist = glm::min(nearest_dist, n.dist);
		if (nearest_dist > p.cohesion_trigger_dist_m) {
			float centroid_lat = 0.f;
			for (const Neighbor& n : neighbors) centroid_lat += n.lat_gap;
			centroid_lat /= (float)neighbors.size();
			cohesion_term = centroid_lat * p.cohesion_k;
		}
	}
	target_offset_delta += cohesion_term;

	dbg_separation_offset = separation_term;
	dbg_draft_offset      = draft_term;
	dbg_lineform_offset   = lineform_term;
	dbg_cohesion_offset   = cohesion_term;

	// ---- Hard clamp: magnetism can never command a shift off the track. ----
	const float road_hw     = course->get_road_half_width(my_bike->course_segment);
	const float lat_limit   = glm::max(road_hw - p.edge_safety_m, 0.1f);
	const float target_lat_raw = my_bike->lateral_pos + target_offset_delta;
	const float target_lat     = glm::clamp(target_lat_raw, -lat_limit, lat_limit);
	dbg_clamped           = (target_lat != target_lat_raw);
	dbg_target_lat_offset = target_lat;

	const float lat_error = target_lat - my_bike->lateral_pos;
	const float lateral_shift = glm::clamp(lat_error * p.lateral_shift_kp, -1.f, 1.f);
	dbg_lateral_shift = lateral_shift;

	// ============================================================
	// 3. Racing-line heading PID — the only contributor to ci.steer.
	// ============================================================
	const float look_dist = p.lookahead_dist_base + speed * p.lookahead_dist_per_ms;
	const glm::vec3 look_pt = course->racing_line_lookahead(my_bike->course_dist_m, look_dist);
	dbg_lookahead_pt = look_pt;

	const float steer_error = angle_to_pt(look_pt);
	steer_integral   = glm::clamp(steer_integral + steer_error * dt, -1.f, 1.f);
	const float steer_deriv = (dt > 1e-4f) ? (steer_error - steer_prev_error) / dt : 0.f;
	steer_prev_error = steer_error;

	const float steer_out = glm::clamp(
		p.steer_kp * steer_error + p.steer_ki * steer_integral + p.steer_kd * steer_deriv,
		-1.f, 1.f);
	dbg_steer_final = steer_out;

	// ============================================================
	// 4. Corner braking — lookahead safety scan, unchanged in spirit.
	// ============================================================
	const float max_decel = 0.8f * 7.f * my_bike->surface_traction;
	float brake_amount = 0.f;
	dbg_v_max = glm::sqrt(p.corner_speed_k * 9.81f * min_r * my_bike->surface_traction);

	for (int i = 0; i < BRAKE_SCAN_STEPS; ++i) {
		const float d_near = i * BRAKE_SCAN_STEP_M;
		const float d_mid  = d_near + BRAKE_SCAN_STEP_M * 0.5f;
		const float raw_r  = course->min_turn_radius_ahead(my_bike->course_dist_m + d_near, BRAKE_SCAN_STEP_M);
		const float r      = glm::max(raw_r, 3.f);
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
	dbg_brake_amount = brake_amount;

	// ============================================================
	// 5. Speed/power PID. No neighbor to hold pace with -> constant cruise
	//    power. Otherwise, PID-track the nearest drafting neighbor's speed
	//    (with a P-term gap correction) so followers actively burn matches to
	//    hold the wheel rather than just relying on steering/shift alone.
	// ============================================================
	float power;
	if (nearest_ahead && nearest_ahead->long_gap < p.draft_dist_m) {
		const float gap_err     = nearest_ahead->long_gap - (p.draft_dist_m * 0.4f);
		const float target_speed = nearest_ahead->rider->speed + glm::clamp(gap_err * 0.5f, -3.f, 3.f);
		dbg_target_speed = target_speed;

		const float speed_error = target_speed - speed;
		speed_integral    = glm::clamp(speed_integral + speed_error * dt, -50.f, 50.f);
		const float speed_deriv = (dt > 1e-4f) ? (speed_error - speed_prev_error) / dt : 0.f;
		speed_prev_error  = speed_error;

		const float pid_out = p.speed_kp * speed_error + p.speed_ki * speed_integral + p.speed_kd * speed_deriv;
		power = glm::clamp(p.base_power_w + pid_out, p.min_power_w, p.max_power_w);
	} else {
		dbg_target_speed = 0.f;
		speed_integral   = 0.f;
		speed_prev_error = 0.f;
		power = p.base_power_w;
	}
	dbg_power_final = power;

	// ---- Lateral velocity bookkeeping (debug / recorder use) ----
	my_bike->lateral_vel      = (dt > 1e-6f) ? (my_bike->lateral_pos - my_bike->prev_lateral_pos) / dt : 0.f;
	my_bike->prev_lateral_pos = my_bike->lateral_pos;

	// ---- Fill ControlInput ----
	BikeObject::ControlInput ci;
	ci.steer         = steer_out;
	ci.lateral_shift = lateral_shift;
	ci.brake_amount  = brake_amount;
	ci.power         = (brake_amount > 0.1f) ? 0.f : power;

	my_bike->update_tick(ci);
}
