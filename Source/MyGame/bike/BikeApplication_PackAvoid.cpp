// BikeApplication_PackAvoid.cpp
// Lateral positioning: clear-air slot resolver and predictive lateral
// avoidance + priority-yield clamp + side-by-side power yield.
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
#include "Input/Sdl2CompatGamepad.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "UI/Gui.h"
#include <algorithm>

// ============================================================
// Pack / gap constants (avoidance layer)
// ============================================================

// Longitudinal power yield when side-by-side: the slightly-behind rider sheds watts
extern float SIDE_BY_SIDE_LONG_M;
extern float SIDE_BY_SIDE_LAT_M;
extern float SIDE_BY_SIDE_POWER_W;

// Predictive inter-rider avoidance (soft steer push, truly side-by-side only)
extern float AVOID_LONG_MAX;
extern float AVOID_BIKE_HALF_W;
extern float AVOID_CLEARANCE;
extern float AVOID_PREDICT_T1;
extern float AVOID_PREDICT_T2;
extern float AVOID_STEER_KP;

// Priority yield (hard enforcement — lower-priority yielder only)
extern float YIELD_LONG_RADIUS;
extern float YIELD_DEADBAND_M;
extern float YIELD_OUTER_LAT;
extern float YIELD_INNER_LAT;
extern float YIELD_SQUEEZE_M;
extern float YIELD_BRAKE_K;

// ============================================================
// Clear-air resolver — pick lat_offset from open slots around the wheel.
// Default ideal slot = 0 (best draft) + personality bias. If neighbors block
// it within [clear_air_long_window × clear_air_lat_window], shift to the
// candidate offset with the most open air. Smoothed over clear_air_damp_tau.
//
// A rider who just finished pulling has wheel == nullptr (they were leading)
// and recovering_s > 0. They fall through to the leader branch below and just
// decay toward their bias — no forced peel steer. The overtaking motion you
// see from riders behind them is purely emergent: update_wheel_picking refuses
// to hand them out as a wheel while recovering, so a follower whose wheel just
// went stale re-targets a new (still-committed) wheel, and this same resolver
// then treats the recovering rider as a normal occupied slot to route around.
// See [[bike/bikeai#Lateral offset rule]].
// ============================================================
void BikeGameApplication::update_clear_air()
{
	ASSERT(eng != nullptr);
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
	ASSERT(eng != nullptr);
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
		// Priority is decided by an explicit course_dist_m comparison with a deadband,
		// NOT riders_sorted array position: two riders can be laterally beside each
		// other with a nearly-tied course_dist_m, and index order for a tied pair is
		// noise — using it as priority made both riders flicker which one yields,
		// frame to frame, i.e. the classic side-by-side dodge-dance. Within the
		// deadband neither yields; the soft separation push above still keeps them apart.
		// The yielder must not steer into a clearly-ahead rider's exclusion zone.
		// Sign: positive steer = road-LEFT (decreases lateral_pos).
		//   Other road-right of me → block road-right movement → block negative steer → min = 0
		//   Other road-left  of me → block road-left  movement → block positive steer → max = 0
		me->avoidance_brake = 0.f;
		BikeAI* ai_rider = dynamic_cast<BikeAI*>(me->input.get());
		if (ai_rider) {
			ai_rider->hard_steer_min = -1.f;
			ai_rider->hard_steer_max =  1.f;

			for (int j = 0; j < n; ++j) {
				if (j == i) continue;
				const BikeObject* other = riders_sorted[j];
				// Skip my wheel: I'm intentionally drafting it. The clamp would otherwise
				// prevent me from converging onto the wheel's lateral track.
				if (other == ai_rider->wheel) continue;
				const float signed_gap = other->course_dist_m - me->course_dist_m;  // +ve: other ahead
				if (signed_gap <= YIELD_DEADBAND_M) continue;  // not clearly ahead of me — no yield
				if (signed_gap >= YIELD_LONG_RADIUS) continue;
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
