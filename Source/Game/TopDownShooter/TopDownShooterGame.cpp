#include "TopDownShooterGame.h"
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
#include "AssetCompile/AnimationSeqLoader.h"
#pragma optimize("",off)
CLASS_H(TopDownSpawnPoint, EntityComponent)
public:
	
};
CLASS_IMPL(TopDownSpawnPoint);


CLASS_H(TopDownGameManager, Entity)
public:
	static TopDownGameManager* instance;

	void pre_start() override {
		ASSERT(instance == nullptr);
		instance = this;
	}
	void start() override {
		the_player = eng->get_level()->spawn_prefab(player_prefab.get());
	}
	void update() {}
	void end() {
		ASSERT(instance == this);
		instance = nullptr;
	}

	static const PropertyInfoList* get_props() {
		START_PROPS(TopDownGameManager)
			REG_ASSET_PTR(player_prefab, PROP_DEFAULT)
		END_PROPS(TopDownGameManager)
	}


	CameraComponent* static_level_cam = nullptr;
	AssetPtr<PrefabAsset> player_prefab;
	Entity* the_player = nullptr;
};
CLASS_IMPL(TopDownGameManager);

TopDownGameManager* TopDownGameManager::instance = nullptr;

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

	void start() {
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
		auto newpos = pos +  direction * speed * (float)eng->get_dt();
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

static void enable_ragdoll_shared(Entity* e, const glm::mat4& last_ws, bool enable)
{
	auto& children = e->get_all_children();
	for (auto c : children) {
		if (c->get_tag() != StringName("Ragdoll")) continue;
		auto phys = c->get_first_component<PhysicsComponentBase>();
		if (!phys) 
			continue;

		auto m = e->get_cached_mesh_component();
		if (!m||!m->get_animator_instance()) continue;
		int i = m->get_index_of_bone(c->get_parent_bone());
		if (i == -1) continue;

		const glm::mat4& this_ws = e->get_ws_transform();

		if (enable) {
			phys->enable_with_initial_transforms(
				last_ws * m->get_animator_instance()->get_last_global_bonemats().at(i),
				this_ws * m->get_animator_instance()->get_global_bonemats().at(i),
				eng->get_dt());
		}
		else {
			phys->set_is_enable(false);
		}
	}
}
CLASS_H(EnableRagdollScript,EntityComponent)
public:
	void start() override {
		auto e = get_owner();
		auto& children = e->get_all_children();
		for (auto c : children) {
			if (c->get_tag() != StringName("Ragdoll")) continue;
			auto phys = c->get_first_component<PhysicsComponentBase>();
			if (!phys) 
				continue;

			auto m = e->get_cached_mesh_component();
			if (!m||!m->get_animator_instance()) continue;
			int i = m->get_index_of_bone(c->get_parent_bone());
			if (i == -1) continue;

			const glm::mat4& this_ws = e->get_ws_transform();

			phys->set_is_enable(true);
		}
		e->get_cached_mesh_component()->get_animator_instance()->set_update_owner_position_to_root(true);
	}
};
CLASS_IMPL(EnableRagdollScript);

