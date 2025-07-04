#pragma once

#include "TopDownShooterGame.h"
#include "Animation/SkeletonData.h"
#include "Sound/SoundComponent.h"
#include "Framework/StructReflection.h"
struct Serializer;
struct MyStruct
{
	STRUCT_BODY();
	REF float x = 0;
	REF float y = 0;
	REF std::string s = "";

	REF void serialize(Serializer& d) {

	}
};


class CameraShake
{
public:
	glm::vec3 evaluate(glm::vec3 pos, glm::vec3 front, float dt) {
		if (time < total_time) {
			glm::vec3 left = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
			glm::vec3 up = glm::cross(left, front);
			float amt = intensity * eval_func(time * 10.0) * eval_mult(time);
			//printf("%f\n", amt);
			time += dt;
			return pos + (left * dir.x + up * dir.y) * amt;
		}
		else
			return pos;


	}
	void start(float intensity = 0.1) {
		this->intensity = intensity;
		time = 0.0;
	}
	float eval_func(float t) {
		return sin(1.1 * t + 0.1) + 2.0 * sin(2.6 * t - 0.8) + sin(0.1 * t + 0.2) + 3.0 * sin(0.6 * t - 1.0);
	}
	float eval_mult(float t) {
		if (t <= fade_in_time) return t / fade_in_time;
		else if (t >= total_time - fade_out_time) return 1.0 - ((t - (total_time - fade_out_time)) / fade_out_time);
		return 1.0;
	}
	glm::vec2 dir = glm::vec2(0.5, -0.5);
	float intensity = 0.1;
	float total_time = 0.25;
	float fade_in_time = 0.02;
	float fade_out_time = 0.02;
	float time = 0.0;
};



class TopDownWeaponDataNew : public Component
{
public:
	CLASS_BODY(TopDownWeaponDataNew);

	REF std::string name;
	REF Model* model = nullptr;
	REF int damage = 0; // damage per shot
	REF float fire_rate = 1.f;	// per second
	REF int type = 0;	// 0 = rifle, 1 = shotgun
	REF int pellets = 0;	// for shotgun
	REF float accuracy = 1.f;
	REF float bullet_speed = 20.f;
	REF PrefabAsset* special_projectile = nullptr;	// what projectile to spawn
	REF std::vector<float> values;
};

class PlayerAgFactory : public ClassBase {
public:
	CLASS_BODY(PlayerAgFactory, scriptable);
	REF virtual void create(const Model* model, agBuilder* builder) {}
};




using std::unordered_map;
using std::unordered_set;
using std::vector;



class TopDownPlayer : public Component
{
public:
	CLASS_BODY(TopDownPlayer);
	REF MyStruct numbers;
	REF std::vector<int> myarray = { 0,5,10,15 };
	REF AnimationSeqAsset* runToStar = nullptr;
	REF AnimationSeqAsset* idleToRun = nullptr;
	SoundComponent* cachedShotgunSound = nullptr;
	REF PrefabAsset* particlePrefab = nullptr;
	REF PrefabAsset* shotgunSoundAsset = nullptr;
	REF AnimationSeqAsset* jumpAnim = nullptr;


	TopDownPlayer() {
		set_ticking(true);
		//mesh = construct_sub_component<MeshComponent>("body");
		//capsule = construct_sub_component<CapsuleComponent>("capsule-body");
		//health = construct_sub_component<TopDownHealthComponent>("health");
		//
		//gun_entity = construct_sub_entity<Entity>("gun-entity");
		//gun_entity->construct_sub_component<MeshComponent>("gun-model");
	}

	void start() final;
	void stop() final {
		
	}


	void shoot_gun() {
		return;
		if (shoot_cooldown <= 0.0)
		{
			Random r(eng->get_game_time());

			int count = 5;
			for (int i = 0; i < count; i++) {
				auto prefab = PrefabAsset::load("top_down/projectile.pfb");
				auto projectile = eng->get_level()->spawn_prefab(prefab);
				auto pc = projectile->get_component<ProjectileComponent>();
				pc->ignore = capsule;
				const float spread = 0.15;
				pc->direction = lookdir + glm::vec3(r.RandF(-spread, spread), 0, r.RandF(-spread, spread));
				if(using_third_person_movement)
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
	}

	void update_view_angles() {
	
		//auto off = con.look->get_value<glm::vec2>();
		//view_angles.x -= off.y;	// pitch
		//view_angles.y += off.x;	// yaw
		//view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
		//view_angles.y = fmod(view_angles.y, TWOPI);
	}
	glm::vec3 get_front_dir() const {
		return AnglesToVector(view_angles.x, view_angles.y);
	}
	int var = 0;

	void update() final;
	void update_view();

	REFLECT();
	AssetPtr<AnimationSeqAsset> jumpSeq;
	REFLECT();
	float move_speed = 5.0;

	CameraShake shake;
	std::unique_ptr<CharacterController> ccontroller;
	Entity* gun_entity = nullptr;
	CapsuleComponent* capsule = nullptr;
	MeshComponent* mesh = nullptr;
	TopDownHealthComponent* health = nullptr;
	CameraComponent* the_camera = nullptr;

	bool ragdoll_enabled = false;
	REF float fov = 50.0;

	REF float shoot_cooldown = 0.0;

	bool is_in_car = false;

	glm::vec3 velocity{};

	glm::vec3 view_angles = glm::vec3(0.f);

	glm::vec3 view_pos = glm::vec3(0, 0, 0);
	glm::vec3 lookdir = glm::vec3(1, 0, 0);
	glm::vec3 mouse_pos = glm::vec3(0, 0, 0);

	bool did_move = false;
	bool is_jumping = false;
	REF bool using_third_person_movement = true;
	bool has_had_update = false;

	glm::mat4 last_ws = glm::mat4(1.f);
};
///

extern float lean_amt;
extern float lean_smooth;

#include "Framework/Serializer.h"
class TopDownPlayer;
class ComponentWithStruct : public Component
{
public:
	CLASS_BODY(ComponentWithStruct);

	void serialize(Serializer& s) {
		Component::serialize(s);
		s.serialize_class("player", player);
		s.serialize_class("what", what);
	}
	TopDownPlayer* player = nullptr;
	Entity* what = nullptr;

	REF glm::vec3 target={};
	// Set your thing here
	REF MyStruct things;
	// Is it happening?
	// Maybe it is.
	REF bool is_happening = false;
	// A list of all the things.
	REF vector<MyStruct> list_of_things;
};