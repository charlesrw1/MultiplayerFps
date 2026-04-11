#include "BikeHeaders.h"

#include "Render/Texture.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/RoadNetworkComponent.h"
#include "Game/Components/SpawnerComponenth.h"
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

// Shared wind state (defined in BikeWind.cpp)
extern glm::vec3 wind_direction;
extern float     wind_speed;
extern float     wind_gust_factor;
extern float     gust_speed_amp;

// Steering expo: >1 compresses small deflections for finer control near center.
// At expo=2: half-stick → 25% input. At expo=1: linear. At expo=3: half-stick → 12.5%.
static float steer_expo = 3.5f;

// Speed hold tuning
static float sh_power_up   = 0.012f;
static float sh_power_down = 0.025f;
static float sh_power_max  = 800.f;

// Freewheel sound fade
static float free_wheel_fade = 0.0002f;

// Debug pointers (set each frame in evaluate)
static BikeObject*          bo_for_debug = nullptr;
static BikePlayer*          bp_for_debug = nullptr;
static BikeGameApplication* g_bike_app   = nullptr;

// Forward declare — defined later alongside the boid debug menu
static void apply_debug_follow_camera();
static bool g_follow_rider = false;

// ============================================================
// Helpers
// ============================================================

static float apply_deadzone(float val, float dz) {
	if (glm::abs(val) < dz) return 0.f;
	return glm::sign(val) * (glm::abs(val) - dz) / (1.f - dz);
}

// ============================================================
// Crack type registry — add rows here for crack2, crack3, etc.
// ============================================================

struct CrackTypeConfig {
	const char* material_path;
	float strength;     // base impulse magnitude (speed-scaled at trigger time)
	float radius_mult;  // multiplier on the decal's natural XZ footprint for the trigger zone
	float cooldown_dist;  // metres to travel before same crack can retrigger (speed-invariant)
	int   count;        // populated by collect_crack_decals, read-only
};

static constexpr int NUM_CRACK_TYPES = 1;
extern CrackTypeConfig crack_types[NUM_CRACK_TYPES];  // defined near update_crack_triggers

// ============================================================
// BikeGameApplication
// ============================================================

void BikeGameApplication::collect_crack_decals()
{
	crack_instances.clear();
	auto components = GameplayStatic::find_components(&DecalComponent::StaticType);
	for (auto* c : components) {
		auto* dc = static_cast<DecalComponent*>(c);
		const std::string& path = dc->get_material_path();
		for (int ti = 0; ti < NUM_CRACK_TYPES; ++ti) {
			if (path == crack_types[ti].material_path) {
				// Use the decal's XZ world-space footprint as the natural trigger size.
				// Decals are unit-box projections, so XZ scale = real-world extent in metres.
				const glm::vec3 ws_scale = dc->get_owner()->get_ws_scale();
				const float footprint_r  = glm::max(ws_scale.x, ws_scale.z) * 0.5f;
				const float trigger_r    = footprint_r * crack_types[ti].radius_mult;
				crack_instances.push_back({ dc->get_owner()->get_ws_position(), trigger_r, ti });
				crack_types[ti].count++;
				break;
			}
		}
	}
	sys_print(Debug, "BikeApp: collected %d crack decal(s)\n", (int)crack_instances.size());
}

void BikeGameApplication::start()
{
	GameplayStatic::change_level("maps/bike_test_map.tmap");

	// Prefer road-network routing if a RoadNetworkComponent exists in the level.
	// Route-hint waypoints ("bike_waypoint" spawners) pick the path through the graph.
	// Falls back to the old pure-spline build if no road network is present.
	{
		auto road_comps = GameplayStatic::find_components(&RoadNetworkComponent::StaticType);
		auto spawners   = GameplayStatic::find_spawners_in_class("bike_waypoint");

		if (!road_comps.empty() && !spawners.empty()) {
			auto* rnc = static_cast<RoadNetworkComponent*>(road_comps[0]);

			// Sort spawners by name (integer order)
			std::sort(spawners.begin(), spawners.end(), [](SpawnerComponent* a, SpawnerComponent* b) {
				return std::atoi(a->get_owner()->get_editor_name().c_str())
				     < std::atoi(b->get_owner()->get_editor_name().c_str());
			});

			std::vector<glm::vec3> hints;
			hints.reserve(spawners.size());
			for (auto* s : spawners)
				hints.push_back(s->get_ws_position());

			sys_print(Info, "BikeApp: building course from road network (%d hints)\n", (int)hints.size());
			course.build_from_road_network(*rnc, hints, 0.5f, /*loop=*/true);
		} else {
			sys_print(Info, "BikeApp: no road network found, falling back to spawner spline\n");
			course.build_from_spawners();
		}
	}

	collect_crack_decals();

	// Spawn player at the course start
	const glm::vec3 start_pos = course.is_built
		? course.sample(0.f).position
		: glm::vec3(0.f);
	create_player(start_pos);

	// Spawn AI riders staggered 5 m apart behind the player
	static constexpr int   NUM_AI        = 8;
	static constexpr float AI_GAP_M      = 5.f;   // spacing along course (m)
	static constexpr float AI_LAT_SPREAD = 1.2f;  // lateral spread so they don't all overlap
	for (int i = 0; i < NUM_AI; ++i) {
		const float dist   = -(i + 1) * AI_GAP_M;  // behind player
		glm::vec3   pos    = start_pos;
		if (course.is_built) {
			const BikeWaypoint wp = course.sample(
				glm::mod(dist + course.total_length_m, course.total_length_m));
			// Spread laterally so separation forces have something to work against
			const float lat = ((i % 3) - 1) * AI_LAT_SPREAD * 0.5f;
			pos = wp.position + wp.right * lat;
		}
		create_ai(pos);
	}
}

void BikeGameApplication::update()
{
	GameplayStatic::reset_debug_text_height();

	g_bike_app = this;
	if (course.is_built) {
		update_course_positions();
		sort_riders();
		update_gaps();
		update_drafting();
		update_boids();
		if (paceline_active)
			update_paceline();
	}
	update_crack_triggers();
	apply_debug_follow_camera();
}

void BikeGameApplication::update_course_positions()
{
	for (auto* r : all_riders) {
		r->course_dist_m = course.project(
			r->get_ws_position(),
			&r->lateral_pos,
			&r->course_segment);
	}
}

void BikeGameApplication::sort_riders()
{
	riders_sorted = all_riders;
	std::sort(riders_sorted.begin(), riders_sorted.end(),
		[](const BikeObject* a, const BikeObject* b) {
			return a->course_dist_m > b->course_dist_m;
		});
	for (int i = 0; i < (int)riders_sorted.size(); ++i)
		riders_sorted[i]->race_position = i + 1;
}

// ============================================================
// Gap context
// ============================================================

// Rider-ahead selection: candidate must be longitudinally ahead by at least this
// fraction of the lateral gap, so side-by-side riders don't count as "ahead".
static float AHEAD_LAT_RATIO = 0.75f;

// Sticky locking: timer fills proportional to how much closer the candidate is.
// 1× urgency = AI_TARGET_SWITCH_DELTA_M metres improvement; must reach DELAY to commit.
static float AI_TARGET_SWITCH_DELAY   = 2.f;   // seconds at 1× urgency
static float AI_TARGET_SWITCH_DELTA_M = 5.f;   // metres improvement = 1× urgency

void BikeGameApplication::update_gaps()
{
	const int   n  = (int)riders_sorted.size();
	const float dt = eng->get_dt();

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];

		// ---- Find best candidate ahead ----
		// Scan all riders: must be within 25 m longitudinally, and far enough ahead
		// relative to their lateral offset (so side-by-side riders don't qualify).
		BikeObject* best_candidate = nullptr;
		float       best_gap       = 999.f;

		for (int j = 0; j < n; ++j) {
			if (j == i) continue;
			BikeObject* other = riders_sorted[j];

			const float long_gap = other->course_dist_m - me->course_dist_m;
			if (long_gap <= 0.f || long_gap > 25.f) continue;

			const float lat_gap = glm::abs(me->lateral_pos - other->lateral_pos);
			if (long_gap < lat_gap * AHEAD_LAT_RATIO) continue;  // too lateral

			if (long_gap < best_gap) {
				best_gap       = long_gap;
				best_candidate = other;
			}
		}

		// ---- Sticky locking with urgency-weighted timer ----
		// Release locked target if it fell behind or drifted out of range.
		if (me->rider_ahead) {
			const float locked_gap = me->rider_ahead->course_dist_m - me->course_dist_m;
			if (locked_gap <= 0.f || locked_gap > 25.f)
				me->rider_ahead = nullptr;
		}

		if (!me->rider_ahead) {
			// No current lock — commit to best candidate immediately.
			me->rider_ahead          = best_candidate;
			me->rider_ahead_switch_t = 0.f;
		} else if (best_candidate != me->rider_ahead) {
			// A different candidate appeared — accumulate urgency-weighted time.
			// Urgency scales with how much closer the candidate is; no improvement → no accrual.
			const float locked_gap    = me->rider_ahead->course_dist_m - me->course_dist_m;
			const float improvement   = locked_gap - best_gap;  // +ve = candidate is closer
			const float urgency       = glm::clamp(improvement / AI_TARGET_SWITCH_DELTA_M, 0.f, 2.f);
			me->rider_ahead_switch_t += dt * urgency;
			if (me->rider_ahead_switch_t >= AI_TARGET_SWITCH_DELAY) {
				me->rider_ahead          = best_candidate;
				me->rider_ahead_switch_t = 0.f;
			}
		} else {
			me->rider_ahead_switch_t = 0.f;
		}

		me->gap_to_ahead_m = me->rider_ahead
		                   ? (me->rider_ahead->course_dist_m - me->course_dist_m)
		                   : 999.f;

		// rider_behind: keep simple — immediate next in sorted order.
		if (i < n - 1) {
			BikeObject* behind   = riders_sorted[i + 1];
			me->rider_behind     = behind;
			me->gap_to_behind_m  = me->course_dist_m - behind->course_dist_m;
		} else {
			me->rider_behind    = nullptr;
			me->gap_to_behind_m = 999.f;
		}
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
// Boid constants
// ============================================================

