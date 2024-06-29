#include "PlayerAnimDriver.h"
#include "Entity.h"
#include "Game_Engine.h" // Debug::
#include "Animation/Runtime/ControlParams.h"
#include "Animation/AnimationUtil.h"
#include "Player.h"

#define FIND_VAR(x) x = vars.find_no_handle(NAME(#x))

void CharacterGraphDriver::on_init() {
	const ControlParam_CFG& vars = *owner->runtime_dat.cfg->get_control_params();

	FIND_VAR(flAimx);
	FIND_VAR(flAimy);
	FIND_VAR(bRunning);
	FIND_VAR(bFalling);
	FIND_VAR(bJumping);
	FIND_VAR(bCrouch);
	FIND_VAR(flSpeed);
	FIND_VAR(flStopPercentage);
	FIND_VAR(bTurnInPlaceLeft);
	FIND_VAR(bTurnInPlaceRight);
	FIND_VAR(bLeftFootForwards);
	FIND_VAR(bRightFootForwards);


	auto model = owner->get_model();
	owner->get_controller(bone_controller_type::lhand).bone_index = model->bone_for_name("mixamorig:LeftHand");
	owner->get_controller(bone_controller_type::lhand).use_two_bone_ik = true;

	owner->get_controller(bone_controller_type::rhand).bone_index = model->bone_for_name("mixamorig:RightHand");
	owner->get_controller(bone_controller_type::rhand).use_two_bone_ik = true;

	owner->get_controller(bone_controller_type::rfoot).bone_index = model->bone_for_name("mixamorig:LeftFoot");
	owner->get_controller(bone_controller_type::rfoot).use_two_bone_ik = true;

	owner->get_controller(bone_controller_type::lfoot).bone_index = model->bone_for_name("mixamorig:RightFot");
	owner->get_controller(bone_controller_type::lfoot).use_two_bone_ik = true;
}

void CharacterGraphDriver::on_update(float dt) {

	if (!owner->owner) return;

	Player& player = *(Player*)owner->owner;

	meshoffset = glm::vec3(0.f);

	glm::vec2 next_vel = glm::vec2(player.velocity.x, player.velocity.z);

	glm::vec2 groundvelocity = next_vel;

	bool moving = glm::length(player.velocity) > 0.001;
	glm::vec2 face_dir = glm::vec2(cos(HALFPI - player.rotation.y), sin(HALFPI - player.rotation.y));
	glm::vec2 side = glm::vec2(-face_dir.y, face_dir.x);
	glm::vec2 relmovedir = glm::vec2(glm::dot(face_dir, groundvelocity), glm::dot(side, groundvelocity));


	glm::vec2 grndaccel(player.esimated_accel.x, player.esimated_accel.z);
	glm::vec2 relaccel = glm::vec2(dot(face_dir, grndaccel), dot(side, grndaccel));


	bool has_input = abs( player.cmd.forward_move ) > 0.01 || abs( player.cmd.lateral_move ) > 0.01;

	ismoving = has_input;
	injump = !player.is_on_ground();


	auto& params = *owner->runtime_dat.cfg->get_control_params();
	auto vars = &owner->runtime_dat.vars;

	auto ray = eng->phys.trace_ray(Ray(player.position, glm::vec3(0, -1, 0)), player.selfid, PF_WORLD);
	float dist_to_ground = ray.dist < 0.0 ? 100000.0 : ray.dist;

	bool should_transition_out_of_jump_or_fall = !player.is_on_ground() && player.velocity.y < 0 && ray.dist < 0.6;


	params.set_bool_nh(vars, bRunning, ismoving && player.is_on_ground());
	params.set_bool_nh(vars, bCrouch, player.is_crouching);
	params.set_bool_nh(vars, bJumping, player.action == Action_State::Jumped && !should_transition_out_of_jump_or_fall);
	params.set_bool_nh(vars, bFalling, player.action == Action_State::Falling && !should_transition_out_of_jump_or_fall);
	params.set_float_nh(vars, flSpeed,glm::length(relmovedir));
	params.set_float_nh(vars, flMovex ,relmovedir.x);
	params.set_float_nh(vars, flMovey, relmovedir.y);

	params.set_bool_nh(vars, bLeftFootForwards, left_foot_is_forward);
	params.set_bool_nh(vars, bRightFootForwards, !left_foot_is_forward);

}

