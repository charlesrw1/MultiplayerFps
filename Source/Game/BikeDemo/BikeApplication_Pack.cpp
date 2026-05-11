// BikeApplication_Pack.cpp
// Pack dynamics: grouping, drafting, gap regulation, and shared avoidance
// constants (externed by BikeApplication_PackAvoid.cpp and BikeApplication_PackDebug.cpp).
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
#include "Debug.h"
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
	ASSERT(!riders_sorted.empty() || true);  // empty pack is valid — early return below
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
	ASSERT(!riders_sorted.empty() || true);  // empty pack is valid
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
// Pack / gap constants (definitions — externed by PackAvoid and PackDebug)
// ============================================================

// Longitudinal power yield when side-by-side: the slightly-behind rider sheds watts
float SIDE_BY_SIDE_LONG_M   = 2.5f;   // longitudinal range to count as abreast (m)
float SIDE_BY_SIDE_LAT_M    = 1.0f;   // lateral range to count as abreast (m)
float SIDE_BY_SIDE_POWER_W  = 60.f;   // max W shed / gained

// Predictive inter-rider avoidance (soft steer push, truly side-by-side only)
// Only fires when riders are nearly abreast (long_gap < AVOID_LONG_MAX).
// Weighted by (1 - long_gap / AVOID_LONG_MAX) so the push fades longitudinally.
float AVOID_LONG_MAX     = 3.5f;   // m — longitudinal range; beyond this = single file, no push
float AVOID_BIKE_HALF_W  = 0.38f;  // m — half shoulder width
float AVOID_CLEARANCE    = 0.25f;  // m — additional margin beyond bike width
float AVOID_PREDICT_T1   = 0.5f;   // s — first prediction horizon
float AVOID_PREDICT_T2   = 1.0f;   // s — second prediction horizon
float AVOID_STEER_KP     = 0.8f;   // additive steer per m of predicted overlap (soft nudge)

// Priority yield (hard enforcement — lower-priority yielder only)
// Lower index in riders_sorted = further ahead = higher priority.
// The yielder (higher index) is forbidden from steering into the higher-priority rider's zone.
// Sign convention fix vs. old HARD_SEP: positive steer = road-LEFT, negative = road-RIGHT.
//   Other road-right → block road-right movement → block negative steer → hard_steer_min = 0
//   Other road-left  → block road-left movement  → block positive steer → hard_steer_max = 0
float YIELD_LONG_RADIUS  = 3.5f;   // m — longitudinal range for yield clamp
float YIELD_OUTER_LAT    = 1.3f;   // m — engage clamp inside this lateral gap
float YIELD_INNER_LAT    = 0.05f;  // m — disengage when already overlapping (escape)
float YIELD_SQUEEZE_M    = 0.35f;  // m — available road width below this triggers brake
float YIELD_BRAKE_K      = 0.55f;  // brake fraction when fully squeezed

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
	ASSERT(!all_riders.empty() || true);  // empty pack is valid
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
