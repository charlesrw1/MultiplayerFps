// BikeApplication_Pack.cpp
// Pack dynamics: grouping, drafting, wheel-picking, paceline FSM,
// clear-air resolver, predictive avoidance, and gap regulation.
// All functions are methods of BikeGameApplication (declared in BikeHeaders.h).

#include "BikeHeaders.h"

#include "Render/Texture.h"
#include "Render/Model.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/DecalComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Game/Entities/CharacterController.h"
#include "Input/InputSystem.h"
#include "GameEnginePublic.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "imgui.h"
#include <SDL2/SDL_gamecontroller.h>
#include <glm/gtc/matrix_transform.hpp>
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "UI/Gui.h"
#include <algorithm>
#include <fstream>
#include <random>

// ============================================================
// Group context
// ============================================================

static constexpr float GROUP_GAP_M = 8.f;  // gap > this splits riders into separate groups
static bool show_bike_groups = true;
void BikeGameApplication::update_groups()
{
	const int n = (int)riders_sorted.size();
	if (n == 0) return;

	// Scan riders front-to-back; start a new group when the gap to the next rider exceeds GROUP_GAP_M.
	// riders_sorted[0] = race leader (highest course_dist_m).
	int group_id    = 0;
	int group_start = 0;

	// Temporary arrays to avoid two passes
	std::vector<int> gids(n);
	std::vector<int> gsizes;

	gids[0] = 0;
	for (int i = 1; i < n; ++i) {
		const float gap = riders_sorted[i - 1]->course_dist_m - riders_sorted[i]->course_dist_m;
		if (gap > GROUP_GAP_M) {
			gsizes.push_back(i - group_start);
			++group_id;
			group_start = i;
		}
		gids[i] = group_id;
	}
	gsizes.push_back(n - group_start);
	const int num_groups = group_id + 1;

	if (show_bike_groups) {
		for (int i = 0; i < num_groups; i++) {
			Bounds b;
			bool found_bounds = false;
			for (int r = 0; r < n; r++) {
				if (gids.at(r) == i) {
					Bounds rider_bounds = Bounds(riders_sorted.at(r)->prev_front_wheel_pos, riders_sorted.at(r)->prev_rear_wheel_pos);
					if (!found_bounds)
						b = Bounds(rider_bounds);
					else
						b = bounds_union(b, rider_bounds);
					found_bounds = true;
				}
			}
			if (found_bounds)
				Debug::add_box(b.get_center(), (b.bmax - b.bmin), COLOR_PINK, -1);
		}
	}

	int pos_in_group = 0;
	for (int i = 0; i < n; ++i) {
		if (i > 0 && gids[i] != gids[i - 1])
			pos_in_group = 0;

		BikeObject* b = riders_sorted[i];
		b->group_id          = gids[i];
		const int gsz        = gsizes[gids[i]];
		b->pos_in_group_norm = (gsz > 1) ? (float)pos_in_group / (float)(gsz - 1) : 0.f;
		b->group_rank_norm   = (num_groups > 1) ? (float)gids[i] / (float)(num_groups - 1) : 0.f;
		b->group_size_norm   = (float)gsz / (float)n;
		++pos_in_group;
	}
}

// ============================================================
// Drafting constants
// ============================================================
static constexpr float DRAFT_LONG_MIN    =  0.3f;  // min longitudinal gap to benefit (m)
static constexpr float DRAFT_LONG_MAX    =  8.0f;  // no benefit beyond this (m)
static constexpr float DRAFT_LAT_MAX     =  1.2f;  // no benefit beyond this lateral offset (m)
static constexpr float DRAFT_MAX_BENEFIT =  0.35f; // max CdA reduction (35%)
static constexpr float DRAFT_FLOOR       =  0.55f; // minimum draft_factor (hard floor)
static constexpr int   DRAFT_STACK_CHECK =  5;     // how many riders ahead to check for stacking

void BikeGameApplication::update_drafting()
{
	const int n = (int)riders_sorted.size();

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];

		float total_benefit  = 0.f;
		float stack_weight   = 1.f;

		// riders_sorted is front-to-back (index 0 = leader), so riders ahead are at lower indices
		for (int j = i - 1; j >= 0 && (i - j) <= DRAFT_STACK_CHECK; --j) {
			const BikeObject* ahead = riders_sorted[j];

			const float long_gap = ahead->course_dist_m - me->course_dist_m;
			const float lat_gap  = glm::abs(ahead->lateral_pos - me->lateral_pos);

			if (long_gap < DRAFT_LONG_MIN || long_gap > DRAFT_LONG_MAX) continue;
			if (lat_gap  >= DRAFT_LAT_MAX)                               continue;

			// Benefit falls off linearly with both gaps
			const float long_factor = 1.f - (long_gap - DRAFT_LONG_MIN) / (DRAFT_LONG_MAX - DRAFT_LONG_MIN);
			const float lat_factor  = 1.f - lat_gap / DRAFT_LAT_MAX;
			const float benefit     = long_factor * lat_factor * DRAFT_MAX_BENEFIT;

			total_benefit += benefit * stack_weight;
			stack_weight  *= 0.5f;  // each additional rider in the stack contributes half as much
		}

		me->draft_factor = glm::max(DRAFT_FLOOR, 1.f - total_benefit);
	}
}

