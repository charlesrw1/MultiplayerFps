#include "RuntimeNodesNew2.h"
#include "Animation/AnimationUtil.h"

void agClipNode::reset()
{
	anim_time = 0.0;
}


static void get_clip_pose_shared(agGetPoseCtx& ctx, const AnimationSeq* clip,
	bool has_sync_group, StringName syncgroupname, sync_opt SyncOption, bool loop, const BoneIndexRetargetMap* remap,
	float speed, float& anim_time, bool& stopped_flag, const Node_CFG* owner)
{
	// synced update
	if (has_sync_group) {
		SyncGroupData& sync = ctx.find_sync_group(syncgroupname);
		if (sync.is_this_first_update()) {
			// do nothing
		}
		else {
			anim_time = sync.time.get() * clip->duration;	// normalized time, TODO: sync markers
		}
		const float time_to_evaluate_sequence = anim_time;
		if (sync.should_write_new_update_weight(SyncOption, 0.5/*TODO*/)) {
			anim_time += ctx.dt * speed * 0.8f;	// fixme
			if (anim_time > clip->duration || anim_time < 0.f) {
				if (loop)
					anim_time = fmod(fmod(anim_time, clip->duration) + clip->duration, clip->duration);
				else {
					anim_time = clip->duration - 0.001f;
					stopped_flag = true;
				}
			}
			sync.write_to_update_time(SyncOption, 0.5/*TODO*/, owner, Percentage(anim_time, clip->duration));
			util_calc_rotations(&ctx.get_skeleton(), clip, time_to_evaluate_sequence, remap, *ctx.pose);
		}
	}
	// unsynced update
	else {
		const float time_to_evaluate_sequence = anim_time;
		anim_time += ctx.dt * speed * 0.8f;	// see above
		if (anim_time > clip->duration || anim_time < 0.f) {
			if (loop)
				anim_time = fmod(fmod(anim_time, clip->duration) + clip->duration, clip->duration);
			else {
				anim_time = clip->duration - 0.001f;
				stopped_flag = true;
			}
		}
		util_calc_rotations(&ctx.get_skeleton(), clip, time_to_evaluate_sequence, remap, *ctx.pose);
	}
}

void agClipNode::get_pose(agGetPoseCtx& ctx)
{
	if (has_init) {
		if (!seq||!clipFrom)
			throw std::runtime_error("agClipNode: no sequence");

		if(!clipFrom->get_skel())
			throw std::runtime_error("agClipNode: sequence without skel");
		remap = ctx.get_skeleton().get_remap(clipFrom->get_skel());
		has_init = true;
	}

	const float playSpeed = speed.get_float(ctx);

	bool stopped_flag = false;
	get_clip_pose_shared(
		ctx, seq, !syncGroup.is_null(),syncGroup, syncType, looping, remap, playSpeed, anim_time, stopped_flag, nullptr);

	ctx.add_playing_clip(this);
}

void agClipNode::set_clip(const Model& m, const string& clipName)
{
	assert(m.get_skel());
	seq = m.get_skel()->find_clip(clipName);
	clipFrom = &m;
}

void agClipNode::set_clip(const AnimationSeqAsset* asset)
{
	seq = asset->seq;
	clipFrom = asset->srcModel.get();
}

void agBlendNode::reset()
{
	input0->reset();
	input1->reset();
}

void agBlendNode::get_pose(agGetPoseCtx& ctx)
{
	const float alphaVal = alpha.get_float(ctx);
	if (alphaVal <= 0.00001) {
		input0->get_pose(ctx);
	}
	else if (alphaVal >= 0.99999) {
		input1->get_pose(ctx);
	}
	else {
		agGetPoseCtx other(ctx);
		input0->get_pose(ctx);
		input1->get_pose(other);
		util_blend(ctx.get_num_bones(), *other.pose, *ctx.pose, alphaVal);
	}
}

void agAddNode::reset()
{
	input0->reset();
	input1->reset();
}

void agAddNode::get_pose(agGetPoseCtx& ctx)
{
	const float alphaVal = alpha.get_float(ctx);
	if (alphaVal <= 0.00001) {
		input0->get_pose(ctx);
	}
	else {
		agGetPoseCtx other(ctx);
		input0->get_pose(ctx);
		input1->get_pose(other);
		util_add(ctx.get_num_bones(), *other.pose, *ctx.pose, alphaVal);
	}
}

