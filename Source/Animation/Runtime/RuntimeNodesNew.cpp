

struct OutGetClip
{
	float next_time = 0.0;
};

static OutGetClip get_clip_pose_shared_new(atUpdateStack& context, const AnimationSeq* clip,
	bool has_sync_group, StringName sync_group_name, sync_opt SyncOption, bool loop, const BoneIndexRetargetMap* remap,
	float speed, const float prev_anim_time, const PoseNodeInst* owner)
{
	const float EPSILON = 0.001f;
	assert(clip);

	const atGraphContext& graph = context.graph;
	const MSkeleton& skeleton = graph.skeleton;

	// synced update
	if (has_sync_group) {
		SyncGroupData& sync = graph.find_sync_group(sync_group_name);

		float next_anim_time = prev_anim_time;
		if (sync.is_this_first_update()) {
			// do nothing
		}
		else {
			next_anim_time = sync.time.get() * clip->duration;	// normalized time, TODO: sync markers
		}
		const float time_to_evaluate_sequence = next_anim_time;

		if (sync.should_write_new_update_weight(SyncOption, 0.5/*TODO*/)) {

			next_anim_time += context.graph.dt * speed * 0.8f;	// HACK !!!!!!! fixme, should be 24 fps instead of 30 but setting it breaks stuff, just do this for now 

			if (next_anim_time > clip->duration || next_anim_time < 0.f) {
				if (loop)
					next_anim_time = fmod(fmod(next_anim_time, clip->duration) + clip->duration, clip->duration);
				else {
					next_anim_time = clip->duration - EPSILON;
				}
			}
			assert(0);
			sync.write_to_update_time(SyncOption, 0.5/*TODO*/, nullptr/*FIXME*/, Percentage(next_anim_time, clip->duration));
			util_calc_rotations(&skeleton, clip, time_to_evaluate_sequence, remap, *context.pose);
		}
		return { next_anim_time };
	}
	// unsynced update
	else {
		const float time_to_evaluate_sequence = prev_anim_time;
		
		float next_anim_time = prev_anim_time;
		next_anim_time += context.graph.dt * speed * 0.8f;	// see above
		if (next_anim_time > clip->duration || next_anim_time < 0.f) {
			if (loop)
				next_anim_time = fmod(fmod(next_anim_time, clip->duration) + clip->duration, clip->duration);
			else {
				next_anim_time = clip->duration - EPSILON;
			}
		}
		util_calc_rotations(&skeleton, clip, time_to_evaluate_sequence, remap, *context.pose);

		return { next_anim_time };
	}
}

void atSlotPlay::Inst::get_pose(atUpdateStack& context) {
	

}


atValueNode* atInitContext::find_value(int nodeid) {
	ClassBase* n = find_node_in_tree_nodes(nodeid);
	if (!n)
		return nullptr;
	return n->cast_to<atValueNode>();
}


PoseNodeInst* atCreateInstContext::create_inst(int nodeid) {
	ClassBase* n = find_node_in_tree_nodes(nodeid);
	if (!n)
		return nullptr;
	auto poseN = n->cast_to<AnimTreePoseNode>();
	if (!poseN)
		return nullptr;
	auto inst = poseN->create_inst(*this);
	assert(inst);
	created_nodes.push_back(uptr<PoseNodeInst>(inst));
	return inst;
}
atValueNode* atCreateInstContext::find_value(int nodeid) {
	ClassBase* n = find_node_in_tree_nodes(nodeid);
	if (!n) return nullptr;
	return n->cast_to<atValueNode>();
}

void atClipNode::Inst::get_pose(atUpdateStack& context)
{
	if (!clip) {
		util_set_to_bind_pose(*context.pose, &context.graph.skeleton);
		return;
	}
	auto [next_anim_time] = get_clip_pose_shared_new(context, clip, has_sync_group(), owner.data.SyncGroup, owner.data.SyncOption, owner.data.loop, remap, get_speed(context), anim_time, this);
	anim_time = next_anim_time;
}
float atClipNode::Inst::get_clip_length() const {
	return clip->get_duration();
}
bool atClipNode::Inst::has_sync_group() const {
	return owner.data.has_sync_group();
}
float atClipNode::Inst::get_speed(atUpdateStack& context) const {
	return speed->get_float(context);
}
void atClipNode::Inst::reset() {
	anim_time = 0.0;
}

SyncGroupData& atGraphContext::find_sync_group(StringName name) const
{
	return obj.find_or_create_sync_group(name);
}

void SampledAnimCurveBuffer::set_curve(StringName s, float f) {
	for (auto& [name, val] : vals) {
		if (s == name) {
			val = f;
			return;
		}
	}
	vals.push_back({ s,f });
}

float SampledAnimCurveBuffer::get_curve(StringName s) {
	for (auto& [name, val] : vals) {
		if (name == s) return val;
	}
	return 0.f;
}

