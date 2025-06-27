
#include "Framework/AddClassToFactory.h"
#include "Assets/AssetRegistry.h"
extern IEditorTool* g_anim_ed_graph;
#ifdef EDITOR_BUILD
class AnimGraphAssetMeta : public AssetMetadata
{
public:
	AnimGraphAssetMeta() {
		extensions.push_back("ag");
	}

	// Inherited via AssetMetadata
	virtual Color32 get_browser_color()  const override
	{
		return { 152, 237, 149 };
	}

	virtual std::string get_type_name() const  override
	{
		return "AnimGraph";
	}

	virtual bool assets_are_filepaths() const { return true; }

	virtual IEditorTool* tool_to_edit_me() const { return g_anim_ed_graph; }
	virtual const ClassTypeInfo* get_asset_class_type() const { return &Animation_Tree_CFG::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(AnimGraphAssetMeta);
#endif


Pool_Allocator<Pose> g_pose_pool = Pool_Allocator<Pose>(100, "g_pose_pool");
Pool_Allocator<MatrixPose> g_matrix_pool = Pool_Allocator<MatrixPose>(10,"g_matrix_pool");

#if 0
CLASS_IMPL(BaseAGNode);

// Pose nodes
CLASS_IMPL(Node_CFG);
CLASS_IMPL(Clip_Node_CFG);
CLASS_IMPL(Frame_Evaluate_CFG);

CLASS_IMPL(Mirror_Node_CFG);
CLASS_IMPL(Statemachine_Node_CFG);
CLASS_IMPL(Add_Node_CFG);
CLASS_IMPL(Subtract_Node_CFG);
CLASS_IMPL(Blend_Node_CFG);
CLASS_IMPL(Blend_Int_Node_CFG);
CLASS_IMPL(BlendSpace2d_CFG);

CLASS_IMPL(Blend_Masked_CFG);
CLASS_IMPL(ModifyBone_CFG);
CLASS_IMPL(LocalToMeshspace_CFG);
CLASS_IMPL(MeshspaceToLocal_CFG);
CLASS_IMPL(TwoBoneIK_CFG);
CLASS_IMPL(GetCachedPose_CFG);
CLASS_IMPL(SavePoseToCache_CFG);
CLASS_IMPL(DirectPlaySlot_CFG);
CLASS_IMPL(CopyBone_CFG);
// Value nodes
CLASS_IMPL(ValueNode);

CLASS_IMPL(FloatConstant);
CLASS_IMPL(CurveNode);
CLASS_IMPL(VectorConstant);
CLASS_IMPL(VariableNode);
CLASS_IMPL(RotationConstant);


CLASS_IMPL(AgSerializeContext);

bool Frame_Evaluate_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	auto rt = get_rt(ctx);

	const AnimationSeq* clip = rt->clip;
	if (!clip) {
		util_set_to_bind_pose(*pose.pose, ctx.get_skeleton());
		return true;
	}
	float frame = explicit_time->get_value<float>(ctx);

	if (normalized_time)
		frame = frame * clip->duration;

	if(wrap_around_time)
		frame = fmod(fmod(frame, clip->duration) + clip->duration, clip->duration);
	else {
		if (frame < 0) frame = 0;
		else if (frame >= clip->duration) frame = clip->duration - 0.000001;
	}


	util_calc_rotations(ctx.get_skeleton(), clip, frame, nullptr/*fixme*/, *pose.pose);

	return true;
}

static void get_clip_pose_shared(NodeRt_Ctx& ctx, GetPose_Ctx pose, const AnimationSeq* clip, 
	bool has_sync_group, StringName hashed_syncgroupname, sync_opt SyncOption, bool loop, const BoneIndexRetargetMap* remap,
	float speed, float& anim_time, bool& stopped_flag, const Node_CFG* owner)
{
	// synced update
	if (has_sync_group) {
		SyncGroupData& sync = ctx.find_sync_group_data(hashed_syncgroupname);

		if (sync.is_this_first_update()) {
			// do nothing
		}
		else {
			anim_time = sync.time.get() * clip->duration;	// normalized time, TODO: sync markers
		}
		const float time_to_evaluate_sequence = anim_time;

		if (sync.should_write_new_update_weight(SyncOption, 0.5/*TODO*/)) {

			anim_time += pose.dt * speed * 0.8f;	// HACK !!!!!!! fixme, should be 24 fps instead of 30 but setting it breaks stuff, just do this for now 

			if (anim_time > clip->duration || anim_time < 0.f) {
				if (loop)
					anim_time = fmod(fmod(anim_time, clip->duration) + clip->duration, clip->duration);
				else {
					anim_time = clip->duration - 0.001f;
					stopped_flag = true;
				}
			}

			sync.write_to_update_time(SyncOption, 0.5/*TODO*/, owner, Percentage(anim_time, clip->duration));

	
			util_calc_rotations(ctx.get_skeleton(), clip, time_to_evaluate_sequence, remap, *pose.pose);
		}
	}
	// unsynced update
	else {
		const float time_to_evaluate_sequence = anim_time;

		anim_time += pose.dt * speed * 0.8f;	// see above

		if (anim_time > clip->duration || anim_time < 0.f) {
			if (loop)
				anim_time = fmod(fmod(anim_time, clip->duration) + clip->duration, clip->duration);
			else {
				anim_time = clip->duration - 0.001f;
				stopped_flag = true;
			}
		}
		util_calc_rotations(ctx.get_skeleton(), clip, time_to_evaluate_sequence, remap, *pose.pose);
	}
}

