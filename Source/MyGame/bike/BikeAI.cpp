#include "BikeHeaders.h"
#include "Framework/MathLib.h"
#include "Debug.h"
#include "GameEnginePublic.h"
#include <glm/gtx/vector_angle.hpp>
#include <vector>
#include <algorithm>
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
// cohesion (draft: pulls in behind whoever's ahead, plus a general lateral
// "bunch up" pull) — see BikeAIParams. The AI computes its current lateral
// offset (derived each tick from the worldspace position, BikeObject::
// tick_transform) and P-controls ci.lateral_shift to close the error.
// ci.lateral_shift bends bike_direction toward the target rather than
// translating a rail position, so position always integrates in worldspace —
// corner-parametrization quirks (fillet seams, uneven waypoint spacing) get
// smoothed out instead of yanking the bike sideways.
//
// Avoidance is a separate, independently-toggleable term and does NOT go
// through any of that: it commands ci.avoidance_lateral_vel, a direct
// worldspace lateral slide applied in tick_transform without touching
// bike_direction/heading at all — seeded from a heading-based approach that
// overshot and yoyoed against cohesion pulling back in (turn momentum has to
// build up AND unwind; a direct velocity has no memory to unwind). See
// [[bike/bikeai]].
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

	// My own local frame — used ONLY by avoidance (section 3) for a true
	// worldspace box-overlap test, deliberately NOT the course's road-tangent
	// frame (wp.right), which diverges from the bike's actual facing through
	// corners/while sliding. Same handedness as BikeObject::tick_transform's
	// own orientation basis ("this file's own cross(bike_direction, up)").
	const glm::vec3 my_right = glm::normalize(glm::cross(my_bike->bike_direction, glm::vec3(0.f, 1.f, 0.f)));

	// ============================================================
	// 1. Sense — ALL nearby riders within sense_radius_m, omnidirectional
	//    (360°). Real geometry only: world-space direction/distance,
	//    course_dist_m/lateral_pos for cohesion's longitudinal/lateral
	//    relationships, ws_fwd_gap/ws_right_gap (projected onto MY
	//    bike_direction/right) for avoidance's box-overlap test. Never array
	//    index.
	//
	//    in_forward_hemisphere additionally flags whether a rider falls
	//    inside the forward sense cone — cohesion (section 2) only acts on
	//    hemisphere-flagged riders (that's what "hemisphere-based" sensing
	//    means for drafting: you draft off whoever's ahead of YOU, not
	//    whoever's behind). Avoidance (section 3) deliberately ignores this
	//    flag and scans everyone in radius: a physical box-overlap is
	//    symmetric — if I don't sense someone who's overlapping my box
	//    (e.g. they're right behind me, outside my forward cone), avoidance
	//    would silently do nothing on my side even though we're overlapping,
	//    leaving only THEIR avoidance to push us apart. Both riders need to
	//    react for the overlap to resolve quickly and for it to look like a
	//    mutual push instead of one bike getting shoved.
	// ============================================================
	struct Neighbor { BikeObject* rider; float dist; float long_gap; float lat_gap; float ws_fwd_gap; float ws_right_gap; bool in_forward_hemisphere; };
	std::vector<Neighbor> all_nearby;
	if (g_bike_app) {
		const float cos_half_angle = glm::cos(glm::radians(p.sense_half_angle_deg));
		for (BikeObject* other : g_bike_app->all_riders) {
			if (other == my_bike) continue;
			const glm::vec3 to_other = other->get_ws_position() - my_pos;
			const float dist = glm::length(to_other);
			if (dist < 0.01f || dist > p.sense_radius_m) continue;
			const bool in_hemi = glm::dot(my_bike->bike_direction, to_other / dist) >= cos_half_angle;
			all_nearby.push_back({ other,
			                        dist,
			                        other->course_dist_m - my_bike->course_dist_m,  // +ve = ahead
			                        other->lateral_pos    - my_bike->lateral_pos,   // +ve = neighbor road-right
			                        glm::dot(to_other, my_bike->bike_direction),    // +ve = ahead of me, MY facing
			                        glm::dot(to_other, my_right),                   // +ve = to my right, MY facing
			                        in_hemi });
		}
	}
	dbg_num_neighbors = (int)std::count_if(all_nearby.begin(), all_nearby.end(),
		[](const Neighbor& n) { return n.in_forward_hemisphere; });

	// Debug breakdown, one entry per sensed neighbor (omnidirectional — same
	// order/size as `all_nearby` above), filled in as each cohesion/avoidance
	// term below decides its contribution. See BikeAIDebugNeighbor /
	// BikeDebugger's selected-rider view.
	dbg_neighbors.clear();
	dbg_neighbors.reserve(all_nearby.size());
	for (const Neighbor& n : all_nearby)
		dbg_neighbors.push_back({ n.rider, n.dist, n.long_gap, n.lat_gap });

	// ============================================================
	// 2. Cohesion (draft) — desired lateral offset, layered on top of the
	// racing line's own precomputed offset. Independently toggleable from
	// avoidance below via p.enable_cohesion.
	// ============================================================
	float target_offset_delta = 0.f;

	// Nearest forward neighbor — hemisphere-sensed, never sorted/array-index
	// (see file header). Drives both cohesion's "behind" pull and its
	// following-gap speed match below, regardless of the cohesion toggle.
	const Neighbor* nearest_ahead = nullptr;
	for (const Neighbor& n : all_nearby) {
		if (!n.in_forward_hemisphere) continue;
		if (n.long_gap <= 0.f) continue;
		if (!nearest_ahead || n.long_gap < nearest_ahead->long_gap)
			nearest_ahead = &n;
	}

	float cohesion_behind_lat = 0.f;
	float cohesion_closer_lat = 0.f;
	bool  cohesion_have_speed_target = false;
	float cohesion_target_speed      = 0.f;

	if (p.enable_cohesion) {
		// "Behind": pull laterally into the nearest ahead-neighbor's wheel
		// track (drive their lat_gap toward 0) and hold a following gap by
		// speed-matching, once within cohesion_follow_dist_m longitudinally —
		// real drafting, sitting behind rather than beside.
		if (nearest_ahead && nearest_ahead->long_gap < p.cohesion_follow_dist_m) {
			// +lat_gap, not -lat_gap: target_lat is an ABSOLUTE lateral position
			// (same frame as lateral_pos), and wp.racing_line_lateral is already
			// close to my current position — so target += lat_gap converges
			// toward leader.lateral_pos (my_pos + lat_gap), i.e. actually pulls
			// in behind them, matching cohesion_closer_lat's sign convention below.
			cohesion_behind_lat = nearest_ahead->lat_gap * p.cohesion_behind_k;
			dbg_neighbors[nearest_ahead - all_nearby.data()].is_cohesion_behind_leader = true;
			dbg_cohesion_gap_m = nearest_ahead->long_gap;

			const float gap_err = nearest_ahead->long_gap - p.cohesion_gap_m;
			cohesion_have_speed_target = true;
			cohesion_target_speed      = nearest_ahead->rider->speed + glm::clamp(gap_err * p.cohesion_gap_kp, -3.f, 3.f);
		}
		target_offset_delta += cohesion_behind_lat;

		// "Closer": always-on lateral magnetism toward the centroid of
		// hemisphere-sensed neighbors (ahead or behind, but still within MY
		// forward cone) — bunches the group up laterally even when nobody is
		// close enough ahead to draft off of.
		{
			float centroid_lat = 0.f;
			int   hemi_count   = 0;
			for (size_t i = 0; i < all_nearby.size(); ++i) {
				const Neighbor& n = all_nearby[i];
				if (!n.in_forward_hemisphere) continue;
				centroid_lat += n.lat_gap;
				++hemi_count;
				dbg_neighbors[i].is_cohesion_closer_member = true;
			}
			if (hemi_count > 0) {
				centroid_lat /= (float)hemi_count;
				cohesion_closer_lat = centroid_lat * p.cohesion_closer_k;
				const BikeWaypoint wp_centroid = course->sample(my_bike->course_dist_m);
				dbg_cohesion_centroid_pt = wp_centroid.position + wp_centroid.right * (my_bike->lateral_pos + centroid_lat);
			}
		}
		target_offset_delta += cohesion_closer_lat;
	}
	dbg_cohesion_behind_lat = cohesion_behind_lat;
	dbg_cohesion_closer_lat = cohesion_closer_lat;

	// ============================================================
	// 3. Avoidance — decentralized overlap prevention, computed and applied
	// entirely in WORLDSPACE (ws_fwd_gap/ws_right_gap, projected onto MY OWN
	// bike_direction/right from section 1 — never course_dist_m/lateral_pos,
	// which is road-tangent-relative and diverges from actual physical
	// position through corners). Conflict test is a true box-overlap
	// (Minkowski sum): every rider shares the same prefab box
	// (avoidance_box_half_long_m/lat_m half-extents), so two riders' boxes
	// touch once the worldspace gap on an axis drops below TWICE the
	// half-extent — that's the hard "drop dead" boundary. A soft margin
	// OUTSIDE that boundary ramps severity 0->1, so the response builds in
	// smoothly instead of snapping to full force. Touches LATERAL POSITION
	// directly (a worldspace displacement vector, ci.avoidance_lateral_vel),
	// never heading/bike_direction — see BikeAIParams::enable_avoidance for
	// why: routing this through the heading PID (like cohesion's
	// lateral_shift_kp path) built up turn momentum that overshot and had to
	// unwind, yoyoing against cohesion pulling back in. A direct velocity,
	// proportional only to this tick's severity, has no memory to unwind.
	// ============================================================
	glm::vec3 avoidance_lateral_vel(0.f);  // direct worldspace slide vector (ControlInput::avoidance_lateral_vel)
	float avoidance_brake = 0.f;  // combined into brake_amount in section 5

	if (p.enable_avoidance) {
		const float hard_long_m    = 2.f * p.avoidance_box_half_long_m;
		const float hard_lat_m     = 2.f * p.avoidance_box_half_lat_m;
		const float trigger_long_m = hard_long_m + p.avoidance_soft_margin_long_m;
		const float trigger_lat_m  = hard_lat_m  + p.avoidance_soft_margin_lat_m;
		auto axis_severity = [](float gap_abs, float hard, float trigger) {
			if (gap_abs >= trigger) return 0.f;
			if (gap_abs <= hard)    return 1.f;
			return (trigger - gap_abs) / (trigger - hard);  // linear ramp through the soft margin
		};

		// Find the most severe active conflict among ALL sensed neighbors,
		// omnidirectionally (all_nearby, NOT hemisphere-filtered — see section
		// 1: someone directly behind me still physically overlaps my box, and
		// I need to react too, not just leave it to their avoidance). Severity
		// is how deep into both axes' zones (0..1 each), multiplied, so being
		// close on only one axis is never treated as urgent as being close on
		// both.
		const Neighbor* conflict = nullptr;
		float conflict_severity  = 0.f;
		for (const Neighbor& n : all_nearby) {
			const float long_abs = glm::abs(n.ws_fwd_gap);
			const float lat_abs  = glm::abs(n.ws_right_gap);
			if (long_abs >= trigger_long_m) continue;
			if (lat_abs  >= trigger_lat_m)  continue;
			const float severity = axis_severity(long_abs, hard_long_m, trigger_long_m)
			                      * axis_severity(lat_abs,  hard_lat_m,  trigger_lat_m);
			if (severity > conflict_severity) { conflict_severity = severity; conflict = &n; }
		}
		if (conflict) {
			BikeAIDebugNeighbor& dn_conflict = dbg_neighbors[conflict - all_nearby.data()];
			dn_conflict.is_avoidance_conflict = true;
			dn_conflict.avoidance_severity    = conflict_severity;
			// Prefer yielding sideways, away from the conflicting neighbor, by a
			// full hard_lat_m clearance (guarantees the box no longer overlaps,
			// not just "outside the soft zone"). Room check, also worldspace: the
			// resulting position must stay on the road (approximated via the
			// course frame — road edges are inherently road-relative), and no
			// OTHER sensed neighbor's box can already be sitting where this move
			// would put me — otherwise I'd just trade one overlap for another.
			const float avoid_sign = (conflict->ws_right_gap >= 0.f) ? -1.f : 1.f;  // move away from them, MY right
			const glm::vec3 move_vec = my_right * (avoid_sign * hard_lat_m);
			const BikeWaypoint wp_now = course->sample(my_bike->course_dist_m);
			const float candidate_lat = my_bike->lateral_pos + glm::dot(move_vec, wp_now.right);
			const float road_limit    = glm::max(wp_now.road_half_width - p.edge_safety_m, 0.1f);
			bool room_available = glm::abs(candidate_lat) <= road_limit;
			if (room_available) {
				for (const Neighbor& n : all_nearby) {
					if (&n == conflict) continue;
					if (glm::abs(n.ws_fwd_gap) >= trigger_long_m) continue;
					const float n_right_after_move = n.ws_right_gap - avoid_sign * hard_lat_m;
					if (glm::abs(n_right_after_move) < hard_lat_m) { room_available = false; break; }
				}
			}
			// Proportional to severity only — no integrator, no accel/momentum
			// to build up or unwind, so it self-cancels the instant the
			// conflict clears instead of overshooting past it.
			if (room_available)
				avoidance_lateral_vel = my_right * (avoid_sign * p.avoidance_lateral_speed_mps * conflict_severity);
			else
				avoidance_brake = p.avoidance_brake_k * conflict_severity;
		}
		dbg_avoidance_active = (conflict != nullptr);
	} else {
		dbg_avoidance_active = false;
	}
	dbg_avoidance_lateral_vel = glm::dot(avoidance_lateral_vel, my_right);
	dbg_avoidance_brake       = avoidance_brake;

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
	// Debug lateral override: hard-pin the target, bypassing racing line and
	// cohesion entirely — see BikeObject::ai_override_lateral_enabled.
	// Avoidance is NOT overridden by this (it's a direct lateral_shift command
	// applied below, not part of the target lateral position).
	const float target_lat_raw = my_bike->ai_override_lateral_enabled
	    ? my_bike->ai_override_lateral_pos_m
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

	// Cohesion/racing-line only — avoidance never touches this (or heading at
	// all); it's a direct worldspace lateral slide instead, see section 3.
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
	// 4. Corner braking — lookahead safety scan, unrelated to cohesion/avoidance.
	// ============================================================
	const float max_decel = 0.8f * 7.f * my_bike->surface_traction;
	float brake_amount = 0.f;
	dbg_v_max = glm::sqrt(p.corner_speed_k * 9.81f * min_r * my_bike->surface_traction);
	// Reset every tick, not just when the loop below finds a new worst corner:
	// these fields only get written inside "if (frac > brake_amount)", so if no
	// corner needs braking this tick they'd otherwise keep holding whatever
	// stale point they last pointed at (maybe a lap ago) — and the debug
	// overlay draws a sphere there whenever brake_amount > 0, including from
	// collision-avoidance braking alone, making it look like it's pointing at
	// a random, unrelated spot on the track.
	dbg_brake_dist_m   = 0.f;
	dbg_brake_corner_r = 0.f;

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

	// Avoidance braking (computed in section 3, applies regardless of whether
	// a corner is also demanding it — take whichever is stronger). Applied
	// directly, bypassing the speed PID below — no integral lag, full
	// severity-scaled brake the instant a conflict has no room to yield.
	brake_amount = glm::clamp(glm::max(brake_amount, avoidance_brake), 0.f, 1.f);
	dbg_brake_amount = brake_amount;

	// ============================================================
	// 5. Speed/power PID. No cohesion follow target -> constant cruise power.
	//    Otherwise, PID-track the behind-leader's speed (with a P-term gap
	//    correction, computed in section 2) so a follower actively burns
	//    matches to hold the wheel rather than just relying on steering
	//    alone. Debug override (BikeObject::ai_override_speed_enabled)
	//    replaces whichever target this tick would otherwise have picked.
	// ============================================================
	float power;
	bool  have_speed_target = false;
	float target_speed      = 0.f;
	if (my_bike->ai_override_speed_enabled) {
		have_speed_target = true;
		target_speed      = my_bike->ai_override_target_speed_ms;
	} else if (cohesion_have_speed_target) {
		have_speed_target = true;
		target_speed      = cohesion_target_speed;
	}

	if (have_speed_target) {
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
	ci.avoidance_lateral_vel = avoidance_lateral_vel;  // direct worldspace slide, bypasses heading entirely — see section 3

	my_bike->update_tick(ci);
}