CLASS_H(TopDownEnemyComponent, EntityComponent)
public:
	void start() override {
		auto health = get_owner()->get_first_component<TopDownHealthComponent>();
		ASSERT(health);
		health->on_death.add(this, [&](DamageEvent dmg)
			{
				set_ticking(true);
				is_dead = true;
				death_time = eng->get_game_time();

				auto cap = get_owner()->get_first_component<PhysicsComponentBase>();
				cap->set_is_enable(false);
				cap->destroy();

				enable_ragdoll_shared(get_owner(), last_ws, true);

				Random r(get_instance_id());
				const StringName names[] = { StringName("mixamorig:Hips"),StringName("mixamorig:Neck1"),StringName("mixamorig:RightLeg") };
				int index = r.RandI(0, 2);
				for (auto c : get_owner()->get_all_children()) {
					if (c->get_parent_bone() == names[index]) {
						auto p = c->get_first_component<PhysicsComponentBase>();
						p->apply_impulse(c->get_ws_position(), dmg.dir * 2.f);
						break;
					}
				}
				
			});

		auto capsule = get_owner()->get_first_component<CapsuleComponent>();
		ccontroller = std::make_unique<CharacterController>(capsule);
		ccontroller->set_position(get_ws_position());
		ccontroller->capsule_height = capsule->height;
		ccontroller->capsule_radius = capsule->radius;


		set_ticking(true);
	}
	void update() {
		if (is_dead) {
			if ((eng->get_game_time() - death_time > 20.0)) {
				get_owner()->destroy();
			}
			return;
		}

		auto the_player = TopDownGameManager::instance->the_player;

		auto to_dir = the_player->get_ws_position() - get_ws_position();
		if (glm::length(to_dir) < 0.001) to_dir = glm::vec3(1, 0, 0);
		else to_dir = glm::normalize(to_dir);

		float dt = eng->get_dt();
		uint32_t flags = 0;
		glm::vec3 outvel;
		float move_speed = 3.0;
		
		ccontroller->move(glm::vec3(to_dir.x, 0, to_dir.z)*move_speed * dt, dt, 0.005f, flags, outvel);

		
		float angle = -atan2(-to_dir.x, to_dir.z);
		auto q = glm::angleAxis(angle, glm::vec3(0, 1, 0));

		last_ws = get_ws_transform();
		get_owner()->set_ws_transform(ccontroller->get_character_pos(), q, get_owner()->get_ls_scale());

	}
	void end() {

	}

	std::unique_ptr<CharacterController> ccontroller;
	glm::mat4 last_ws = glm::mat4(1.f);
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
			//printf("%f\n", amt);
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
	void start() {

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


CLASS_H(TopDownWeaponData, ClassBase)
public:
	std::string name;
	AssetPtr<Model> model;
	int damage = 0; // damage per shot
	float fire_rate = 1.f;	// per second
	int type = 0;	// 0 = rifle, 1 = shotgun
	int pellets = 0;	// for shotgun
	float accuracy = 1.f;
	float bullet_speed = 20.f;
	AssetPtr<PrefabAsset> special_projectile;	// what projectile to spawn
};
CLASS_IMPL(TopDownWeaponData);

class TopDownPlayerInventory
{
public:
	TopDownWeaponData* selected = nullptr;
	std::vector<TopDownWeaponData*> weapons;
};