// ============================================================
// Pack / gap constants
// ============================================================

// Longitudinal power yield when side-by-side: the slightly-behind rider sheds watts
static float SIDE_BY_SIDE_LONG_M   = 2.5f;   // longitudinal range to count as abreast (m)
static float SIDE_BY_SIDE_LAT_M    = 1.0f;   // lateral range to count as abreast (m)
static float SIDE_BY_SIDE_POWER_W  = 60.f;   // max W shed / gained

// Predictive inter-rider avoidance (soft steer push, truly side-by-side only)
// Only fires when riders are nearly abreast (long_gap < AVOID_LONG_MAX).
// Weighted by (1 - long_gap / AVOID_LONG_MAX) so the push fades longitudinally.
static float AVOID_LONG_MAX     = 3.5f;   // m — longitudinal range; beyond this = single file, no push
static float AVOID_BIKE_HALF_W  = 0.38f;  // m — half shoulder width
static float AVOID_CLEARANCE    = 0.25f;  // m — additional margin beyond bike width
static float AVOID_PREDICT_T1   = 0.5f;   // s — first prediction horizon
static float AVOID_PREDICT_T2   = 1.0f;   // s — second prediction horizon
static float AVOID_STEER_KP     = 0.8f;   // additive steer per m of predicted overlap (soft nudge)

// Priority yield (hard enforcement — lower-priority yielder only)
// Lower index in riders_sorted = further ahead = higher priority.
// The yielder (higher index) is forbidden from steering into the higher-priority rider's zone.
// Sign convention fix vs. old HARD_SEP: positive steer = road-LEFT, negative = road-RIGHT.
//   Other road-right → block road-right movement → block negative steer → hard_steer_min = 0
//   Other road-left  → block road-left movement  → block positive steer → hard_steer_max = 0
static float YIELD_LONG_RADIUS  = 3.5f;   // m — longitudinal range for yield clamp
static float YIELD_OUTER_LAT    = 1.3f;   // m — engage clamp inside this lateral gap
static float YIELD_INNER_LAT    = 0.05f;  // m — disengage when already overlapping (escape)
static float YIELD_SQUEEZE_M    = 0.35f;  // m — available road width below this triggers brake
static float YIELD_BRAKE_K      = 0.55f;  // brake fraction when fully squeezed

// ============================================================
// Wheel picking — choose the rider directly ahead I'm following.
// Score candidates in (group, ahead by [long_min, long_max], lateral overlap) and
// pick the highest. Sets BikeAI::wheel each frame; null = leader.
// See [[bike/bikeai#Wheel picking]].
// ============================================================
void BikeGameApplication::update_wheel_picking()
{
	const BikeAIParams& p = g_ai_params;
	const int n = (int)riders_sorted.size();

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];
		BikeAI* ai = dynamic_cast<BikeAI*>(me->input.get());
		if (!ai) continue;

		BikeObject* best       = nullptr;
		float       best_score = -FLT_MAX;
		const float long_norm  = glm::max(0.5f, p.wheel_long_max - p.wheel_long_min);

		for (int j = 0; j < n; ++j) {
			if (j == i) continue;
			BikeObject* other = riders_sorted[j];
			if (other->group_id != me->group_id) continue;

			// Skip riders in transient states — Peeling drifts sideways, DriftingBack
			// is a slow rider exiting the rotation. Following them just yanks the chain
			// off the line until they re-enter Following.
			if (BikeAI* oai = dynamic_cast<BikeAI*>(other->input.get())) {
				if (oai->paceline_state == PacelineState::Peeling ||
				    oai->paceline_state == PacelineState::DriftingBack) continue;
			}

			const float signed_long = other->course_dist_m - me->course_dist_m;
			if (signed_long < p.wheel_long_min || signed_long > p.wheel_long_max) continue;

			const float lat_gap = glm::abs(other->lateral_pos - (me->lateral_pos + ai->lat_offset));
			if (lat_gap > p.wheel_lat_max) continue;

			// Score components in [0,1]
			const float long_close = 1.f - glm::clamp(glm::abs(signed_long - p.wheel_long_gap)
			                                           / long_norm, 0.f, 1.f);
			const float lat_align  = 1.f - lat_gap / p.wheel_lat_max;
			const float draft_b    = 1.f - other->draft_factor;  // 0 = open air, ~0.45 = full draft

			float score = p.wheel_w_long  * long_close
			            + p.wheel_w_lat   * lat_align
			            + p.wheel_w_draft * draft_b;
			if (other == ai->wheel) score += p.wheel_stickiness;

			if (score > best_score) { best_score = score; best = other; }
		}

		if (best && best_score >= p.wheel_score_thresh) {
			ai->wheel = best;
		} else {
			ai->wheel = nullptr;
		}
		ai->dbg_has_wheel   = (ai->wheel != nullptr);
		ai->dbg_wheel_score = (best ? best_score : 0.f);
	}
}