// Separation — push away when side-by-side
static float BOID_SEP_LONG_RADIUS   = 2.5f;
static float BOID_SEP_MIN_LAT       = 1.0f;    // minimum lateral clearance (m)
static float BOID_SEP_KP            = 1.2f;    // steer per metre of overlap
static float BOID_SEP_KD            = 0.8f;    // steer per (m/s) of closing lateral velocity
static float BOID_SEP_POWER_MAX     = 60.f;    // max W shed by the yielding (slightly-behind) rider

// Heading alignment — match direction of nearby riders
static float BOID_ALIGN_STEER_RADIUS = 8.f;
static float BOID_ALIGN_STEER_KP     = 0.15f;
static float BOID_ALIGN_STEER_SMOOTH = 0.20f;

// Cohesion — PD controller targeting rider_ahead only (forms paceline chain)
// No output smoother — derivative term damps overshoot directly
static float BOID_COHESION_KP     = 0.14f;
static float BOID_COHESION_KD     = 0.40f;

// Player boid scale multipliers (applied on top of the shared raw values)
static float player_boid_sep_scale = 0.0f;
static float player_boid_coh_scale = 0.0f;
static float player_boid_aln_scale = 0.0f;

// Speed alignment — each rider matches rider-ahead speed (chain propagation, not pack average)
static float BOID_SPEED_RADIUS = 20.f;   // max gap to still react to rider ahead
static float BOID_SPEED_KP     = 15.f;   // W per (m/s) of speed difference
static float BOID_SPEED_MAX    = 60.f;
static float BOID_SPEED_SMOOTH = 0.04f;

// AI longitudinal gap-following PD (two-sided: backs off when too close, closes when too far)
static float AI_GAP_TARGET        =  4.f;   // desired metres behind rider ahead
static float AI_GAP_KP            =  8.f;   // W per metre of gap error
static float AI_GAP_KD            = 20.f;   // W per (m/s) of relative speed (dampens overshoot)
static float AI_GAP_MAX_PULL      = 60.f;   // max extra watts to close gap
static float AI_GAP_MAX_PUSH      = 40.f;   // max watts shed when too close

static float BOID_SEP_MULT_SIMPLE = 1.0;

// Hard steer cutoff — blocks any steer that closes the gap once a rider enters the
// inner exclusion zone.  Operates entirely on steer commands; no position snapping.
// Outer radius: beyond this, no hard limit (soft boid sep handles it).
// Inner radius: below this the bikes are already overlapping; allow any steer so they
//               can escape rather than getting locked with zero steer.
static float HARD_SEP_LONG_RADIUS = 3.0f;  // m — longitudinal range (side-by-side check)
static float HARD_SEP_OUTER_LAT   = 0.7f;  // m — engage hard clamp inside this lateral gap
static float HARD_SEP_INNER_LAT   = 0.05f; // m — disengage (escape) when already overlapping

void BikeGameApplication::update_boids()
{
	const int   n  = (int)riders_sorted.size();
	const float dt = eng->get_dt();

	// road-left vector helper: cross(fwd, world_up) is road-LEFT in this engine.
	// Separation pushes in road-left units which matches steer convention (positive steer = road-left).
	// Cohesion / heading alignment must negate lat_err (road-right positive) to get steer direction.

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];

		// Road-left vector for this rider (positive steer = this direction)
		const glm::vec3 road_left = glm::normalize(glm::cross(me->bike_direction, glm::vec3(0, 1, 0)));

		float             sep_accum        = 0.f;
		float             long_sep_power   = 0.f;
		glm::vec3         dir_sum          = glm::vec3(0.f);
		int               dir_cnt          = 0;

		// Cohesion target: use sticky rider_ahead, but skip any rider who is
		// currently drifting back (they're moving out of the line — don't follow them).
		const BikeObject* cohesion_target  = me->rider_ahead;
		{
			const BikeObject* candidate = cohesion_target;
			while (candidate) {
				const BikeAI* cai = dynamic_cast<const BikeAI*>(candidate->input.get());
				if (!cai || cai->paceline_state != PacelineState::DriftingBack)
					break;  // found a non-drifting rider (or player) → use them
				candidate = candidate->rider_ahead;  // walk further up the chain
			}
			cohesion_target = candidate;
		}

		for (int j = 0; j < n; ++j) {
			if (j == i) continue;
			const BikeObject* other = riders_sorted[j];

			const float signed_gap = other->course_dist_m - me->course_dist_m; // +ve = other is ahead
			const float long_gap   = glm::abs(signed_gap);
			const float lat_gap    = glm::abs(me->lateral_pos - other->lateral_pos);

			// --- Separation: PD controller on lateral gap ---
			// positive steer = road-LEFT (engine convention).
			// dir_away = steer sign that moves me AWAY from other.
			//   I'm to the RIGHT → need road-right → negative steer → dir_away = -1
			//   I'm to the LEFT  → need road-left  → positive steer → dir_away = +1
			if (long_gap < BOID_SEP_LONG_RADIUS && lat_gap < BOID_SEP_MIN_LAT) {
				const float long_weight = 1.f - (long_gap / BOID_SEP_LONG_RADIUS);
				const float dir_away    = (me->lateral_pos >= other->lateral_pos) ? -1.f : 1.f;

				// KP: proportional to overlap depth
				const float overlap = BOID_SEP_MIN_LAT - lat_gap;
				const float kp_term = overlap * BOID_SEP_KP;

				// KD: resist closing velocity.
				// closing > 0 when I'm moving TOWARD other.
				// dir_away=-1 (I'm right), lat_vel<0 (moving left=closing): lat_vel*dir_away = +ve ✓
				const float lat_vel = (dt > 1e-6f)
				    ? (me->lateral_pos - me->prev_lateral_pos) / dt : 0.f;
				const float closing = glm::max(0.f, lat_vel * dir_away);
				const float kd_term = closing * BOID_SEP_KD;

				sep_accum += dir_away * long_weight * (kp_term + kd_term);

				// Longitudinal power yield: behind rider (+signed_gap) pushes, ahead rider yields.
				const float yield = (signed_gap > 0.f ? 1.f : -1.f) * long_weight * BOID_SEP_POWER_MAX;
				long_sep_power += yield;
			}

			// --- Heading alignment: match direction of nearby pack ---
			if (long_gap < BOID_ALIGN_STEER_RADIUS) {
				dir_sum += other->bike_direction;
				++dir_cnt;
			}

			// Speed alignment is handled below using rider_ahead directly.

		}

		// Separation — applied raw, no smoother (smoother causes lag → oscillation)
		me->boid_separation_steer = glm::clamp(sep_accum, -1.f, 1.f);
		me->boid_long_sep_power   = glm::clamp(long_sep_power* BOID_SEP_MULT_SIMPLE, -BOID_SEP_POWER_MAX, BOID_SEP_POWER_MAX);

		// Heading alignment — fast smoother, lower gain
		float raw_align_steer = 0.f;
		if (dir_cnt > 0) {
			const glm::vec3 avg_dir = dir_sum / (float)dir_cnt;  // un-normalised: magnitude encodes agreement
			raw_align_steer = glm::clamp(glm::dot(avg_dir, road_left) * BOID_ALIGN_STEER_KP, -1.f, 1.f);
		}
		me->boid_align_steer = damp_dt_independent(raw_align_steer, me->boid_align_steer, BOID_ALIGN_STEER_SMOOTH, dt);

		// Cohesion — PD controller toward the closest rider found ahead within radius.
		// No pre-computed pointer: found fresh each frame in the scan above.
		// If no rider is within range (leader, or pack spread out) → no cohesion force.
		float cohesion_steer = 0.f;
		if (cohesion_target) {
			// Double echelon: blend the cohesion lateral target between rider-ahead's
			// position and this rider's preferred lane. lane_strength=0 → pure follow-wheel.
			float lat_target = cohesion_target->lateral_pos;
			const BikeAI* my_ai = dynamic_cast<const BikeAI*>(me->input.get());
			if (my_ai && my_ai->lane_strength > 0.f)
				lat_target = glm::mix(lat_target, my_ai->preferred_lateral, my_ai->lane_strength);

			const float lat_err = lat_target - me->lateral_pos;
			const float lat_vel = (dt > 1e-6f) ? (me->lateral_pos - me->prev_lateral_pos) / dt : 0.f;
			cohesion_steer = glm::clamp(-(lat_err * BOID_COHESION_KP - lat_vel * BOID_COHESION_KD), -1.f, 1.f);
			me->dbg_cohesion_lat_err = lat_err;
			me->dbg_cohesion_lat_vel = lat_vel;
		} else {
			// No rider ahead: if echelon mode, drift toward preferred_lateral on our own
			const BikeAI* my_ai = dynamic_cast<const BikeAI*>(me->input.get());
			if (my_ai && my_ai->lane_strength > 0.f && my_ai->preferred_lateral != 0.f) {
				const float lat_err = my_ai->preferred_lateral - me->lateral_pos;
				const float lat_vel = (dt > 1e-6f) ? (me->lateral_pos - me->prev_lateral_pos) / dt : 0.f;
				cohesion_steer = glm::clamp(-(lat_err * BOID_COHESION_KP * my_ai->lane_strength
				                              - lat_vel * BOID_COHESION_KD), -1.f, 1.f);
				me->dbg_cohesion_lat_err = lat_err;
				me->dbg_cohesion_lat_vel = lat_vel;
			} else {
				me->dbg_cohesion_lat_err = 0.f;
				me->dbg_cohesion_lat_vel = 0.f;
			}
		}
		me->boid_cohesion_steer = cohesion_steer;

		// Speed alignment — match rider-ahead speed only (chain: each rider follows the one in front).
		// This propagates speed changes down the line instead of averaging the whole pack.
		{
			const float target_speed = (me->rider_ahead && me->gap_to_ahead_m < BOID_SPEED_RADIUS)
			                         ? me->rider_ahead->speed : me->speed;
			const float raw_nudge = glm::clamp((target_speed - me->speed) * BOID_SPEED_KP, -BOID_SPEED_MAX, BOID_SPEED_MAX);
			me->boid_align_power_nudge = damp_dt_independent(raw_nudge, me->boid_align_power_nudge, BOID_SPEED_SMOOTH, dt);
		}

		// Hard steer limits — block any steer that would close the gap when a neighbour is
		// already inside the exclusion zone.  Reset each frame; narrowed by each nearby rider.
		me->hard_steer_min = -1.f;
		me->hard_steer_max =  1.f;
		for (int j = 0; j < n; ++j) {
			if (j == i) continue;
			const BikeObject* other = riders_sorted[j];
			const float h_long  = glm::abs(other->course_dist_m - me->course_dist_m);
			if (h_long >= HARD_SEP_LONG_RADIUS) continue;
			const float lat_diff = other->lateral_pos - me->lateral_pos; // +ve = other is right
			const float h_lat    = glm::abs(lat_diff);
			if (h_lat >= HARD_SEP_OUTER_LAT) continue;
			if (h_lat <  HARD_SEP_INNER_LAT) continue; // already overlapping — let them escape
			if (lat_diff > 0.f)
				me->hard_steer_max = 0.f; // other to my right: block rightward steer
			else
				me->hard_steer_min = 0.f; // other to my left:  block leftward steer
		}

		// Save lateral position for derivative computation next frame
		me->prev_lateral_pos = me->lateral_pos;
	}
}

