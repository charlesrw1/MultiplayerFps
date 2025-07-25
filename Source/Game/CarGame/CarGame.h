#pragma once
#include "Framework/Reflection2.h"
#include "Game/Entity.h"
#include "Game/Components/CameraComponent.h"
#include "Game/EntityComponent.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Framework/MulticastDelegate.h"
#include "Game/Entities/CharacterController.h"
#include "Debug.h"
#include "Input/InputSystem.h"
#include "Framework/MathLib.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Assets/AssetDatabase.h"
#include "Game/LevelAssets.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/AnimationSeqAsset.h"
#include "imgui.h"
#include "Physics/Physics2.h"

#include "./Sound/SoundComponent.h"

/// <summary>
//
/// </summary>

class CarGameMode : public Entity
{
public:
	CLASS_BODY(CarGameMode);

	static CarGameMode* instance;
	void pre_start() {
		ASSERT(!instance);
		instance = this;
	}
	void stop() {
		instance = nullptr;
	}
	Entity* thecar = nullptr;

	REFLECT();
	MulticastDelegate<> on_player_damaged;
};


class WheelComponent : public Component
{
public:
	CLASS_BODY(WheelComponent);
	REF bool front = false;
	REF bool left = false;
};
class CarComponent : public Component
{
public:
	CLASS_BODY(CarComponent);

	CarComponent() {
		memset(wheels, 0, sizeof(wheels));
	}
	void start() {
		body = get_owner()->get_component<PhysicsBody>();
		for (auto c : get_owner()->get_children()) {
			auto w = c->get_component<WheelComponent>();
			if (w) {
				int index = (!w->front) * 2 + (!w->left);
				wheels[index] = w;
				localspace_wheel_pos[index] = w->get_owner()->get_ls_position();
			}
		}
		sys_print(Debug, "BODY MASS:  %f\n", body->get_mass());
		set_ticking(true);

		for (int i = 0; i < 4; i++) {
			last_dist[i] = 0;
			last_ws_pos[i] = glm::vec3(0.f);
			visual_wheel_dist[i] = 0.f;
			wheel_angles[i] = 0;
		}
	}
	void stop() {
	}
	void update() override;

	float tire_screech_amt = 0.0;

	float forward_force = 0.f;
	float steer_angle = 0.0;
	float brake_force = 0.0;

	PhysicsBody* body = nullptr;
	WheelComponent* wheels[4];
	glm::vec3 localspace_wheel_pos[4];
	glm::vec3 last_ws_pos[4];
	float last_dist[4];
	float wheel_angles[4];

	float visual_wheel_dist[4];

	// 0 = left front, 1 = right front
	// 2 = left back, 3 = right back
};

class CarSoundMaker : public Component
{
public:
	CLASS_BODY(CarSoundMaker);

	CarSoundMaker() : r(4052347) {}

	REF AssetPtr<PrefabAsset> engineSoundAsset;
	REF AssetPtr<PrefabAsset> tireSoundAsset;

	SoundComponent* engineSound{};
	SoundComponent* tireSound{};
	CarComponent* car = nullptr;


	void start() {
		car = get_owner()->get_component<CarComponent>();
		{
			// spawn_prefab(engineAsset)->get_comp<SoundComponent>()
			// e = spawn_prefab(tireAsset)
			// e->get_children()
			// e->get_comp<>()
			// e->add_comp<>()
			// e->parent_to()
			// e->get_parent()
			// 
			// spawn_prefab_deferred()
			// 
			// 
			// get_owner()->parent_to(get_owner())

			engineSound = eng->get_level()->spawn_prefab(engineSoundAsset.get())->get_component<SoundComponent>();
			tireSound = eng->get_level()->spawn_prefab(tireSoundAsset.get())->get_component<SoundComponent>();
			engineSound->get_owner()->parent_to(get_owner());
			tireSound->get_owner()->parent_to(get_owner());

		}
		engineSound->set_play(true);
		set_ticking(true);
	}
	void update();
	Random r;
	bool is_squel = false;
	float squel_start = 0.0;
	float cur_pitch = 1.0;
};

class CarDriver : public Component
{
public:
	CLASS_BODY(CarDriver);


	void start() override {
	
		auto camobj = eng->get_level()->spawn_entity();
		camera = camobj->create_component<CameraComponent>();
		camera->set_is_enabled(true);

		car = get_owner()->get_component<CarComponent>();
		set_ticking(true);
	}
	//
	void update();
	void stop() {
	}

	float set_steer_angle = 0.0;

	glm::vec3 camera_pos = glm::vec3(0.f);
	glm::quat cam_dir = glm::quat();

	bool top_view = false;
	CarComponent* car = nullptr;
	CameraComponent* camera = nullptr;
};