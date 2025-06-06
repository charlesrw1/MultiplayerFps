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
class ComponentWithStruct : public Component
{
public:
	CLASS_BODY(ComponentWithStruct);

	REF MyStruct things;
	REF bool is_happening = false;
	//REF vector<MyStruct> list_of_things;
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

class TopDownWeaponData : public ClassBase
{
public:
	CLASS_BODY(TopDownWeaponData);
	REF std::string name;
	REF AssetPtr<Model> model;
	REF int damage = 0; // damage per shot
	REF float fire_rate = 1.f;	// per second
	REF int type = 0;	// 0 = rifle, 1 = shotgun
	REF int pellets = 0;	// for shotgun
	REF float accuracy = 1.f;
	REF float bullet_speed = 20.f;
	REF AssetPtr<PrefabAsset> special_projectile;	// what projectile to spawn
	REF std::vector<float> values;
};

struct TopDownControls
{
	InputActionInstance* shoot{};
	InputActionInstance* move{};
	InputActionInstance* look{};
};


using std::unordered_map;
using std::unordered_set;
using std::vector;



class TopDownPlayer : public Component
{
public:
	CLASS_BODY(TopDownPlayer);
	REF MyStruct numbers;

	TopDownControls con;
	REF AssetPtr<PrefabAsset> shotgunSoundAsset;
	REF AssetPtr<AnimationSeqAsset> runToStart;
	REF AssetPtr<AnimationSeqAsset> idleToRun;
	SoundComponent* cachedShotgunSound = nullptr;

	TopDownPlayer() {
		set_ticking(true);
		//mesh = construct_sub_component<MeshComponent>("body");
		//capsule = construct_sub_component<CapsuleComponent>("capsule-body");
		//health = construct_sub_component<TopDownHealthComponent>("health");
		//
		//gun_entity = construct_sub_entity<Entity>("gun-entity");
		//gun_entity->construct_sub_component<MeshComponent>("gun-model");
	}

	bool using_controller() const {
		return inputPtr->get_device()->get_type() == InputDeviceType::Controller;
	}

	void start() final {

		mesh = get_owner()->get_component<MeshComponent>();
		capsule = get_owner()->get_component<CapsuleComponent>();
		assert(mesh && capsule);
		{
			auto cameraobj = eng->get_level()->spawn_entity();
			the_camera = cameraobj->create_component<CameraComponent>();
			the_camera->set_is_enabled(true);
			ASSERT(CameraComponent::get_scene_camera() == the_camera);
		}

		shotgunSoundAsset = g_assets.find_sync<PrefabAsset>("top_down/shotgun_sound.pfb");
		cachedShotgunSound = eng->get_level()->spawn_prefab(shotgunSoundAsset.get())->get_component<SoundComponent>();

		ccontroller = std::make_unique<CharacterController>(capsule);
		ccontroller->set_position(get_ws_position());
		ccontroller->capsule_height = capsule->height;
		ccontroller->capsule_radius = capsule->radius;

		inputPtr = g_inputSys.register_input_user(0);
		inputPtr->assign_device(g_inputSys.get_keyboard_device());
		for (auto d : g_inputSys.get_connected_devices())
			if (d->get_type() == InputDeviceType::Controller) {
				inputPtr->assign_device(d);
				break;
			}
		inputPtr->on_changed_device.add(this, [&]()
			{
				if (!inputPtr->get_device())
					inputPtr->assign_device(g_inputSys.get_keyboard_device());
			});



		inputPtr->enable_mapping("game");
		inputPtr->enable_mapping("ui");
		con.shoot = inputPtr->get("game/shoot");
		con.move = inputPtr->get("game/move");
		con.look = inputPtr->get("game/look");
		inputPtr->get("game/jump")->on_start.add(this, [&]()
			{
				using_third_person_movement = !using_third_person_movement;

				mesh->get_animator_instance()->play_animation_in_slot(
					jumpSeq,
					StringName("ACTION"),
					1.f,
					0.f
				);

				// is_jumping = !is_jumping;
			});
		inputPtr->get("game/test1")->on_start.add(this, [&]()
			{
				ragdoll_enabled = !ragdoll_enabled;

				TopDownUtil::enable_ragdoll_shared(get_owner(), last_ws, ragdoll_enabled);

				if (!ragdoll_enabled) {
					int index = get_owner()->get_cached_mesh_component()->get_index_of_bone(StringName("mixamorig:Hips"));
					glm::mat4 ws = get_ws_transform() * get_owner()->get_cached_mesh_component()->get_animator_instance()->get_global_bonemats().at(index);	//root
					glm::vec3 pos = ws[3];
					pos.y = 0;
					get_owner()->set_ws_position(pos);
					ccontroller->set_position(pos);
					get_owner()->get_cached_mesh_component()->get_animator_instance()->set_update_owner_position_to_root(false);
				}
				else {
					get_owner()->get_cached_mesh_component()->get_animator_instance()->set_update_owner_position_to_root(true);
					//set_ws_transform(glm::mat4(1.f));
				}
			});


		velocity = {};

		ccontroller->set_position(glm::vec3(0, 0.0, 0));

		//eng->set_game_focused(true);
	}
	void end() final {
		g_inputSys.device_connected.remove(this);
	}


