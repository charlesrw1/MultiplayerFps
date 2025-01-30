#include "CharacterController.h"
#include "BikeEntity.h"
#include "Game/Components/MeshComponent.h"
#include "Assets/AssetDatabase.h"
#include "Render/Model.h"


#include "Framework/MathLib.h"

#include "GameEnginePublic.h"

CLASS_IMPL(BikeEntity);

BikeEntity::BikeEntity()
{
	bike_direction = { 0,0,1 };
	auto m = construct_sub_component<MeshComponent>("BikeMesh");
	m->set_model(GetAssets().find_assetptr_unsafe<Model>("bike.cmdl"));
	//m->disable_physics = true;

	set_ticking(true);
}
void BikeEntity::start()
{
	ccontrol = std::make_unique<CharacterController>(nullptr);
	ccontrol->set_position(get_ws_position());
}

#include "imgui.h"
float bike_friction = 0.02;
float bike_forward_mult = 3.0;
float grnd_fric = 0.1;
float turn_radius = 2.0;
float bike_damp = 0.1;
void bike_vars_menu()
{
	ImGui::InputFloat("friction", &bike_friction);
	ImGui::InputFloat("forward", &bike_forward_mult);
	ImGui::InputFloat("grnd forward", &grnd_fric);
	ImGui::InputFloat("turn_radius", &turn_radius);
	ImGui::InputFloat("bike_damp", &bike_damp);



}
ADD_TO_DEBUG_MENU(bike_vars_menu);
#include "Debug.h"
void BikeEntity::update()
{
	const float dt = eng->get_dt();


	float air_friction = bike_friction;
	float forward_force = forward_strength * bike_forward_mult;
	float change_speed = forward_force - air_friction * forward_speed * forward_speed - grnd_fric;
	forward_speed = forward_speed + change_speed * dt;
	forward_speed = glm::max(forward_speed, 0.f);

	// update steering
	if (glm::abs(turn_strength) > 0.0001) {
		glm::vec3 side = -glm::normalize(glm::cross(bike_direction, glm::vec3(0, 1, 0)));
		auto curvel = bike_direction;
		curvel += side * turn_radius * turn_strength * dt;
		bike_direction = glm::normalize(curvel);
	}

	glm::vec3 velocity = bike_direction * forward_speed;
	velocity.y -= 10.f * eng->get_dt();	// not integrated
	glm::vec3 outvel;
	uint32_t outflags;
	ccontrol->move(velocity * dt, dt, 0.001, outflags,outvel);
	auto pos = ccontrol->get_character_pos();


	const float max_roll = glm::radians(45.f);

	float mult = 10.0;
	float alpha = glm::min(glm::abs(turn_strength) * forward_speed, mult)/ mult;

	float roll = -glm::mix(0.f, max_roll, alpha)*glm::sign(turn_strength);

	current_roll = damp_dt_independent<float>(roll, current_roll, bike_damp, dt);
	glm::quat q(glm::vec3(0, atan2(bike_direction.x, bike_direction.z), 0));
	q *= glm::quat(glm::vec3(0, 0, current_roll));
	set_ws_transform(pos,q,glm::vec3(1.f));
}