 bool Clip_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	 RT_TYPE* rt = get_rt(ctx);
	 //util_set_to_bind_pose(*pose.pose, ctx.get_skeleton());
	 //return true;

	const AnimationSeq* clip = get_clip(ctx);
	if (!clip) {
		util_set_to_bind_pose(*pose.pose, ctx.get_skeleton());
		return true;
	}

	/*
	if node is synced:
		if sync is first frame:
			util_calc_rotations(0)
		else
			time = sync.get_time()
			util_calc_rotations(sync.get_time())
		if sync.wants_update()
			time += dt
			sync.set_time(time_for_sync)
	else:
		util_calc_rotations(time)
		time += dt
	*/

	get_clip_pose_shared(
		ctx,pose,clip,has_sync_group(),hashed_syncgroupname,SyncOption,loop,rt->remap,speed,rt->anim_time,rt->stopped_flag,this);

	const int root_index = 0;
	for (int i = 0; i < 3; i++) {
		if (rm[i] == rootmotion_setting::remove) {
			pose.pose->pos[root_index][i] = rt->root_pos_first_frame[i];
		}
	}

	bool outres = !rt->stopped_flag;
	float cur_t = rt->anim_time;
	float clip_t = clip->duration;
	outres &= !pose.has_auto_transition || (pose.automatic_transition_time + cur_t < clip_t);

	return outres;

}

 bool BlendSpace2d_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
 {
	 return false;
	 auto rt = get_rt(ctx);
	 const bool is_1d = indicies.size() == 0;
	 // if 1d, find 2 nodes to blend between
	 if (is_1d) {
		 float x = xparam->get_value<float>(ctx);
		 int i = 0;
		 for (; i < verticies.size(); i++) {
			 if (x < verticies[i].x) {
				 break;
			 }
		 }
		 if (i == 0 || i==verticies.size()) {
			 // set to first or last pose
			 bool dummystopflag = false;
			 int index = (i == 0) ? 0 : verticies.size()-1;
			 get_clip_pose_shared(ctx, pose, verticies[index].seq.ptr->seq, true, hashed_syncgroupname, SyncOption, true,rt->retargets[index], 1.f, rt->current_times[index], dummystopflag, this);
			 if (is_additive_blend_space) {

			 }
			 return true;
		 }
		 else {
			 // blend
			 int first_pose = i - 1;
			 int next_pose = i;
			 ASSERT(first_pose >= 0&&next_pose<verticies.size());

			 return true;
		 }
	 }


	 return false;
 }


