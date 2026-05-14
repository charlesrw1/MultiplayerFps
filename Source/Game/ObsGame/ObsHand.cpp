#include "ObsGameHeaders.h"

#include "GameEnginePublic.h"
#include "Physics/Physics2.h"
#include "Physics/ChannelsAndPresets.h"
#include "Game/GameplayStatic.h"
#include "Framework/MathLib.h"
#include "Debug.h"

#include <glm/gtc/matrix_transform.hpp>

// ============================================================
// ObsHand
// ============================================================

void ObsHand::start()
{
	// owner_player and body are wired up by ObsGameApplication::spawn_player
	// AFTER create_component<ObsHand> returns, so they are null during start().
	// tick_logic guards against null.
	set_ticking(false);
}

void ObsHand::stop()
{
	if (grab_joint) {
		grab_joint->destroy();
		grab_joint = nullptr;
	}
}

void ObsHand::update() {}

glm::vec3 ObsHand::get_hand_pos() const
{
	return get_owner()->get_ws_position();
}

// ============================================================
// Grab probe + joint management
// ============================================================

void ObsHand::try_begin_grab(const glm::vec3& probe_center)
{
	auto* app = ObsGameApplication::get();
	if (!app) return;
	if (state == ObsHandState::Holding) return;

	overlap_query_result res;
	const uint32_t mask = (1u << (int)PL::Default) | (1u << (int)PL::StaticObject);
	if (!g_physics.sphere_is_overlapped(res, app->grab_radius, probe_center, mask))
		return;

	// Find the first overlap that isn't us or our own player's capsule or the other hand.
	PhysicsBody* chosen = nullptr;
	for (int i = 0; i < res.overlaps.size(); ++i) {
		PhysicsBody* hit = res.overlaps[i];
		if (!hit) continue;
		if (hit == body) continue;
		if (owner_player && hit == owner_player->capsule) continue;
		if (owner_player && owner_player->left_hand  && hit == owner_player->left_hand->body)  continue;
		if (owner_player && owner_player->right_hand && hit == owner_player->right_hand->body) continue;
		chosen = hit;
		break;
	}
	if (!chosen) return;

	Entity* grabbed_ent = chosen->get_owner();
	if (!grabbed_ent) return;

	// Create the locked D6 joint. Defaults are all-Locked motion — start()
	// of the joint component runs immediately and creates a world-anchored
	// locked joint at the hand's current WS pose. set_target() then refreshes
	// it to anchor between hand and the grabbed entity, capturing the current
	// relative pose as the fixed offset.
	grab_joint = get_owner()->create_component<AdvancedJointComponent>();
	ASSERT(grab_joint);
	grab_joint->set_target(grabbed_ent);

	grab_world_pos = get_hand_pos();
	state = ObsHandState::Holding;
}

void ObsHand::end_grab()
{
	if (grab_joint) {
		grab_joint->destroy();
		grab_joint = nullptr;
	}
	state = ObsHandState::Idle;
}

// ============================================================
// Per-frame tick
// ============================================================

void ObsHand::tick_logic(const glm::vec3& chest_pos,
                         const glm::vec3& cam_forward,
                         const glm::vec3& cam_right,
                         const glm::vec3& cam_up,
                         float trigger_value)
{
	auto* app = ObsGameApplication::get();
	if (!app || !body || !owner_player) return;

	last_trigger = trigger_value;

	const bool want_grab = trigger_value > 0.1f;
	const bool release   = trigger_value < 0.05f;

	// --- Compute desired hand world position ---
	// Neutral shoulder rest = chest + side_offset + height_offset + small forward.
	const glm::vec3 shoulder_rest = chest_pos
		+ cam_right   * (app->shoulder_offset_x * shoulder_x_sign)
		+ cam_up      * app->shoulder_offset_y
		+ cam_forward * app->shoulder_offset_z;

	// When the trigger pulls, extend along camera-forward up to reach_distance.
	const glm::vec3 target = shoulder_rest
		+ cam_forward * (app->reach_distance * trigger_value);
	last_target_pos = target;

	// --- State transitions ---
	if (state == ObsHandState::Holding) {
		if (release) end_grab();
	}
	else {
		// Idle or Reaching.
		state = want_grab ? ObsHandState::Reaching : ObsHandState::Idle;
	}

	// --- Drive hand body ---
	if (state == ObsHandState::Holding) {
		// Joint pins the hand; no force needed.
		return;
	}

	// PD force toward target.
	const glm::vec3 hand_pos = get_hand_pos();
	const glm::vec3 hand_vel = body->get_linear_velocity();
	const glm::vec3 to_target = target - hand_pos;
	const glm::vec3 force = app->arm_stiffness * to_target - app->arm_damping * hand_vel;
	// PhysicsBody::apply_force(point_world, force_world) — we apply at COM (hand_pos).
	body->apply_force(hand_pos, force);

	// --- Probe for grab while reaching ---
	if (state == ObsHandState::Reaching) {
		try_begin_grab(hand_pos);
	}
}
