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
#include "CarGame.h"

float spring_damp = 200;
float spring_constant = 4000.0;
float forward_force_mult = 600;
float brake_force_mult = 500;
float side_force_mult = 100;
float friction_coeff = 2.3f;
float steer_mult = 0.04;
float wind_resist = 0.80;
float epsilon_friction = 0.005;
float max_spring = 0.8;
float visual_wheel_interp = 0.0053;

void car_debug_menu()
{
	ImGui::DragFloat("spring_damp", &spring_damp,0.1,200);
	ImGui::DragFloat("spring_constant", &spring_constant,0.1,0,500);

	ImGui::DragFloat("forward_force_mult", &forward_force_mult,0.1,0,100);
	ImGui::DragFloat("side_force_mult", &side_force_mult,0.1,0,100);

	ImGui::DragFloat("friction_coeff", &friction_coeff,0.01,0,50);

	ImGui::DragFloat("steer_mult", &steer_mult,0.005,0,2);

	ImGui::DragFloat("visual_wheel_interp", &visual_wheel_interp,0.01,0,1);


	ImGui::DragFloat("wind_resist", &wind_resist,0.005,0,2);

	ImGui::DragFloat("epsilon_friction", &epsilon_friction,0.005,0,0.2);

	ImGui::DragFloat("max_spring", &max_spring,0.05,0.1,1.5);
}
ADD_TO_DEBUG_MENU(car_debug_menu);//

void CarComponent::update() {
	auto body_t = body->get_ws_transform();
	glm::vec3 up_vec = glm::mat3(body_t) * glm::vec3(0, 1, 0);
	glm::vec3 side_vec = glm::mat3(body_t) * glm::vec3(1, 0, 0);
	glm::vec3 front_vec = glm::mat3(body_t) * glm::vec3(0, 0, 1);

	//const float max_spring = 1.0;
	const float wheel_radius = 0.43;

	float current_speed = glm::length(body->get_linear_velocity());
	const float delta_angle = (current_speed * eng->get_dt()) / wheel_radius;
	tire_screech_amt = 0.0;
	//Debug::add_line(body_t[3], glm::vec3(body_t[3]) + up_vec*2.f, COLOR_PINK, 0);
	for (int i = 0; i < 4; i++) {
		auto w = wheels[i];
		auto o = w->get_owner();
		auto ws_pos = glm::vec3(body_t * glm::vec4(localspace_wheel_pos[i], 1));


		Ray r;
		r.pos = ws_pos;
		r.dir = -up_vec;
		glm::vec3 intersect;
		float dist = 1000.0;
		world_query_result res;
		TraceIgnoreVec vec;
		vec.push_back(body);
		//if (g_physics.trace_ray(res, r.pos, r.pos + r.dir * max_spring, &vec, UINT32_MAX))
		//	dist = res.distance;
		if (g_physics.sweep_sphere(res, wheel_radius, r.pos + up_vec * wheel_radius, -up_vec, max_spring + wheel_radius, UINT32_MAX, &vec))
			dist = res.distance;


		float d = glm::max(0.f, max_spring - dist);
		bool touching_ground = d > 0.f;
		if (dist < -0.1)touching_ground = false;

		float dxdt = (dist - last_dist[i]) / (float)eng->get_dt();



		if (touching_ground) {
			body->apply_force(ws_pos, up_vec * (d * spring_constant - dxdt * spring_damp));

			wheel_angles[i] += delta_angle * glm::sign(glm::dot(body->get_linear_velocity(), front_vec));

			// apply braking force
			body->apply_force(ws_pos, -glm::sign(glm::dot(body->get_linear_velocity(), front_vec)) * front_vec * brake_force * brake_force_mult);

			if (i == 0 || i == 1) {

				// apply forward force
				glm::vec3 front_wheel = localspace_wheel_pos[i];
				front_wheel.y -= 0.46;
				auto ws = glm::vec3(body_t * glm::vec4(front_wheel, 1));
				body->apply_force(ws, front_vec * forward_force * forward_force_mult);
			}
		}

		glm::vec3 ddt = (ws_pos - last_ws_pos[i]) / (float)eng->get_dt();

		glm::vec3 local_sidevec = side_vec;
		if (i == 0 || i == 1) {
			// front tires
			glm::vec3 side = glm::vec3(cos(-steer_angle), 0, sin(-steer_angle));
			local_sidevec = glm::mat3(body_t) * side;
		}

		// get lateral velocity
		glm::vec3 side_ddt = glm::dot(ddt, local_sidevec) * local_sidevec;

		float l = glm::length(side_ddt);
		if (l > epsilon_friction && touching_ground) {
			tire_screech_amt += l;
			side_ddt = glm::normalize(side_ddt);
			float normal_force = (d * spring_constant - dxdt * spring_damp);
			if (touching_ground) {
				body->apply_force(ws_pos, -side_ddt * normal_force * friction_coeff);
			}

		}
		//Debug::add_line(ws_pos, ws_pos - side_ddt, COLOR_RED, 0);



		auto wheelpos = localspace_wheel_pos[i] - glm::vec3(0, glm::min(dist, max_spring) - wheel_radius, 0);
		if (!touching_ground)
			wheelpos = localspace_wheel_pos[i] - glm::vec3(0, max_spring - wheel_radius, 0);


		float yang = (i == 0 || i == 1) ? steer_angle : 0.0;
		if (i == 1 || i == 3) yang += PI;

		float xang = wheel_angles[i];
		if (i == 1 || i == 3)xang *= -1;
		wheels[i]->get_owner()->set_ls_transform(
			wheelpos,
			glm::quat(glm::vec3(xang, yang, 0)),
			glm::vec3(1.f));

		last_dist[i] = touching_ground ? dist : max_spring;
		last_ws_pos[i] = ws_pos;
	}


	glm::vec3 velocity = body->get_linear_velocity();
	float len = glm::length(velocity);
	if (len > 0.0001) {
		velocity /= len;
		body->apply_force(body->get_ws_position(), -velocity * len * len * wind_resist);
	}
	//body->apply_force(ws_pos, side_vec  *steer_force* side_force_mult);
}
void CarDriver::update() {
	auto move_vec = move->get_value<glm::vec2>();
	//printf("%f %f\n", move_vec.x, move_vec.y);

	float accel_val = accel->get_value<float>();
	float deccel_val = brake->get_value<float>();
	car->forward_force = accel_val;
	car->brake_force = deccel_val;

	float speed = glm::length(car->body->get_linear_velocity());

	float what_val = glm::max(1.0 - speed * steer_mult, 0.1) * glm::sign(move_vec.x) * glm::pow(glm::abs(move_vec.x), 2.0) * 0.7;

	set_steer_angle = damp_dt_independent(what_val, set_steer_angle, 0.0001, eng->get_dt());

	car->steer_angle = set_steer_angle;
	//car->forward_force = move_vec.y;// move_vec.y;

	glm::vec3 dir{};

	glm::vec3 pos = get_ws_position();
	if (top_view) {

		auto t = car->get_ws_transform();
		auto front = -t[2];


		dir = glm::vec3(front.x * 4.0, 1.5, front.z * 4.0) * 3.0f;
	}
	else
		dir = glm::vec3(0, 2.5, -5);
	pos = pos + dir;

	camera->fov = 60.0;
	camera_pos = damp_dt_independent(pos, camera_pos, 0.02, eng->get_dt());
	{
		dir = glm::normalize(dir);
		glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), dir));
		glm::vec3 up = glm::cross(dir, right);
		glm::mat3 rotationMatrix(right, up, dir);
		glm::quat q(rotationMatrix);
		cam_dir = damp_dt_independent(q, cam_dir, 0.02, eng->get_dt());
	}
	glm::mat4 cam_transform = glm::translate(glm::mat4(1.f), camera_pos) * glm::mat4_cast(cam_dir);

	camera->get_owner()->set_ws_transform(cam_transform);
}
// expected heirarchy:
// Body Object (CarComponent, PhysicsComponent)
//			Wheels (WheelCompoonents)
//