SyncGroupData& agGetPoseCtx::find_sync_group(StringName name) const
{
	// TODO: insert return statement here
	return object.find_or_create_sync_group(name);
}

float ValueType::get_float(agGetPoseCtx& ctx)
{
	if (std::holds_alternative<float>(value))
		return std::get<float>(value);
	else if (std::holds_alternative<StringName>(value))
		return ctx.get_float_var(std::get<StringName>(value));
	throw std::runtime_error("ValueType::get_float: doesn't hold float");
}

int ValueType::get_int(agGetPoseCtx& ctx)
{
	if (std::holds_alternative<int>(value))
		return std::get<int>(value);
	else if (std::holds_alternative<StringName>(value))
		return ctx.get_int_var(std::get<StringName>(value));
	throw std::runtime_error("ValueType::get_int: doesn't hold int");
}

int ValueType::get_bool(agGetPoseCtx& ctx)
{
	if (std::holds_alternative<bool>(value))
		return std::get<bool>(value);
	else if (std::holds_alternative<StringName>(value))
		return ctx.get_bool_var(std::get<StringName>(value));
	throw std::runtime_error("ValueType::get_bool: doesn't hold bool");
}

glm::vec3 ValueType::get_vec3(agGetPoseCtx& ctx)
{
	if (std::holds_alternative<glm::vec3>(value))
		return std::get<glm::vec3>(value);
	else if (std::holds_alternative<StringName>(value))
		return ctx.get_vec3_var(std::get<StringName>(value));
	throw std::runtime_error("ValueType::get_vec3: doesn't hold vec3");
}

void agIk2Bone::reset()
{
	input->reset();
}


static glm::mat4 build_global_transform_for_bone_index(Pose* pose, const MSkeleton* skel, int index)
{
	const int ALLOCED_MATS = 36;
	glm::mat4 mats[ALLOCED_MATS];

	int count = 0;
	while (index != -1) {
		assert(count < 36);
		glm::mat4x4 matrix = glm::mat4_cast(pose->q[index]);
		matrix[3] = glm::vec4(pose->pos[index], 1.0);
		mats[count++] = matrix;
		index = skel->get_bone_parent(index);
	}
	for (int i = count - 2; i >= 0; i--) {
		mats[i] = mats[i + 1] * mats[i];
	}
	glm::mat4 final_ = mats[0];
	return final_;
}