// ============================================================
// Paceline rotation constants
// ============================================================
static float PACELINE_PULL_MIN_S    = 6.f;   // minimum pull duration (seconds)
static float PACELINE_PULL_RANGE_S  = 9.f;   // randomised on top: total = min + rand*range
static float PACELINE_PEEL_OFFSET   = 2.4f;  // lateral offset from centre while peeling/drifting
static float PACELINE_PULL_POWER    = 220.f; // watts while pulling at front
static float PACELINE_DRIFT_POWER   = 90.f;  // watts while drifting to back
static float PACELINE_PEEL_KP       = 0.50f; // proportional steer toward peel target
static float PACELINE_PEEL_KD       = 0.40f; // derivative damp on peel steer
static float PACELINE_DRIFT_KP      = 0.30f; // proportional steer while holding peel offset
static float PACELINE_DRIFT_KD      = 0.30f; // derivative damp while drifting

// Lateral offset (m) within which a rider is considered "clear" of the paceline.
static float PACELINE_PEEL_CLEAR    = 0.7f;
// Approximate road half-width — clamp peel target within this.
static float PACELINE_ROAD_HW       = 3.8f;
// Echelon lane width: ±this value from road centre
static float ECHELON_LANE_OFFSET    = 1.5f;
static float ECHELON_LANE_STRENGTH  = 0.30f; // blend strength in cohesion

void BikeGameApplication::update_paceline()
{
	const int   n  = (int)riders_sorted.size();
	const float dt = eng->get_dt();

	for (int i = 0; i < n; ++i) {
		BikeObject* me = riders_sorted[i];
		BikeAI*     ai = dynamic_cast<BikeAI*>(me->input.get());
		if (!ai) continue;  // skip player

		// ---- Determine if this AI is at the virtual front of the AI group ----
		// "Front" means: no FOLLOWING or PULLING AI is anywhere ahead (no distance limit).
		// The distance cutoff was removed: a Puller can accelerate far ahead, and without
		// this fix every Follower would eventually see "nobody within Xm" and cascade-promote.
		// (Player, peeling, and drifting-back riders are ignored.)
		auto is_at_front = [&]() -> bool {
			for (int j = i - 1; j >= 0; --j) {
				const BikeObject* other = riders_sorted[j];
				const BikeAI* oai = dynamic_cast<const BikeAI*>(other->input.get());
				if (!oai) continue;  // player: ignore
				if (oai->paceline_state == PacelineState::Peeling ||
				    oai->paceline_state == PacelineState::DriftingBack) continue;
				return false;  // FOLLOWING or PULLING AI ahead → not front
			}
			return true;
		};

		// ---- Determine if this AI is at the virtual back of the AI group ----
		// "Back" means: no FOLLOWING or PULLING AI is within 30m behind.
		auto is_at_back = [&]() -> bool {
			for (int j = i + 1; j < n; ++j) {
				const BikeObject* other = riders_sorted[j];
				if (me->course_dist_m - other->course_dist_m > 30.f) break;
				const BikeAI* oai = dynamic_cast<const BikeAI*>(other->input.get());
				if (!oai) continue;  // player: ignore
				if (oai->paceline_state == PacelineState::DriftingBack) continue;
				return false;  // a non-drifting AI is still behind
			}
			return true;
		};

		const float lat_vel = (dt > 1e-6f) ? (me->lateral_pos - me->prev_lateral_pos) / dt : 0.f;

		switch (ai->paceline_state) {

		case PacelineState::Following:
			if (is_at_front()) {
				ai->paceline_state = PacelineState::Pulling;
				ai->pull_timer     = 0.f;
				// Randomise pull duration
				ai->pull_duration  = PACELINE_PULL_MIN_S + (float)(rand() % 1000) * 0.001f * PACELINE_PULL_RANGE_S;
				ai->target_power_watts = PACELINE_PULL_POWER;
			}
			break;

		case PacelineState::Pulling:
			ai->target_power_watts = PACELINE_PULL_POWER;
			// If a stable rider (Following or Pulling) appears ahead, immediately revert
			// to Following so we catch the wheel rather than continuing a solo pull.
			if (me->rider_ahead) {
				const BikeAI* ahead_ai = dynamic_cast<const BikeAI*>(me->rider_ahead->input.get());
				const bool ahead_stable = !ahead_ai ||  // player counts as stable
				    (ahead_ai->paceline_state == PacelineState::Following ||
				     ahead_ai->paceline_state == PacelineState::Pulling);
				if (ahead_stable) {
					ai->paceline_state     = PacelineState::Following;
					ai->pull_timer         = 0.f;
					ai->target_power_watts = 200.f;
					break;
				}
			}
			ai->pull_timer += dt;
			if (ai->pull_timer >= ai->pull_duration) {
				// Choose peel direction: away from road centre (or to the right if centred)
				ai->peel_dir        = (me->lateral_pos < 0.f) ? -1.f : 1.f;
				ai->peel_lateral_tgt = ai->peel_dir * PACELINE_PEEL_OFFSET;
				ai->peel_lateral_tgt = glm::clamp(ai->peel_lateral_tgt,
				                                   -(PACELINE_ROAD_HW - 0.4f),
				                                    PACELINE_ROAD_HW - 0.4f);
				ai->paceline_state  = PacelineState::Peeling;
			}
			break;

		case PacelineState::Peeling: {
			// Override cohesion steer: drive hard toward the peel lateral target.
			const float lat_err    = ai->peel_lateral_tgt - me->lateral_pos;
			const float peel_steer = glm::clamp(
			    -(lat_err * PACELINE_PEEL_KP - lat_vel * PACELINE_PEEL_KD), -1.f, 1.f);
			me->boid_cohesion_steer = peel_steer;

			// Cut power while peeling
			ai->target_power_watts = PACELINE_DRIFT_POWER;

			// Transition once we've moved far enough laterally from centre
			if (glm::abs(me->lateral_pos - ai->peel_lateral_tgt) < PACELINE_PEEL_CLEAR)
				ai->paceline_state = PacelineState::DriftingBack;
			break;
		}

		case PacelineState::DriftingBack: {
			// Hold lateral offset while drifting to the rear.
			const float lat_err     = ai->peel_lateral_tgt - me->lateral_pos;
			const float drift_steer = glm::clamp(
			    -(lat_err * PACELINE_DRIFT_KP - lat_vel * PACELINE_DRIFT_KD), -1.f, 1.f);
			me->boid_cohesion_steer = drift_steer;

			// Run at reduced power to drift backward through the pack
			ai->target_power_watts = PACELINE_DRIFT_POWER;

			// Transition back to Following when at the back of the AI group
			if (is_at_back()) {
				ai->paceline_state      = PacelineState::Following;
				ai->pull_timer          = 0.f;
				ai->target_power_watts  = 200.f;   // resume normal tempo
				ai->peel_lateral_tgt    = 0.f;
			}
			break;
		}
		}
	}
}

