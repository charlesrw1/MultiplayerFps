#include "BikeHeaders.h"
#include "Framework/MathLib.h"
#include "Debug.h"
#include "GameEnginePublic.h"
#include <glm/gtx/vector_angle.hpp>
#include <vector>
#include <cfloat>
#include <cmath>

// BikeGameApplication::all_riders is the sense source for the hemisphere scan
// below. It is NOT sorted-order-relative — every neighbor relationship here
// (ahead/behind/beside) is computed from real world positions / course_dist_m /
// lateral_pos, never from array index. See [[bike/bikeai#Ground rule]].
extern BikeGameApplication* g_bike_app;

// ============================================================
// BikeAI::evaluate
//
// Target lateral position = the racing line's precomputed offset, adjusted by
// pack-behavior magnetism (cohesion/separation/draft/line-formation; off by
// default via p.enable_magnetism while worldspace steering is being tuned —
// see BikeAIParams). The AI computes its current lateral offset (derived each
// tick from the worldspace position, BikeObject::tick_transform) and
// aggressively P-controls ci.lateral_shift to close the error. ci.lateral_shift
// bends bike_direction toward the target rather than translating a rail
// position, so position always integrates in worldspace — corner-parametrization
// quirks (fillet seams, uneven waypoint spacing) get smoothed out instead of
// yanking the bike sideways. See [[bike/bikeai]].
// ============================================================

