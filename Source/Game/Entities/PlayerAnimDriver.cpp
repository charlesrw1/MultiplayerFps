#include "PlayerAnimDriver.h"
#include "Game/Entity.h"
#include "Debug.h" // Debug::

#include "Animation/AnimationUtil.h"
#include "Game/Entities/Player.h"

#include "Framework/AddClassToFactory.h"

CLASS_IMPL(CharacterGraphDriver, AnimatorInstance);

void CharacterGraphDriver::on_init() {
	auto model = get_model();
	
}

void CharacterGraphDriver::on_update(float dt) {

	if (!get_owner()) return;
	if (!get_owner()->cast_to<Player>())
		return;
	Player& player = *get_owner()->cast_to<Player>();


	meshoffset = glm::vec3(0.f);
#if 0
	glm::vec2 next_vel = glm::vec2(player.velocity.x, player.velocity.z);

	glm::vec2 groundvelocity = next_vel;

	bool moving = glm::length(player.velocity) > 0.001;
	glm::vec2 face_dir = glm::vec2(cos(HALFPI - player.rotation.y), sin(HALFPI - player.rotation.y));
	glm::vec2 side = glm::vec2(-face_dir.y, face_dir.x);
	glm::vec2 relmovedir = glm::vec2(glm::dot(face_dir, groundvelocity), glm::dot(side, groundvelocity));

	s
	glm::vec2 grndaccel(player.esimated_accel.x, player.esimated_accel.z);
	glm::vec2 relaccel = glm::vec2(dot(face_dir, grndaccel), dot(side, grndaccel));

#endif
	bool has_input = abs( player.cmd.forward_move ) > 0.01 || abs( player.cmd.lateral_move ) > 0.01;

	ismoving = has_input;
	injump = !player.is_on_ground();

	//auto ray = eng->phys.trace_ray(Ray(player.position, glm::vec3(0, -1, 0)), player.selfid, PF_WORLD);
	//float dist_to_ground = ray.dist < 0.0 ? 100000.0 : ray.dist;
	float dist_to_ground = 0.0;
	bool should_transition_out_of_jump_or_fall = false;// !player.is_on_ground() && player.velocity.y < 0 && ray.dist < 0.6;


	bRunning =(  ismoving && player.is_on_ground());
	bCrouch = (player.is_crouching);
	if (bCrouch)
		vLeftFootPosition = glm::vec3(-0.2, -0.3, 0);
	else
		vLeftFootPosition = {};

	bJumping =(  player.action == Action_State::Jumped && !should_transition_out_of_jump_or_fall);
	bFalling =(  player.action == Action_State::Falling && !should_transition_out_of_jump_or_fall);
	flSpeed = glm::length(1.f);
	flMovex = 0;// relmovedir.x;
	flMovey = 0;// relmovedir.y;

	bLeftFootForwards= left_foot_is_forward;
	bRightFootForwards= !left_foot_is_forward;

}

void CharacterGraphDriver::on_post_update() {

	auto& cached_bonemats = get_global_bonemats();
	auto model = get_model();

	const int lfoot = model->bone_for_name("mixamorig:LeftFoot");
	const int rfoot = model->bone_for_name("mixamorig:RightFoot");

	glm::vec3 lfoot_pos = cached_bonemats[lfoot][3];
	glm::vec3 rfoot_pos = cached_bonemats[rfoot][3];

	left_foot_is_forward = glm::dot(glm::vec3(0, 0, 1), lfoot_pos) < glm::dot(glm::vec3(0, 0, 1), rfoot_pos);
}