void CharacterGraphDriver::pre_ik_update(Pose& pose, float dt) {

	auto& cached_bonemats = owner->get_global_bonemats();
	auto model = owner->get_model();

	const int lfoot = model->bone_for_name("mixamorig:LeftFoot");
	const int rfoot = model->bone_for_name("mixamorig:RightFoot");

	glm::vec3 lfoot_pos = cached_bonemats[lfoot][3];
	glm::vec3 rfoot_pos = cached_bonemats[rfoot][3];

	left_foot_is_forward = glm::dot(glm::vec3(0, 0, 1), lfoot_pos) < glm::dot(glm::vec3(0, 0, 1), rfoot_pos);
	return;
#if 0
	Pose* source = Pose_Pool::get().alloc(1);
	*source = pose;
	auto ent = owner->owner;



	glm::vec3 rhand_target = cached_bonemats[rhand] * glm::vec4(0.0, 0.0, 0.0, 1.0);
	glm::vec3 lhand_target = cached_bonemats[lhand] * glm::vec4(0.0, 0.0, 0.0, 1.0);

	glm::mat4 ent_transform = ent->get_world_transform() * model->skeleton_root_transform;
	{
		glm::vec3 world_hand = ent_transform * glm::vec4(rhand_target, 1.0);
		Debug::add_sphere(world_hand, 0.01, COLOR_RED, 0.0, true);
		world_hand = ent_transform * glm::vec4(lhand_target, 1.0);
		Debug::add_sphere(world_hand, 0.01, COLOR_RED, 0.0, true);
	}

	owner->get_controller(bone_controller_type::rhand).enabled = true;
	owner->get_controller(bone_controller_type::rhand).position = rhand_target;


	owner->get_controller(bone_controller_type::lhand).enabled = true;
	owner->get_controller(bone_controller_type::lhand).position = lhand_target;

	owner->get_controller(bone_controller_type::rfoot).enabled = false;
	owner->get_controller(bone_controller_type::lfoot).enabled = false;

	// feet ik
	if (!ismoving && !injump)
	{
		const int rightfoot = model->bone_for_name("mixamorig:RightFoot");
		const int rightleg = model->bone_for_name("mixamorig:RightLeg");
		const int rightlegupper = model->bone_for_name("mixamorig:RightUpLeg");
		const int leftfoot = model->bone_for_name("mixamorig:LeftFoot");
		const int leftleg = model->bone_for_name("mixamorig:LeftLeg");
		const int leftlegupper = model->bone_for_name("mixamorig:LeftUpLeg");

		glm::vec3 worldspace_rfoot = ent_transform * cached_bonemats[rightfoot] * glm::vec4(0.f, 0.f, 0.f, 1.f);
		glm::vec3 worldspace_lfoot = ent_transform * cached_bonemats[leftfoot] * glm::vec4(0.f, 0.f, 0.f, 1.f);
		Ray r;
		r.pos = worldspace_rfoot + vec3(0, 2, 0);
		r.dir = vec3(0, -1, 0);
		RayHit hit = eng->phys.trace_ray(r, -1, PF_WORLD);
		Debug::add_box(hit.pos, vec3(0.2), COLOR_PINK, 0.f);

		float rfootheight = hit.pos.y - ent->position.y;
		r.pos = worldspace_lfoot + vec3(0, 2, 0);
		hit = eng->phys.trace_ray(r, -1, PF_WORLD);

		Debug::add_box(hit.pos, vec3(0.2), COLOR_PINK, 0.f);

		float lfootheight = hit.pos.y - ent->position.y;
		// now need to offset mesh so that both hiehgts are >= 0
		float add = glm::min(lfootheight, rfootheight);

		//printf("add %f\n", add);

		meshoffset = glm::vec3(0, add, 0);

		glm::mat4 invent = glm::inverse(ent_transform);

		// now do ik for left and right feet
		glm::vec3 lfoottarget = invent * glm::vec4(worldspace_lfoot + vec3(0, lfootheight - add, 0), 1.f);
		glm::vec3 rfoottarget = invent * glm::vec4(worldspace_rfoot + vec3(0, rfootheight - add, 0), 1.f);

		owner->get_controller(bone_controller_type::rfoot).enabled = true;
		owner->get_controller(bone_controller_type::rfoot).position = rfoottarget;

		owner->get_controller(bone_controller_type::lfoot).enabled = true;
		owner->get_controller(bone_controller_type::lfoot).position = lfoottarget;
	}


	Pose_Pool::get().free(1);
#endif
}

void CharacterGraphDriver::post_ik_update() {

}