struct TopDownControls
{
	InputActionInstance* shoot{};
	InputActionInstance* move{};
	InputActionInstance* look{};
};
#include "Animation/SkeletonData.h"
CLASS_H(TopDownPlayer, Entity)
public:

	TopDownControls con;

	TopDownPlayer() {
		set_ticking(true);
		mesh = construct_sub_component<MeshComponent>("body");
		capsule = construct_sub_component<CapsuleComponent>("capsule-body");
		health = construct_sub_component<TopDownHealthComponent>("health");

		gun_entity = construct_sub_entity<Entity>("gun-entity");
		gun_entity->construct_sub_component<MeshComponent>("gun-model");
	}

	bool using_controller() const {
		return inputPtr->get_device()->get_type() == InputDeviceType::Controller;
	}

	virtual void start() override {

		{
			auto cameraobj = eng->get_level()->spawn_entity_class<Entity>();
			the_camera = cameraobj->create_and_attach_component_type<CameraComponent>();
			the_camera->set_is_enabled(true);
			ASSERT(CameraComponent::get_scene_camera() == the_camera);
		}



		ccontroller = std::make_unique<CharacterController>(capsule);
		ccontroller->set_position(get_ws_position());
		ccontroller->capsule_height = capsule->height;
		ccontroller->capsule_radius = capsule->radius;

		inputPtr = GetGInput().register_input_user(0);
		inputPtr->assign_device(GetGInput().get_keyboard_device());
		for(auto d : GetGInput().get_connected_devices())
			if (d->get_type() == InputDeviceType::Controller) {
				inputPtr->assign_device(d);
				break;
			}
		inputPtr->on_changed_device.add(this, [&]()
		 {
			if(!inputPtr->get_device())
				inputPtr->assign_device(GetGInput().get_keyboard_device());
		});



		inputPtr->enable_mapping("game");
		inputPtr->enable_mapping("ui");
		con.shoot = inputPtr->get("game/shoot");
		con.move = inputPtr->get("game/move");
		con.look = inputPtr->get("game/look");
		inputPtr->get("game/jump")->on_start.add(this, [&]()
			{
				mesh->get_animator_instance()->play_animation_in_slot(
					jumpSeq.get()->seq,
					StringName("ACTION"),
					1.f,
					0.f
				);

				is_jumping = !is_jumping;
			});
		inputPtr->get("game/test1")->on_start.add(this, [&]()
			{
				ragdoll_enabled = !ragdoll_enabled;

				enable_ragdoll_shared(this, last_ws, ragdoll_enabled);

				if (!ragdoll_enabled) {
					int index = get_cached_mesh_component()->get_index_of_bone(StringName("mixamorig:Hips"));
					glm::mat4 ws = get_ws_transform() * get_cached_mesh_component()->get_animator_instance()->get_global_bonemats().at(index);	//root
					glm::vec3 pos = ws[3];
					pos.y = 0;
					set_ws_position(pos);
					ccontroller->set_position(pos);
					get_cached_mesh_component()->get_animator_instance()->set_update_owner_position_to_root(false);
				}
				else {
					get_cached_mesh_component()->get_animator_instance()->set_update_owner_position_to_root(true);
					//set_ws_transform(glm::mat4(1.f));
				}
			});


		velocity = {};

		ccontroller->set_position(glm::vec3(0,0.0,0));
	}
	virtual void end() override {
		GetGInput().free_input_user(inputPtr);
		GetGInput().device_connected.remove(this);
	}
	void exit_car() {
		is_in_car = false;
		mesh->set_is_visible( true );
	}

	void enter_car(TopDownCar* car) {
		is_in_car = true;
		mesh->set_is_visible(false);
	}

	void shoot_gun() {
		if (shoot_cooldown <= 0.0)
		{
			Random r(eng->get_game_time());
			
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
			shake.start(0.08);
			shoot_cooldown = 0.8;
		}
	}

	virtual void update() override {

		did_move = false;
		if (is_in_car)
			return;

		if (ragdoll_enabled) {
			update_view();
			last_ws = get_ws_transform();
			return;
		}

		if (shoot_cooldown > 0.0)shoot_cooldown -= eng->get_dt();

		if (con.shoot->get_value<bool>())
			shoot_gun();

		if (has_had_update) {

			if (using_controller()) {
				auto stick = con.look->get_value<glm::vec2>();
				if (glm::length(stick) > 0.01) {
					lookdir = glm::normalize(glm::vec3(-stick.x, 0, -stick.y));
				}
				mouse_pos = get_ws_position();
			}
			else {
				glm::ivec2 mouse;
				SDL_GetMouseState(&mouse.x, &mouse.y);

				Ray r;
				r.dir = unproject_mouse_to_ray(CameraComponent::get_scene_camera()->last_vs, mouse.x, mouse.y);
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

		}
	
		auto move = con.move->get_value<glm::vec2>();
		float len = glm::length(move);
		if(len>1.0)
			move = glm::normalize(move);
		if (len > 0.01)
			did_move = true;
		float dt = eng->get_dt();
		uint32_t flags = 0;
		glm::vec3 outvel;

		glm::vec3 move_front = glm::vec3(0, 0, 1);
		glm::vec3 move_side = glm::vec3(1, 0, 0);
		if (is_jumping) {
			move_front = glm::normalize(glm::vec3(-1, 0, 1));
			move_side = -glm::cross(move_front, glm::vec3(0, 1, 0));
		}


		ccontroller->move((move_front*move.y+move_side*move.x)*move_speed * dt, dt, 0.005f, flags, outvel);

		float angle = -atan2(-lookdir.x, lookdir.z);
		auto q = glm::angleAxis(angle, glm::vec3(0, 1, 0));


		last_ws = get_ws_transform();
		set_ws_transform(ccontroller->get_character_pos(), q, get_ls_scale());

		has_had_update = true;

		update_view();
	}
	void update_view() {
		auto pos = get_ws_position();
		//pos = glm::mix(pos, mouse_pos, 0.15);
		glm::vec3 camera_pos;
		if (is_jumping)
			camera_pos = glm::vec3(pos.x + 3.0, pos.y + 2.0, pos.z - 3.0);
		else
			camera_pos = glm::vec3(pos.x, pos.y + 12.0, pos.z - 1.0);
		glm::vec3 camera_dir = glm::normalize(camera_pos - (pos + glm::vec3(0,1,0)));

		this->view_pos =  damp_dt_independent(camera_pos, this->view_pos, 0.002, eng->get_dt());
		auto finalpos = shake.evaluate(this->view_pos, camera_dir, eng->get_dt());

		auto viewMat = glm::lookAt(finalpos, finalpos - camera_dir, glm::vec3(0, 1, 0));

		the_camera->get_owner()->set_ws_transform(glm::inverse(viewMat));
		if (ragdoll_enabled) {
			glm::vec3 linvel = glm::vec3(get_ws_transform()[3] - last_ws[3]) / (float)eng->get_dt();
			float speed = glm::length(linvel);
			float desire_fov = glm::mix(50.0, 60.0, glm::min(speed*0.1,1.0));
			fov = damp_dt_independent(desire_fov, fov, 0.002f, (float)eng->get_dt());
		}
		else {
			fov = damp_dt_independent(50.0f, fov, 0.002f, (float)eng->get_dt());
		}
		the_camera->fov = fov;
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(TopDownPlayer)
			REG_FLOAT(move_speed, PROP_DEFAULT,"5.0"),
			REG_ASSET_PTR(jumpSeq, PROP_DEFAULT)
		END_PROPS(TopDownPlayer)
	}

	AssetPtr<AnimationSeqAsset> jumpSeq;
	CameraShake shake;
	std::unique_ptr<CharacterController> ccontroller;
	Entity* gun_entity = nullptr;
	CapsuleComponent* capsule = nullptr;
	MeshComponent* mesh = nullptr;
	TopDownHealthComponent* health = nullptr;
	InputUser* inputPtr = nullptr;
	CameraComponent* the_camera = nullptr;

	bool ragdoll_enabled = false;
	float fov = 50.0;

	float shoot_cooldown = 0.0;

	bool is_in_car = false;

	float move_speed = 5.0;
	glm::vec3 velocity{};

	glm::vec3 view_pos=glm::vec3(0,0,0);
	glm::vec3 lookdir=glm::vec3(1,0,0);
	glm::vec3 mouse_pos = glm::vec3(0, 0, 0);

	bool did_move = false;
	bool is_jumping = false;

	bool has_had_update = false;

	glm::mat4 last_ws = glm::mat4(1.f);
};
CLASS_IMPL(TopDownPlayer);