// Inherited via At_Node

 bool Subtract_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {
	auto reftemp = g_pose_pool.allocate_scoped();
	ref->get_pose(ctx, pose);
	GetPose_Ctx pose2 = pose;
	pose2.pose = reftemp.get();
	source->get_pose(ctx, pose2);
	util_subtract(ctx.num_bones(), *reftemp.get(), *pose.pose);
	return true;
}

bool Add_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	float lerp = param->get_value<float>(ctx);

	auto addtemp = g_pose_pool.allocate_scoped();
	base->get_pose(ctx, pose);

	GetPose_Ctx pose2 = pose;
	pose2.pose = addtemp.get();

	diff->get_pose(ctx, pose2);
	util_add(ctx.num_bones(), *addtemp.get(), *pose.pose, lerp);
	return true;
}

 bool Blend_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	if (!alpha) {
		util_set_to_bind_pose(*pose.pose, ctx.get_skeleton());
		return true;
	}
	RT_TYPE* rt = get_rt(ctx);
	float value = alpha->get_value<float>(ctx);
	if (value <= 0.00001)
		return inp0->get_pose(ctx, pose);
	bool ret = inp0->get_pose(ctx, pose);
	auto addtemp = g_pose_pool.allocate_scoped();
	inp1->get_pose(ctx, pose.set_pose(addtemp.get()));
	util_blend(ctx.num_bones(), *addtemp.get(), *pose.pose, value);
	return ret;
}


 bool Blend_Int_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
 {
	 if (!param) {
		 util_set_to_bind_pose(*pose.pose, ctx.get_skeleton());
		 return true;
	 }
	 RT_TYPE* rt = get_rt(ctx);

	 // param never changes
	 if (!store_value_on_reset) {
		 int val = param->get_value<int>(ctx);
		 int real_idx = get_actual_index(val);

		 if (real_idx != rt->active_i) {

			 if (rt->fade_out_i == real_idx) { /* already transitioning from that, swap with active */
				 rt->lerp_amt = 1.0 - rt->lerp_amt;
				 std::swap(rt->fade_out_i, rt->active_i);
			 }
			 else if (rt->fade_out_i != -1) {/* alread transitioning, abrupt stop */
				 rt->lerp_amt = 1.0;
				 rt->fade_out_i = -1;
				 rt->active_i = real_idx;
			 }
			 else {/* normal fade out */
				 rt->fade_out_i = rt->active_i;
				 rt->active_i = real_idx;
				 rt->lerp_amt = 0.0;
			 }
		 }

		 //rt->lerp_amt = damp_dt_independent(1.0f, rt->lerp_amt, damp_factor, pose.dt);

		 if (rt->lerp_amt >= 0.99999)
			 rt->fade_out_i = -1;
	 }
	 /* else, value never changes, so use saved one from reset */

	 bool keep_going = true;

	 if (store_value_on_reset || !rt->fade_out_i) {
		 keep_going &= input[rt->active_i]->get_pose(ctx, pose);
	 }
	 else {
		 auto addtemp = g_pose_pool.allocate_scoped();
		 keep_going &= input[rt->active_i]->get_pose(ctx, pose);
		 keep_going &= input[rt->fade_out_i]->get_pose(ctx, pose.set_pose(addtemp.get()));
		 util_blend(ctx.num_bones(), *addtemp.get(), *pose.pose, rt->lerp_amt);
	 }
	 return keep_going;
 }