// ============================================================
// Paceline tactical FSM — Following / Pulling / Peeling / DriftingBack.
//
// Wheel-picker is the "what wheel am I on" decision. This is "what am I doing
// strategically" — it modifies the wheel result (forces a peel-side lat_offset,
// scales target_power) and gates pull re-entry.
//
// Cascade-safe promotion: Pulling riders accelerate, so the gap to the next
// rider can exceed wheel_long_max within seconds. is_at_front() walks ALL
// riders ahead in the same group with no distance cutoff; only Peeling and
// DriftingBack are skipped (transient sideways/slow states). Without this,
// every following rider eventually self-promotes when its puller pulls away.
// See [[bike/bikeai#Tactical FSM]].
// ============================================================
void BikeGameApplication::update_paceline()
{
	const BikeAIParams& p = g_ai_params;
	const float dt = eng->get_dt();
	const int   n  = (int)riders_sorted.size();

	auto is_at_front = [&](int idx) -> bool {
		BikeObject* me = riders_sorted[idx];
		for (int j = idx - 1; j >= 0; --j) {
			BikeObject* other = riders_sorted[j];
			if (other->group_id != me->group_id) continue;
			BikeAI* oai = dynamic_cast<BikeAI*>(other->input.get());
			if (!oai) return false;  // player counts as a stable wheel
			if (oai->paceline_state == PacelineState::Peeling ||
			    oai->paceline_state == PacelineState::DriftingBack) continue;
			return false;
		}
		return true;
	};

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];
		BikeAI*     ai = dynamic_cast<BikeAI*>(me->input.get());
		if (!ai) continue;

		ai->paceline_timer_s += dt;
		if (ai->pull_cooldown_s > 0.f)
			ai->pull_cooldown_s = glm::max(0.f, ai->pull_cooldown_s - dt);

		const bool at_front = is_at_front(i);

		switch (ai->paceline_state) {
		case PacelineState::Following:
			// Becoming a leader by emergence triggers a pull (unless still cooling down).
			if (at_front && ai->pull_cooldown_s <= 0.f) {
				ai->paceline_state   = PacelineState::Pulling;
				ai->paceline_timer_s = 0.f;
			}
			break;

		case PacelineState::Pulling:
			// Pull until duration elapses, then peel off. If a stable rider appears
			// ahead (someone moved up), drop back to Following without peeling.
			if (!at_front) {
				ai->paceline_state   = PacelineState::Following;
				ai->paceline_timer_s = 0.f;
			} else if (ai->paceline_timer_s >= p.pull_duration_s) {
				ai->paceline_state   = PacelineState::Peeling;
				ai->paceline_timer_s = 0.f;
				// Peel AWAY from road centre (right if exactly centred). Matches
				// the BikeAIPaceline.PeelDir tests.
				ai->peel_side_sign = (me->lateral_pos < 0.f) ? -1.f : +1.f;
			}
			break;

		case PacelineState::Peeling:
			if (ai->paceline_timer_s >= p.peel_duration_s) {
				ai->paceline_state   = PacelineState::DriftingBack;
				ai->paceline_timer_s = 0.f;
			}
			break;

		case PacelineState::DriftingBack:
			// Done drifting once we've found a stable wheel, or after a hard cap.
			if (ai->wheel || ai->paceline_timer_s >= p.drift_duration_s) {
				ai->paceline_state   = PacelineState::Following;
				ai->paceline_timer_s = 0.f;
				ai->pull_cooldown_s  = p.pull_cooldown_s;
			}
			break;
		}
	}
}