CLASS_H(PlayerTriggerComponent2, EntityComponent)
public:
	void start() override {
		auto ptr = get_owner()->get_first_component<PhysicsComponentBase>();
		if (ptr) {
			ptr->on_trigger_start.add(this, [&](PhysicsComponentBase* c) {
				if (c->get_owner() == TopDownGameManager::instance->the_player) {
					for (auto eptr : objects_to_active) {
						auto e = eptr.get();
						if(e)
							e->set_active(true);
					}
					eng->get_level()->queue_deferred_delete(get_owner());
				}
				});
		}
		else {
			sys_print(Error, "PlayerTriggerComponent needs physics component\n");
		}
	}
	std::vector<EntityPtr<Entity>> objects_to_active;

	static const PropertyInfoList* get_props() {
		MAKE_VECTORCALLBACK_ATOM(EntityPtr<Entity>, objects_to_active);
		START_PROPS(PlayerTriggerComponent2)
			REG_STDVECTOR(objects_to_active,PROP_DEFAULT)
		END_PROPS(PlayerTriggerComponent2)
	}

};
CLASS_IMPL(PlayerTriggerComponent2);

void TopDownCar::update()
{
	auto moveAction = driver->inputPtr->get("game/move");
	auto movedir = moveAction->get_value<glm::vec2>();
}

CLASS_H(TopDownCameraReg, EntityComponent)
public:
	void start() override {
		ASSERT(TopDownGameManager::instance);
		auto cam = get_owner()->get_first_component<CameraComponent>();
		TopDownGameManager::instance->static_level_cam = cam;
		cam->set_is_enabled(true);
	}
};
CLASS_IMPL(TopDownCameraReg);
#include "Game/Components/LightComponents.h"

