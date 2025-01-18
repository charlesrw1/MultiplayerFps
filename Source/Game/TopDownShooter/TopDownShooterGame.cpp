#include "TopDownShooterGame.h"
#include "Game/Entity.h"
#include "Game/GameMode.h"
#include "Game/BasePlayer.h"
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

CLASS_H(TopDownSpawnPoint, EntityComponent)
public:
	
};
CLASS_IMPL(TopDownSpawnPoint);
CLASS_H(TopDownGameMode, GameMode)
public:
};
CLASS_IMPL(TopDownGameMode);

CLASS_H(TopDownHealthComponent,EntityComponent)
public:
	void deal_damage();

	MulticastDelegate<Entity*, int> on_take_damage;
	MulticastDelegate<Entity*> on_death;

	int max_health = 100;

	static const PropertyInfoList* get_props() {
		START_PROPS(TopDownHealthComponent)
			REG_INT(max_health, PROP_DEFAULT, "100")
		END_PROPS(TopDownHealthComponent)
	}

	int current_health = 0;
};
CLASS_IMPL(TopDownHealthComponent);


static float ground_friction = 8.2;
static float air_friction = 0.01;
static float ground_accel = 6;
static float ground_accel_crouch = 4;
static float air_accel = 3;
static float jumpimpulse = 5.f;
static float max_ground_speed = 5.7;
static float max_sprint_speed = 8.0;
static float sprint_accel = 8;
static float max_air_speed = 2;