void BikeGameApplication::debug_draw_course() const
{
	course.debug_draw();
}

BikeObject* BikeGameApplication::create_player(glm::vec3 pos)
{
	Entity* e = GameplayStatic::spawn_entity();
	e->set_ws_position(pos);
	auto bo = e->create_component<BikeObject>();
	bo->input = std::make_unique<BikePlayer>();
	all_riders.push_back(bo);
	riders_sorted.push_back(bo);
	return bo;
}

BikeObject* BikeGameApplication::create_ai(glm::vec3 pos)
{
	Entity* e = GameplayStatic::spawn_entity();
	e->set_ws_position(pos);
	auto bo  = e->create_component<BikeObject>();
	auto ai  = std::make_unique<BikeAI>();
	ai->course = &course;

	// Echelon lane assignment: alternate AI riders between ±ECHELON_LANE_OFFSET.
	// Index into all_riders at this point (player is always index 0).
	if (echelon_mode) {
		const int ai_idx = (int)all_riders.size();  // 1-based since player is 0
		ai->preferred_lateral = (ai_idx % 2 == 0) ? ECHELON_LANE_OFFSET : -ECHELON_LANE_OFFSET;
		ai->lane_strength     = ECHELON_LANE_STRENGTH;
	}

	bo->input  = std::move(ai);
	all_riders.push_back(bo);
	riders_sorted.push_back(bo);
	return bo;
}

// ============================================================
// BikePlayer — constructor / destructor
// ============================================================

BikePlayer::BikePlayer()
{
	auto* cc_entity = GameplayStatic::spawn_entity();
	auto* cc_comp   = cc_entity->create_component<CameraComponent>();
	cc_comp->set_is_enabled(true);
	cc_comp->set_fov(65.f);
	assert(CameraComponent::get_scene_camera() == cc_comp);
	this->cc = cc_comp;

	freewheel_player = isound->register_sound_player();
	freewheel_player->asset          = SoundFile::load("sounds/free_wheel.wav");
	freewheel_player->looping        = true;
	freewheel_player->attenuate      = false;
	freewheel_player->spatialize     = false;
	freewheel_player->volume_multiply = 0.f;
	freewheel_player->set_play(true);

	wind_player = isound->register_sound_player();
	wind_player->asset           = SoundFile::load("sounds/wind_loop.wav");
	wind_player->looping         = true;
	wind_player->attenuate       = false;
	wind_player->spatialize      = false;
	wind_player->volume_multiply = 0.f;
	wind_player->set_play(true);

	pedal_player = isound->register_sound_player();
	pedal_player->asset           = SoundFile::load("sounds/bike_pedal.wav");
	pedal_player->looping         = true;
	pedal_player->attenuate       = false;
	pedal_player->spatialize      = false;
	pedal_player->volume_multiply = 0.f;
	pedal_player->set_play(true);

	heart_icon_tex = Texture::load("ui/heart_icon.png");

	// Speed lines particle object
	{
		speedlines_handle = idraw->get_scene()->register_particle_obj();
		Particle_Object spo{};
		spo.meshbuilder = &speedlines_mb;
		idraw->get_scene()->update_particle_obj(speedlines_handle, spo);
	}
	// Wind lines particle object
	{
		wind_handle = idraw->get_scene()->register_particle_obj();
		Particle_Object wpo{};
		wpo.meshbuilder = &wind_mb;
		idraw->get_scene()->update_particle_obj(wind_handle, wpo);
	}
}

BikePlayer::~BikePlayer()
{
	isound->remove_sound_player(freewheel_player);
	isound->remove_sound_player(wind_player);
	isound->remove_sound_player(pedal_player);
	idraw->get_scene()->remove_particle_obj(speedlines_handle);
	idraw->get_scene()->remove_particle_obj(wind_handle);
}

// ============================================================
// BikePlayer::evaluate
// ============================================================
#include "Debug.h"
void BikePlayer::evaluate(BikeObject* my_bike)
{
	const float dt = eng->get_dt();

	// --- Gamepad input ---
	const float steer        = apply_deadzone((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX), 0.15f);
	const float brake_amount = apply_deadzone((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT), 0.05f);

	const bool power_up_press   = Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_UP);
	const bool power_down_press = Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	const bool power_up_held    = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_DPAD_UP);
	const bool power_down_held  = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	const bool coast_btn      = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_B);
	const bool speed_hold_btn = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_X);

	// Keyboard fallback
	const bool kb_left       = Input::is_key_down(SDL_SCANCODE_A) || Input::is_key_down(SDL_SCANCODE_LEFT);
	const bool kb_right      = Input::is_key_down(SDL_SCANCODE_D) || Input::is_key_down(SDL_SCANCODE_RIGHT);
	const bool kb_brake      = Input::is_key_down(SDL_SCANCODE_SPACE);
	const bool kb_up_press   = Input::was_key_pressed(SDL_SCANCODE_UP);
	const bool kb_down_press = Input::was_key_pressed(SDL_SCANCODE_DOWN);
	const bool kb_up_held    = Input::is_key_down(SDL_SCANCODE_UP);
	const bool kb_down_held  = Input::is_key_down(SDL_SCANCODE_DOWN);
	const bool kb_coast      = Input::is_key_down(SDL_SCANCODE_C);
	const bool kb_speed_hold = Input::is_key_down(SDL_SCANCODE_V);

	// --- Power level stepping: tap = 1 step, hold = rapid repeat after delay ---
	constexpr float POWER_HOLD_DELAY    = 0.2f;  // seconds before repeat starts
	constexpr float POWER_REPEAT_RATE   = 0.04f; // seconds per repeat step

	auto step_power = [&](int dir) {
		power_level_idx = glm::clamp(power_level_idx + dir, 0, BIKE_NUM_POWER_LEVELS - 1);
	};

	if (power_up_press   || kb_up_press)   step_power(+1);
	if (power_down_press || kb_down_press) step_power(-1);

	const int held_dir = (power_up_held || kb_up_held) ? 1 : (power_down_held || kb_down_held) ? -1 : 0;
	if (held_dir != 0) {
		power_hold_timer += dt;
		if (power_hold_timer >= POWER_HOLD_DELAY) {
			power_repeat_timer += dt;
			while (power_repeat_timer >= POWER_REPEAT_RATE) {
				step_power(held_dir);
				power_repeat_timer -= POWER_REPEAT_RATE;
			}
		}
	} else {
		power_hold_timer   = 0.f;
		power_repeat_timer = 0.f;
	}
	is_coasting = coast_btn || kb_coast || brake_amount > 0.f || kb_brake;

	// --- Speed hold ---
	const bool want_speed_hold = (speed_hold_btn || kb_speed_hold) && !is_coasting;
	if (want_speed_hold && !speed_hold_active) {
		speed_hold_active = true;
		speed_hold_target = my_bike->speed;
		speed_hold_power  = (float)BIKE_POWER_LEVELS[power_level_idx];
	} else if (!want_speed_hold) {
		speed_hold_active = false;
	}

	// --- Combine steer ---
	float steer_combined = steer;
	if (kb_left)  steer_combined -= 1.f;
	if (kb_right) steer_combined += 1.f;
	steer_combined = glm::clamp(steer_combined, -1.f, 1.f);

	// Expo curve: compresses small deflections for finer gamepad control near centre.
	// Keyboard is already binary (±1) so skip it there.
	if (!kb_left && !kb_right && glm::abs(steer_combined) > 0.0001f)
		steer_combined = glm::sign(steer_combined) * glm::pow(glm::abs(steer_combined), steer_expo);

	// Capture steer pre-boids for debug
	this->dbg_steer_before_boids = steer_combined;

	// Boid steer forces — same three as AI, lower weights so player retains control.
	// All sign conventions are handled inside update_boids(), so just scale and add.
	const float sep_s = my_bike->boid_separation_steer * player_boid_sep_scale;
	const float coh_s = my_bike->boid_cohesion_steer   * player_boid_coh_scale;
	const float aln_s = my_bike->boid_align_steer       * player_boid_aln_scale;
	steer_combined += sep_s + coh_s + aln_s;

	this->dbg_boid_sep_steer      = sep_s;
	this->dbg_boid_cohesion_steer = coh_s;
	this->dbg_boid_align_steer    = aln_s;

	steer_combined = glm::clamp(steer_combined, -1.f, 1.f);
	this->dbg_steer_final = steer_combined;



	// --- Fill ControlInput ---
	BikeObject::ControlInput ci;
	ci.steer        = steer_combined;
	ci.brake_amount = kb_brake ? 1.f : brake_amount;

	if (speed_hold_active) {
		const float v            = glm::max(my_bike->speed, 0.3f);
		const float eff_wind_spd = wind_speed * (1.f + wind_gust_factor * 1.5f);
		const glm::vec3 wdir_n   = glm::length(wind_direction) > 0.001f
		                           ? glm::normalize(wind_direction) : glm::vec3(0.f);
		const float wind_along   = glm::dot(wdir_n, my_bike->bike_direction) * eff_wind_spd;
		const float app_speed    = my_bike->speed - wind_along;
		const float drag         = 0.5f * 1.225f * 0.3f * app_speed * glm::abs(app_speed);
		const float rolling      = 0.004f * 83.f * 9.81f;
		const float slope        = 83.f * 9.81f * glm::sin(my_bike->terrain_gradient);
		const float ideal_power  = glm::max((drag + rolling + slope) * v, 0.f);

		const float toward = (ideal_power > speed_hold_power) ? sh_power_up : sh_power_down;
		speed_hold_power = damp_dt_independent(ideal_power, speed_hold_power, toward, dt);
		speed_hold_power = glm::clamp(speed_hold_power, 0.f, sh_power_max);

		// Draft-seek: when in draft range but gap is widening, auto-add power to close it.
		if (my_bike->rider_ahead && my_bike->gap_to_ahead_m > 2.5f && my_bike->gap_to_ahead_m < 15.f) {
			const float seek_bonus = glm::clamp((my_bike->gap_to_ahead_m - 2.5f) * 8.f, 0.f, 60.f);
			speed_hold_power += seek_bonus;
			this->dbg_draft_seek_power = seek_bonus;
		}

		speed_hold_power = glm::clamp(speed_hold_power, 0.f, sh_power_max);
		ci.power = speed_hold_power;
	} else {
		ci.power = is_coasting ? 0.f : (float)BIKE_POWER_LEVELS[power_level_idx];
	}
	// Boid alignment + draft-seek: only applied when actually pedalling (not coasting).
	this->dbg_boid_align_power = 0.f;
	this->dbg_draft_seek_power = 0.f;
	if (!is_coasting && ci.power > 0.f) {
		const float align_nudge = my_bike->boid_align_power_nudge * 0.80f;
		ci.power += align_nudge;
		this->dbg_boid_align_power = align_nudge;
	}

	current_power = ci.power;
	my_bike->update_tick(ci);

	// --- Wind ---
	update_wind(my_bike);

	// --- Camera ---
	if (!g_follow_rider)
		update_camera(my_bike, ci.steer, ci.brake_amount);

	// --- Sound: freewheel ---
	const float fw_target = ci.is_coasting() ? 1.f : 0.f;
	freewheel_player->volume_multiply = damp_dt_independent(fw_target, freewheel_player->volume_multiply, free_wheel_fade, dt);
	freewheel_player->pitch_multiply  = 0.8f + my_bike->speed * 0.04f;
	freewheel_player->update();

	// --- Sound: pedalling ---
	// Audible when power is applied; pitch tracks cadence (rev/s → semitone-linear scale).
	// Cadence of 1.5 rev/s (target ~90 RPM) maps to pitch 1.0.
	{
		float pedal_vol = ci.is_coasting() ? 0.f : 1.f;
		pedal_vol *= glm::clamp(my_bike->cadence / 0.9f, 0.f, 1.f) * 0.35f;
		pedal_player->volume_multiply = damp_dt_independent(pedal_vol, pedal_player->volume_multiply, 0.03f, dt);
		pedal_player->pitch_multiply = map_range(my_bike->cadence * 90.f, 70.f, 110.f, 0.95, 1.05);
		pedal_player->update();
	}

	// --- Sound: wind ambient ---
	{
		const float eff_spd = my_bike->speed-my_bike->get_wind_along_bike();// wind_speed* (1.f + wind_gust_factor * gust_speed_amp);
		const float vol_target = glm::clamp(eff_spd / 10.f, 0.f, 1.f)*0.8 + 0.4;
		wind_player->volume_multiply = damp_dt_independent(vol_target, wind_player->volume_multiply, 0.04f, dt);
		wind_player->pitch_multiply  = 0.75f + vol_target * 0.5f;
		wind_player->spatialize = true;
		auto wind_dir = my_bike->get_wind_along_bike_vector();
		//Debug::add_line(my_bike->get_ws_position(), my_bike->get_ws_position() + wind_dir, COLOR_WHITE, -1);
		wind_player->spatial_pos = cc->get_ws_position() - wind_dir;

		wind_player->update();
	}

	// --- Sound: gear change one-shot ---
	if (my_bike->just_shifted) {
		static const SoundFile* gear_snd = SoundFile::load("sounds/gear_change.wav");
		isound->play_sound(gear_snd, 0.2, 1.2f, 0.f, 0.f, SndAtn::Linear, false, false, {});
	}

	draw_power_meter(ci.power, power_level_idx, is_coasting, speed_hold_active, speed_hold_power,
	                 my_bike->stamina.actual_power, my_bike->stamina.power_ceiling);
	draw_stamina_ui(my_bike->stamina, my_bike->rider);

	bp_for_debug = this;
	bo_for_debug = my_bike;
}