void BikeAI::evaluate(BikeObject* my_bike)
{
	if (!course || !course->is_built) return;

	const BikeAIParams& p = g_ai_params;
	const float dt    = eng->get_dt();
	const float speed = my_bike->speed;
	const glm::vec3 my_pos = my_bike->get_ws_position();

	// ---- Corner lookahead (shared by braking + speed target + manual offset blend) ----
	const float corner_scan_m = glm::max(p.corner_look_m, speed * 0.8f);
	const float raw_min_r     = course->min_turn_radius_ahead(my_bike->course_dist_m, corner_scan_m);
	const float min_r         = glm::max(raw_min_r, 3.f);
	dbg_min_r = min_r;

	// Manual lateral offset blend: full weight on a straight (min_r large),
	// low-passed toward 0 as upcoming curvature tightens, so a debug-assigned
	// offset that made sense on the preceding straight doesn't put the bike on
	// a nonsense line through the corner — it's already back on the racing
	// line by corner entry, same anticipation horizon as the braking scan above.
	const float offset_weight_raw = glm::clamp(
		(min_r - p.offset_corner_r_m) / glm::max(p.offset_straight_r_m - p.offset_corner_r_m, 0.01f),
		0.f, 1.f);
	offset_blend = damp_dt_independent(offset_weight_raw, offset_blend, p.offset_blend_tau_s, dt);

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
	// 2. Magnetism — desired lateral offset, layered on top of the racing
	// line's own precomputed offset. Disabled via p.enable_magnetism: AI then
	// just tracks the racing line with no pack-position adjustment.
	// ============================================================
	float target_offset_delta = 0.f;

	// Nearest forward neighbor is still sensed (drives draft speed-matching in
	// section 5 below regardless of the magnetism toggle).
	const Neighbor* nearest_ahead = nullptr;
	for (const Neighbor& n : neighbors) {
		if (n.long_gap <= 0.f) continue;
		if (!nearest_ahead || n.long_gap < nearest_ahead->long_gap)
			nearest_ahead = &n;
	}

	float separation_term = 0.f;
	float lineform_term   = 0.f;
	float cohesion_term   = 0.f;
	float draft_blend     = 0.f;  // 0..1 — see draft note below
	float avoidance_brake = 0.f;  // combined into brake_amount in section 3

	if (p.enable_magnetism) {
		// Separation: push away from any neighbor closer than separation_dist_m
		// (real 3D distance), weighted by inverse falloff. Soft/continuous —
		// not decisive enough on its own to stop an actual overlap when someone
		// cuts across laterally; see the collision-avoidance block below for that.
		for (const Neighbor& n : neighbors) {
			if (n.dist >= p.separation_dist_m) continue;
			const float weight = 1.f - n.dist / p.separation_dist_m;
			separation_term -= glm::sign(n.lat_gap) * weight * p.separation_k;
		}
		target_offset_delta += separation_term;

		// Draft: blend fraction toward the leader's own lateral_pos, applied
		// further down once the racing-line target is known — see
		// BikeAIParams::draft_follow_k. Computed here (not as a target_offset_delta
		// term) because "adopt their line" and "nudge toward their line" are
		// different operations; strengthens as the gap closes.
		if (nearest_ahead && nearest_ahead->long_gap < p.draft_dist_m)
			draft_blend = glm::clamp(1.f - nearest_ahead->long_gap / p.draft_dist_m, 0.f, 1.f) * p.draft_follow_k;

		// Line formation: smaller, always-on pull toward alignment with the nearest
		// forward neighbor (any range within the sense hemisphere), so riders settle
		// into a single-file line even before they're close enough to draft.
		if (nearest_ahead)
			lineform_term = -nearest_ahead->lat_gap * p.lineformation_k;
		target_offset_delta += lineform_term;

		// Cohesion: only when isolated (nearest neighbor farther than the trigger
		// distance) — pull toward the lateral centroid of sensed neighbors so a
		// rider doesn't drift away from the group when nobody is close enough for
		// draft/line-formation to matter.
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

		// ---- Collision avoidance ----
		// Find the most severe active conflict among ALL sensed neighbors (not
		// just nearest_ahead — someone beside or cutting in matters just as
		// much as someone directly ahead). "Active" = inside BOTH the
		// longitudinal and lateral collision thresholds at once; severity is
		// how deep into both zones (0..1 each), multiplied, so being close on
		// only one axis is never treated as urgent as being close on both.
		const Neighbor* conflict = nullptr;
		float conflict_severity  = 0.f;
		for (const Neighbor& n : neighbors) {
			if (glm::abs(n.long_gap) >= p.collision_long_m) continue;
			if (glm::abs(n.lat_gap)  >= p.collision_lat_m)  continue;
			const float long_severity = 1.f - glm::abs(n.long_gap) / p.collision_long_m;
			const float lat_severity  = 1.f - glm::abs(n.lat_gap)  / p.collision_lat_m;
			const float severity = long_severity * lat_severity;
			if (severity > conflict_severity) { conflict_severity = severity; conflict = &n; }
		}
		float avoidance_lat_term = 0.f;
		if (conflict) {
			// Prefer yielding sideways, away from the conflicting neighbor.
			// Room check: the resulting lateral position must stay on the
			// road, and no OTHER sensed neighbor at a similar longitudinal
			// offset can already be sitting where this move would put me —
			// otherwise I'd just trade one overlap for another.
			const float avoid_dir     = (conflict->lat_gap >= 0.f) ? -1.f : 1.f;  // move away from them
			const float candidate_lat = my_bike->lateral_pos + avoid_dir * p.collision_lat_m;
			const BikeWaypoint wp_now = course->sample(my_bike->course_dist_m);
			const float road_limit    = glm::max(wp_now.road_half_width - p.edge_safety_m, 0.1f);
			bool room_available = glm::abs(candidate_lat) <= road_limit;
			if (room_available) {
				for (const Neighbor& n : neighbors) {
					if (&n == conflict) continue;
					if (glm::abs(n.long_gap) >= p.collision_long_m) continue;
					const float n_gap_after_move = n.lat_gap - avoid_dir * p.collision_lat_m;
					if (glm::abs(n_gap_after_move) < p.collision_lat_m) { room_available = false; break; }
				}
			}
			if (room_available)
				avoidance_lat_term = avoid_dir * p.avoidance_lateral_k * conflict_severity;
			else
				avoidance_brake = p.avoidance_brake_k * conflict_severity;
		}
		target_offset_delta += avoidance_lat_term;
		dbg_avoidance_active   = (conflict != nullptr);
		dbg_avoidance_lat_term = avoidance_lat_term;
		dbg_avoidance_brake    = avoidance_brake;
	}

	dbg_separation_offset = separation_term;
	dbg_draft_blend       = draft_blend;
	dbg_lineform_offset   = lineform_term;
	dbg_cohesion_offset   = cohesion_term;

	// ---- Target lateral position: racing line + magnetism, hard-clamped to
	// the road surface so the AI can never be commanded off the track.
	//
	// Sampled a lookahead distance AHEAD of the bike, not at its current
	// position. At zero lookahead the target is the instantaneous racing-line
	// offset, which changes fast per metre of arc-length through a tight
	// corner (small radius = large curvature); the P-controller below was
	// chasing that fast-moving point and overshooting every tick, visible as
	// wobble specifically on the hardcoded course's two hairpins. Previewing
	// the target this far ahead (a standard pure-pursuit tweak) gives the
	// steering something to anticipate instead of react to. ----
	const float steer_lookahead_m = glm::max(p.steer_lookahead_m, speed * p.steer_lookahead_time_s);
	const BikeWaypoint wp_ahead   = course->sample(my_bike->course_dist_m + steer_lookahead_m);

	const float manual_offset_term = my_bike->manual_lateral_offset * offset_blend;

	const float lat_limit = glm::max(wp_ahead.road_half_width - p.edge_safety_m, 0.1f);
	const float racing_line_target_raw = wp_ahead.racing_line_lateral + target_offset_delta + manual_offset_term;
	// Draft blend applied last, on top of everything else: at draft_blend=1
	// (locked onto the leader's wheel) this fully overrides the racing-line
	// target with their actual lateral_pos — that's the point of drafting,
	// sitting where they are rather than where the ideal line says to be.
	const float target_lat_raw = (nearest_ahead)
	    ? glm::mix(racing_line_target_raw, nearest_ahead->rider->lateral_pos, draft_blend)
	    : racing_line_target_raw;
	const float target_lat = glm::clamp(target_lat_raw, -lat_limit, lat_limit);
	dbg_clamped           = (target_lat != target_lat_raw);
	dbg_target_lat_offset = target_lat;
	dbg_lookahead_pt      = wp_ahead.position + wp_ahead.right * target_lat;  // world-space target point

	// Deliberately proportional-only: this just picks the setpoint for
	// BikeObject's own heading PID (bike_heading_gains) — that's where the
	// real feedback control (and its damping) lives now. See BikeAIParams::
	// lateral_shift_kp / BikeObject_Local.h.
	const float lat_error = target_lat - my_bike->lateral_pos;

	// Heading error: lat_error alone only ever aims at the ROAD's own tangent
	// (wp_here.forward in tick_transform) offset by an angle — it has no idea
	// the racing line itself is turning faster/slower than the road through a
	// corner (that divergence IS the apex line). Finite-difference the racing
	// line's own tangent near the lookahead point and steer toward matching
	// it too, weighted in only as curvature ramps up (corner_weight — full
	// strength mid-corner, ~0 on a straight where the two tangents already
	// agree, using the same min_r-derived blend as the manual offset above).
	const float corner_weight = 1.f - offset_blend;
	const glm::vec3 rl_pos_a = wp_ahead.racing_line_pos;
	const glm::vec3 rl_pos_b = course->sample(my_bike->course_dist_m + steer_lookahead_m + 2.f).racing_line_pos;
	const glm::vec3 rl_tangent = (glm::distance(rl_pos_a, rl_pos_b) > 1e-4f)
	    ? glm::normalize(rl_pos_b - rl_pos_a) : wp_ahead.forward;
	const float heading_error = std::atan2(glm::dot(glm::vec3(0, 1, 0), glm::cross(my_bike->bike_direction, rl_tangent)),
	                                        glm::dot(my_bike->bike_direction, rl_tangent));
	dbg_heading_error = heading_error;

	const float lateral_shift = glm::clamp(
		p.lateral_shift_kp * lat_error + p.heading_shift_kp * heading_error * corner_weight, -1.f, 1.f);
	dbg_lateral_shift = lateral_shift;

	// ci.steer doesn't command heading either — it only drives cosmetic
	// fork/handlebar twist (BikeObject::tick_steer computes roll/lean from the
	// track's own curvature, not from this). Negated because physical lateral
	// movement follows wp.right (BikeObject::tick_transform), the exact
	// opposite handedness of the engine's own cross(bike_direction, up)
	// "right" the fork-twist math is built on — negating keeps the fork
	// turned toward the side the bike is actually steering to.
	dbg_steer_final = -lateral_shift;

	// ============================================================
	// 3. Corner braking — lookahead safety scan, unchanged in spirit.
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

	// Collision-avoidance braking (computed in section 2, applies regardless
	// of whether a corner is also demanding it — take whichever is stronger).
	brake_amount = glm::clamp(glm::max(brake_amount, avoidance_brake), 0.f, 1.f);
	dbg_brake_amount = brake_amount;

	// ============================================================
	// 4. Speed/power PID. No neighbor to hold pace with -> constant cruise
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

	// ---- Fill ControlInput ----
	BikeObject::ControlInput ci;
	ci.steer         = -lateral_shift;  // cosmetic lean only, see note above (sign flipped vs. lateral_shift)
	ci.lateral_shift = lateral_shift;
	ci.brake_amount  = brake_amount;
	ci.power         = (brake_amount > 0.1f) ? 0.f : power;

	my_bike->update_tick(ci);
}
