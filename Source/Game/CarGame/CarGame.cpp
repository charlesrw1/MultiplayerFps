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
#include "imgui.h"
#include "Physics/Physics2.h"
static float spring_damp = 200;
static float spring_constant = 4000.0;
static float forward_force_mult = 600;
static float brake_force_mult = 500;
static float side_force_mult = 100;
static float friction_coeff = 2.2f;
static float steer_mult = 0.04;
static float wind_resist = 0.80;
static float epsilon_friction = 0.005;
static float max_spring = 0.8;
static float visual_wheel_interp = 0.0002;

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
ADD_TO_DEBUG_MENU(car_debug_menu);

// expected heirarchy:
// Body Object (CarComponent, PhysicsComponent)
//			Wheels (WheelCompoonents)
//

CLASS_H(CarGameMode, Entity)
public:
	static CarGameMode* instance;
	void pre_start() {
		ASSERT(!instance);
		instance = this;
	}
	void end() {
		instance = nullptr;
	}

	Entity* the_car = nullptr;
};
CarGameMode* CarGameMode::instance = nullptr;


CLASS_H(WheelComponent, EntityComponent)
public:
	void start() override {
	}
	void update() override {
	}
	void end() override {
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(WheelComponent)
		REG_BOOL(front,PROP_DEFAULT, "0"),
		REG_BOOL(left,PROP_DEFAULT, "0"),
		END_PROPS(WheelComponent)
	}
	bool front = false;
	bool left = false;
};

CLASS_H(CarComponent, EntityComponent)
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
	void update() {
		auto body_t = body->get_ws_transform();
		glm::vec3 up_vec = glm::mat3(body_t) * glm::vec3(0, 1, 0);
		glm::vec3 side_vec = glm::mat3(body_t) * glm::vec3(1, 0, 0);
		glm::vec3 front_vec = glm::mat3(body_t) * glm::vec3(0, 0, 1);

		//const float max_spring = 1.0;
		const float wheel_radius = 0.46;

			float current_speed = glm::length(body->get_linear_velocity());
			const float delta_angle = (current_speed * eng->get_dt()) / wheel_radius;

		//Debug::add_line(body_t[3], glm::vec3(body_t[3]) + up_vec*2.f, COLOR_PINK, 0);
		for (int i = 0; i < 4; i++) {
			auto w = wheels[i];
			auto o = w->get_owner();
			auto ws_pos = glm::vec3(body_t * glm::vec4(localspace_wheel_pos[i],1));

			
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

			//                                                                                                                const float max_spring = 1.0;
			const float wheel_radius = 0.46;

			float d = glm::max(0.f,max_spring - dist);
			bool touching_ground = d > 0.f;
			if (dist < -0.1)touching_ground = false;

			float dxdt = (dist - last_dist[i]) / (float)eng->get_dt();

			
			
			if (touching_ground) {
				body->apply_force(ws_pos, up_vec * (d * spring_constant - dxdt * spring_damp));

				wheel_angles[i] += delta_angle * glm::sign(glm::dot(body->get_linear_velocity(),front_vec));

				// apply braking force
				body->apply_force(ws_pos, -glm::sign(glm::dot(body->get_linear_velocity(),front_vec)) * front_vec * brake_force * brake_force_mult);
			
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
				side_ddt = glm::normalize(side_ddt);
				float normal_force = (d * spring_constant - dxdt * spring_damp);
				if (touching_ground) {
					body->apply_force(ws_pos, -side_ddt * normal_force * friction_coeff);
				}

			}
			//Debug::add_line(ws_pos, ws_pos - side_ddt, COLOR_RED, 0);



			auto wheelpos = localspace_wheel_pos[i] - glm::vec3(0, glm::min(dist,max_spring) - wheel_radius, 0);
			if (!touching_ground)
				wheelpos = localspace_wheel_pos[i] - glm::vec3(0, max_spring - wheel_radius, 0);


			float yang = (i == 0 || i == 1) ? steer_angle : 0.0;
			if (i == 1 || i == 3) yang += PI;

			float xang = wheel_angles[i];
			if (i == 1 || i == 3)xang *= -1;
			wheels[i]->get_owner()->set_ls_transform(
				wheelpos,
				glm::quat(glm::vec3(xang,yang,0)),
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

	static const PropertyInfoList* get_props() = delete;

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
CLASS_IMPL(CarComponent);
CLASS_IMPL(WheelComponent);


CLASS_H(CarDriver,EntityComponent)
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
		for(auto d : GetGInput().get_connected_devices())
			if (d->get_type() == InputDeviceType::Controller) {
				inputUser->assign_device(d);
				break;
			}
	}
	void update() override {
		auto move_vec = move->get_value<glm::vec2>();
		//printf("%f %f\n", move_vec.x, move_vec.y);

		float accel_val = accel->get_value<float>();
		float deccel_val = brake->get_value<float>();
		car->forward_force = accel_val;
		car->brake_force = deccel_val;

		float speed = glm::length(car->body->get_linear_velocity());

		float what_val = glm::max(1.0-speed*steer_mult,0.1)* glm::sign(move_vec.x) * glm::pow(glm::abs(move_vec.x), 2.0) * 0.7;

		set_steer_angle = damp_dt_independent(what_val, set_steer_angle, 0.0001, eng->get_dt());

		car->steer_angle = set_steer_angle;
		//car->forward_force = move_vec.y;// move_vec.y;

		glm::vec3 dir{};

		glm::vec3 pos = get_ws_position();
		if (top_view) {

			auto t = car->get_ws_transform();
			auto front = -t[2];


			dir =  glm::vec3(front.x*4.0,1.5,front.z*4.0)*3.0f;
		}
		else
			dir =  glm::vec3(0, 2.5, -5);
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
CLASS_IMPL(CarDriver);

// HAD TO DO IT TO EM
CLASS_H(PedestrianComponent, EntityComponent)
public:
	void start() override {

	}
	void update() override {

	}
	void end() override {

	}

	void enable_ragdoll() {

	}
};