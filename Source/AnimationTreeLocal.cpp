#include "AnimationTreeLocal.h"

#include "AnimationUtil.h"

bool ScriptExpression::evaluate(NodeRt_Ctx& ctx) const
{
	return compilied.execute(*ctx.vars).ival;
}

State* State::get_next_state(NodeRt_Ctx& ctx)
{
	for (int i = 0; i < transitions.size(); i++) {
		// evaluate condition
		if (transitions[i].script.evaluate(ctx))
			return transitions[i].transition_state;
	}
	return this;
}

// Inherited via At_Node

 bool Clip_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	Clip_Node_RT* rt = get_rt<Clip_Node_RT>(ctx);


	if (rt->clip_index == -1) {
		util_set_to_bind_pose(*pose.pose, ctx.model);
		return true;
	}

	const Animation& clip = ctx.set->clips[rt->clip_index];

	if (pose.sync)
		rt->frame = clip.total_duration * pose.sync->normalized_frame;


	if (!pose.sync || pose.sync->first_seen) {
		if (pose.rootmotion_scale >= 0) {
			// want to match character_speed and speed_of_anim
			float speedup = pose.rootmotion_scale * rt->inv_speed_of_anim_root;
			pose.dt *= speedup;
		}

		rt->frame += clip.fps * pose.dt * speed;

		if (rt->frame > clip.total_duration || rt->frame < 0.f) {
			if (loop)
				rt->frame = fmod(fmod(rt->frame, clip.total_duration) + clip.total_duration, clip.total_duration);
			else {
				rt->frame = clip.total_duration - 0.001f;
				rt->stopped_flag = true;
			}
		}

		if (pose.sync) {
			pose.sync->first_seen = false;
			pose.sync->normalized_frame = rt->frame / clip.total_duration;
		}

	}
	util_calc_rotations(ctx.set, rt->frame, rt->clip_index, ctx.model, *pose.pose);


	int root_index = ctx.model->root_bone_index;
	for (int i = 0; i < 3; i++) {
		if (rootmotion[i] == Remove) {
			pose.pose->pos[root_index][i] = rt->root_pos_first_frame[i];
		}
	}

	return !rt->stopped_flag;
}

// Inherited via At_Node

 bool Subtract_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {

	Pose* reftemp = Pose_Pool::get().alloc(1);
	ref->get_pose(ctx, pose);
	GetPose_Ctx pose2 = pose;
	pose2.pose = reftemp;
	source->get_pose(ctx, pose2);
	util_subtract(ctx.model->bones.size(), *reftemp, *pose.pose);
	Pose_Pool::get().free(1);
	return true;
}

 bool Add_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	float lerp = ctx.vars->get(param).fval;

	Pose* addtemp = Pose_Pool::get().alloc(1);
	base_pose->get_pose(ctx, pose);

	GetPose_Ctx pose2 = pose;
	pose2.pose = addtemp;

	diff_pose->get_pose(ctx, pose2);
	util_add(ctx.model->bones.size(), *addtemp, *pose.pose, lerp);
	Pose_Pool::get().free(1);
	return true;
}

 bool Blend_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	float dest = ctx.vars->get(param).fval;

	Blend_Node_RT* rt = get_rt<Blend_Node_RT>(ctx);

	rt->lerp_amt = damp_dt_independent(dest, rt->lerp_amt, damp_factor, pose.dt);


	Pose* addtemp = Pose_Pool::get().alloc(1);
	posea->get_pose(ctx, pose);
	poseb->get_pose(ctx, pose.set_pose(addtemp));
	util_blend(ctx.num_bones(), *addtemp, *pose.pose, rt->lerp_amt);
	Pose_Pool::get().free(1);
	return true;
}

