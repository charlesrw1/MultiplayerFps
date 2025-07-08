#pragma once
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

#include "Scripting/FunctionReflection.h"
#include "Framework/Reflection2.h"
#include "Physics/Physics2.h"

#include "Game/GameSingleton.h"

#include "Game/EntityComponent.h"
#include "UI/GUISystemPublic.h"

class TopDownGameManager : public Component
{
public:
	CLASS_BODY(TopDownGameManager);

	static TopDownGameManager* instance;

	void pre_start() override {
		ASSERT(instance == nullptr);
		instance = this;
	}
	void start() override {
		the_player = eng->get_level()->spawn_prefab(player_prefab.get());
	}
	void update() {}
	void stop() {
		ASSERT(instance == this);
		instance = nullptr;
	}

	Entity* the_player = nullptr;
	CameraComponent* static_level_cam = nullptr;
	
	REF AssetPtr<PrefabAsset> player_prefab;
	REF obj<Component> what_component;
	REF obj<Component> my2nd;
	REF obj<Component> my3rd;
};


struct DamageEvent
{
	int amt = 0;
	glm::vec3 position;
	glm::vec3 dir;
};

class TopDownHealthComponent : public Component
{
public:
	CLASS_BODY(TopDownHealthComponent);

	void deal_damage(DamageEvent dmg) {
		current_health -= dmg.amt;
		if (current_health <= 0)
			on_death.invoke(dmg);
	}

	REFLECT();
	MulticastDelegate<Entity*, int> on_take_damage;

	MulticastDelegate<DamageEvent> on_death;

	REFLECT();
	int max_health = 100;

	void start() {
		current_health = max_health;
	}

	int current_health = 0;
};




class ProjectileComponent : public Component
{
public:
	CLASS_BODY(ProjectileComponent);

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
			auto owner = res.component->get_owner()->get_component<TopDownHealthComponent>();
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

	float time_created = 0.0;
	PhysicsBody* ignore = nullptr;
	glm::vec3 direction = glm::vec3(1, 0, 0);
	REF float speed = 10.0;
	REF int damage = 10;
};

class EnableRagdollScript : public Component
{
public:
	CLASS_BODY(EnableRagdollScript);

	void start() override {
		auto e = get_owner();
		auto& children = e->get_children();
		for (auto c : children) {
			if (c->get_tag() != StringName("Ragdoll")) continue;
			auto phys = c->get_component<PhysicsBody>();
			if (!phys)
				continue;

			auto m = e->get_cached_mesh_component();
			if (!m || !m->get_animator()) 
				continue;
			int i = m->get_index_of_bone(c->get_parent_bone());
			if (i == -1) continue;

			const glm::mat4& this_ws = e->get_ws_transform();

			phys->set_is_enable(true);
		}


		e->get_cached_mesh_component()->get_animator()->set_update_owner_position_to_root(true);
	}
};

class TopDownUtil
{
public:
	static void enable_ragdoll_shared(Entity* e, const glm::mat4& last_ws, bool enable)
	{
		auto& children = e->get_children();
		for (auto c : children) {
			if (c->get_tag() != StringName("Ragdoll")) 
				continue;
			auto phys = c->get_component<PhysicsBody>();
			if (!phys||phys->is_a<AdvancedJointComponent>())
				continue;

			auto m = e->get_cached_mesh_component();
			if (!m || !m->get_animator()) 
				continue;
			int i = m->get_index_of_bone(c->get_parent_bone());
			if (i == -1) 
				continue;

			const glm::mat4& this_ws = e->get_ws_transform();

			if (enable) {
				phys->enable_with_initial_transforms(
					last_ws * m->get_animator()->get_last_global_bonemats().at(i),
					this_ws * m->get_animator()->get_global_bonemats().at(i),
					eng->get_dt());
			}
			else {
				phys->set_is_enable(false);
			}
		}
	}
	static glm::vec3 unproject_mouse_to_ray(const View_Setup& vs, const int mx, const int my)
	{
		Ray r;

		auto size = UiSystem::inst->get_vp_rect().get_size();

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


};

class TopDownEnemyComponent : public Component
{
public:
	CLASS_BODY(TopDownEnemyComponent);