CarGameMode* CarGameMode::instance = nullptr;



float car_pitch_min = 0.5;
float car_pitch_max = 2.0;
float pitchinterp = 0.1;
float sqthresh = 2.0;
void car_pitch_menu()
{
	ImGui::DragFloat("car_pitch_min", &car_pitch_min, 0.01);
	ImGui::DragFloat("car_pitch_max", &car_pitch_max, 0.01);
	ImGui::DragFloat("pitchinterp", &pitchinterp, 0.01);
	ImGui::DragFloat("sqthresh", &sqthresh, 0.01);


}
ADD_TO_DEBUG_MENU(car_pitch_menu);


void CarSoundMaker::update() {
	float target = glm::mix(car_pitch_min, car_pitch_max, car->forward_force);
	cur_pitch = damp_dt_independent(target, cur_pitch, pitchinterp, eng->get_dt());
	engineSound->set_pitch(cur_pitch);

	if (car->tire_screech_amt > sqthresh) {
		if (!is_squel) {
			tireSound->set_pitch(r.RandF(0.9, 1.1));
			is_squel = true;
		}
		squel_start = eng->get_game_time();
		tireSound->set_play(true);
	}
	else if (squel_start + 0.4 < eng->get_game_time()) {
		is_squel = false;
		tireSound->set_play(false);
	}
}


CLASS_H(CannonballC, Component)
public:
	void start() override {
		starttime = eng->get_game_time();
	}
	void update() override {


	}
	void end() override {

	}

	float starttime = 0.0;
	glm::vec3 target_pos{};
	glm::vec3 velocity{};
	float speed = 0.0;
};
CLASS_IMPL(CannonballC);

CLASS_H(CannonC, Component)
public:
	void start() override {
		set_ticking(true);
	}
	void update() override {
		auto playerpos = CarGameMode::instance->thecar->get_ws_position();
		
		if (eng->get_game_time() > next_cannon_time) {
			
			auto level = eng->get_level();
			{
				Entity* e = nullptr;
				auto scope = level->spawn_prefab_deferred(e, projectile.get());
				auto cball = e->get_component<CannonballC>();
				cball->target_pos = playerpos;
				e->set_ws_position(get_ws_position());
			}

			update_next_shoot_time();
		}
	}
	void end() override {

	}
	void update_next_shoot_time() {
		next_cannon_time = eng->get_game_time() + rand.RandF(cannon_fire_min, cannon_fire_max);
	}

	REFLECT()
	AssetPtr<PrefabAsset> projectile;
	Random rand;
	REFLECT()
	float cannon_fire_min = 2.0;
	REFLECT()
	float cannon_fire_max = 3.0;
	REFLECT()
	float next_cannon_time = 0.0;
};
CLASS_IMPL(CannonC);