// ============================================================
// Debug menu: Bike Status
// ============================================================

static void bike_status_debug()
{
	if (!bp_for_debug) return;
	const int power_level_idx = bp_for_debug->power_level_idx;
	const bool is_coasting    = bp_for_debug->is_coasting;
	auto* my_bike = bo_for_debug;

	const float speed_kmh = my_bike->speed * 3.6f;
	ImGui::Text("Speed:   %.1f km/h", speed_kmh);
	if (bp_for_debug->speed_hold_active)
		ImGui::Text("Power:   (speed hold %.1f km/h)  %.0f W",
			bp_for_debug->speed_hold_target * 3.6f, bp_for_debug->speed_hold_power);
	else
		ImGui::Text("Power:   %s  %d W",
			is_coasting ? "(coast)" : "",
			is_coasting ? 0 : BIKE_POWER_LEVELS[power_level_idx]);
	ImGui::Text("Cadence: %.0f rpm", my_bike->cadence * 60.f);
	ImGui::Text("Gear:    %d / %d   [%d-%d]",
		my_bike->gear.current_low_gear + 1,
		my_bike->gear.current_high_gear + 1,
		my_bike->gear.front_cogs[my_bike->gear.current_low_gear],
		my_bike->gear.back_cogs[my_bike->gear.current_high_gear]);
	const float eff_ws       = wind_speed * (1.f + wind_gust_factor * 1.5f);
	const glm::vec3 wdn      = glm::length(wind_direction) > 0.001f ? glm::normalize(wind_direction) : glm::vec3(0.f);
	const float wind_comp    = glm::dot(wdn, my_bike->bike_direction) * eff_ws;
	const float app_spd      = my_bike->speed - wind_comp;
	const float aero_drag_N  = 0.5f * 1.225f * 0.3f * app_spd * glm::abs(app_spd);
	ImGui::Text("Aero drag: %.1f N  (app wind %.1f m/s, %+.1f along)", aero_drag_N, eff_ws, wind_comp);
	ImGui::Text("Gradient:  %.1f%%  (%.1f deg)",
		glm::tan(my_bike->terrain_gradient) * 100.f,
		glm::degrees(my_bike->terrain_gradient));
	ImGui::Text("Steer committed: %.3f", my_bike->current_steer);
	ImGui::Text("[UP/DOWN] Power  [C/B] Coast  [SPACE] Brake  [V/X] Speed Hold");

	if (bo_for_debug) {
		const StaminaState& s = bo_for_debug->stamina;
		const RiderStats&   r = bo_for_debug->rider;
		ImGui::SeparatorText("Stamina");
		ImGui::Text("Glycogen:     %.1f%%   eff_FTP=%.0fW  (%s)",
			s.glycogen * 100.f, s.effective_ftp, s.legs_descriptor());
		ImGui::Text("W':           %.0f / %.0f J  (%d bars)  ceiling=%.0fW",
			s.w_prime, r.w_prime_max, s.w_prime_bars(r.w_prime_max), s.power_ceiling);
		ImGui::Text("HR:           %.0f bpm  (drift +%.1f  lactate +%.1f  heat +%.1f)  zone %d",
			s.hr_current, s.hr_drift, s.lactate * 0.002f, s.heat_stress * 20.f,
			s.hr_zone(r.hr_rest, r.hr_max));
		ImGui::Text("Heat stress:  %.1f%%   eff_FTP=%.0fW (heat factor %.2f)",
			s.heat_stress * 100.f, s.effective_ftp, 1.f - s.heat_stress * 0.12f);
	}

	ImGui::SeparatorText("Boids (player)");
	if (bp_for_debug && bo_for_debug) {
		const BikePlayer* bp = bp_for_debug;
		const BikeObject* bo = bo_for_debug;

		ImGui::Text("Steer before boids: %+.3f", bp->dbg_steer_before_boids);
		ImGui::Text("  + separation:      %+.3f  (raw=%.3f)", bp->dbg_boid_sep_steer, bo->boid_separation_steer);
		ImGui::Text("  + cohesion:        %+.3f  (raw=%.3f)", bp->dbg_boid_cohesion_steer, bo->boid_cohesion_steer);
		ImGui::Text("  + alignment:       %+.3f  (raw=%.3f)", bp->dbg_boid_align_steer, bo->boid_align_steer);
		ImGui::Text("Steer final:         %+.3f", bp->dbg_steer_final);

		if (bo->rider_ahead) {
			const float lat_err_raw = bo->rider_ahead->lateral_pos - bo->lateral_pos;
			ImGui::Text("Rider ahead: gap=%.2f m  their_lat=%+.2f  my_lat=%+.2f  err=%+.2f (clamped %+.2f)",
				bo->gap_to_ahead_m,
				bo->rider_ahead->lateral_pos,
				bo->lateral_pos,
				lat_err_raw,
				glm::clamp(lat_err_raw, -1.f, 1.f));
		} else {
			ImGui::TextDisabled("Rider ahead: none (leading)");
		}

		ImGui::Spacing();
		ImGui::Text("Align power nudge:  %+.1f W  (applied %+.1f W)", bo->boid_align_power_nudge, bp->dbg_boid_align_power);
		ImGui::Text("Draft-seek bonus:   %+.1f W", bp->dbg_draft_seek_power);
		ImGui::Text("Draft factor:        %.2f  (%.0f%% drag)", bo->draft_factor, bo->draft_factor * 100.f);
	}

	ImGui::SeparatorText("Steering");
	ImGui::DragFloat("steer_expo", &steer_expo, 0.05f, 1.f, 4.f);
	ImGui::Text("  half-stick → %.0f%% input  (1=linear, 2=quad, 3=cubic)", glm::pow(0.5f, steer_expo) * 100.f);

	ImGui::SeparatorText("Speed Hold");
	{
		ImGui::DragFloat("sh_power_up",   &sh_power_up,   0.001f, 0.001f, 1.f);
		ImGui::DragFloat("sh_power_down", &sh_power_down, 0.001f, 0.001f, 1.f);
		ImGui::DragFloat("sh_power_max",  &sh_power_max,  10.f,   0.f,   2000.f);
	}
	ImGui::SeparatorText("Sound");
	{
		ImGui::InputFloat("free_wheel_fade", &free_wheel_fade);
	}
}
ADD_TO_DEBUG_MENU(bike_status_debug);

