// BikeApplication_Pack.cpp
// Pack dynamics: grouping and drafting. All functions are methods of
// BikeGameApplication (declared in BikeHeaders.h). Wheel-picking, paceline
// FSM, clear-air resolver, and priority-yield have been removed — magnetism
// (BikeAI::evaluate) is the only positioning layer now. See [[bike/bikeai]].
#include "BikeHeaders.h"

#include "Game/GameplayStatic.h"
#include "Framework/MathLib.h"
#include "Render/DrawPublic.h"
#include "Debug.h"
#include <algorithm>

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

// riders_sorted is used only as a search-order optimization here (front-to-back
// so we can early-exit after DRAFT_STACK_CHECK candidates) — every benefit
// decision below is gated by real course_dist_m/lateral_pos gaps, never by
// array index adjacency. See [[bike/bikeai#Ground rule]].
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