void agIk2Bone::get_pose(agGetPoseCtx& ctx)
{
	if (!has_init) {
		bone_idx = ctx.get_skeleton().get_bone_index(bone_name);
		if (bone_idx==-1) {
			sys_print(Error, " agIk2Bone::get_pose: model doesnt have bone %s\n", bone_name.get_c_str());
			throw std::runtime_error("doesn't have bone");
		}
		if (ik_in_bone_space) {
			other_bone_idx = ctx.get_skeleton().get_bone_index(other_bone);
			if(other_bone_idx==-1) {
				sys_print(Error, " agIk2Bone::get_pose: other model doesnt have bone %s\n", bone_name.get_c_str());
				throw std::runtime_error("doesn't have other bone");
			}
		}
		has_init = true;
	}
	const float alphaVal = alpha.get_float(ctx);
	vec3 tagetVec = target.get_vec3(ctx);

	input->get_pose(ctx);
	auto& pose = *ctx.pose;
	// build up global matrix when needed instead of recreating it every step
   // not sure if this is optimal, should profile different ways to pass around pose
	const int ALLOCED_MATS = 36;
	glm::mat4 mats[ALLOCED_MATS];
	int indicies[36];

	auto& skel = ctx.get_skeleton();
	int index = bone_idx;
	int count = 0;
	while (index != -1) {
		assert(count < ALLOCED_MATS);
		glm::mat4x4 matrix = glm::mat4_cast(pose.q[index]);
		matrix[3] = glm::vec4(pose.pos[index], 1.0);
		mats[count++] = matrix;
		indicies[count - 1] = index;
		index = skel.get_bone_parent(index);
	}
	for (int i = count - 2; i >= 0; i--) {
		mats[i] = mats[i + 1] * mats[i];
	}

	if (count <= 2) {
		sys_print(Error, "agIk2Bone::get_pose: ik attempted on some root bone %s\n", bone_name.get_c_str());
		throw std::runtime_error("ik error");
	}

	auto ikfunctor = [&](glm::quat& outlocal1, glm::quat& outlocal2, vec3 target, bool print = false) {

		const float dist_eps = 0.0001f;
		// GLOBAL positions
		vec3 a = mats[2] * glm::vec4(0.0, 0.0, 0.0, 1.0);
		vec3 b = mats[1] * glm::vec4(0.0, 0.0, 0.0, 1.0);
		vec3 c = mats[0] * glm::vec4(0.0, 0.0, 0.0, 1.0);
		float dist = length(c - target);
		if (dist <= dist_eps) {
			return;
		}

		//Debug::add_sphere(ent_transform * vec4(a, 1.0), 0.01, COLOR_GREEN, 0.0, true);
		//Debug::add_sphere(ent_transform * vec4(b, 1.0), 0.01, COLOR_BLUE, 0.0, true);
		//Debug::add_sphere(ent_transform * vec4(c, 1.0), 0.01, COLOR_CYAN, 0.0, true);

		glm::quat a_global = glm::quat_cast(mats[2]);
		glm::quat b_global = glm::quat_cast(mats[1]);
		util_twobone_ik(a, b, c, target, vec3(0.0, 0.0, 1.0), a_global, b_global, outlocal2, outlocal1);
	};

	glm::quat target_rotation = {};

	if (ik_in_bone_space) {
		glm::mat4 matrix = build_global_transform_for_bone_index(&pose, &skel, other_bone_idx);
		tagetVec = matrix * glm::vec4(tagetVec, 1.0);
		if (take_rotation_of_other)
			target_rotation = glm::quat_cast(matrix);
	}


	int index1 = indicies[1];
	int index2 = indicies[2];

	ikfunctor(pose.q[index1], pose.q[index2], tagetVec, false);


	if (ik_in_bone_space&&take_rotation_of_other) {
		// compute the global rotation now
		glm::quat q = {};
		if (count >= 4)
			q = glm::quat_cast(mats[3]);
		q = q * pose.q[index2];
		q = q * pose.q[index1];

		pose.q[bone_idx] = glm::inverse(q) * target_rotation;
	}
}

void agModifyBone::reset()
{
	input->reset();
}

void agModifyBone::get_pose(agGetPoseCtx& ctx)
{
	if (!has_init) {
		this->bone_index = ctx.get_skeleton().get_bone_index(boneName);
		if (bone_index == -1) {
			sys_print(Error, "agModifyBone::get_pose: no bone found %s\n", boneName.get_c_str());
			throw std::runtime_error("couldnt find bone");
		}
		has_init = true;
	}

	input->get_pose(ctx);
	const int MYBONEINDEX = this->bone_index;

	// build up global matrix when needed instead of recreating it every step
	// not sure if this is optimal, should profile different ways to pass around pose
	const int ALLOCED_MATS = 36;
	glm::mat4 mats[ALLOCED_MATS];
	const MSkeleton& skel = ctx.get_skeleton();
	Pose& pose = *ctx.pose;
	bool more_than_one = false;
	{
		int count = 0;
		int index = MYBONEINDEX;
		while (index != -1) {
			assert(count < ALLOCED_MATS);
			glm::mat4x4 matrix = glm::mat4_cast(pose.q[index]);
			matrix[3] = glm::vec4(pose.pos[index], 1.0);
			mats[count++] = matrix;
			index = skel.get_bone_parent(index);
		}
		for (int i = count - 2; i >= 0; i--) {
			mats[i] = mats[i + 1] * mats[i];
		}
		more_than_one = count > 1;
	}

	const glm::vec3 set_pos = translationVal.get_vec3(ctx);// ->get_value<glm::vec3>(ctx);
	const glm::quat set_rot = glm::quat(rotationVal.get_vec3(ctx));// rotation->get_value<glm::quat>(ctx);

	const glm::vec3 global_pos = mats[0][3];
	const glm::quat global_rot = glm::quat_cast(mats[0]);

	const bool apply_position = translation != ModifyBoneType::None;
	const bool apply_position_meshspace = translation == ModifyBoneType::Meshspace || translation == ModifyBoneType::MeshspaceAdd;
	const bool apply_position_additive = translation == ModifyBoneType::LocalspaceAdd || translation == ModifyBoneType::MeshspaceAdd;

	const bool apply_rotation = rotation != ModifyBoneType::None;
	const bool apply_rotation_meshspace = rotation == ModifyBoneType::Meshspace || rotation == ModifyBoneType::MeshspaceAdd;
	const bool apply_rotation_additive = rotation == ModifyBoneType::LocalspaceAdd || rotation == ModifyBoneType::MeshspaceAdd;


	if (apply_position) {
		if (apply_position_meshspace) {
			if (apply_position_additive)
				mats[0][3] = glm::vec4(global_pos + set_pos, 1.0f);
			else
				mats[0][3] = glm::vec4(set_pos, 1.0f);
		}
		else {
			if (apply_position_additive)
				pose.pos[MYBONEINDEX] += set_pos;
			else
				pose.pos[MYBONEINDEX] = set_pos;
		}
	}
	if (apply_rotation) {
		if (apply_rotation_meshspace) {
			if (apply_rotation_additive) {
				glm::quat q = set_rot * global_rot;
				glm::vec4 lastcol = mats[0][3];
				mats[0] = glm::mat4_cast(q);
				mats[0][3] = lastcol;
			}
			else {
				glm::quat q = set_rot;
				glm::vec4 lastcol = mats[0][3];
				mats[0] = glm::mat4_cast(q);
				mats[0][3] = lastcol;
			}
		}
		else {
			if (apply_rotation_additive)
				pose.q[MYBONEINDEX] = set_rot * pose.q[MYBONEINDEX];
			else
				pose.q[MYBONEINDEX] = set_rot;
		}
	}

	if (apply_position_meshspace || apply_rotation_meshspace) {
		// go from global to local again
		if (more_than_one)
			mats[0] = glm::inverse(mats[1]) * mats[0];
		if (apply_rotation && apply_rotation_meshspace)
			pose.q[MYBONEINDEX] = glm::quat_cast(mats[0]);
		if (apply_position && apply_position_meshspace)
			pose.pos[MYBONEINDEX] = mats[0][3];
	}
}