CLASS_H(TopDownPlayer, Entity)
public:
	TopDownPlayer() {
		set_ticking(true);
		mesh = construct_sub_component<MeshComponent>("body");
		capsule = construct_sub_component<CapsuleComponent>("capsule-body");
		health = construct_sub_component<TopDownHealthComponent>("health");

		gun_entity = construct_sub_entity<Entity>("gun-entity");
		gun_entity->construct_sub_component<MeshComponent>("gun-model");
	}
	virtual void start() override {

		ccontroller = std::make_unique<CharacterController>();
		ccontroller->set_position(get_ws_position());
		ccontroller->capsule_height = capsule->height;
		ccontroller->capsule_radius = capsule->radius;

		capsule->destroy();
		capsule = nullptr;

		eng->set_game_focused(true);

		ccontroller = std::make_unique<CharacterController>();

		inputPtr = GetGInput().register_input_user(0);

		std::vector<const InputDevice*> devices;
		GetGInput().get_connected_devices(devices);
		int deviceIdx = 0;
		for (; deviceIdx < devices.size(); deviceIdx++) {
			if (devices[deviceIdx]->type == InputDeviceType::Controller) {
				inputPtr->assign_device(devices[deviceIdx]->selfHandle);
				break;
			}
		}
		if (deviceIdx == devices.size())
			inputPtr->assign_device(GetGInput().get_keyboard_device_handle());

		 GetGInput().device_connected.add(this, [&](handle<InputDevice> handle) {
				inputPtr->assign_device(handle);
			 });

		 inputPtr->on_lost_device.add(this, [&]()
			 {
				inputPtr->assign_device(GetGInput().get_keyboard_device_handle());
			});

		inputPtr->enable_mapping("game");
		inputPtr->enable_mapping("ui");

		velocity = {};

		ccontroller->set_position(glm::vec3(0,1.0,0));
	}
	virtual void end() override {
		GameInputSystem::get().free_input_user(inputPtr);
		GetGInput().device_connected.remove(this);
	}
	virtual void update() override {
		auto moveAction = inputPtr->get("game/move");
		auto lookAction = inputPtr->get("game/look");
		auto jumpAction = inputPtr->get("game/jump");
		
		auto move = moveAction->get_value<glm::vec2>();
		float len = glm::length(move);
		if(len>1.0)
			move = glm::normalize(move);
		float dt = eng->get_tick_interval();
		uint32_t flags = 0;
		glm::vec3 outvel;

		ccontroller->move(glm::vec3(move.x, 0, move.y)*move_speed * dt, dt, 0.005f, flags, outvel);

	//	auto pos = get_ws_position();
		//pos += glm::vec3(move.x, 0, move.y) * move_speed * dt;
		//set_ws_position(pos);


		set_ws_position(ccontroller->get_character_pos());

		if (flags & CCCF_BELOW)
			Debug::add_box(ccontroller->get_character_pos(), glm::vec3(0.5), COLOR_RED, 0);

		return;


		//auto move = moveAction->get_value<glm::vec2>();
		float length = glm::length(move);
		if (length > 1.0)
			move /= length;


		const bool is_sprinting = inputPtr->get("game/sprint")->get_value<bool>();


		float friction_value = ground_friction;
		float speed = glm::length(velocity);

		if (speed >= 0.0001) {
			float dropamt = friction_value * speed * eng->get_tick_interval();
			float newspd = speed - dropamt;
			if (newspd < 0)
				newspd = 0;
			float factor = newspd / speed;
			velocity.x *= factor;
			velocity.z *= factor;
		}

		glm::vec2 inputvec = glm::vec2(move.y, move.x);
		float inputlen = glm::length(inputvec);
		//if (inputlen > 0.00001)
		//	inputvec = inputvec / inputlen;
		//if (inputlen > 1)
		//	inputlen = 1;
		using namespace glm;

		glm::vec3 look_front = glm::vec3(1, 0, 0);// AnglesToVector(view_angles.x, view_angles.y);
		look_front.y = 0;
		look_front = normalize(look_front);
		glm::vec3 look_side = -normalize(cross(look_front, vec3(0, 1, 0)));

		const bool player_on_ground = true;
		float acceleation_val = (player_on_ground) ? 
			((is_sprinting) ? sprint_accel : ground_accel) :
			air_accel;
		acceleation_val =  acceleation_val;


		float maxspeed_val = (player_on_ground) ? 
			((is_sprinting) ? max_sprint_speed : max_ground_speed) :
			max_air_speed;

		vec3 wishdir = (look_front * inputvec.x + look_side * inputvec.y);
		wishdir = vec3(wishdir.x, 0.f, wishdir.z);
		vec3 xz_velocity = vec3(velocity.x, 0, velocity.z);

		float wishspeed = inputlen * maxspeed_val;
		float addspeed = wishspeed - dot(xz_velocity, wishdir);
		addspeed = glm::max(addspeed, 0.f);
		float accelspeed = acceleation_val * wishspeed * eng->get_tick_interval();
		accelspeed = glm::min(accelspeed, addspeed);
		xz_velocity += accelspeed * wishdir;

		len = glm::length(xz_velocity);
		//if (len > maxspeed)
		//	xz_velocity = xz_velocity * (maxspeed / len);
		if (len < 0.3 && accelspeed < 0.0001)
			xz_velocity = vec3(0);
		velocity = vec3(xz_velocity.x, velocity.y, xz_velocity.z);
		
		velocity.y -= 10.0 * eng->get_tick_interval();

		//uint32_t flags = 0;

		glm::vec3 out_vel;
		ccontroller->move(velocity*(float)eng->get_tick_interval(), eng->get_tick_interval(), 0.001f, flags, out_vel);
		
		velocity = out_vel;

		set_ws_position(ccontroller->get_character_pos());

		if (flags & CCCF_BELOW)
			Debug::add_box(ccontroller->get_character_pos(), glm::vec3(0.5), COLOR_RED, 0);
		if (flags & CCCF_ABOVE)
			Debug::add_box(ccontroller->get_character_pos() + glm::vec3(0, ccontroller->capsule_height, 0), glm::vec3(0.5), COLOR_GREEN, 0.5);

		auto line_start = ccontroller->get_character_pos() + glm::vec3(0, 0.5, 0);
		Debug::add_line(line_start, line_start + velocity * 3.0f, COLOR_CYAN,0);

		Debug::add_line(line_start, line_start + wishdir * 2.0f, COLOR_RED, 0);

	}
	void get_view(glm::mat4& viewMat, float& fov) {
		auto pos = get_ws_position();
		auto camera_pos = glm::vec3(pos.x, pos.y + 10.0, pos.z - 2.0);
		glm::vec3 camera_dir = glm::normalize(camera_pos - pos);


		this->view_pos =  damp_dt_independent(camera_pos, this->view_pos, 0.01, eng->get_tick_interval());

		//viewMat = glm::lookAt(camera_pos, vec3(pos.x,0,pos.z), glm::vec3(0, 1, 0));

		viewMat = glm::lookAt(this->view_pos, this->view_pos - camera_dir, glm::vec3(0, 1, 0));

		//org = camera_pos;
		//ang = view_angles;
		fov = g_fov.get_float();
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(TopDownPlayer)
			REG_FLOAT(move_speed, PROP_DEFAULT,"5.0")
		END_PROPS(TopDownPlayer)
	}

	std::unique_ptr<CharacterController> ccontroller;
	Entity* gun_entity = nullptr;
	CapsuleComponent* capsule = nullptr;
	MeshComponent* mesh = nullptr;
	TopDownHealthComponent* health = nullptr;
	InputUser* inputPtr = nullptr;

	float move_speed = 5.0;
	glm::vec3 velocity{};
	glm::vec3 view_pos{};
};
CLASS_IMPL(TopDownPlayer);

CLASS_H(TopDownPlayerController, PlayerBase)
public:
	void start() {
		auto prefab = GetAssets().find_sync<PrefabAsset>("top_down/player.pfb");
		player = (TopDownPlayer*)eng->get_level()->spawn_prefab(prefab.get());
	}
	void end() {

	}
	void get_view(glm::mat4& viewMat, float& fov) {
		player->get_view(viewMat, fov);
	}

	TopDownPlayer* player = nullptr;
};
CLASS_IMPL(TopDownPlayerController);