	void start() override {
		auto health = get_owner()->get_component<TopDownHealthComponent>();
		ASSERT(health);
		health->on_death.add(this, [&](DamageEvent dmg)
			{
				set_ticking(true);
				is_dead = true;
				death_time = eng->get_game_time();

				auto cap = get_owner()->get_component<PhysicsBody>();
				cap->set_is_enable(false);
				cap->destroy_deferred();

				TopDownUtil::enable_ragdoll_shared(get_owner(), last_ws, true);

				Random r(get_instance_id());
				const StringName names[] = { StringName("mixamorig:Hips"),StringName("mixamorig:Neck1"),StringName("mixamorig:RightLeg") };
				int index = r.RandI(0, 2);
				for (auto c : get_owner()->get_children()) {
					if (c->get_parent_bone() == names[index]) {
						auto p = c->get_component<PhysicsBody>();
						p->apply_impulse(c->get_ws_position(), dmg.dir * 0.3f);
						break;
					}
				}

			});

		auto capsule = get_owner()->get_component<CapsuleComponent>();
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
		facing_dir = to_dir;

		float dt = eng->get_dt();
		int flags = 0;
		glm::vec3 outvel;
		float move_speed = 3.0;
		ccontroller->move(glm::vec3(to_dir.x, 0, to_dir.z) * move_speed * dt, dt, 0.005f, flags, outvel);


		float angle = -atan2(-to_dir.x, to_dir.z);
		auto q = glm::angleAxis(angle, glm::vec3(0, 1, 0));

		last_ws = get_ws_transform();
		get_owner()->set_ws_transform(ccontroller->get_character_pos(), q, get_owner()->get_ls_scale());

	}
	void stop() {

	}

	glm::vec3 facing_dir = glm::vec3(1, 0, 0);
	std::unique_ptr<CharacterController> ccontroller;
	glm::mat4 last_ws = glm::mat4(1.f);
	bool is_dead = false;
	float death_time = 0.0;
};

class TopDownCameraReg : public Component {
public:
	CLASS_BODY(TopDownCameraReg);

	void start() override {
		ASSERT(TopDownGameManager::instance);
		auto cam = get_owner()->get_component<CameraComponent>();
		TopDownGameManager::instance->static_level_cam = cam;
		cam->set_is_enabled(true);
	}
};


class TopDownSpawner : public Component {
public:
	CLASS_BODY(TopDownSpawner);
	void start() {
		
	}

	REFLECT();
	void enable_object() {
		Entity* e{};
		float ofs = 0.0;
		for (int i = 0; i < 1; i++)
		{
			auto scope = eng->get_level()->spawn_prefab_deferred(e, prefab.get());
			e->set_ws_position(get_ws_position() + glm::vec3(0, ofs, 0));
			ofs += 1.5;
		}
	}

	REF AssetPtr<PrefabAsset> prefab;
	REF int count = 1;
	bool wait_to_spawn = false;
	bool start_disabled = false;
};
class TDSpawnOverTime : public Component {
public:
	CLASS_BODY(TDSpawnOverTime);
	void start() final {
		set_ticking(true);
	}
	void update() final {
		//return;
		if (eng->get_game_time() >= last_spawn + spawn_interval) {
			for (int i = 0; i < 1; i++) {
				Entity* e = nullptr;
				auto scope = eng->get_level()->spawn_prefab_deferred(e, prefab.get());
				e->set_ws_position(get_ws_position() +glm::vec3(i%5,0,i/5));
				last_spawn = eng->get_game_time();
			}
		}
	}
	float last_spawn = 0.0;


	REF AssetPtr<PrefabAsset> prefab;
	REF float spawn_interval = 1.0;	// every x seconds
	REF int max_count = 2000;
};

class TopDownSpawnPoint : public Component {
public:
	CLASS_BODY(TopDownSpawnPoint);
};