float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float lerp(float t, float a, float b) {
    return a + t * (b - a);
}
void randomGradient(int ix, int iy, float& gx, float& gy) {
    float random = 2920.0f * sin(ix * 21942.0f + iy * 171324.0f + 8912.0f) * cos(ix * 23157.0f * iy * 217832.0f + 9758.0f);
    gx = cos(random);
    gy = sin(random);
}
float dotGridGradient(int ix, int iy, float x, float y) {
    float gx, gy;
	randomGradient(ix, iy, gx, gy);
    float dx = x - static_cast<float>(ix);
    float dy = y - static_cast<float>(iy);
    return (dx * gx + dy * gy);
}
float perlin(float x, float y) {
    int x0 = static_cast<int>(floor(x));
    int x1 = x0 + 1;
    int y0 = static_cast<int>(floor(y));
    int y1 = y0 + 1;

    float sx = fade(x - static_cast<float>(x0));
    float sy = fade(y - static_cast<float>(y0));

    float n0, n1, ix0, ix1, value;
    n0 = dotGridGradient(x0, y0, x, y);
    n1 = dotGridGradient(x1, y0, x, y);
    ix0 = lerp(sx, n0, n1);

    n0 = dotGridGradient(x0, y1, x, y);
    n1 = dotGridGradient(x1, y1, x, y);
    ix1 = lerp(sx, n0, n1);

    value = lerp(sy, ix0, ix1);
    return (value + 1.0) / 2.0;  // Normalize to [0, 1]
}


CLASS_H(TopDownFireScript,EntityComponent)
public:
	TopDownFireScript() {
		set_call_init_in_editor(true);
	}
	void start() override {
		light = get_owner()->get_first_component<PointLightComponent>();
		set_ticking(true);
	}
	void update() override {
		if (!light) return;
		float a = perlin(eng->get_game_time() * 2.0+ofs, eng->get_game_time()-ofs*0.2);
		light->intensity = glm::mix(min_intensity,max_intensity,pow(a,2.0));
		light->on_changed_transform();
	}

	float ofs = 0.0;
	float min_intensity = 0.0;
	float max_intensity = 30.0;
	PointLightComponent* light = nullptr;
};
CLASS_IMPL(TopDownFireScript);

#include "Animation/Runtime/Animation.h"
CLASS_H(TopDownAnimDriver, AnimatorInstance)
public:

	virtual void on_init() override {
		if (get_owner()) {
			p = get_owner()->cast_to<TopDownPlayer>();
			e = get_owner()->get_first_component<TopDownEnemyComponent>();
		}
	}
	virtual void on_update(float dt) override {
		if (p) {
			bRunning = p->did_move;
			bIsJumping = p->is_jumping;
		}
		else if (e) {
			bRunning = !e->is_dead;
		}
		else
			bRunning = false;
	}
	virtual void on_post_update() override {

	}

	TopDownPlayer* p = nullptr;
	TopDownEnemyComponent* e = nullptr;

	static const PropertyInfoList* get_props() {
		START_PROPS(TopDownAnimDriver)
			REG_FLOAT(flMovex,PROP_DEFAULT,""),
			REG_FLOAT(flMovey, PROP_DEFAULT, ""),
			REG_BOOL(bRunning, PROP_DEFAULT, ""),
		END_PROPS(TopDownAnimDriver)
	}

	bool bIsJumping = false;
	bool bRunning = false;
	float flMovex = 0.0;
	float flMovey = 0.0;
};
CLASS_IMPL(TopDownAnimDriver);

CLASS_H(TopDownSpawner,EntityComponent)
public:
	void start() {
		Entity* e{};
		{
			auto scope = eng->get_level()->spawn_prefab_deferred(e, prefab.get());
			e->set_ws_position(get_ws_position());
		}
	}

	static const PropertyInfoList* get_props() {
		START_PROPS(TopDownSpawner)
			REG_ASSET_PTR(prefab,PROP_DEFAULT)
		END_PROPS(TopDownSpawner)
	}

	AssetPtr<PrefabAsset> prefab;
};
CLASS_IMPL(TopDownSpawner);