void atComposePoses::Inst::get_pose(atUpdateStack& context) {
	const float alphaVal = alpha->get_float(context);
	if (is_additive) {
		if (alphaVal <= 0.00001) {
			pose0->get_pose(context);
		}
		else {
			atUpdateStack addStack(context);
			pose1->get_pose(addStack);
			pose0->get_pose(context);
			util_add(context.graph.get_num_bones(), *addStack.pose, *context.pose, alphaVal);
		}
	}
	else {
		if (alphaVal <= 0.00001) {
			pose0->get_pose(context);
		}
		else if (alphaVal >= 0.9999) {
			pose1->get_pose(context);
		}
		else {
			atUpdateStack stack1(context);
			pose0->get_pose(context);
			pose1->get_pose(stack1);
			util_blend(context.graph.get_num_bones(), *stack1.pose, *context.pose, alphaVal);
		}
	}
}
void atSubtract::Inst::get_pose(atUpdateStack& ctx) {

	atUpdateStack stackSub(ctx);
	sub->get_pose(stackSub);
	util_subtract(ctx.graph.get_num_bones(), *stackSub.pose, *ctx.pose);
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


void atIk2Bone::Inst::get_pose(atUpdateStack& ctx) {
	input->get_pose(ctx);
	if (bone_index==-1||(owner.ik_in_bone_space&&other_bone_index==-1))
		return;

	// build up global matrix when needed instead of recreating it every step
   // not sure if this is optimal, should profile different ways to pass around pose
	const int ALLOCED_MATS = 36;
	glm::mat4 mats[ALLOCED_MATS];
	int indicies[36];

	Pose& pose = *ctx.pose;
	const MSkeleton& skel = ctx.graph.skeleton;
	int index = bone_index;
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
		sys_print(Warning, "ik attempted on some root bone %s\n", owner.bone_name.get_c_str());
		return;
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

	glm::vec3 target_vec = target->get_vector3(ctx);// ->get_value<glm::vec3>(ctx);
	glm::quat target_rotation = {};

	if (owner.ik_in_bone_space && other_bone_index != -1) {
		glm::mat4 matrix = build_global_transform_for_bone_index(&pose, &skel, other_bone_index);
		target_vec = matrix * glm::vec4(target_vec, 1.0);
		if (owner.take_rotation_of_other_bone)
			target_rotation = glm::quat_cast(matrix);
	}


	int index1 = indicies[1];
	int index2 = indicies[2];

	ikfunctor(pose.q[index1], pose.q[index2], target_vec, false);


	if (owner.take_rotation_of_other_bone && other_bone_index != -1) {
		// compute the global rotation now
		glm::quat q = {};
		if (count >= 4)
			q = glm::quat_cast(mats[3]);
		q = q * pose.q[index2];
		q = q * pose.q[index1];

		pose.q[bone_index] = glm::inverse(q) * target_rotation;
	}
}

void atModifyBone::Inst::get_pose(atUpdateStack& ctx) {
	input->get_pose(ctx);
	const int MYBONEINDEX = this->bone_index;
	if (MYBONEINDEX == -1)
		return;


	// build up global matrix when needed instead of recreating it every step
	// not sure if this is optimal, should profile different ways to pass around pose
	const int ALLOCED_MATS = 36;
	glm::mat4 mats[ALLOCED_MATS];
	const MSkeleton& skel = ctx.graph.skeleton;
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

	const glm::vec3 set_pos = translation->get_vector3(ctx);// ->get_value<glm::vec3>(ctx);
	const glm::quat set_rot = glm::quat(rotation->get_vector3(ctx));// rotation->get_value<glm::quat>(ctx);

	const glm::vec3 global_pos = mats[0][3];
	const glm::quat global_rot = glm::quat_cast(mats[0]);

	const bool apply_position = owner.translation != ModifyBoneType::None;
	const bool apply_position_meshspace = owner.translation == ModifyBoneType::Meshspace || owner.translation == ModifyBoneType::MeshspaceAdd;
	const bool apply_position_additive = owner.translation == ModifyBoneType::LocalspaceAdd || owner.translation == ModifyBoneType::MeshspaceAdd;

	const bool apply_rotation = owner.rotation != ModifyBoneType::None;
	const bool apply_rotation_meshspace = owner.rotation == ModifyBoneType::Meshspace || owner.rotation == ModifyBoneType::MeshspaceAdd;
	const bool apply_rotation_additive = owner.rotation == ModifyBoneType::LocalspaceAdd || owner.rotation == ModifyBoneType::MeshspaceAdd;


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


void atAnimStatemachine::Inst::get_pose(atUpdateStack& ctx) {

}

void atCopyBone::Inst::get_pose(atUpdateStack& context) {
	input->get_pose(context);
	if (source_bone == -1 || target_bone == -1)
		return;
	const MSkeleton& skel = context.graph.skeleton;
	Pose& pose = *context.pose;
	if (owner.copyBonespace) {
		// simple, just copy over pos/quat
		pose.q[target_bone] = pose.q[source_bone];
		pose.pos[target_bone] = pose.pos[source_bone];
		pose.scale[target_bone] = pose.scale[source_bone];
	}
	else {
		glm::mat4 mat = build_global_transform_for_bone_index(&pose, &skel, source_bone);
		pose.q[target_bone] = glm::quat_cast(mat);
		pose.pos[target_bone] = mat[3];
		pose.scale[target_bone] = glm::length(mat[0]);

	}
}