// Inherited via At_Node

 bool Mirror_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{

	auto rt = get_rt(ctx);
	 if (!store_value_on_reset) {

		 float amt = param->get_value<float>(ctx);

		 //rt->saved_f = damp_dt_independent(amt, rt->saved_f, damp_time, pose.dt);
	 }
	bool ret = input->get_pose(ctx, pose);

	bool has_mirror_map = ctx.get_skeleton()->has_mirroring_table();

	if (has_mirror_map && rt->saved_f >= 0.000001) {
		const Model* m = ctx.model;
		auto posemirrored_s = g_pose_pool.allocate_scoped();
		auto posemirrored = posemirrored_s.get();
		// mirror the bones
		for (int i = 0; i <ctx.num_bones(); i++) {


			int from = ctx.get_skeleton()->get_mirrored_bone(i);
			if (from == -1) 
				from = i;	// set to self

			glm::vec3 frompos = pose.pose->pos[from];
			posemirrored->pos[i] = glm::vec3(-frompos.x, frompos.y, frompos.z);
			glm::quat fromquat = pose.pose->q[from];
			posemirrored->q[i] = glm::quat(fromquat.w, fromquat.x, -fromquat.y, -fromquat.z);
			posemirrored->scale[i] = pose.pose->scale[from];
		}

		util_blend(ctx.num_bones(), *posemirrored, *pose.pose, rt->saved_f);

	}
	return ret;
}



 // base64 encoding

 char byte_to_base64(uint8_t byte)
 {
	 if (byte <= 25) {
		 return 'A' + byte;
	 }
	 else if (byte <= 51) {
		 return 'a' + (byte - 26);
	 }
	 else if (byte <= 61) {
		 return '0' + (byte - 52);
	 }
	 else if (byte == 62) return '+';
	 else if (byte == 63) return '/';
	 else
		 ASSERT(0);

	 return 0;
 }

 static void write_base64(uint8_t* data, int size, std::string& buf)
 {
	 buf.clear();
	 for (int i = 0; i < size / 3; i++) {
		 int idx = i * 3;
		 uint32_t bits = (uint32_t)data[idx] | (uint32_t)data[idx + 1] << 8 | (uint32_t)data[idx + 2] << 16;
		 for (int j = 0; j < 4; j++) {
			 buf.push_back(byte_to_base64(bits & 63));
			 bits >>= 6;
		 }
	 }
 }




 void AgSerializeContext::set_tree(Animation_Tree_CFG* tree)
 {
	 this->tree = tree;
	 for (int i = 0; i < tree->all_nodes.size(); i++) {
		 ptr_to_index[tree->all_nodes[i]] = i;
	 }
 }

 class AgSerializeNodeCfg : public IPropertySerializer
 {
	 // Inherited via IPropertySerializer
	 virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	 {
		 ASSERT(user && user->is_a<AgSerializeContext>());
		 Node_CFG* ptr = *(Node_CFG**)info.get_ptr(inst);

		 int64_t num = (int64_t)ptr;

		 return std::to_string(num);
	 }
	 virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user, IAssetLoadingInterface* load) override
	 {
		 uintptr_t index = atoi(token.to_stack_string().c_str());

		 Node_CFG** ptr_to_ptr = (Node_CFG**)info.get_ptr(inst);
		 *ptr_to_ptr = (Node_CFG*)(index);	// This gets fixed up in a post process step
	 }
 };

 static AddClassToFactory<AgSerializeNodeCfg, IPropertySerializer> ab123123c(IPropertySerializer::get_factory(), "AgSerializeNodeCfg");

 PropertyInfoList* get_nodecfg_ptr_type()
 {
	 static PropertyInfo props[] = {
		 make_struct_property("_value",0,PROP_SERIALIZE,"AgSerializeNodeCfg")
	 };
	 static PropertyInfoList list = { props,1,"nodecfg_ptr" };
	 return &list;
 }




 bool Blend_Masked_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
 {
	 auto rt = get_rt(ctx);
	 if(!rt->mask)
		 return base->get_pose(ctx, pose);

	 float alpha_val = alpha->get_value<float>(ctx);

	 // epsilon
	 if (alpha_val <= 0.000001) {
		 return base->get_pose(ctx, pose);
	 }

	 // handle blending of curves, events, etc.
	 if (meshspace_rotation_blend) {
		 auto base_layer = g_pose_pool.allocate_scoped();
		 bool ret = base->get_pose(ctx, pose.set_pose(base_layer.get()));
		 layer->get_pose(ctx, pose); // ignore return value, fixme?
		 util_global_blend(ctx.get_skeleton(),base_layer.get(), pose.pose, alpha_val, rt->mask->weight);

		 return ret;
	 }
	 else {
		 auto layer = g_pose_pool.allocate_scoped();
		 bool ret = base->get_pose(ctx, pose);
		 this->layer->get_pose(ctx, pose.set_pose(layer.get()));
		 util_blend_with_mask(ctx.num_bones(), *layer.get(), *pose.pose, alpha_val, rt->mask->weight);
		 return ret;
	 }
 }

