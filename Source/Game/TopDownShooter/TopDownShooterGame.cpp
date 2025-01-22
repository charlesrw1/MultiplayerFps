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

struct DamageEvent
{
	int amt = 0;
	glm::vec3 position;
	glm::vec3 dir;
};

CLASS_H(TopDownHealthComponent,EntityComponent)
public:
	void deal_damage(DamageEvent dmg) {
		current_health -= dmg.amt;
		if (current_health <= 0)
			on_death.invoke(dmg);
	}

	MulticastDelegate<Entity*, int> on_take_damage;
	MulticastDelegate<DamageEvent> on_death;

	int max_health = 100;

	void on_init() {
		current_health = max_health;
	}

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
#include "Physics/Physics2.h"

CLASS_H(ProjectileComponent,EntityComponent)
public:
	void on_init() override {
		if (eng->is_editor_level()) return;
		set_ticking(true);
		time_created = eng->get_game_time();
	}
	void update() override {

		if (eng->get_game_time() - time_created >= 3.0) {
			get_owner()->destroy();
			return;
		}

		auto pos = get_ws_position();
		auto newpos = pos +  direction * speed * (float)eng->get_tick_interval();
		world_query_result res;
		TraceIgnoreVec ig;
		ig.push_back(ignore);

		g_physics.trace_ray(res, pos, newpos, &ig, (1 << (int)PL::Character));

		if (res.component) {
			auto owner = res.component->get_owner()->get_first_component<TopDownHealthComponent>();
			if (owner) {
				DamageEvent dmg;
				dmg.amt = damage;
				dmg.position = pos;
				dmg.dir = direction;

				owner->deal_damage(dmg);

				get_owner()->destroy();
				return;
			}
		}
		res = world_query_result();
		g_physics.trace_ray(res, pos, newpos, &ig, (1 << (int)PL::Default) | (1 << (int)PL::StaticObject));
		if (res.component) {
			get_owner()->destroy();
			return;
		}

		get_owner()->set_ws_position(newpos);
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(ProjectileComponent)
			REG_INT(damage, PROP_DEFAULT,"10"),
			REG_FLOAT(speed, PROP_DEFAULT,"10.0")
		END_PROPS(ProjectileComponent)
	}

	float time_created = 0.0;
	PhysicsComponentBase* ignore = nullptr;
	glm::vec3 direction = glm::vec3(1,0,0);
	float speed = 10.0;
	int damage = 10;
};
CLASS_IMPL(ProjectileComponent);

CLASS_H(TopDownEnemyComponent, EntityComponent)
public:
	void on_init() {
		if (eng->is_editor_level()) return;
		auto health = get_owner()->get_first_component<TopDownHealthComponent>();
		ASSERT(health);
		set_ticking(false);
		health->on_death.add(this, [&](DamageEvent dmg)
			{
				set_ticking(true);
				death_time = eng->get_game_time();

				auto cap = get_owner()->get_first_component<PhysicsComponentBase>();
				cap->set_is_simulating(true);
				cap->apply_impulse(dmg.position+glm::vec3(0,0.3,0), dmg.dir*5.0f);
			});
	}
	void update() {
		if (eng->get_game_time() - death_time > 3.0) {
			get_owner()->destroy();
		}
	}
	bool is_dead = false;
	float death_time = 0.0;
};
CLASS_IMPL(TopDownEnemyComponent);

glm::vec3 unproject_mouse_to_ray(const View_Setup& vs, const int mx, const int my)
{
	Ray r;

	auto size = eng->get_game_viewport_size();

	glm::vec3 ndc = glm::vec3(float(mx) / size.x, float(my) / size.y, 0);
		ndc = ndc * 2.f - 1.f;
		ndc.y *= -1;
	{
		r.pos = vs.origin;
		glm::mat4 invviewproj = glm::inverse(vs.viewproj);
		glm::vec4 point = invviewproj * glm::vec4(ndc, 1.0);
		point /= point.w;

		glm::vec3 dir = glm::normalize(glm::vec3(point) - r.pos);

		r.dir = dir;
	}
	return r.dir;
}