// ============================================================
// Clear-air resolver — pick lat_offset from open slots around the wheel.
// Default ideal slot = 0 (best draft) + personality bias. If neighbors block
// it within [clear_air_long_window × clear_air_lat_window], shift to the
// candidate offset with the most open air. Smoothed over clear_air_damp_tau.
// Peeling state forces lat_offset toward ±peel_offset_m, bypassing the search.
// See [[bike/bikeai#Lateral offset rule]].
// ============================================================
void BikeGameApplication::update_clear_air()
{
	const BikeAIParams& p = g_ai_params;
	const float dt = eng->get_dt();
	const int   n  = (int)riders_sorted.size();
	// Damping coefficient — frame-rate independent low-pass.
	const float lerp = (p.clear_air_damp_tau > 1e-3f)
	                   ? (1.f - glm::exp(-dt / p.clear_air_damp_tau))
	                   : 1.f;

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];
		BikeAI*     ai = dynamic_cast<BikeAI*>(me->input.get());
		if (!ai) continue;

		// Peeling: forced offset, no search.
		if (ai->paceline_state == PacelineState::Peeling) {
			const float target = ai->peel_side_sign * p.peel_offset_m;
			ai->dbg_lat_offset_target = target;
			ai->lat_offset += (target - ai->lat_offset) * lerp;
			continue;
		}

		// Leader: lat_offset is unused (racing line is the target). Decay toward 0
		// so re-acquiring a wheel starts from the bias-neutral position.
		if (!ai->wheel) {
			ai->dbg_lat_offset_target = ai->lat_offset_bias;
			ai->lat_offset += (ai->lat_offset_bias - ai->lat_offset) * lerp;
			continue;
		}

		// Follower: search candidate offsets in the wheel's road frame.
		const BikeWaypoint wheel_wp    = course.sample(ai->wheel->course_dist_m);
		const glm::vec3    wheel_pos   = ai->wheel->get_ws_position();
		const glm::vec3    wheel_fwd   = ai->wheel->bike_direction;
		const glm::vec3    wheel_right = wheel_wp.right;
		const float        road_hw     = course.get_road_half_width(ai->wheel->course_segment);
		const float        safe_lat    = road_hw - p.edge_safety_m;
		const float        bias        = ai->lat_offset_bias;

		const int   half_steps  = (int)glm::ceil(p.clear_air_max_offset / glm::max(p.clear_air_step, 0.05f));
		const float lat_inv     = 1.f / glm::max(p.clear_air_lat_window,  1e-3f);
		const float long_inv    = 1.f / glm::max(p.clear_air_long_window, 1e-3f);

		float best_score  = FLT_MAX;
		float best_offset = bias;

		for (int s = -half_steps; s <= half_steps; ++s) {
			const float cand = bias + (float)s * p.clear_air_step;
			const float cand_world_lat = ai->wheel->lateral_pos + cand;
			if (glm::abs(cand_world_lat) > safe_lat) continue;

			const glm::vec3 cand_pos = wheel_pos
			                         - wheel_fwd   * ai->long_gap
			                         + wheel_right * cand;

			float occ = 0.f;
			for (int j = 0; j < n; ++j) {
				if (j == i) continue;
				BikeObject* other = riders_sorted[j];
				if (other == ai->wheel) continue;
				if (other->group_id != me->group_id) continue;

				const glm::vec3 d    = other->get_ws_position() - cand_pos;
				const float dl   = glm::dot(d, wheel_fwd);
				const float dlat = glm::dot(d, wheel_right);
				if (glm::abs(dl)   > p.clear_air_long_window) continue;
				if (glm::abs(dlat) > p.clear_air_lat_window)  continue;
				const float lat_close  = 1.f - glm::abs(dlat) * lat_inv;
				const float long_close = 1.f - glm::abs(dl)   * long_inv;
				occ += lat_close * long_close;
			}
			const float score = occ + p.clear_air_center_bias * glm::abs(cand - bias);
			if (score < best_score) { best_score = score; best_offset = cand; }
		}

		ai->dbg_lat_offset_target = best_offset;
		ai->lat_offset += (best_offset - ai->lat_offset) * lerp;

		// Final road clamp on the smoothed value (the resolver already filtered
		// out off-road candidates, but a small overshoot can still slip through
		// because it tracks the wheel's prior frame).
		const float lat_world = ai->wheel->lateral_pos + ai->lat_offset;
		if (glm::abs(lat_world) > safe_lat)
			ai->lat_offset = glm::sign(lat_world) * safe_lat - ai->wheel->lateral_pos;
	}
}

