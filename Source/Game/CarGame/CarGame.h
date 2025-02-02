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
#include "Input/InputAction.h"
#include "Framework/MathLib.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Assets/AssetDatabase.h"
#include "Game/LevelAssets.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/AnimationSeqAsset.h"
#include "imgui.h"
#include "Physics/Physics2.h"
#include "Game/AssetPtrMacro.h"
#include "./Sound/SoundComponent.h"

class PhysicsComponentBase;

NEWCLASS(HelloWorld, EntityComponent)
public:
	void start() {

	}
	void update() {

	}
	// xyz
	int i = 0;
	REFLECT();
	AssetPtr<PrefabAsset> myprefab;
	REFLECT();
	AssetPtr<Model> mymodel;

	REFLECT()
	float start_velocity = 0.0;
	REFLECT(type="BoolButton",transient,hint="Press Me")
	bool enable_xyz = false;

	REFLECT()
	EntityPtr otherEnt;
	REFLECT(getter)
	Entity* get_exyz() {
		return otherEnt.get();
	}

	REFLECT(transient,hide)
	AssetPtr<SoundFile> theSound;
};


NEWCLASS(CarGameMode, Entity)
public:
	static CarGameMode* instance;
	void pre_start() {
		ASSERT(!instance);
		instance = this;
	}
	void end() {
		instance = nullptr;
	}
	Entity* thecar = nullptr;

	REFLECT();
	MulticastDelegate<> on_player_damaged;
};

NEWCLASS(WheelComponent, EntityComponent)
public:
	REFLECT();
	bool front = false;
	REFLECT();
	bool left = false;
};


NEWCLASS(CarComponent, EntityComponent)
public:
	CarComponent() {
		memset(wheels, 0, sizeof(wheels));
	}
	void start() {
		body = get_owner()->get_first_component<PhysicsComponentBase>();
		for (auto c : get_owner()->get_all_children()) {
			auto w = c->get_first_component<WheelComponent>();
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
	void end() {
	}
	void update() override;

	float tire_screech_amt = 0.0;

	float forward_force = 0.f;
	float steer_angle = 0.0;
	float brake_force = 0.0;

	PhysicsComponentBase* body = nullptr;
	WheelComponent* wheels[4];
	glm::vec3 localspace_wheel_pos[4];
	glm::vec3 last_ws_pos[4];
	float last_dist[4];
	float wheel_angles[4];

	float visual_wheel_dist[4];

	// 0 = left front, 1 = right front
	// 2 = left back, 3 = right back
};


NEWCLASS(CarSoundMaker, EntityComponent)
public:
	CarSoundMaker() : r(4052347) {}

	REFLECT();
	AssetPtr<PrefabAsset> engineSoundAsset;
	REFLECT();
	AssetPtr<PrefabAsset> tireSoundAsset;

	SoundComponent* engineSound{};
	SoundComponent* tireSound{};

	CarComponent* car = nullptr;
	void start() {
		car = get_owner()->get_first_component<CarComponent>();
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

			engineSound = eng->get_level()->spawn_prefab(engineSoundAsset.get())->get_first_component<SoundComponent>();
			tireSound = eng->get_level()->spawn_prefab(tireSoundAsset.get())->get_first_component<SoundComponent>();
			engineSound->get_owner()->parent_to_entity(get_owner());
			tireSound->get_owner()->parent_to_entity(get_owner());

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

NEWCLASS(CarDriver, EntityComponent)
public:
	void start() override {
		inputUser = GameInputSystem::get().register_input_user(0);
		inputUser->assign_device(GameInputSystem::get().get_keyboard_device());
		inputUser->enable_mapping("game");
		auto camobj = eng->get_level()->spawn_entity_class<Entity>();
		camera = camobj->create_and_attach_component_type<CameraComponent>();
		camera->set_is_enabled(true);
		move = inputUser->get("game/move");
		accel = inputUser->get("game/accelerate");
		brake = inputUser->get("game/deccelerate");

		car = get_owner()->get_first_component<CarComponent>();
		set_ticking(true);

		inputUser->get("game/jump")->on_start.add(
			this, [&]()
			{
				top_view = !top_view;
			}
		);
		for (auto d : GetGInput().get_connected_devices())
			if (d->get_type() == InputDeviceType::Controller) {
				inputUser->assign_device(d);
				break;
			}
	}
	//
	void update();
	void end() {
		inputUser->destroy();
	}

	float set_steer_angle = 0.0;

	glm::vec3 camera_pos = glm::vec3(0.f);
	glm::quat cam_dir = glm::quat();

	bool top_view = false;
	CarComponent* car = nullptr;
	CameraComponent* camera = nullptr;
	InputActionInstance* move = nullptr;
	InputActionInstance* accel = nullptr;
	InputActionInstance* brake = nullptr;

	InputUser* inputUser = nullptr;
};