// ============================================================
// Debug menu: Course / Race State
// ============================================================

static void bike_course_debug()
{
	if (!g_bike_app) return;
	const BikeCourse& c = g_bike_app->course;

	if (!c.is_built) {
		ImGui::TextColored({1,0.4f,0.4f,1}, "Course not built — no bike_waypoint spawners in level?");
		return;
	}

	ImGui::Text("Waypoints: %d   Length: %.0f m", (int)c.waypoints.size(), c.total_length_m);

	ImGui::SeparatorText("Riders");
	const auto& sorted = g_bike_app->riders_sorted;
	for (int i = 0; i < (int)sorted.size(); ++i) {
		const BikeObject* r = sorted[i];
		ImGui::Text("P%d  dist=%.0f m  lat=%+.2f m  draft=%.2f",
			r->race_position, r->course_dist_m, r->lateral_pos, r->draft_factor);
	}

	ImGui::SeparatorText("Visualisation");
	static bool draw_course = false;
	static bool draw_ai_lookahead = true;
	ImGui::Checkbox("Draw course spline", &draw_course);
	ImGui::Checkbox("Draw AI lookahead points", &draw_ai_lookahead);
	if (draw_course)
		c.debug_draw();
	if (draw_ai_lookahead) {
		for (auto* r : g_bike_app->all_riders) {
			if (auto* ai = dynamic_cast<BikeAI*>(r->input.get())) {
				Debug::add_sphere(ai->dbg_lookahead_pt, 0.4f, COLOR_CYAN, -1.f);
				Debug::add_line(r->get_ws_position(), ai->dbg_lookahead_pt,
				                Color32(0, 0xff, 0xff, 0x88), -1.f);
			}
		}
	}
}
ADD_TO_DEBUG_MENU(bike_course_debug);

// ============================================================
// Debug camera follow state — file scope so update() can read it
// ============================================================
static int   g_follow_idx    = 0;
static float g_follow_dist   = 2.0f;
static float g_follow_height = 1.8f;
static float g_follow_pitch  = 0.f;

// Called by BikeGameApplication::update() after all rider updates
static void apply_debug_follow_camera()
{
	if (!g_follow_rider || !g_bike_app) return;
	const auto& all = g_bike_app->all_riders;
	if (all.empty()) return;
	const int idx = glm::clamp(g_follow_idx, 0, (int)all.size() - 1);
	BikeObject* bo = all[idx];

	CameraComponent* cc = CameraComponent::get_scene_camera();
	if (!cc) return;

	const glm::vec3 fwd      = glm::normalize(bo->bike_direction);
	const glm::vec3 world_up = glm::vec3(0, 1, 0);
	const glm::vec3 pivot    = bo->get_ws_position() + world_up * g_follow_height;
	const glm::vec3 right    = glm::normalize(glm::cross(fwd, world_up));

	const glm::quat pitch_rot = glm::angleAxis(glm::radians(g_follow_pitch), right);
	const glm::vec3 orbit_dir = glm::normalize(pitch_rot * (-fwd));
	const glm::vec3 cam_pos   = pivot + orbit_dir * g_follow_dist;

	const glm::vec3 look_at   = pivot + fwd * 3.f;
	const glm::vec3 cam_fwd   = glm::normalize(look_at - cam_pos);
	const glm::vec3 cam_right = glm::normalize(glm::cross(cam_fwd, world_up));
	const glm::vec3 cam_up    = glm::normalize(glm::cross(cam_right, cam_fwd));

	cc->get_owner()->set_ws_transform(glm::mat4(
		glm::vec4(cam_right, 0.f),
		glm::vec4(cam_up,    0.f),
		glm::vec4(-cam_fwd,  0.f),
		glm::vec4(cam_pos,   1.f)));

	BikePlayer* bp = dynamic_cast<BikePlayer*>(bo->input.get());
	BikeAI*     ai = dynamic_cast<BikeAI*>(bo->input.get());
	if (bp) {
		GameplayStatic::debug_text(string_format("[Player] steer_final:   %.2f", bp->dbg_steer_final));
		GameplayStatic::debug_text(string_format("[Player] before_boids:  %.2f", bp->dbg_steer_before_boids));
		GameplayStatic::debug_text(string_format("[Player] sep:%.2f coh:%.2f aln:%.2f",
			bp->dbg_boid_sep_steer, bp->dbg_boid_cohesion_steer, bp->dbg_boid_align_steer));
		GameplayStatic::debug_text(string_format("[Player] coh kp_in(lat_err):%.2f  kd_in(lat_vel):%.2f",
			bo->dbg_cohesion_lat_err, bo->dbg_cohesion_lat_vel));
	} else if (ai) {
		GameplayStatic::debug_text(string_format("[AI] steer_final:  %.2f", ai->dbg_steer_final));
		GameplayStatic::debug_text(string_format("[AI] pid_pre_boid: %.2f", ai->dbg_steer_pre_boids));
		GameplayStatic::debug_text(string_format("[AI] sep:%.2f coh:%.2f aln:%.2f",
			bo->boid_separation_steer, bo->boid_cohesion_steer, bo->boid_align_steer));
		GameplayStatic::debug_text(string_format("[AI] coh kp_in(lat_err):%.2f  kd_in(lat_vel):%.2f",
			bo->dbg_cohesion_lat_err, bo->dbg_cohesion_lat_vel));
		GameplayStatic::debug_text(string_format("[AI] power base:%.0fW  align:%+.0fW  seek:%+.0fW  = %.0fW",
			ai->dbg_power_base, ai->dbg_power_align_nudge, ai->dbg_power_seek_bonus, ai->dbg_power_final));
		GameplayStatic::debug_text(string_format("[AI] locked_gap: %.2fm  switch_t: %.1fs",
			bo->gap_to_ahead_m, bo->rider_ahead_switch_t));
	}
}

// ============================================================
// Debug menu: Boid visualisation
// ============================================================