bool LocalToMeshspace_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	// this function does nothing mwhahah
	bool res = input->get_pose(ctx, pose);
	return res;
	
}
bool MeshspaceToLocal_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	bool res = input->get_pose(ctx, pose);
	return res;
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

 bool TwoBoneIK_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {
	 bool res = input->get_pose(ctx, pose);
	 auto rt = get_rt(ctx);
	 if (rt->bone_index == -1)
		 return res;

	 // build up global matrix when needed instead of recreating it every step
	// not sure if this is optimal, should profile different ways to pass around pose
	 const int ALLOCED_MATS = 36;
	 glm::mat4 mats[ALLOCED_MATS];
	 int indicies[36];

	 auto skel = ctx.get_skeleton();
	 int index = rt->bone_index;
	 int count = 0;
	 while (index != -1) {
		 assert(count < ALLOCED_MATS);
		 glm::mat4x4 matrix = glm::mat4_cast(pose.pose->q[index]);
		 matrix[3] = glm::vec4(pose.pose->pos[index], 1.0);
		 mats[count++] = matrix;
		 indicies[count - 1] = index;
		 index = skel->get_bone_parent(index);
	 }
	 for (int i = count - 2; i >= 0; i--) {
		 mats[i] = mats[i + 1] * mats[i];
	 }

	 if (count <= 2) {
		 sys_print(Warning, "ik attempted on some root bone %s\n", bone_name.c_str());
		 return res;
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

	 glm::vec3 target_vec = position->get_value<glm::vec3>(ctx);
	 glm::quat target_rotation = {};

	 if (ik_in_bone_space&&rt->target_bone_index!=-1) {
		 glm::mat4 matrix = build_global_transform_for_bone_index(pose.pose, skel, rt->target_bone_index);
		 target_vec = matrix * glm::vec4(target_vec, 1.0);
		 if (take_rotation_of_other_bone)
			 target_rotation = glm::quat_cast(matrix);
	 }


	 int index1 = indicies[1];
	 int index2 = indicies[2];

	 ikfunctor(pose.pose->q[index1], pose.pose->q[index2], target_vec, false);


	 if (take_rotation_of_other_bone && rt->target_bone_index != -1) {
		// compute the global rotation now
		 glm::quat q = {};
		 if (count >= 4)
			 q = glm::quat_cast(mats[3]);
		 q = q * pose.pose->q[index2];
		 q = q * pose.pose->q[index1];

		 pose.pose->q[rt->bone_index] = glm::inverse(q) * target_rotation;
	 }

	 return res;
}


 bool ModifyBone_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {
	 bool res = input->get_pose(ctx, pose);
	 auto rt = get_rt(ctx);
	 if (rt->bone_index == -1)
		 return res;

	 // build up global matrix when needed instead of recreating it every step
	 // not sure if this is optimal, should profile different ways to pass around pose
	 const int ALLOCED_MATS = 36;
	 glm::mat4 mats[ALLOCED_MATS];
	 auto skel = ctx.get_skeleton();
	 int index = rt->bone_index;
	 int count = 0;
	 while (index != -1) {
		 assert(count < ALLOCED_MATS);
		 glm::mat4x4 matrix = glm::mat4_cast(pose.pose->q[index]);
		 matrix[3] = glm::vec4(pose.pose->pos[index], 1.0);
		 mats[count++] = matrix;
		 index = skel->get_bone_parent(index);
	 }
	 for (int i = count - 2; i >= 0; i--) {
		 mats[i] = mats[i + 1] * mats[i];
	 }

	 glm::vec3 set_pos = position->get_value<glm::vec3>(ctx);
	 glm::quat set_rot = rotation->get_value<glm::quat>(ctx);

	 glm::vec3 global_pos = mats[0][3];
	 glm::quat global_rot = glm::quat_cast(mats[0]);

	 if (apply_position) {
		 if (apply_position_meshspace) {
			 if (apply_position_additive)
				 mats[0][3] = glm::vec4(global_pos + set_pos, 1.0f);
			 else
				 mats[0][3] = glm::vec4(set_pos, 1.0f);
		 }
		 else {
			 if (apply_position_additive)
				 pose.pose->pos[rt->bone_index] += set_pos;
			 else
				 pose.pose->pos[rt->bone_index] = set_pos;
		 }
	 }
	 if (apply_rotation) {
		 if (apply_rotation_meshspace) {
			 if (apply_rotation_additive) {
				 glm::quat q = set_rot * global_rot;
				 auto lastcol = mats[0][3];
				 mats[0] = glm::mat4_cast(q);
				 mats[0][3] = lastcol;
			 }
			 else {
				 glm::quat q = set_rot;
				 auto lastcol = mats[0][3];
				 mats[0] = glm::mat4_cast(q);
				 mats[0][3] = lastcol;
			 }
		 }
		 else {
			 if (apply_rotation_additive)
				 pose.pose->q[rt->bone_index] = set_rot * pose.pose->q[rt->bone_index];
			 else
				 pose.pose->q[rt->bone_index] = set_rot;
		 }
	 }

	 if (apply_position_meshspace || apply_rotation_meshspace) {
		 // go from global to local again
		 if (count > 1)
			 mats[0] = glm::inverse(mats[1]) * mats[0];
		 if (apply_rotation && apply_rotation_meshspace)
			 pose.pose->q[rt->bone_index] = glm::quat_cast(mats[0]);
		 if (apply_position && apply_position_meshspace)
			 pose.pose->pos[rt->bone_index] = mats[0][3];
	 }
	 return res;
 }



 bool CopyBone_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {
	 bool res = input->get_pose(ctx, pose);
	 auto rt = get_rt(ctx);
	 if (rt->src_bone_index == -1 || rt->target_bone_index == -1)
		 return res;
	 auto skel = ctx.get_skeleton();

	 if (copy_bonespace) {
		 // simple, just copy over pos/quat
		 pose.pose->q[rt->target_bone_index] = pose.pose->q[rt->src_bone_index];
		 pose.pose->pos[rt->target_bone_index] = pose.pose->pos[rt->src_bone_index];
		 pose.pose->scale[rt->target_bone_index] = pose.pose->scale[rt->src_bone_index];
		 return res;
	 }
	 glm::mat4 mat = build_global_transform_for_bone_index(pose.pose, skel, rt->src_bone_index);
	 pose.pose->q[rt->target_bone_index] = glm::quat_cast(mat);
	 pose.pose->pos[rt->target_bone_index] = mat[3];
	 pose.pose->scale[rt->target_bone_index] = glm::length(mat[0]);

	 return res;

 }

 void calc_rootmotion(GetPose_Ctx& out, float time, float last_time, const AnimationSeq* clip, float alpha_blend)
 {
	 if (!clip->has_rootmotion)
		 return;

	 auto& m = *out.accumulated_root_motion;
	 int keyframe = clip->get_frame_for_time(last_time);
	 float lerp_amt = MidLerp(clip->get_time_of_keyframe(keyframe), clip->get_time_of_keyframe(keyframe + 1), last_time);
	 ScalePositionRot transformlast = clip->get_keyframe(0, keyframe, lerp_amt);
	 keyframe = clip->get_frame_for_time(time);
	 lerp_amt = MidLerp(clip->get_time_of_keyframe(keyframe), clip->get_time_of_keyframe(keyframe + 1), time);
	 ScalePositionRot transformnow = clip->get_keyframe(0, keyframe, lerp_amt);
	 m.position_displacement += (transformnow.pos - transformlast.pos)*alpha_blend;
	 {
		 glm::quat target = quat_delta(transformlast.rot,transformnow.rot) * m.rotation_displacement;
		 m.rotation_displacement =  glm::slerp(m.rotation_displacement, target, alpha_blend);
	 }
 }

 bool DirectPlaySlot_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
 {
	 if (slot_index == -1)
		 return input->get_pose(ctx, pose);
	 auto& slot = ctx.get_slot_for_index(slot_index);
	 auto rt = get_rt(ctx);
	 if (!slot.active) {
		 if (rt->fading_out_pose) {
			 g_pose_pool.free(rt->fading_out_pose);
			 rt->fading_out_pose = nullptr;
		 }
		 rt->remap = nullptr;
		 return input->get_pose(ctx, pose);
	 }

	 // slot is active, a few choices:
	 // to blend smoothly, capture the last pose

	 if (slot.state == slot.FadingIn) {
		 if (!rt->fading_out_pose) {
			 rt->fading_out_pose = (Pose*)g_pose_pool.allocate();
			 input->get_pose(ctx, pose.set_pose(rt->fading_out_pose));

			 rt->remap = ctx.get_skeleton()->get_remap(slot.active->srcModel.get()->get_skel());
		 }
		 // now have a pose from fading out
		util_calc_rotations(ctx.get_skeleton(), slot.active->seq, slot.time, rt->remap, *pose.pose);

		// blend
		
		float alpha = 1.0 - evaluate_easing(Easing::CubicEaseInOut, slot.fade_percentage);
		util_blend(ctx.num_bones(), *rt->fading_out_pose, *pose.pose, 1.0 -evaluate_easing(Easing::CubicEaseInOut, slot.fade_percentage));
		calc_rootmotion(pose, slot.time, slot.lasttime, slot.active->seq, alpha);
	 }
	 else if (slot.state == slot.Full) {
		 if (rt->fading_out_pose) {
			 g_pose_pool.free(rt->fading_out_pose);
			 rt->fading_out_pose = nullptr;
		 }
		 util_calc_rotations(ctx.get_skeleton(), slot.active->seq, slot.time, rt->remap, *pose.pose);
		 calc_rootmotion(pose, slot.time, slot.lasttime, slot.active->seq, 1.0);

	 }
	 else {// fadingOut

		 if (!rt->fading_out_pose) {
			 rt->fading_out_pose = (Pose*)g_pose_pool.allocate();
			 util_calc_rotations(ctx.get_skeleton(), slot.active->seq, slot.time, rt->remap,*rt->fading_out_pose);
		 }
		 // now have a pose from fading out

		

		input->get_pose(ctx, pose);
		 // blend
		float alpha = evaluate_easing(Easing::CubicEaseInOut, slot.fade_percentage);
		 util_blend(ctx.num_bones(), *rt->fading_out_pose, *pose.pose, alpha);
		 calc_rootmotion(pose, slot.time, slot.lasttime, slot.active->seq, alpha);
	 }



	 // sample rootmotion,curves,events here
	 return true;
 }
#endif