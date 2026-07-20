// BikeApplication_PackAI.cpp
// Tactical AI decisions: wheel-picking and paceline FSM (Following / Pulling).
// Peeling off and drifting back are not scripted states here — they're an emergent
// result of the magnetism layer (update_wheel_picking skipping recovering riders as
// wheel candidates, update_clear_air/update_avoidance treating them as a normal
// obstacle once demoted). See [[bike/bikeai#Magnetism]].
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

			// Skip riders still recovering from a pull — they're actively shedding pace,
			// so following them just yanks the chain off the line. Once recovering_s
			// hits 0 they're a normal candidate again (by which point they've usually
			// fallen back in the order naturally).
			if (BikeAI* oai = dynamic_cast<BikeAI*>(other->input.get())) {
				if (oai->recovering_s > 0.f) continue;
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
// Paceline tactical FSM — Following / Pulling.
//
// Wheel-picker is the "what wheel am I on" decision. This is "what am I doing
// strategically" — it decides when to take a pull and for how long, and starts
// the recovery window when a pull ends. It does NOT script the peel/drift-back
// motion: once a rider is recovering, update_wheel_picking refuses to let anyone
// draft them and update_clear_air/update_avoidance treat them as a normal
// obstacle, so the fall-back and the following riders flicking around them
// happen as an emergent result of the magnetism layer, not a forced lat_offset
// or timer-driven drift. See [[bike/bikeai#Magnetism]].
//
// Cascade-safe promotion: Pulling riders accelerate, so the gap to the next
// rider can exceed wheel_long_max within seconds. is_at_front() walks ALL
// riders in the same group with no distance cutoff (only recovering riders
// are skipped) — without this, every following rider eventually self-promotes
// when its puller pulls away.
//
// Deliberately does NOT use riders_sorted array position as "who's ahead" —
// two riders can be laterally beside each other with a nearly-tied (or just
// noisy) course_dist_m sort order that means nothing about front-status.
// Instead it does an explicit numeric course_dist_m comparison plus a lateral
// window, over every group member: only a rider roughly in my line who is
// actually further along blocks me from being "at the front."
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
		for (BikeObject* other : riders_sorted) {
			if (other == me) continue;
			if (other->group_id != me->group_id) continue;
			if (glm::abs(other->lateral_pos - me->lateral_pos) > p.wheel_lat_max) continue;
			if (other->course_dist_m <= me->course_dist_m) continue;  // not actually ahead of me
			BikeAI* oai = dynamic_cast<BikeAI*>(other->input.get());
			if (!oai) return false;  // player counts as a stable wheel
			if (oai->recovering_s > 0.f) continue;
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
		if (ai->recovering_s > 0.f)
			ai->recovering_s = glm::max(0.f, ai->recovering_s - dt);

		const bool at_front = is_at_front(i);

		switch (ai->paceline_state) {
		case PacelineState::Following:
			// Becoming a leader by emergence triggers a pull (unless still cooling down).
			if (at_front && ai->pull_cooldown_s <= 0.f) {
				ai->paceline_state    = PacelineState::Pulling;
				ai->paceline_timer_s  = 0.f;
				// Randomized per-pull duration — avoids a metronomic rotation without
				// needing the stamina system wired in.
				const float t = (float)rand() / (float)RAND_MAX;
				ai->pull_duration_roll = glm::mix(p.pull_duration_min_s, p.pull_duration_max_s, t);
			}
			break;

		case PacelineState::Pulling:
			// Pull until the rolled duration elapses, then end the pull and start
			// recovering. If a stable rider appears ahead (someone moved up), drop
			// back to Following immediately — no recovery penalty, the pull never
			// really happened.
			if (!at_front) {
				ai->paceline_state   = PacelineState::Following;
				ai->paceline_timer_s = 0.f;
			} else if (ai->paceline_timer_s >= ai->pull_duration_roll) {
				ai->paceline_state   = PacelineState::Following;
				ai->paceline_timer_s = 0.f;
				ai->recovering_s     = p.recovery_duration_s;
				ai->pull_cooldown_s  = p.pull_cooldown_s;
			}
			break;
		}
	}
}