static void bike_boid_debug()
{
	if (!g_bike_app) return;
	const auto& all    = g_bike_app->all_riders;
	const auto& sorted = g_bike_app->riders_sorted;
	if (all.empty()) return;

	// ---- Rider picker ----
	static int  selected_idx  = 0;          // index into all_riders
	static bool draw_boids    = true;

	ImGui::Checkbox("Draw boid debug", &draw_boids);

	// Build label list: "Player" for BikePlayer, "AI #N" for BikeAI
	ImGui::Text("Select rider:");
	for (int i = 0; i < (int)all.size(); ++i) {
		const bool is_ai = (dynamic_cast<BikeAI*>(all[i]->input.get()) != nullptr);
		char label[32];
		if (is_ai) {
			// Count which AI number this is
			int ai_n = 0;
			for (int k = 0; k < i; ++k)
				if (dynamic_cast<BikeAI*>(all[k]->input.get())) ++ai_n;
			snprintf(label, sizeof(label), "AI #%d", ai_n + 1);
		} else {
			snprintf(label, sizeof(label), "Player");
		}
		ImGui::SameLine();
		if (ImGui::RadioButton(label, selected_idx == i))
			selected_idx = i;
	}
	selected_idx = glm::clamp(selected_idx, 0, (int)all.size() - 1);
	BikeObject* bo = all[selected_idx];

	// ---- Camera follow ----
	g_follow_idx = selected_idx;
	ImGui::Checkbox("Follow selected rider (camera)", &g_follow_rider);
	if (g_follow_rider) {
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("dist",   &g_follow_dist,   0.05f, 0.5f, 10.f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("height", &g_follow_height, 0.05f, 0.5f,  5.f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("pitch°", &g_follow_pitch,  0.5f,  0.f, 80.f);
	}

	// ---- Text readout ----
	ImGui::Separator();
	ImGui::Text("sep_steer (smoothed): %+.3f", bo->boid_separation_steer);
	ImGui::Text("align_power_nudge:    %+.1f W", bo->boid_align_power_nudge);
	ImGui::Text("gap_to_ahead:         %.2f m", bo->gap_to_ahead_m);
	ImGui::Text("gap_to_behind:        %.2f m", bo->gap_to_behind_m);
	if (bo->rider_ahead)
		ImGui::Text("rider_ahead lat:      %+.2f m  (my lat %+.2f)", bo->rider_ahead->lateral_pos, bo->lateral_pos);

	// ---- Tuning sliders ----
	ImGui::SeparatorText("Separation");
	ImGui::DragFloat("sep long radius", &BOID_SEP_LONG_RADIUS, 0.05f, 0.5f, 10.f);
	ImGui::DragFloat("sep min lat",     &BOID_SEP_MIN_LAT,     0.05f, 0.1f,  3.f);
	ImGui::DragFloat("sep kp",          &BOID_SEP_KP,          0.05f, 0.f,   5.f);
	ImGui::DragFloat("sep kd",          &BOID_SEP_KD,          0.05f, 0.f,   5.f);
	ImGui::DragFloat("sep power max W", &BOID_SEP_POWER_MAX,   5.f,   0.f, 200.f);
	ImGui::DragFloat("BOID_SEP_MULT_SIMPLE", &BOID_SEP_MULT_SIMPLE, 0.05, -1, 1);
	ImGui::SeparatorText("Cohesion (paceline, PD)");
	ImGui::DragFloat("coh kp",      &BOID_COHESION_KP,     0.01f, 0.f,  1.f);
	ImGui::DragFloat("coh kd",      &BOID_COHESION_KD,     0.02f, 0.f,  2.f);

	ImGui::SeparatorText("Heading Alignment");
	ImGui::DragFloat("align radius", &BOID_ALIGN_STEER_RADIUS, 0.5f,  1.f, 30.f);
	ImGui::DragFloat("align kp",     &BOID_ALIGN_STEER_KP,     0.01f, 0.f,  1.f);
	ImGui::DragFloat("align smooth", &BOID_ALIGN_STEER_SMOOTH, 0.002f, 0.001f, 0.5f);

	ImGui::SeparatorText("Speed Alignment (rider-ahead chain)");
	ImGui::DragFloat("speed radius", &BOID_SPEED_RADIUS, 1.f,   1.f, 60.f);
	ImGui::DragFloat("speed kp",     &BOID_SPEED_KP,     0.5f,  0.f, 60.f);
	ImGui::DragFloat("speed max W",  &BOID_SPEED_MAX,    5.f,   0.f, 200.f);
	ImGui::DragFloat("speed smooth", &BOID_SPEED_SMOOTH, 0.002f, 0.001f, 0.5f);

	ImGui::SeparatorText("Rider-Ahead Selection");
	ImGui::DragFloat("ahead lat ratio", &AHEAD_LAT_RATIO, 0.05f, 0.f, 3.f);
	ImGui::DragFloat("target switch delay s", &AI_TARGET_SWITCH_DELAY,   0.1f, 0.f, 10.f);
	ImGui::DragFloat("target switch delta m", &AI_TARGET_SWITCH_DELTA_M, 0.5f, 0.1f, 20.f);

	// Apply Stanley / corner tuning to all AI riders
	ImGui::SeparatorText("AI Cornering (Stanley)");
	{
		static float s_stanley_k      = 0.5f;
		static float s_corner_look_m  = 30.f;
		static float s_corner_speed_k = 5.0f;
		bool changed = false;
		changed |= ImGui::DragFloat("stanley_k",      &s_stanley_k,      0.02f, 0.05f, 5.f);
		ImGui::SameLine(); ImGui::TextDisabled("lat gain");
		changed |= ImGui::DragFloat("corner_look_m",  &s_corner_look_m,  1.f,   5.f,  80.f);
		changed |= ImGui::DragFloat("corner_speed_k", &s_corner_speed_k, 0.1f,  1.f,  20.f);
		ImGui::SameLine(); ImGui::TextDisabled("~1/traction_lean_comp");
		if (changed) {
			for (auto* r : g_bike_app->all_riders) {
				if (auto* ai = dynamic_cast<BikeAI*>(r->input.get())) {
					ai->stanley_k      = s_stanley_k;
					ai->corner_look_m  = s_corner_look_m;
					ai->corner_speed_k = s_corner_speed_k;
				}
			}
		}
		// Show live corner data for first AI
		for (auto* r : g_bike_app->all_riders) {
			if (auto* ai = dynamic_cast<BikeAI*>(r->input.get())) {
				const float look = glm::max(s_corner_look_m, r->speed * 1.5f);
				const float min_r = g_bike_app->course.min_turn_radius_ahead(r->course_dist_m, look);
				const float v_max = glm::sqrt(s_corner_speed_k * 9.81f * min_r * r->surface_traction);
				ImGui::Text("  min_r=%.1fm  v_max=%.1fm/s  spd=%.1f", min_r, v_max, r->speed);
				break;
			}
		}
	}

	ImGui::SeparatorText("AI Gap Following");
	ImGui::DragFloat("gap target m",   &AI_GAP_TARGET,   0.1f,  0.5f, 20.f);
	ImGui::DragFloat("gap kp W/m",     &AI_GAP_KP,       0.5f,  0.f,  40.f);
	ImGui::DragFloat("gap kd W/(m/s)", &AI_GAP_KD,       0.5f,  0.f, 100.f);
	ImGui::DragFloat("gap max pull W", &AI_GAP_MAX_PULL,  5.f,  0.f, 200.f);
	ImGui::DragFloat("gap max push W", &AI_GAP_MAX_PUSH, 5.f, 0.f, 200.f);

	ImGui::SeparatorText("Player scales");
	ImGui::DragFloat("player sep scale", &player_boid_sep_scale, 0.01f, 0.f, 2.f);
	ImGui::DragFloat("player coh scale", &player_boid_coh_scale, 0.01f, 0.f, 2.f);
	ImGui::DragFloat("player aln scale", &player_boid_aln_scale, 0.01f, 0.f, 2.f);

	ImGui::SeparatorText("Paceline Rotation");
	ImGui::Checkbox("paceline_active", &g_bike_app->paceline_active);
	ImGui::SameLine();
	ImGui::Checkbox("echelon_mode (restart needed)", &g_bike_app->echelon_mode);
	ImGui::DragFloat("pull min s",      &PACELINE_PULL_MIN_S,    0.5f,  2.f,  60.f);
	ImGui::DragFloat("pull range s",    &PACELINE_PULL_RANGE_S,  0.5f,  0.f,  60.f);
	ImGui::DragFloat("pull power W",    &PACELINE_PULL_POWER,    5.f,   50.f, 600.f);
	ImGui::DragFloat("drift power W",   &PACELINE_DRIFT_POWER,   5.f,   20.f, 300.f);
	ImGui::DragFloat("peel offset m",   &PACELINE_PEEL_OFFSET,   0.1f,  0.5f,   4.f);
	ImGui::DragFloat("peel clear m",    &PACELINE_PEEL_CLEAR,    0.05f, 0.1f,   2.f);
	ImGui::DragFloat("peel kp",         &PACELINE_PEEL_KP,       0.02f, 0.f,    2.f);
	ImGui::DragFloat("peel kd",         &PACELINE_PEEL_KD,       0.02f, 0.f,    2.f);
	ImGui::DragFloat("drift kp",        &PACELINE_DRIFT_KP,      0.02f, 0.f,    2.f);
	ImGui::DragFloat("drift kd",        &PACELINE_DRIFT_KD,      0.02f, 0.f,    2.f);
	ImGui::DragFloat("echelon lane m",  &ECHELON_LANE_OFFSET,    0.1f,  0.3f,   3.5f);
	ImGui::DragFloat("echelon strength",&ECHELON_LANE_STRENGTH,  0.01f, 0.f,    1.f);

	// Per-AI paceline state readout
	ImGui::SeparatorText("Paceline state per AI");
	for (auto* r : g_bike_app->all_riders) {
		const BikeAI* ai = dynamic_cast<const BikeAI*>(r->input.get());
		if (!ai) { ImGui::TextDisabled("P%d [Player]", r->race_position); continue; }
		ImGui::Text("P%d  %-10s  pull=%.1f/%.1fs  lat=%+.2f  tgt=%+.2f  pref=%+.2f",
			r->race_position, paceline_state_name(ai->paceline_state),
			ai->pull_timer, ai->pull_duration,
			r->lateral_pos, ai->peel_lateral_tgt, ai->preferred_lateral);
	}

	if (!draw_boids) return;

	// ============================================================
	// World-space drawing
	// ============================================================
	const glm::vec3 my_pos   = bo->get_ws_position();
	const glm::vec3 up        = glm::vec3(0, 0.05f, 0);  // slight y offset so lines clear the ground
	const glm::vec3 right_ws  = glm::normalize(glm::cross(bo->bike_direction, glm::vec3(0, 1, 0)));

	// Highlight selected rider with a white sphere
	Debug::add_sphere(my_pos + glm::vec3(0, 1.2f, 0), 0.25f, COLOR_WHITE, -1.f);

	// ---- Separation debug ----
	// Draw the separation zone boundary (rectangle in road plane)
	{
		const glm::vec3 fwd_ws = bo->bike_direction;
		// right_ws = road-LEFT (cross(fwd, up)), so road-right = -right_ws
		const glm::vec3 road_right = -right_ws;
		const float hl = BOID_SEP_LONG_RADIUS;  // half-length (longitudinal)
		const float hw = BOID_SEP_MIN_LAT;       // half-width  (lateral)
		const glm::vec3 box_y = glm::vec3(0, 0.15f, 0);
		// Four corners
		glm::vec3 c[4] = {
			my_pos + box_y + fwd_ws * hl + road_right * hw,
			my_pos + box_y - fwd_ws * hl + road_right * hw,
			my_pos + box_y - fwd_ws * hl - road_right * hw,
			my_pos + box_y + fwd_ws * hl - road_right * hw,
		};
		const Color32 box_col(0xff, 0x60, 0x00, 0x88);
		for (int ci = 0; ci < 4; ++ci)
			Debug::add_line(c[ci], c[(ci + 1) % 4], box_col, -1.f);
	}

	// Per-neighbour separation contributions
	const float my_lat_vel = (eng->get_dt() > 1e-6f)
	    ? (bo->lateral_pos - bo->prev_lateral_pos) / eng->get_dt() : 0.f;

	for (BikeObject* other : all) {
		if (other == bo) continue;

		const float long_gap = glm::abs(bo->course_dist_m - other->course_dist_m);
		const float lat_gap  = glm::abs(bo->lateral_pos   - other->lateral_pos);

		if (long_gap >= BOID_SEP_LONG_RADIUS) continue;

		const glm::vec3 other_pos = other->get_ws_position();

		if (lat_gap >= BOID_SEP_MIN_LAT) {
			// In longitudinal range but lateral is safe — dim sphere
			Debug::add_sphere(other_pos + glm::vec3(0, 0.8f, 0),
			                  0.15f, Color32(0xff, 0x80, 0x00, 0x44), -1.f);
			continue;
		}

		// Inside danger zone — compute exact same values as update_boids
		const float long_weight = 1.f - (long_gap / BOID_SEP_LONG_RADIUS);
		const float dir_away    = (bo->lateral_pos >= other->lateral_pos) ? -1.f : 1.f;
		const float overlap     = BOID_SEP_MIN_LAT - lat_gap;
		const float kp_term     = overlap * BOID_SEP_KP;
		const float closing     = glm::max(0.f, my_lat_vel * dir_away);
		const float kd_term     = closing * BOID_SEP_KD;
		const float contribution = dir_away * long_weight * (kp_term + kd_term);

		// Red sphere — brighter = more overlap
		const uint8_t alpha = (uint8_t)(80 + 175.f * (overlap / BOID_SEP_MIN_LAT));
		Debug::add_sphere(other_pos + glm::vec3(0, 1.f, 0), 0.35f,
		                  Color32(0xff, 0x20, 0x20, alpha), -1.f);

		// Line to them
		Debug::add_line(my_pos + up, other_pos + up,
		                Color32(0xff, 0x40, 0x40, 0xaa), -1.f);

		// Push arrow: right_ws = road-LEFT, so steer direction = right_ws * contribution
		// (negative contribution → road-right arrow, positive → road-left)
		const float arrow_len   = glm::abs(contribution) * 2.f;
		const glm::vec3 arrow_end = my_pos + up + right_ws * contribution * 2.f;
		Debug::add_line(my_pos + up, arrow_end, Color32(0xff, 0x99, 0x00, 0xff), -1.f);
		Debug::add_sphere(arrow_end, 0.12f, Color32(0xff, 0x99, 0x00, 0xff), -1.f);

		// KD closing-velocity indicator: magenta dot if KD is firing
		if (kd_term > 0.01f) {
			Debug::add_sphere(other_pos + glm::vec3(0, 1.5f, 0),
			                  0.2f, Color32(0xff, 0x00, 0xff, 0xcc), -1.f);
		}

		GameplayStatic::debug_text(string_format(
		    "sep neighbour: overlap=%.2fm  kp=%.2f  closing=%.2f  kd=%.2f  total=%.2f",
		    overlap, kp_term, closing, kd_term, contribution));
	}

	// Net separation steer — green arrow, height 4, scaled by steer value
	// right_ws = road-LEFT so positive steer → arrow goes left, negative → right
	{
		const float sep = bo->boid_separation_steer;
		GameplayStatic::debug_text(string_format("sep_steer=%.3f  lat_vel=%.3f m/s", sep, my_lat_vel));
		const glm::vec3 base = my_pos + up * 4.f;
		Debug::add_line(base, base + right_ws * sep * 3.f, Color32(0x00, 0xff, 0x60, 0xff), -1.f);
		Debug::add_sphere(base + right_ws * sep * 3.f, 0.2f, Color32(0x00, 0xff, 0x60, 0xff), -1.f);
	}

	// ---- Rider ahead — lateral alignment target (blue) ----
	if (bo->rider_ahead && bo->gap_to_ahead_m < 12.f) {
		const glm::vec3 ahead_pos = bo->rider_ahead->get_ws_position();
		// Blue line from us to them
		Debug::add_line(my_pos + up * 2.f, ahead_pos + up * 2.f, Color32(0x40, 0x80, 0xff, 0xcc), -1.f);
		Debug::add_sphere(ahead_pos + glm::vec3(0, 1.f, 0), 0.3f, Color32(0x40, 0x80, 0xff, 0xcc), -1.f);

		// Lateral error arrow (cyan) — points toward rider ahead's lateral position.
		// Negated because cross(fwd,up) is road-LEFT in this engine; negate to get road-right.
		const float lat_err = glm::clamp(bo->rider_ahead->lateral_pos - bo->lateral_pos, -1.f, 1.f);
		if (glm::abs(lat_err) > 0.01f) {
			const glm::vec3 lat_end = my_pos + up * 3.f - right_ws * lat_err * 1.5f;
			Debug::add_line(my_pos + up * 3.f, lat_end, Color32(0x00, 0xff, 0xff, 0xff), -1.f);
			Debug::add_sphere(lat_end, 0.15f, Color32(0x00, 0xff, 0xff, 0xff), -1.f);
		}
	}

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

// ============================================================
// Crack trigger: fire bump FX when a rider rolls over a crack decal
// ============================================================

// Per-type crack configuration (add more rows for crack2, crack3, etc.)
CrackTypeConfig crack_types[NUM_CRACK_TYPES] = {
	{ "materials/decals/crack1_decal.mi", 0.5f, 1.0f, 0.8f, 0 },
	//  material                          str   rmul  dist  count
};

// Speed scaling: impulse = strength * clamp(speed / speed_ref, speed_min_mult, speed_max_mult)
static float crack_speed_ref      = 10.f;   // m/s at which mult = 1.0
static float crack_speed_min_mult = 0.2f;   // minimum multiplier (stationary)
static float crack_speed_max_mult = 2.5f;   // cap at high speed

static bool  crack_debug_draw = false;

void BikeGameApplication::update_crack_triggers()
{
	if (crack_instances.empty()) return;
	const float dt = eng->get_dt();

	for (auto* bike : all_riders) {
		if (bike->crack_cooldown > 0.f) {
			bike->crack_cooldown -= dt;
			bike->crack_impulse = 0.f;
			continue;
		}
		bike->crack_impulse = 0.f;

		const glm::vec3 bpos = bike->get_owner()->get_ws_position();
		for (const auto& ci : crack_instances) {
			const CrackTypeConfig& cfg = crack_types[ci.type_idx];
			const float r2 = ci.trigger_radius * ci.trigger_radius;
			const float dx = bpos.x - ci.pos.x;
			const float dz = bpos.z - ci.pos.z;
			if (dx * dx + dz * dz < r2) {
				const float speed_mult = glm::clamp(bike->speed / crack_speed_ref,
				                                    crack_speed_min_mult, crack_speed_max_mult);
				bike->crack_impulse  = cfg.strength * speed_mult;
				// cooldown_dist / speed keeps retrigger rate constant per metre travelled
				static constexpr float MIN_SPEED_FOR_COOLDOWN = 1.f; // m/s floor
				bike->crack_cooldown = cfg.cooldown_dist / glm::max(bike->speed, MIN_SPEED_FOR_COOLDOWN);
				break;
			}
		}
	}

	if (crack_debug_draw) {
		for (const auto& ci : crack_instances) {
			Debug::add_sphere(ci.pos + glm::vec3(0, 0.1f, 0),
			                  ci.trigger_radius,
			                  Color32(0xff, 0x44, 0x00, 0xcc), -1.f);
		}
	}
}

static void crack_debug_menu()
{
	ImGui::SeparatorText("Crack Triggers");
	ImGui::Checkbox("debug draw", &crack_debug_draw);
	ImGui::DragFloat("speed_ref",      &crack_speed_ref,      0.5f,  1.f, 40.f);
	ImGui::DragFloat("speed_min_mult", &crack_speed_min_mult, 0.01f, 0.f, 1.f);
	ImGui::DragFloat("speed_max_mult", &crack_speed_max_mult, 0.05f, 1.f, 5.f);
	for (int ti = 0; ti < NUM_CRACK_TYPES; ++ti) {
		CrackTypeConfig& cfg = crack_types[ti];
		ImGui::PushID(ti);
		char label[64];
		snprintf(label, sizeof(label), "Type %d (%d found): %s", ti, cfg.count, cfg.material_path);
		if (ImGui::CollapsingHeader(label)) {
			ImGui::DragFloat("strength",    &cfg.strength,    0.05f, 0.f,  5.f);
			ImGui::DragFloat("radius_mult", &cfg.radius_mult, 0.05f, 0.1f, 5.f);
			ImGui::DragFloat("cooldown_dist (m)", &cfg.cooldown_dist, 0.1f, 0.1f, 20.f);
			ImGui::TextDisabled("(radius_mult scales each decal's natural XZ footprint)");
		}
		ImGui::PopID();
	}
}
ADD_TO_DEBUG_MENU(crack_debug_menu);