class CameraShake
{
public:
	glm::vec3 evaluate(glm::vec3 pos, glm::vec3 front, float dt) {
		if (time < total_time) {
			glm::vec3 left = glm::normalize( glm::cross(front, glm::vec3(0, 1, 0)) );
			glm::vec3 up = glm::cross(left, front);
			float amt = intensity*eval_func(time*10.0) * eval_mult(time);
			printf("%f\n", amt);
			time += dt;
			return pos + (left * dir.x + up*dir.y)*amt;
		}
		else
			return pos;


	}
	void start(float intensity=0.1) {
		this->intensity = intensity;
		time = 0.0;
	}
	float eval_func(float t) {
		return sin(1.1 * t + 0.1) + 2.0*sin(2.6 * t - 0.8) + sin(0.1 * t + 0.2) + 3.0* sin(0.6 * t - 1.0);
	}
	float eval_mult(float t) {
		if (t <= fade_in_time) return t / fade_in_time;
		else if (t>= total_time - fade_out_time) return 1.0 -( (t-(total_time-fade_out_time)) / fade_out_time );
		return 1.0;
	}
	glm::vec2 dir = glm::vec2(0.5, -0.5);
	float intensity = 0.1;
	float total_time = 0.25;
	float fade_in_time = 0.02;
	float fade_out_time = 0.02;
	float time = 0.0;
};