	void shoot_gun() {
		if (shoot_cooldown <= 0.0)
		{
			Random r(eng->get_game_time());

			int count = 5;
			for (int i = 0; i < count; i++) {

				auto projectile = eng->get_level()->spawn_prefab(g_assets.find_sync<PrefabAsset>("top_down/projectile.pfb").get());
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
		
		auto off = con.look->get_value<glm::vec2>();
		view_angles.x -= off.y;	// pitch
		view_angles.y += off.x;	// yaw
		view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
		view_angles.y = fmod(view_angles.y, TWOPI);
	}
	glm::vec3 get_front_dir() const {
		return AnglesToVector(view_angles.x, view_angles.y);
	}

	void update() final {

		did_move = false;
		if (is_in_car)
			return;

		update_view_angles();

		if (ragdoll_enabled) {
			update_view();
			last_ws = get_ws_transform();
			return;
		}

		if (shoot_cooldown > 0.0)shoot_cooldown -= eng->get_dt();

		if (con.shoot->get_value<bool>())
			shoot_gun();

		if (has_had_update) {

			if (using_third_person_movement) {

			}
			else {
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
					r.dir = TopDownUtil::unproject_mouse_to_ray(CameraComponent::get_scene_camera()->last_vs, mouse.x, mouse.y);
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

		}

		auto move = con.move->get_value<glm::vec2>();
		float len = glm::length(move);
		if (len > 1.0)
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
		if (using_third_person_movement) {
			move_front = get_front_dir();
			move_front.y = 0;
			move_front = glm::normalize(move_front);
			move_side = -(glm::cross(move_front, glm::vec3(0, 1, 0)));
			lookdir = move_front;
		}

		ccontroller->move((move_front * move.y + move_side * move.x) * move_speed * dt, dt, 0.005f, flags, outvel);

		float angle = -atan2(-lookdir.x, lookdir.z);
		auto q = glm::angleAxis(angle, glm::vec3(0, 1, 0));


		last_ws = get_ws_transform();
		get_owner()->set_ws_transform(ccontroller->get_character_pos(), q, get_owner()->get_ls_scale());

		has_had_update = true;

		update_view();
	}
	void update_view() {
		auto pos = get_ws_position();
		//pos = glm::mix(pos, mouse_pos, 0.15);
		glm::vec3 camera_pos;
		if (is_jumping)
			camera_pos = glm::vec3(pos.x + 3.0, pos.y + 1.0, pos.z - 3.0);
		else
			camera_pos = glm::vec3(pos.x, pos.y + 12.0, pos.z - 1.0);
		glm::vec3 camera_dir = glm::normalize(camera_pos - (pos + glm::vec3(0, 1, 0)));

		if (using_third_person_movement) {
			auto front = get_front_dir();
			camera_pos = pos + glm::vec3(0, 2, 0) - front*3.0f;
			camera_dir = -front;
		}

		if (ragdoll_enabled || !using_third_person_movement)
			this->view_pos = damp_dt_independent(camera_pos, this->view_pos, 0.002, eng->get_dt());
		else
			this->view_pos = camera_pos;

		auto finalpos = shake.evaluate(this->view_pos, camera_dir, eng->get_dt());

		auto viewMat = glm::lookAt(finalpos, finalpos - camera_dir, glm::vec3(0, 1, 0));

		the_camera->get_owner()->set_ws_transform(glm::inverse(viewMat));
		if (ragdoll_enabled) {
			glm::vec3 linvel = glm::vec3(get_ws_transform()[3] - last_ws[3]) / (float)eng->get_dt();
			float speed = glm::length(linvel);
			float desire_fov = glm::mix(50.0, 60.0, glm::min(speed * 0.1, 1.0));
			fov = damp_dt_independent(desire_fov, fov, 0.002f, (float)eng->get_dt());
		}
		else {
			fov = damp_dt_independent(50.0f, fov, 0.002f, (float)eng->get_dt());
		}
		the_camera->fov = fov;
	}

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
	std::unique_ptr<InputUser> inputPtr;
	CameraComponent* the_camera = nullptr;

	bool ragdoll_enabled = false;
	float fov = 50.0;

	float shoot_cooldown = 0.0;

	bool is_in_car = false;

	glm::vec3 velocity{};

	glm::vec3 view_angles = glm::vec3(0.f);

	glm::vec3 view_pos = glm::vec3(0, 0, 0);
	glm::vec3 lookdir = glm::vec3(1, 0, 0);
	glm::vec3 mouse_pos = glm::vec3(0, 0, 0);

	bool did_move = false;
	bool is_jumping = false;
	bool using_third_person_movement = false;
	bool has_had_update = false;

	glm::mat4 last_ws = glm::mat4(1.f);
};


extern float lean_amt;
extern float lean_smooth;

class TopDownAnimDriver : public AnimatorInstance
{
public:
	CLASS_BODY(TopDownAnimDriver);

	virtual void on_init() override {
		if (get_owner()) {
			p = get_owner()->get_component<TopDownPlayer>();
			e = get_owner()->get_component<TopDownEnemyComponent>();
		}
	}
	virtual void on_update(float dt) override {
		if (p) {
			bRunning = p->did_move;
			bIsJumping = p->is_jumping;

			auto lastvel = vel;
			vel = (p->get_ws_position() - lastpos) / dt;
			accel = (vel - lastvel) / dt;
			lastpos = p->get_ws_position();

			glm::vec3 side = glm::normalize(glm::cross(p->get_front_dir(), glm::vec3(0, 1, 0)));
			float d = glm::dot(side, accel);
			d *= lean_amt;
			flMovex = damp_dt_independent(glm::clamp(d * 0.5 + 0.5, 0.0, 1.0), (double)flMovex, lean_smooth, dt);
		}
		else if (e) {
			bRunning = !e->is_dead;
			auto lastvel = vel;
			vel = (e->get_ws_position() - lastpos) / dt;
			accel = (vel - lastvel) / dt;
			lastpos = e->get_ws_position();

			glm::vec3 side = glm::normalize(glm::cross(e->facing_dir, glm::vec3(0, 1, 0)));
			float d = glm::dot(side, accel);
			d *= lean_amt;
			flMovex = damp_dt_independent(glm::clamp(d * 0.5 + 0.5, 0.0, 1.0),(double)flMovex,lean_smooth,dt);
		}
		else
			bRunning = false;
	}
	virtual void on_post_update() override {

	}

	TopDownPlayer* p = nullptr;
	TopDownEnemyComponent* e = nullptr;

	glm::vec3 lastpos{};
	glm::vec3 vel{};
	glm::vec3 accel{};

	REF bool bIsJumping = false;
	REF bool bRunning = false;
	REF float flMovex = 0.0;
	REF float flMovey = 0.0;
};