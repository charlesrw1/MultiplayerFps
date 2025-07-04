#pragma once

#include "DefaultGameHeader.h"
#include "Game/Components/CameraComponent.h"
#include "Input/InputSystem.h"
#include "Input/InputAction.h"
#include "Sound/SoundComponent.h"
#include "Game/Components/PhysicsComponents.h"

class TPPlayer;

class TPGameMode : public Component
{
public:
	CLASS_BODY(TPGameMode);

	static TPGameMode* instance;
	static TPGameMode& get() {
		ASSERT(instance);
		return *instance;
	}

	void pre_start() override {
		
	}
	void start() override {
		player = class_cast<TPPlayer>(eng->get_level()->spawn_prefab(playerPrefab));
		ASSERT(player);

		{
			auto ent = eng->get_level()->spawn_entity();
			game_camera = ent->create_component<CameraComponent>();
			game_camera->set_is_enabled(true);	// enable it
		}
	}

	REF AssetPtr<PrefabAsset> playerPrefab;
	REF TPPlayer* get_player() {
		return player;
	}

	CameraComponent* game_camera = nullptr;
	TPPlayer* player = nullptr;
};



class TPGun
{
public:
	TPPlayer* owner = nullptr;

	void try_shoot_gun();
	float shoot_cooldown = 0.0;
};

class TPPlayer : public Component
{
public:
	CLASS_BODY(TPPlayer);

	REF AssetPtr<AnimationSeqAsset> jump_seq;
	REF AssetPtr<AnimationSeqAsset> idle_to_run_seq;
	REF AssetPtr<AnimationSeqAsset> run_to_idle_seq;
	REF AssetPtr<PrefabAsset> projectile;
	REF AssetPtr<PrefabAsset> shotgunSound;


	CharacterController cc;
	glm::vec3 velocity = glm::vec3(0.f);

	CapsuleComponent* capsule = nullptr;

	void try_shoot_gun() {
		
	}

	void start() override {
		cc.set_position(get_ws_position());
	

		capsule = get_owner()->get_component<CapsuleComponent>();
		ASSERT(capsule);
	}

	void update() override {

	}

	void stop() override {

	}
};


class TPProjectile : public Component
{
public:
	CLASS_BODY(TPProjectile);

	void start() override {
		set_ticking(true);
		time_created = eng->get_game_time();
	}
	void update() override {

		if (eng->get_game_time() - time_created >= 3.0) {
			get_owner()->destroy();
			return;
		}

		auto pos = get_ws_position();
		auto newpos = pos + direction * speed * (float)eng->get_dt();
		world_query_result res;
		TraceIgnoreVec ig;
		ig.push_back(ignore);

		g_physics.trace_ray(res, pos, newpos, &ig, (1 << (int)PL::Character));

		if (res.component) {
			//auto owner = res.component->get_owner()->get_component<TopDownHealthComponent>();
			//if (owner) {
				//DamageEvent dmg;
				//dmg.amt = damage;
				//dmg.position = pos;
				//dmg.dir = direction;
				//
				//owner->deal_damage(dmg);
				//
				//get_owner()->destroy();
			//	return;
			//}
		}
		res = world_query_result();
		g_physics.trace_ray(res, pos, newpos, &ig, (1 << (int)PL::Default) | (1 << (int)PL::StaticObject));
		if (res.component) {
			get_owner()->destroy();
			return;
		}

		get_owner()->set_ws_position(newpos);
	}

	float time_created = 0.0;
	PhysicsBody* ignore = nullptr;
	glm::vec3 direction = glm::vec3(1, 0, 0);
	REFLECT();
	float speed = 10.0;
	REFLECT();
	int damage = 10;
};

inline void TPGun::try_shoot_gun()
{
#if 0
	if (shoot_cooldown <= 0.0)
	{
		Random r(eng->get_game_time());

		int count = 5;
		for (int i = 0; i < count; i++) {

			auto projectile = eng->get_level()->spawn_prefab(owner->projectile);
			auto pc = projectile->get_component<TPProjectile>();
			pc->ignore = owner->get_owner()->get_component<PhysicsComponentBase>();
			const float spread = 0.15;
			pc->direction = lookdir + glm::vec3(r.RandF(-spread, spread), 0, r.RandF(-spread, spread));
			if (using_third_person_movement)
				pc->direction = get_front_dir();
			else
				glm::normalize(pc->direction);
			pc->speed += r.RandF(-2, 2);

			glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), pc->direction));
			glm::vec3 up = glm::cross(pc->direction, right);
			glm::mat3 rotationMatrix(-pc->direction, up, right);

			pc->get_owner()->set_ws_transform(get_ws_position() + glm::vec3(0, 0.5, 0), glm::quat_cast(rotationMatrix), pc->get_owner()->get_ls_scale());
		}

		shake.start(0.08);
		shoot_cooldown = 0.8;

		cachedShotgunSound->play_one_shot_at_pos(get_ws_position());

	}
#endif
}