// ============================================================
// Predictive lateral avoidance + priority-yield clamp + side-by-side power yield.
// Reflex layer: catches surges, brake events, line changes the clear-air resolver
// (which acts on a longer 1.5s timescale) cannot react to in time.
// See [[bike/bikeai#Rider-rider avoidance]].
// ============================================================
void BikeGameApplication::update_avoidance()
{
	const int   n  = (int)riders_sorted.size();
	const float dt = eng->get_dt();

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];

		// Longitudinal power yield: when side-by-side, the slightly-behind rider sheds watts.
		float long_sep_power = 0.f;
		for (int j = 0; j < n; ++j) {
			if (j == i) continue;
			const BikeObject* other = riders_sorted[j];
			const float signed_gap = other->course_dist_m - me->course_dist_m;
			const float long_gap   = glm::abs(signed_gap);
			const float lat_gap    = glm::abs(me->lateral_pos - other->lateral_pos);
			if (long_gap >= SIDE_BY_SIDE_LONG_M || lat_gap >= SIDE_BY_SIDE_LAT_M) continue;
			const float long_weight = 1.f - long_gap / SIDE_BY_SIDE_LONG_M;
			long_sep_power += (signed_gap > 0.f ? 1.f : -1.f) * long_weight * SIDE_BY_SIDE_POWER_W;
		}
		me->boid_long_sep_power = glm::clamp(long_sep_power, -SIDE_BY_SIDE_POWER_W, SIDE_BY_SIDE_POWER_W);

		// ---- Predictive lateral separation (soft push, side-by-side only) ----
		// Only applies when riders are nearly abreast (long_gap < AVOID_LONG_MAX).
		// Weighted by (1 - long_gap/AVOID_LONG_MAX) so single-file trains get zero push.
		// BikeAI reads avoidance_sep_steer and adds it after edge avoidance.
		{
			BikeAI* me_ai = dynamic_cast<BikeAI*>(me->input.get());
			float avoid_accum = 0.f;
			const float full_sep = (AVOID_BIKE_HALF_W + AVOID_CLEARANCE) * 2.f;

			for (int j = 0; j < n; ++j) {
				if (j == i) continue;
				const BikeObject* other = riders_sorted[j];
				// Skip my wheel: drafting it intentionally; clear-air resolver handles offset.
				if (me_ai && other == me_ai->wheel) continue;
				const float long_gap = glm::abs(other->course_dist_m - me->course_dist_m);
				if (long_gap >= AVOID_LONG_MAX) continue;
				const float long_weight = 1.f - long_gap / AVOID_LONG_MAX;

				float worst_overlap = 0.f;
				const float ts[3] = { 0.f, AVOID_PREDICT_T1, AVOID_PREDICT_T2 };
				for (float t : ts) {
					const float me_lat  = me->lateral_pos    + me->lateral_vel    * t;
					const float o_lat   = other->lateral_pos + other->lateral_vel * t;
					const float overlap = glm::max(0.f, full_sep - glm::abs(me_lat - o_lat));
					worst_overlap = glm::max(worst_overlap, overlap);
				}

				if (worst_overlap > 0.f) {
					// dir_away: steer sign that moves me away from other.
					// I'm road-right → steer road-right (negative) → dir_away = -1
					// I'm road-left  → steer road-left  (positive) → dir_away = +1
					const float dir_away = (me->lateral_pos >= other->lateral_pos) ? -1.f : 1.f;
					avoid_accum += dir_away * long_weight * worst_overlap * AVOID_STEER_KP;
				}
			}
			float avoid_out = glm::clamp(avoid_accum, -1.f, 1.f);
			// Don't let avoidance push a rider further into the edge danger zone.
			// If the rider is already in the safety margin, suppress the component
			// that would move them closer to their near edge.
			{
				const float road_hw   = course.get_road_half_width(me->course_segment);
				const float safe_edge = road_hw - g_ai_params.edge_safety_m;
				if (glm::abs(me->lateral_pos) > safe_edge) {
					// near_edge_dir: +1 if near road-right edge, -1 if near road-left
					const float near_edge_dir = glm::sign(me->lateral_pos);
					// steer that moves toward the near edge is negative (road-right = steer negative)
					// i.e., steer_toward_edge = -near_edge_dir
					// suppress if avoid_out has same sign as steer_toward_edge direction
					if (glm::sign(avoid_out) == -near_edge_dir)
						avoid_out = 0.f;
				}
			}
			me->avoidance_sep_steer = avoid_out;
		}

		// ---- Priority yield: hard steer clamp + brake escape (BikeAI riders only) ----
		// Lower index in riders_sorted = further ahead in race = higher priority.
		// The yielder (higher index i) must not steer into a higher-priority rider's exclusion zone.
		// Sign: positive steer = road-LEFT (decreases lateral_pos).
		//   Other road-right of me → block road-right movement → block negative steer → min = 0
		//   Other road-left  of me → block road-left  movement → block positive steer → max = 0
		me->avoidance_brake = 0.f;
		BikeAI* ai_rider = dynamic_cast<BikeAI*>(me->input.get());
		if (ai_rider) {
			ai_rider->hard_steer_min = -1.f;
			ai_rider->hard_steer_max =  1.f;

			for (int j = 0; j < i; ++j) {  // only higher-priority riders
				const BikeObject* other = riders_sorted[j];
				// Skip my wheel: I'm intentionally drafting it. The clamp would otherwise
				// prevent me from converging onto the wheel's lateral track.
				if (other == ai_rider->wheel) continue;
				const float long_gap = glm::abs(other->course_dist_m - me->course_dist_m);
				if (long_gap >= YIELD_LONG_RADIUS) continue;
				const float lat_diff = other->lateral_pos - me->lateral_pos;  // +ve: other road-right
				const float h_lat    = glm::abs(lat_diff);
				if (h_lat >= YIELD_OUTER_LAT) continue;
				if (h_lat <  YIELD_INNER_LAT) continue;  // escape zone: already overlapping

				// Only clamp if I'm actively closing toward the other rider.
				// closing_vel > 0 when my lateral_vel is moving toward them.
				const float closing_vel = glm::sign(lat_diff) * me->lateral_vel;
				if (closing_vel <= 0.f) continue;  // already moving away — no clamp needed

				if (lat_diff > 0.f)
					ai_rider->hard_steer_min = 0.f;  // block road-right (negative steer)
				else
					ai_rider->hard_steer_max = 0.f;  // block road-left (positive steer)
			}

			// Squeeze detection: if clamped in one direction and road edge is close on the other.
			// Brake to create longitudinal gap when lateral escape is unavailable.
			const float road_hw = course.get_road_half_width(me->course_segment);
			const float cur_lat = me->lateral_pos;
			if (ai_rider->hard_steer_min == 0.f && ai_rider->hard_steer_max == 0.f) {
				// Boxed in on both sides
				me->avoidance_brake = YIELD_BRAKE_K;
			} else if (ai_rider->hard_steer_min == 0.f) {
				// Can't go road-right — check if road-left space is tight
				const float left_space = cur_lat + road_hw;
				if (left_space < YIELD_SQUEEZE_M)
					me->avoidance_brake = (1.f - left_space / YIELD_SQUEEZE_M) * YIELD_BRAKE_K;
			} else if (ai_rider->hard_steer_max == 0.f) {
				// Can't go road-left — check if road-right space is tight
				const float right_space = road_hw - cur_lat;
				if (right_space < YIELD_SQUEEZE_M)
					me->avoidance_brake = (1.f - right_space / YIELD_SQUEEZE_M) * YIELD_BRAKE_K;
			}
		}

		// Save lateral position and compute velocity for observation
		me->lateral_vel      = (dt > 1e-6f) ? (me->lateral_pos - me->prev_lateral_pos) / dt : 0.f;
		me->prev_lateral_pos = me->lateral_pos;
	}
}