// Inherited via At_Node

 bool Mirror_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	float amt = ctx.vars->get(param).fval;

	auto rt = get_rt<Mirror_Node_RT>(ctx);
	rt->lerp_amt = damp_dt_independent(amt, rt->lerp_amt, damp_time, pose.dt);

	bool ret = input->get_pose(ctx, pose);


	if (rt->lerp_amt >= 0.000001) {
		const Model* m = ctx.model;
		Pose* posemirrored = Pose_Pool::get().alloc(1);
		// mirror the bones
		for (int i = 0; i < m->bones.size(); i++) {
			int from = m->bones[i].remap_index;
			glm::vec3 frompos = pose.pose->pos[from];
			posemirrored->pos[i] = glm::vec3(-frompos.x, frompos.y, frompos.z);
			glm::quat fromquat = pose.pose->q[from];
			posemirrored->q[i] = glm::quat(fromquat.w, fromquat.x, -fromquat.y, -fromquat.z);
		}

		util_blend(m->bones.size(), *posemirrored, *pose.pose, rt->lerp_amt);

		Pose_Pool::get().free(1);

	}
	return ret;
}

 bool Statemachine_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {

	auto rt = get_rt<Statemachine_Node_RT>(ctx);


	// evaluate state machine
	if (rt->active_state == nullptr) {
		rt->active_state = start_state;
		rt->active_weight = 0.0;
	}
	State* next_state;// = (change_to_next_state) ? active_state->next_state : active_state->get_next_state(animator);
	if (rt->change_to_next) next_state = rt->active_state->next_state;
	else {
		next_state = rt->active_state->get_next_state(ctx);
		State* next_state2 = next_state->get_next_state(ctx);
		int infinite_loop_check = 0;
		while (next_state2 != next_state) {
			next_state = next_state2;
			next_state2 = next_state->get_next_state(ctx);
			infinite_loop_check++;

			ASSERT(infinite_loop_check < 100);
		}
	}

	rt->change_to_next = false;
	if (rt->active_state != next_state) {
		if (next_state == rt->fading_out_state) {
			std::swap(rt->active_state, rt->fading_out_state);
			rt->active_weight = 1.0 - rt->active_weight;
		}
		else {
			rt->fading_out_state = rt->active_state;
			rt->active_state = next_state;
			rt->active_state->tree->reset(ctx);
			//fade_in_time = g_fade_out;
			rt->active_weight = 0.f;
		}
		printf("changed to state %s\n", rt->active_state->name.c_str());
	}

	rt->active_weight += pose.dt / fade_in_time;
	if (rt->active_weight > 1.f) {
		rt->active_weight = 1.f;
		rt->fading_out_state = nullptr;
	}

	bool notdone = rt->active_state->tree->get_pose(ctx, pose);

	if (rt->fading_out_state) {
		Pose* fading_out_pose = Pose_Pool::get().alloc(1);


		rt->fading_out_state->tree->get_pose(ctx,
			pose.set_dt(0.f)
			.set_pose(fading_out_pose)
		);
		//printf("%f\n", active_weight);
		assert(rt->fading_out_state != rt->active_state);
		util_blend(ctx.num_bones(), *fading_out_pose, *pose.pose, 1.0 - rt->active_weight);
		Pose_Pool::get().free(1);
	}

	if (!notdone) {	// if done
		if (rt->active_state->next_state) {
			rt->change_to_next = true;
			return true;
		}
		else {
			return false;	// bubble up the finished event
		}
	}
	return true;
}

// Inherited via At_Node

 bool Blend2d_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	//walk_fade_in = g_walk_fade_in;
	//walk_fade_out = g_walk_fade_out;
	//run_fade_in = g_run_fade_in;

	auto rt = get_rt<Directionalblend_Node_RT>(ctx);

	glm::vec2 relmovedir = glm::vec2(
		ctx.vars->get(xparam).fval,
		ctx.vars->get(yparam).fval
	);

	float actual_character_move_speed = glm::length(relmovedir);

	rt->character_blend_weights = damp_dt_independent(relmovedir,
		rt->character_blend_weights, weight_damp, pose.dt);

	float character_ground_speed = glm::length(rt->character_blend_weights);
	float character_angle = PI;
	// blend between angles
	if (character_ground_speed >= 0.0000001f) {
		glm::vec2 direction = rt->character_blend_weights / character_ground_speed;;
		//character_angle = modulo_lerp(atan2f(direction.y, direction.x) + PI, character_angle, TWOPI, 0.94f);
		character_angle = atan2f(direction.y, direction.x) + PI;
	}


	float anglelerp = 0.0;
	int pose1 = 0, pose2 = 1;
	for (int i = 0; i < 8; i++) {
		if (character_angle - PI <= -PI + PI / 4.0 * (i + 1)) {
			pose1 = i;
			pose2 = (i + 1) % 8;
			anglelerp = MidLerp(-PI + PI / 4.0 * i, -PI + PI / 4.0 * (i + 1), character_angle - PI);
			break;
		}
	}

	// highest weighted pose controls syncing
	Pose* scratchposes = Pose_Pool::get().alloc(3);
	if (character_ground_speed <= fade_in) {

		if (anglelerp <= 0.5) {
			directions[pose1]->get_pose(ctx, pose.set_pose(&scratchposes[1]));
			directions[pose2]->get_pose(ctx, pose);
		}
		else {
			directions[pose2]->get_pose(ctx, pose);
			directions[pose1]->get_pose(ctx, pose.set_pose(&scratchposes[1]));
		}
		idle->get_pose(ctx, pose.set_pose(&scratchposes[0]));

		util_blend(ctx.num_bones(), scratchposes[1], *pose.pose, anglelerp);
		float speed_lerp = MidLerp(0.0, fade_in, character_ground_speed);
		util_blend(ctx.num_bones(), scratchposes[0], *pose.pose, 1.0 - speed_lerp);
	}
	else {
		if (anglelerp <= 0.5) {
			directions[pose1]->get_pose(ctx, pose.set_pose(&scratchposes[1]));
			directions[pose2]->get_pose(ctx, pose);
		}
		else {
			directions[pose2]->get_pose(ctx, pose);
			directions[pose1]->get_pose(ctx, pose.set_pose(&scratchposes[1]));
		}

		util_blend(ctx.num_bones(), scratchposes[0], *pose.pose, anglelerp);
	}


	Pose_Pool::get().free(3);

	return true;
}
