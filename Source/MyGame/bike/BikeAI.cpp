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

	// "Ride 2nd wheel" debug override (BikeObject::ride_2nd_wheel_enabled, set
	// per-rider from BikeDebugger): locks the "behind" sub-term below onto the
	// CURRENT LEADER of my own group specifically — not whichever
	// hemisphere-sensed rider happens to be nearest ahead — and, unlike normal
	// sensing, targets them regardless of distance (chases back onto their
	// wheel from anywhere). Group leader = the rider sharing my group_id with
	// pos_in_group_norm==0 (real position, written by
	// BikeGameApplication::update_groups — never array index). If I AM the
	// leader (or my group has only me), there's no one to target — falls
	// through to normal hemisphere-based behavior below.
	BikeObject* forced_leader = nullptr;
	if (my_bike->ride_2nd_wheel_enabled && g_bike_app) {
		for (BikeObject* other : g_bike_app->all_riders) {
			if (other == my_bike) continue;
			if (other->group_id != my_bike->group_id) continue;
			if (other->pos_in_group_norm > 0.0001f) continue;
			forced_leader = other;
			break;
		}
	}
	BikeObject* behind_rider    = forced_leader ? forced_leader : (nearest_ahead ? nearest_ahead->rider : nullptr);
	const float behind_long_gap = forced_leader ? (forced_leader->course_dist_m - my_bike->course_dist_m)
	                                             : (nearest_ahead ? nearest_ahead->long_gap : 0.f);
	const float behind_lat_gap  = forced_leader ? (forced_leader->lateral_pos - my_bike->lateral_pos)
	                                             : (nearest_ahead ? nearest_ahead->lat_gap : 0.f);
	// Group's actual rank-0 leader (pos_in_group_norm==0 — a RANK, uniquely
	// identifies whichever single rider is furthest along regardless of how
	// close a tie is, real position not array index, same rule forced_leader
	// above uses). near_group_front then measures "at the front" by actual
	// course_dist_m distance to that leader instead of by rank/group-size —
	// see BikeAIParams::front_abreast_join_dist_m for why rank alone doesn't
	// work here. If I AM the leader myself, the search below finds no one
	// else at rank 0, my_group_leader stays null, and near_group_front
	// defaults to true for me — correctly "at the front" with a 0m gap.
	BikeObject* my_group_leader = nullptr;
	if (g_bike_app) {
		for (BikeObject* other : g_bike_app->all_riders) {
			if (other == my_bike) continue;
			if (other->group_id != my_bike->group_id) continue;
			if (other->pos_in_group_norm > 0.0001f) continue;
			my_group_leader = other;
			break;
		}
	}
	auto near_group_front = [&](BikeObject* b) {
		if (!my_group_leader) return true;
		return (my_group_leader->course_dist_m - b->course_dist_m) <= p.front_abreast_join_dist_m;
	};

	// "Stay at front" (BikeObject::ai_behavior_state == StayingAtFront), once
	// this rider is close enough to the front of its group (near_group_front
	// above): suppresses the "behind" draft term entirely, overriding even a
	// forced_leader from the override above (mutually exclusive in practice;
	// this one wins). Without this, cohesion would pull the rider laterally
	// into whichever other front-of-group rider it senses ahead and
	// speed-match into a following gap — i.e. tuck into single file —
	// defeating the point of holding the front. With it suppressed, several
	// StayingAtFront riders near the front are left to the dedicated
	// front_abreast separation force below (and avoidance's side-by-side
	// rule as a last resort) to settle apart laterally instead, which is
	// what "riding abreast" actually looks like.
	const bool suppress_front_draft = my_bike->ai_behavior_state == BikeAIBehaviorState::StayingAtFront
	    && near_group_front(my_bike);
	// Forced leader is targeted at any distance; normal sensing still gates on
	// cohesion_follow_dist_m (don't draft off someone way out of range).
	const bool behind_target_active = !suppress_front_draft && (forced_leader != nullptr
	    || (nearest_ahead != nullptr && nearest_ahead->long_gap < p.cohesion_follow_dist_m));

	// Front-abreast pace match: suppress_front_draft above kills the normal
	// draft speed-match (cohesion_have_speed_target below), so without this a
	// StayingAtFront rider holding the front would just independently PID to
	// base_power_w cruise — no coupling to whoever else is riding abreast, so
	// tiny power/gradient differences compound over time and the line drifts
	// apart or slides into a diagonal.
	//
	// Per-neighbor target = their current speed, corrected by how far ahead
	// (+) or behind (-) I am of THEM along the course (front_abreast_gap_kp,
	// same clamped-proportional shape as cohesion_gap_kp's draft follow-gap
	// term above) — averaged over every OTHER StayingAtFront rider in my
	// group, whether or not they've reached the front yet (a groupmate still
	// sprinting to catch up should still pull my target down if I'm ahead of
	// them, not just once they arrive). Matching raw speed alone would only
	// ever hold whatever gap the pair happened to have when speeds first
	// matched — being slightly ahead in POSITION, even at identical speed,
	// still has to cost some target speed or the gap never actually closes.
	// Uses g_bike_app->all_riders directly rather than the hemisphere-limited
	// all_nearby scan — an abreast rider beside me is often outside my own
	// forward sense cone entirely.
	bool  front_abreast_have_speed_target = false;
	float front_abreast_target_speed      = 0.f;  // may go <= 0 — see the coast branch in section 5
	if (suppress_front_draft && g_bike_app) {
		float target_sum = 0.f;
		int   count       = 0;
		for (BikeObject* other : g_bike_app->all_riders) {
			if (other == my_bike) continue;
			if (other->group_id != my_bike->group_id) continue;
			if (other->ai_behavior_state != BikeAIBehaviorState::StayingAtFront) continue;
			const float lead_m = my_bike->course_dist_m - other->course_dist_m;  // +ve = I'm ahead of them
			const float correction = glm::clamp(lead_m * p.front_abreast_gap_kp, -p.front_abreast_gap_cap_ms, p.front_abreast_gap_cap_ms);
			target_sum += other->speed - correction;
			++count;
		}
		if (count > 0) {
			front_abreast_have_speed_target = true;
			front_abreast_target_speed      = target_sum / (float)count;
		}
	}

	float cohesion_behind_lat = 0.f;
	float cohesion_closer_lat = 0.f;
	bool  cohesion_have_speed_target = false;
	float cohesion_target_speed      = 0.f;

	if (p.enable_cohesion) {
		// "Behind": pull laterally into the target's wheel track (drive their
		// lat_gap toward 0) and hold a following gap by speed-matching — real
		// drafting, sitting behind rather than beside.
		if (behind_target_active && behind_rider) {
			// +lat_gap, not -lat_gap: target_lat is an ABSOLUTE lateral position
			// (same frame as lateral_pos), and wp.racing_line_lateral is already
			// close to my current position — so target += lat_gap converges
			// toward leader.lateral_pos (my_pos + lat_gap), i.e. actually pulls
			// in behind them, matching cohesion_closer_lat's sign convention below.
			cohesion_behind_lat = behind_lat_gap * p.cohesion_behind_k;
			for (size_t i = 0; i < all_nearby.size(); ++i) {
				if (all_nearby[i].rider == behind_rider) { dbg_neighbors[i].is_cohesion_behind_leader = true; break; }
			}
			dbg_cohesion_gap_m = behind_long_gap;

			const float gap_err = behind_long_gap - p.cohesion_gap_m;
			cohesion_have_speed_target = true;
			cohesion_target_speed      = behind_rider->speed + glm::clamp(gap_err * p.cohesion_gap_kp, -3.f, 3.f);
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
				// A fellow front-abreast rider is handled entirely by the dedicated
				// separation force below (a fixed target spacing), not this
				// bunch-together pull — mixing the two fights itself: "closer" pulls
				// them together, the separation force pushes them apart, and
				// whichever direction wins changes tick to tick.
				if (suppress_front_draft && n.rider->ai_behavior_state == BikeAIBehaviorState::StayingAtFront
				    && n.rider->group_id == my_bike->group_id && near_group_front(n.rider))
					continue;
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

	// Front-abreast lateral separation — independent of p.enable_cohesion (a
	// distinct feature, not a cohesion sub-term). Continuous/proportional,
	// unlike avoidance's binary box-overlap trigger below: without this,
	// several StayingAtFront riders holding the front would all aim at the
	// SAME racing-line target lateral position with nothing to differentiate
	// them, so avoidance's binary push would be the only thing keeping them
	// apart — constantly flipping in and out of its hard/soft zone as the
	// racing-line target pulled them back together, i.e. visibly "pushing the
	// other rider away" every time they drift back into range instead of
	// settling into a stable line. This instead holds every OTHER
	// StayingAtFront rider currently also at the front of my group at a fixed
	// spacing (front_abreast_spacing_m) via a smooth per-metre-of-encroachment
	// push, so the two riders settle at that spacing and avoidance's own
	// (much narrower) zone never engages between them.
	float front_abreast_lat = 0.f;
	if (suppress_front_draft && g_bike_app) {
		for (BikeObject* other : g_bike_app->all_riders) {
			if (other == my_bike) continue;
			if (other->group_id != my_bike->group_id) continue;
			if (other->ai_behavior_state != BikeAIBehaviorState::StayingAtFront) continue;
			if (!near_group_front(other)) continue;
			const float lat_gap = other->lateral_pos - my_bike->lateral_pos;  // +ve = they're road-right of me
			const float gap_abs = glm::abs(lat_gap);
			if (gap_abs >= p.front_abreast_spacing_m) continue;
			const float push_sign = (lat_gap >= 0.f) ? -1.f : 1.f;  // push away from them, my frame
			const float encroach  = p.front_abreast_spacing_m - gap_abs;
			front_abreast_lat += push_sign * encroach * p.front_abreast_separation_k;
		}
	}
	target_offset_delta += front_abreast_lat;

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

		// Find the most severe conflict I'M RESPONSIBLE for, among ALL sensed
		// neighbors, omnidirectionally (all_nearby, NOT hemisphere-filtered —
		// see section 1). Severity is how deep into both axes' zones (0..1
		// each), multiplied, so being close on only one axis is never treated
		// as urgent as being close on both.
		//
		// Role: NOT symmetric — the TRAILING rider yields (n.ws_fwd_gap > 0,
		// i.e. this neighbor is ahead of me, so I'm catching up from behind),
		// same as a real pack: the rider out front has no reason to react to
		// someone approaching from behind. Only within avoidance_side_by_side_m
		// (neither rider unambiguously ahead/behind) do BOTH yield. Every
		// overlapping neighbor still gets marked in dbg_neighbors regardless
		// of role, so the debug overlay can show "conflict, not my job" vs.
		// "conflict, I'm yielding" distinctly.
		const Neighbor* conflict = nullptr;
		float conflict_severity  = 0.f;
		for (size_t i = 0; i < all_nearby.size(); ++i) {
			const Neighbor& n = all_nearby[i];
			const float long_abs = glm::abs(n.ws_fwd_gap);
			const float lat_abs  = glm::abs(n.ws_right_gap);
			if (long_abs >= trigger_long_m) continue;
			if (lat_abs  >= trigger_lat_m)  continue;
			const float severity = axis_severity(long_abs, hard_long_m, trigger_long_m)
			                      * axis_severity(lat_abs,  hard_lat_m,  trigger_lat_m);
			if (severity <= 0.f) continue;

			const bool near_side_by_side = long_abs < p.avoidance_side_by_side_m;
			const bool im_trailing       = n.ws_fwd_gap > 0.f;  // they're ahead of me -> I'm catching up from behind
			const bool responsible       = near_side_by_side || im_trailing;

			BikeAIDebugNeighbor& dn = dbg_neighbors[i];
			dn.is_avoidance_conflict    = true;
			dn.is_avoidance_responsible = responsible;
			dn.avoidance_severity       = severity;

			if (!responsible) continue;  // clear gap and they're out front — leave it to them
			if (severity > conflict_severity) { conflict_severity = severity; conflict = &n; }
		}
		if (conflict) {
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
	// "Move to front" / "Stay at front" debug behaviors (BikeObject::
	// ai_behavior_state, toggled from BikeDebugger's per-rider panel).
	// MovingToFront auto-cancels back to Default the instant this rider
	// literally BECOMES the sole rank-0 leader of its own group
	// (pos_in_group_norm reaches 0 — a one-shot goal, so the strict rank
	// check is correct here) — checked every tick, not just on the debug
	// button click, so it can't stay stuck active after the goal is already
	// reached. StayingAtFront does NOT auto-cancel (that's the whole point of
	// "stay"); instead it only sprints until near_group_front(my_bike) (see
	// section 2 — actual course_dist_m proximity to the leader, NOT rank),
	// then falls through to the front_abreast speed match computed in
	// section 2 (cohesion's "behind" term is suppressed for it there — see
	// suppress_front_draft — so it matches the pace of whoever else is
	// abreast instead of drafting into them). Using suppress_front_draft
	// (distance-based) instead of the rank check here specifically fixes a
	// permanent back-and-forth: with the rank check, a trailing rider in a
	// group bigger than 2-3 could never register as "at front" even once
	// genuinely alongside the leader, so it kept sprinting at full power,
	// overtook, and then the rider it just passed started sprinting in turn.
	if (my_bike->ai_behavior_state == BikeAIBehaviorState::MovingToFront
	    && my_bike->pos_in_group_norm <= 0.0001f)
		my_bike->ai_behavior_state = BikeAIBehaviorState::Default;

	// ai_override_speed_enabled is a hard debug override and still wins over
	// either of these — same priority as it already had over cohesion below.
	const bool sprinting_to_front = !my_bike->ai_override_speed_enabled
	    && ((my_bike->ai_behavior_state == BikeAIBehaviorState::MovingToFront)
	        || (my_bike->ai_behavior_state == BikeAIBehaviorState::StayingAtFront && !suppress_front_draft));

	// Runs the shared speed-PID formula against whichever (integral,
	// prev_error) state pair is passed in — factored out so the front-abreast
	// case below can use its own dedicated state (front_abreast_speed_integral/
	// _prev_error) instead of speed_integral/speed_prev_error, without
	// duplicating the PID math itself.
	auto run_speed_pid = [&](float target, float& integral, float& prev_error) {
		dbg_target_speed = target;
		const float speed_error = target - speed;
		integral   = glm::clamp(integral + speed_error * dt, -50.f, 50.f);
		const float speed_deriv = (dt > 1e-4f) ? (speed_error - prev_error) / dt : 0.f;
		prev_error = speed_error;
		const float pid_out = p.speed_kp * speed_error + p.speed_ki * integral + p.speed_kd * speed_deriv;
		return glm::clamp(p.base_power_w + pid_out, p.min_power_w, p.max_power_w);
	};

	const bool use_front_abreast_target = !my_bike->ai_override_speed_enabled && front_abreast_have_speed_target;

	bool  have_speed_target = false;
	float target_speed      = 0.f;
	if (my_bike->ai_override_speed_enabled) {
		have_speed_target = true;
		target_speed      = my_bike->ai_override_target_speed_ms;
	} else if (cohesion_have_speed_target) {
		have_speed_target = true;
		target_speed      = cohesion_target_speed;
	}

	float power;
	if (sprinting_to_front) {
		// Full-effort sprint: bypass the speed PID entirely — there's no fixed
		// speed to converge on, the goal is "get to the front", not "hold a
		// speed". Corner braking (section 4) and avoidance still apply normally
		// on top of this; only the cruise/draft power choice is replaced.
		dbg_target_speed = 0.f;
		speed_integral                 = 0.f;
		speed_prev_error               = 0.f;
		front_abreast_speed_integral   = 0.f;
		front_abreast_speed_prev_error = 0.f;
		power = p.move_to_front_power_w;
	} else if (use_front_abreast_target) {
		speed_integral   = 0.f;
		speed_prev_error = 0.f;
		if (front_abreast_target_speed <= 0.f) {
			// The gap/speed correction wants me slower than stopped -- I'm far
			// enough ahead that no feasible pedal power actually corrects it in
			// one tick. Coast for real (power=0, no min_power_w floor) rather
			// than bottoming out at min_power_w, which would still be gently
			// accelerating away from the groupmate I'm supposed to be waiting
			// for. Resets the dedicated PID state so there's no stale integral
			// to snap back from once the correction shrinks enough to resume it.
			dbg_target_speed                = 0.f;
			front_abreast_speed_integral    = 0.f;
			front_abreast_speed_prev_error  = 0.f;
			power = 0.f;
		} else {
			// Dedicated PID state (see run_speed_pid above) -- keeps this
			// converging smoothly onto the abreast target speed without
			// carrying over integral windup from the shared speed_integral
			// (used by ai_override/cohesion below), and vice versa.
			power = run_speed_pid(front_abreast_target_speed, front_abreast_speed_integral, front_abreast_speed_prev_error);
		}
	} else if (have_speed_target) {
		front_abreast_speed_integral   = 0.f;
		front_abreast_speed_prev_error = 0.f;
		power = run_speed_pid(target_speed, speed_integral, speed_prev_error);
	} else {
		dbg_target_speed = 0.f;
		speed_integral                 = 0.f;
		speed_prev_error               = 0.f;
		front_abreast_speed_integral   = 0.f;
		front_abreast_speed_prev_error = 0.f;
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