// ============================================================
// Gap regulation — match the explicit wheel rider's power, with P-control on
// gap error. wheel == null  →  leader, holds tactical free power.
// See [[bike/bikeai#Power]].
// ============================================================

float GAP_POWER_K         = 50.f;   // W correction per metre of gap error
float GAP_POWER_MAX_DELTA = 250.f;  // max ±W applied on top of wheel rider's power
float GAP_FREE_POWER_W    = 250.f;  // power when leader (no wheel)

void BikeGameApplication::update_gap_regulation()
{
	const BikeAIParams& p = g_ai_params;
	for (BikeObject* me : all_riders) {
		BikeAI* ai = dynamic_cast<BikeAI*>(me->input.get());
		if (!ai) continue;

		// Base target: match wheel power + P on gap, or free power if leader.
		float base = GAP_FREE_POWER_W;
		if (ai->wheel) {
			const glm::vec3 to_wheel = ai->wheel->get_ws_position() - me->get_ws_position();
			const float gap_m      = glm::dot(to_wheel, me->bike_direction);
			const float gap_err    = gap_m - ai->long_gap;  // +ve = too far back
			const float correction = glm::clamp(gap_err * GAP_POWER_K,
			                                    -GAP_POWER_MAX_DELTA, GAP_POWER_MAX_DELTA);
			base = ai->wheel->stamina.actual_power + correction;
		}

		// Tactical FSM modifies the base.
		switch (ai->paceline_state) {
		case PacelineState::Pulling:      base *= p.pull_power_frac;          break;
		case PacelineState::Peeling:      base += p.peel_power_delta_w;       break;
		case PacelineState::DriftingBack: base *= p.drift_power_frac;         break;
		case PacelineState::Following:    /* unchanged */                     break;
		}

		ai->target_power_watts = glm::clamp(base, 50.f, 1000.f);
	}
}

