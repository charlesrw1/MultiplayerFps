#include "PlayerAnimDriver.h"
#include "Entity.h"
#include "Game_Engine.h" // Debug::
#include "ScriptVars.h"

void CharacterGraphDriver::on_init() {
	const ScriptVars_CFG& vars = owner->runtime_dat.cfg->parameters;
	flMovex = vars.find("flMovex");
	flMovey = vars.find("flMovey");
	flSpeed = vars.find("flSpeed");
	bCrouch = vars.find("bCrouch");
	bJumping = vars.find("bJumping");
	bFalling = vars.find("bFalling");
	bMoving = vars.find("bMoving");
	flAimx = vars.find("flAimx");
	flAimy = vars.find("flAimy");

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

	Entity& player = *owner->owner;
	bool mirrored = player.state & PMS_CROUCHING;

	meshoffset = glm::vec3(0.f);

	glm::vec2 next_vel = glm::vec2(player.velocity.x, player.velocity.z);

	glm::vec2 groundvelocity = next_vel;

	bool moving = glm::length(player.velocity) > 0.001;
	glm::vec2 face_dir = glm::vec2(cos(HALFPI - player.rotation.y), sin(HALFPI - player.rotation.y));
	glm::vec2 side = glm::vec2(-face_dir.y, face_dir.x);
	glm::vec2 relmovedir = glm::vec2(glm::dot(face_dir, groundvelocity), glm::dot(side, groundvelocity));

	if (mirrored) relmovedir.y *= -1;

	glm::vec2 grndaccel(player.esimated_accel.x, player.esimated_accel.z);
	glm::vec2 relaccel = glm::vec2(dot(face_dir, grndaccel), dot(side, grndaccel));

	ismoving = glm::length(player.velocity) > 0.1;
	injump = player.state & PMS_JUMPING;


	auto& params = owner->runtime_dat.parameters;

	params.get(bMoving).ival = ismoving;
	params.get(bCrouch).ival = int(player.state & PMS_CROUCHING);
	params.get(bJumping).ival = int(player.state & PMS_JUMPING);
	params.get(bFalling).ival = !bool(player.state & PMS_GROUND);
	params.get(flSpeed).fval = glm::length(relmovedir);
	params.get(flMovex).fval = relmovedir.x;
	params.get(flMovey).fval = relmovedir.y;

}

void CharacterGraphDriver::pre_ik_update(Pose& pose, float dt) {

	Pose* source = Pose_Pool::get().alloc(1);
	*source = pose;
	auto model = owner->get_model();
	auto ent = owner->owner;

	auto& cached_bonemats = owner->get_global_bonemats();

	const int rhand = model->bone_for_name("mixamorig:RightHand");
	const int lhand = model->bone_for_name("mixamorig:LeftHand");

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


}

void CharacterGraphDriver::post_ik_update() {

}
