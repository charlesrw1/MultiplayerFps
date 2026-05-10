// BikeApplication_PackAI.cpp
// Tactical AI decisions: wheel-picking and paceline FSM
// (Following / Pulling / Peeling / DriftingBack).
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

// ============================================================
// Wheel picking — choose the rider directly ahead I'm following.
// Score candidates in (group, ahead by [long_min, long_max], lateral overlap) and
// pick the highest. Sets BikeAI::wheel each frame; null = leader.
// See [[bike/bikeai#Wheel picking]].
// ============================================================
void BikeGameApplication::update_wheel_picking()
{
	ASSERT(!riders_sorted.empty() || true);  // empty pack is valid
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
	ASSERT(eng != nullptr);
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