// ============================================================
// Externs from BikeApplication.cpp — shared debug / follow state
// ============================================================
extern BikeGameApplication* g_bike_app;
extern bool  g_follow_rider;
extern int   g_follow_idx;
extern float g_follow_dist;
extern float g_follow_height;
extern float g_follow_pitch;

// Snapshot helpers — defined in BikeApplication.cpp
extern void snapshot_record();
extern void snapshot_restore();
extern bool snapshot_has_data();  // true when s_rider_snapshots is non-empty
extern int  snapshot_count();     // number of saved riders

// ============================================================
// Debug menu: Boid visualisation (placed here so pack-static vars are in scope)
// ============================================================

static void bike_boid_debug()
{
	if (!g_bike_app) return;
	const auto& all    = g_bike_app->all_riders;
	const auto& sorted = g_bike_app->riders_sorted;
	if (all.empty()) return;

	// ---- Snapshot ----
	ImGui::SeparatorText("Segment Replay");
	if (ImGui::Button("Record positions"))
		snapshot_record();
	ImGui::SameLine();
	const bool has_snap = snapshot_has_data();
	if (!has_snap) ImGui::BeginDisabled();
	if (ImGui::Button("Teleport to recorded"))
		snapshot_restore();
	if (!has_snap) ImGui::EndDisabled();
	if (has_snap)
		ImGui::SameLine(), ImGui::TextDisabled("(%d riders saved)", snapshot_count());

	// ---- Rider picker ----
	static int  selected_idx  = 0;          // index into all_riders
	static bool draw_boids    = true;

	ImGui::Checkbox("Draw boid debug", &draw_boids);

	// Build label list: "Player" for BikePlayer, "AI #N" for BikeAI
	ImGui::Text("Select rider:");
	ImGui::SameLine();
	ImGui::InputInt("##label", &selected_idx,1);
	selected_idx = glm::clamp(selected_idx, 0, (int)all.size() - 1);
	BikeObject* bo = all[selected_idx];

	// ---- Camera follow ----
	g_follow_idx = selected_idx;
	ImGui::Checkbox("Follow selected rider (camera)", &g_follow_rider);
	if (g_follow_rider) {
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("dist",   &g_follow_dist,   0.05f, 0.5f, 10.f);
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("height", &g_follow_height, 0.05f, 0.5f,  5.f);
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("pitch°", &g_follow_pitch,  0.5f,  0.f, 80.f);
	}

	// ---- Text readout ----
	ImGui::Separator();
	ImGui::Text("long_sep_power:    %+.1f W", bo->boid_long_sep_power);

	// ---- Tuning sliders ----
	ImGui::SeparatorText("Side-by-side power yield");
	ImGui::DragFloat("long m",     &SIDE_BY_SIDE_LONG_M,  0.05f, 0.5f, 10.f);
	ImGui::DragFloat("lat m",      &SIDE_BY_SIDE_LAT_M,   0.05f, 0.1f,  3.f);
	ImGui::DragFloat("power max W",&SIDE_BY_SIDE_POWER_W, 5.f,   0.f, 200.f);

	ImGui::SeparatorText("AI Cornering");
	ImGui::DragFloat("lookahead_dist_base",   &g_ai_params.lookahead_dist_base,   0.1f,  0.1f, 20.f);
	ImGui::SameLine(); ImGui::TextDisabled("base lookahead m (+ speed*per_ms)");
	ImGui::DragFloat("lookahead_dist_per_ms", &g_ai_params.lookahead_dist_per_ms, 0.05f, 0.f,  3.f);
	ImGui::DragFloat("steer_k",               &g_ai_params.steer_k,               0.1f,  0.1f, 10.f);
	ImGui::SameLine(); ImGui::TextDisabled("lateral error gain");
	ImGui::DragFloat("corner_look_m",         &g_ai_params.corner_look_m,         1.f,   5.f, 120.f);
	ImGui::DragFloat("corner_speed_k",        &g_ai_params.corner_speed_k,        0.05f, 0.1f,  5.f);
	ImGui::SameLine(); ImGui::TextDisabled("v_max=sqrt(k*g*R)");
	ImGui::DragFloat("anticipation dist scale", &g_ai_params.anticipation_dist_scale, 0.1f, 1.f, 5.f);
	ImGui::SameLine(); ImGui::TextDisabled("far lookahead = near * this");
	ImGui::DragFloat("anticipation k",          &g_ai_params.anticipation_k,          0.05f, 0.f, 1.f);
	ImGui::SameLine(); ImGui::TextDisabled("0=disabled 1=all-far");

	ImGui::SeparatorText("AI Boundary Avoidance");
	ImGui::DragFloat("edge predict t", &g_ai_params.edge_predict_t, 0.05f, 0.1f, 3.f);
	ImGui::SameLine(); ImGui::TextDisabled("seconds ahead to project arc");
	ImGui::DragFloat("edge safety m",  &g_ai_params.edge_safety_m,  0.05f, 0.f,  2.f);
	ImGui::SameLine(); ImGui::TextDisabled("margin inside road edge");
	ImGui::DragFloat("edge steer k",   &g_ai_params.edge_steer_k,   0.05f, 0.f,  5.f);
	ImGui::SameLine(); ImGui::TextDisabled("P gain: steer per metre beyond safe zone");
	ImGui::DragFloat("edge vel damp",  &g_ai_params.edge_vel_damp,  0.05f, 0.f,  3.f);
	ImGui::SameLine(); ImGui::TextDisabled("D gain: damp correction when already returning");
	ImGui::DragFloat("edge off brake k",   &g_ai_params.edge_off_brake_k,   0.05f, 0.f, 3.f);
	ImGui::SameLine(); ImGui::TextDisabled("brake fraction per metre past road edge");
	ImGui::DragFloat("edge off brake max", &g_ai_params.edge_off_brake_max, 0.05f, 0.f, 1.f);
	ImGui::SameLine(); ImGui::TextDisabled("max brake fraction during off-track recovery");

	ImGui::SeparatorText("Soft Lateral Separation");
	ImGui::DragFloat("avoid long max m",  &AVOID_LONG_MAX,    0.1f,  0.5f, 10.f);
	ImGui::DragFloat("avoid half-w m",    &AVOID_BIKE_HALF_W, 0.01f, 0.1f,  1.f);
	ImGui::DragFloat("avoid clearance m", &AVOID_CLEARANCE,   0.01f, 0.f,   1.f);
	ImGui::DragFloat("avoid predict t1",  &AVOID_PREDICT_T1,  0.05f, 0.f,   3.f);
	ImGui::DragFloat("avoid predict t2",  &AVOID_PREDICT_T2,  0.05f, 0.f,   3.f);
	ImGui::DragFloat("avoid steer kp",    &AVOID_STEER_KP,    0.05f, 0.f,   5.f);

	ImGui::SeparatorText("Priority Yield (Hard Clamp)");
	ImGui::DragFloat("yield long radius m", &YIELD_LONG_RADIUS, 0.1f, 0.5f, 10.f);
	ImGui::DragFloat("yield outer lat m",   &YIELD_OUTER_LAT,   0.05f, 0.1f,  3.f);
	ImGui::DragFloat("yield inner lat m",   &YIELD_INNER_LAT,   0.01f, 0.f,   0.5f);
	ImGui::DragFloat("yield squeeze m",     &YIELD_SQUEEZE_M,   0.05f, 0.f,   2.f);
	ImGui::DragFloat("yield brake k",       &YIELD_BRAKE_K,     0.05f, 0.f,   1.f);

	if (!draw_boids) return;

	// ============================================================
	// World-space drawing
	// ============================================================
	const glm::vec3 my_pos   = bo->get_ws_position();
	const glm::vec3 up        = glm::vec3(0, 0.05f, 0);  // slight y offset so lines clear the ground
	const glm::vec3 right_ws  = glm::normalize(glm::cross(bo->bike_direction, glm::vec3(0, 1, 0)));

	// Highlight selected rider with a white sphere
	Debug::add_sphere(my_pos + glm::vec3(0, 1.2f, 0), 0.25f, COLOR_WHITE, -1.f);

	// ---- Draft factor bar — horizontal line above rider, length = (1-draft_factor)*4 ----
	{
		const float benefit_len = (1.f - bo->draft_factor) * 4.f;  // 0 = no draft, up to ~1.4m at full draft
		if (benefit_len > 0.01f) {
			const glm::vec3 bar_start = my_pos + glm::vec3(0, 2.f, 0) - right_ws * benefit_len * 0.5f;
			const glm::vec3 bar_end   = bar_start + right_ws * benefit_len;
			Debug::add_line(bar_start, bar_end, Color32(0xff, 0xff, 0x00, 0xff), -1.f);  // yellow = draft
		}
	}
}
ADD_TO_DEBUG_MENU(bike_boid_debug);