void agCopyBone::reset()
{
	input->reset();
}

void agCopyBone::get_pose(agGetPoseCtx& ctx)
{
	if (!has_init) {
		source_bone_idx = ctx.get_skeleton().get_bone_index(sourceBone);
		target_bone_idx = ctx.get_skeleton().get_bone_index(targetBone);

		if (source_bone_idx == -1 || target_bone_idx == -1) {
			sys_print(Error, "agCopyBone::get_pose: couldn't find bone\n");
			throw std::runtime_error("no bone found");
		}

		has_init = true;
	}

	input->get_pose(ctx);

	const MSkeleton& skel = ctx.get_skeleton();
	Pose& pose = *ctx.pose;
	if (copyBonespace) {
		// simple, just copy over pos/quat
		pose.q[target_bone_idx] = pose.q[source_bone_idx];
		pose.pos[target_bone_idx] = pose.pos[source_bone_idx];
		pose.scale[target_bone_idx] = pose.scale[source_bone_idx];
	}
	else {
		glm::mat4 mat = build_global_transform_for_bone_index(&pose, &skel, source_bone_idx);
		pose.q[target_bone_idx] = glm::quat_cast(mat);
		pose.pos[target_bone_idx] = mat[3];
		pose.scale[target_bone_idx] = glm::length(mat[0]);

	}
}

float agGetPoseCtx::get_float_var(StringName name) const
{
	auto var = object.get_float_variable(name);
	if (var.has_value()) 
		return var.value();
	auto curve = object.get_curve_value(name);
	return curve.value_or(0.f);
}

glm::vec3 agGetPoseCtx::get_vec3_var(StringName name) const
{
	auto var = object.get_vec3_variable(name);
	if (var.has_value()) 
		return var.value();
	sys_print(Error, "agGetPoseCtx::get_vec3_var: no variable exists: %s\n", name.get_c_str());
	throw std::runtime_error("no variable exists");
}

bool agGetPoseCtx::get_bool_var(StringName name) const
{
	auto var = object.get_bool_variable(name);
	if (var.has_value()) 
		return var.value();
	sys_print(Error, "agGetPoseCtx::get_bool_var: no variable exists: %s\n", name.get_c_str());
	throw std::runtime_error("no variable exists");
}

int agGetPoseCtx::get_int_var(StringName name) const
{
	auto var = object.get_int_variable(name);
	if (var.has_value()) 
		return var.value();
	sys_print(Error, "agGetPoseCtx::get_int_var: no variable exists: %s\n", name.get_c_str());
	throw std::runtime_error("no variable exists");
}
