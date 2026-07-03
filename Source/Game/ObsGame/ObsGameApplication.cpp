#include "ObsGameHeaders.h"

#include "Game/GameplayStatic.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Game/Entities/CharacterController.h"
#include "Physics/ChannelsAndPresets.h"
#include "Render/Model.h"
#include "Framework/MathLib.h"
#include "Debug.h"

#include <glm/gtc/matrix_transform.hpp>

ObsGameApplication* ObsGameApplication::instance = nullptr;

// ============================================================
// Level construction
// ============================================================

namespace {

// Spawn a static BoxComponent of the given world-space size, centered at pos.
// BoxComponent half-extents come from owner->get_ls_scale() * 0.5 (see
// Source/Game/Components/PhysicsShapes.cpp:39-42).
static Entity* spawn_static_box(glm::vec3 center, glm::vec3 size, PL layer = PL::StaticObject)
{
	Entity* e = GameplayStatic::spawn_entity();
	e->set_ws_position(center);
	e->set_ls_scale(size);
	auto* bc = e->create_component<BoxComponent>();
	bc->set_body_type(BodyType::Static);
	bc->set_physics_layer(layer);
	return e;
}

// Spawn a static SphereComponent of the given radius at pos.
static Entity* spawn_static_sphere(glm::vec3 center, float radius, PL layer = PL::StaticObject)
{
	Entity* e = GameplayStatic::spawn_entity();
	e->set_ws_position(center);
	auto* sc = e->create_component<SphereComponent>();
	sc->set_body_type(BodyType::Static);
	sc->set_physics_layer(layer);
	sc->set_radius(radius);
	return e;
}

// Spawn a static trigger box at center with size; on_trigger fires
// the supplied callback with the overlapping entity.
template <typename F>
static Entity* spawn_trigger_box(glm::vec3 center, glm::vec3 size, F&& on_overlap)
{
	Entity* e = GameplayStatic::spawn_entity();
	e->set_ws_position(center);
	e->set_ls_scale(size);
	auto* bc = e->create_component<BoxComponent>();
	bc->set_body_type(BodyType::Static);
	bc->set_is_trigger(true);
	//bc->set_send_overlap(true);
	//bc->on_trigger.add(e, [cb = std::forward<F>(on_overlap)](PhysicsBodyEventArg arg) {
	//	if (arg.entered_trigger && arg.who.get())
	//		cb(arg.who.get());
	//});
	return e;
}

} // namespace

void ObsGameApplication::build_level()
{
	eng->load_level("obstacle_game_level.tmap");	// empty level

	// +Y up, -Z forward (player spawns facing -Z and climbs deeper into -Z).

	// 1. Floor — 20 x 0.5 x 20 at y=-0.25 so top sits at y=0.
	spawn_static_box(glm::vec3(0.f, -0.25f, 0.f), glm::vec3(20.f, 0.5f, 20.f));

	// 2. Box A — low climb, top at y=1.
	spawn_static_box(glm::vec3(0.f, 0.5f, -3.f), glm::vec3(1.f, 1.f, 1.f));

	// 3. Box B — tall climb, top at y=2.
	spawn_static_box(glm::vec3(0.f, 1.f, -6.f), glm::vec3(2.f, 2.f, 2.f));

	// 4. Ledge mantle — thin overhanging shelf at y=3.
	spawn_static_box(glm::vec3(0.f, 3.f, -9.f), glm::vec3(4.f, 0.2f, 0.6f));

	// 5. Ladder — 5 sphere rungs at z=-12, y stepping 0.5..2.5.
	for (int i = 0; i < 5; ++i) {
		const float y = 0.5f + 0.5f * i;
		spawn_static_sphere(glm::vec3(0.f, y, -12.f), 0.08f);
	}

	// 6. Goal trigger — wide platform at y=5, z=-15. Overlap fires on_goal_reached.
	spawn_trigger_box(glm::vec3(0.f, 5.f, -15.f), glm::vec3(2.f, 0.4f, 2.f),
		[this](Entity* /*who*/) { on_goal_reached(); });
}

// ============================================================
// Player composition
// ============================================================

void ObsGameApplication::spawn_player(glm::vec3 pos)
{
	// Chest entity — capsule body driven by CharacterController.
	Entity* chest = GameplayStatic::spawn_entity();
	chest->set_ws_position(pos + glm::vec3(0.f, capsule_height * 0.5f + 0.1f, 0.f));

	auto* cap = chest->create_component<CapsuleComponent>();
	cap->set_data(capsule_height, capsule_radius, 0.f);
	cap->set_body_type(BodyType::Kinematic);
	cap->set_physics_layer(PL::Character);

	auto* mov = chest->create_component<CharacterMovementComponent>();
	mov->set_capsule_info(capsule_height, capsule_radius, 0.05f);
	mov->set_physics_body(cap);
	mov->set_position(chest->get_ws_position());

	player = chest->create_component<ObsPlayer>();
	player->capsule  = cap;
	player->movement = mov;

	// --- Hand entities ---
	auto spawn_hand = [&](float x_sign) -> ObsHand* {
		Entity* h = GameplayStatic::spawn_entity();
		const glm::vec3 hand_pos = chest->get_ws_position()
			+ glm::vec3(shoulder_offset_x * x_sign, shoulder_offset_y, -shoulder_offset_z);
		h->set_ws_position(hand_pos);

		auto* sph = h->create_component<SphereComponent>();
		sph->set_body_type(BodyType::Dynamic);
		sph->set_radius(hand_radius);
		sph->set_density(hand_density);
		sph->set_physics_layer(PL::Character);

		auto* hand = h->create_component<ObsHand>();
		hand->owner_player    = player;
		hand->shoulder_x_sign = x_sign;
		hand->body            = sph;
		return hand;
	};
	player->left_hand  = spawn_hand(-1.f);
	player->right_hand = spawn_hand(+1.f);

	// --- Camera entity ---
	Entity* cam_ent = GameplayStatic::spawn_entity();
	camera = cam_ent->create_component<ObsCamera>();
	auto* cc = cam_ent->create_component<CameraComponent>();
	cc->set_is_enabled(true);
	camera->cc = cc;
}

// ============================================================
// Lifecycle
// ============================================================

void ObsGameApplication::start()
{
	instance = this;
	reached_goal = false;

	build_level();
	spawn_player(glm::vec3(0.f, 0.f, 0.f));

	sys_print(Info, "ObsGame: level built, player spawned at origin\n");
}

void ObsGameApplication::update()
{
	GameplayStatic::reset_debug_text_height();
	if (reached_goal) {
		GameplayStatic::debug_text(std::string("Goal reached!"));
	}
}

void ObsGameApplication::on_goal_reached()
{
	if (reached_goal) return;
	reached_goal = true;
	sys_print(Info, "ObsGame: goal reached!\n");
}