class TopDownPlayer;
CLASS_H(TopDownCar, EntityComponent)
public:
	void on_init() {

	}
	void update();
	void enter_car(TopDownPlayer* player) {
		driver = player;
	}

	glm::vec3 front=glm::vec3(1,0,0);
	glm::vec2 velocity{};
	TopDownPlayer* driver = nullptr;
};
CLASS_IMPL(TopDownCar);

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

		ccontroller = std::make_unique<CharacterController>(capsule);
		ccontroller->set_position(get_ws_position());
		ccontroller->capsule_height = capsule->height;
		ccontroller->capsule_radius = capsule->radius;

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
		inputPtr->get("game/shoot")->bind_active_function(
			[&]() {
				if (shoot_cooldown <= 0.0)
				{
					Random r(eng->get_game_tick());
					
					int count = 5;
					for (int i = 0; i < count; i++) {

						auto projectile = eng->get_level()->spawn_prefab(GetAssets().find_sync<PrefabAsset>("top_down/projectile.pfb").get());
						auto pc = projectile->get_first_component<ProjectileComponent>();
						pc->ignore = capsule;
						const float spread = 0.15;
						pc->direction = lookdir+glm::vec3(r.RandF(-spread,spread),0,r.RandF(-spread,spread));
						pc->direction = glm::normalize(pc->direction);
						pc->speed += r.RandF(-2, 2);

						glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0,1,0), pc->direction));
						glm::vec3 up = glm::cross(pc->direction, right);
						glm::mat3 rotationMatrix(-pc->direction, up, right);

						pc->get_owner()->set_ws_transform(get_ws_position() + glm::vec3(0, 0.5, 0),glm::quat_cast(rotationMatrix),pc->get_owner()->get_ls_scale());
					}
					shake.start(0.04);
					shoot_cooldown = 0.3;
				}
			}
		);
		//inputPtr->get("game/use")->bind_triggered_function(
		//	[&]() {
		//		if (is_in_car) {
		//			world_query_result res;
		//			TraceIgnoreVec ig;
		//			ig.push_back(capsule);
		//			g_physics.trace_ray(res, get_ws_position() + glm::vec3(0, 0.5, 0), lookdir, 2.0, &ig, UINT32_MAX);
		//			if (res.component) {
		//				auto car = res.component->get_owner()->get_first_component<TopDownCar>();
		//				if (car)
		//					enter_car(car);
		//			}
		//		}
		//		else {
		//			exit_car();
		//		}
		//
		//	}
		//);

		velocity = {};

		ccontroller->set_position(glm::vec3(0,0.0,0));
	}
	virtual void end() override {
		GameInputSystem::get().free_input_user(inputPtr);
		GetGInput().device_connected.remove(this);
	}
	void exit_car() {
		is_in_car = false;
		mesh->visible = true;
	}

	void enter_car(TopDownCar* car) {
		is_in_car = true;
		mesh->visible = false;
	}

	virtual void update() override {
		auto moveAction = inputPtr->get("game/move");

		if (is_in_car)
			return;

		if (shoot_cooldown > 0.0)shoot_cooldown -= eng->get_tick_interval();

	
		this->lookdir=glm::vec3(1,0,0);

		if (has_had_update) {
			glm::ivec2 mouse;
			SDL_GetMouseState(&mouse.x, &mouse.y);
			auto player = (PlayerBase*)eng->get_local_player();
			Ray r;
			r.dir = unproject_mouse_to_ray(player->last_view_setup, mouse.x, mouse.y);
			r.pos = view_pos;
			glm::vec3 intersect(0.f);
			ray_plane_intersect(r, glm::vec3(0, 1, 0), glm::vec3(0.8f), intersect);
			auto mypos = get_ws_position();
			lookdir = intersect - mypos;
			lookdir.y = 0;
			if (glm::length(lookdir) < 0.000001) lookdir = glm::vec3(1, 0, 0);
			else lookdir = glm::normalize(lookdir);

			mouse_pos = intersect;
		}
	
		auto move = moveAction->get_value<glm::vec2>();
		float len = glm::length(move);
		if(len>1.0)
			move = glm::normalize(move);
		float dt = eng->get_tick_interval();
		uint32_t flags = 0;
		glm::vec3 outvel;

		ccontroller->move(glm::vec3(move.x, 0, move.y)*move_speed * dt, dt, 0.005f, flags, outvel);

		float angle = -atan2(lookdir.z, lookdir.x);
		auto q = glm::angleAxis(angle, glm::vec3(0, 1, 0));

		set_ws_transform(ccontroller->get_character_pos(), q, get_ls_scale());

		has_had_update = true;
	}
	void get_view(glm::mat4& viewMat, float& fov) {
		auto pos = get_ws_position();
		pos = glm::mix(pos, mouse_pos, 0.15);
		auto camera_pos = glm::vec3(pos.x, pos.y + 16.0, pos.z - 2.0);
		glm::vec3 camera_dir = glm::normalize(camera_pos - pos);


		this->view_pos =  damp_dt_independent(camera_pos, this->view_pos, 0.002, eng->get_tick_interval());
		auto finalpos = shake.evaluate(this->view_pos, camera_dir, eng->get_frame_time());

		viewMat = glm::lookAt(finalpos, finalpos - camera_dir, glm::vec3(0, 1, 0));

		fov = 50.0;
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(TopDownPlayer)
			REG_FLOAT(move_speed, PROP_DEFAULT,"5.0")
		END_PROPS(TopDownPlayer)
	}

	CameraShake shake;
	std::unique_ptr<CharacterController> ccontroller;
	Entity* gun_entity = nullptr;
	CapsuleComponent* capsule = nullptr;
	MeshComponent* mesh = nullptr;
	TopDownHealthComponent* health = nullptr;
	InputUser* inputPtr = nullptr;

	float shoot_cooldown = 0.0;

	bool is_in_car = false;

	float move_speed = 5.0;
	glm::vec3 velocity{};

	glm::vec3 view_pos=glm::vec3(0,0,0);
	glm::vec3 lookdir=glm::vec3(1,0,0);
	glm::vec3 mouse_pos = glm::vec3(0, 0, 0);

	bool has_had_update = false;
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


CLASS_H(PlayerTriggerComponent2, EntityComponent)
public:
	void on_init() override {
		if (eng->is_editor_level()) return;

		auto ptr = get_owner()->get_first_component<PhysicsComponentBase>();
		if (ptr) {
			ptr->on_trigger_start.add(this, [](EntityComponent*) {
				sys_print(Debug, "triggered\n");
				});
		}
		else {
			sys_print(Error, "PlayerTriggerComponent needs physics component\n");
		}
	}
	static const PropertyInfoList* get_props() = delete;

};
CLASS_IMPL(PlayerTriggerComponent2);

void TopDownCar::update()
{
	auto moveAction = driver->inputPtr->get("game/move");
	auto movedir = moveAction->get_value<glm::vec2>();
}