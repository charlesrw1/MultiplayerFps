#include "AnimationTreeLocal.h"
#include "../AnimationUtil.h"
#include "Framework/DictWriter.h"
#include "Framework/DictParser.h"
#include "Framework/ReflectionRegisterDefines.h"
#include "Framework/StdVectorReflection.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/WriteObject.h"

#include "Statemachine_cfg.h"

#include "AssetCompile/Someutils.h"
#include "AssetRegistry.h"
#include "Framework/Files.h"
#include "Animation/Editor/AnimationGraphEditorPublic.h"
class AnimGraphAssetMeta : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() override
	{
		return { 152, 237, 149 };
	}

	virtual std::string get_type_name() override
	{
		return "AnimGraph";
	}
	virtual void index_assets(std::vector<std::string>& filepaths) override
	{
		auto tree = FileSys::find_files("./Data/Graphs");
		for (auto file : tree) {
			filepaths.push_back((file.substr(14)));
		}
	}
	virtual bool assets_are_filepaths() { return true; }
	virtual std::string root_filepath() override
	{
		return "./Data/Graphs/";
	}
	virtual IEditorTool* tool_to_edit_me() const { return g_anim_ed_graph; }
};
static AutoRegisterAsset<AnimGraphAssetMeta> animgraph_register_0987;




Pool_Allocator g_pose_pool = Pool_Allocator(sizeof(Pose), 8);

CLASS_IMPL(BaseAGNode);

// Pose nodes
CLASS_IMPL(Node_CFG);
CLASS_IMPL(Clip_Node_CFG);
CLASS_IMPL(Sync_Node_CFG);
CLASS_IMPL(Mirror_Node_CFG);
CLASS_IMPL(Statemachine_Node_CFG);
CLASS_IMPL(Add_Node_CFG);
CLASS_IMPL(Subtract_Node_CFG);
CLASS_IMPL(Blend_Node_CFG);
CLASS_IMPL(Blend_Int_Node_CFG);
CLASS_IMPL(BlendSpace2d_CFG);
CLASS_IMPL(BlendSpace1d_CFG);
CLASS_IMPL(Blend_Masked_CFG);
CLASS_IMPL(ModifyBone_CFG);
CLASS_IMPL(LocalToMeshspace_CFG);
CLASS_IMPL(MeshspaceToLocal_CFG);
CLASS_IMPL(TwoBoneIK_CFG);
CLASS_IMPL(GetCachedPose_CFG);
CLASS_IMPL(SavePoseToCache_CFG);
CLASS_IMPL(DirectPlaySlot_CFG);

// Value nodes
CLASS_IMPL(ValueNode);

CLASS_IMPL(FloatConstant);
CLASS_IMPL(CurveNode);
CLASS_IMPL(VectorConstant);
CLASS_IMPL(VariableNode);
CLASS_IMPL(RotationConstant);



ENUM_START(anim_graph_value)
	STRINGIFY_EUNM(anim_graph_value::bool_t,	0),
	STRINGIFY_EUNM(anim_graph_value::float_t,	1),
	STRINGIFY_EUNM(anim_graph_value::int_t,		2),
	STRINGIFY_EUNM(anim_graph_value::vec3_t,	3),
	STRINGIFY_EUNM(anim_graph_value::quat_t,	4)
ENUM_IMPL(anim_graph_value);



ENUM_START(rootmotion_setting)
	STRINGIFY_EUNM(rootmotion_setting::keep, 0),
	STRINGIFY_EUNM(rootmotion_setting::remove, 1),
	STRINGIFY_EUNM(rootmotion_setting::add_velocity, 2)
ENUM_IMPL(rootmotion_setting);


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

	const bool has_a_sync_already = pose.sync != nullptr && allow_sync;

	if (has_a_sync_already)
		rt->anim_time = clip->duration * pose.sync->normalized_frame;

	const bool can_update_sync = pose.sync && pose.sync->first_seen && can_be_leader && allow_sync;
	const bool update_self_time = !allow_sync || !pose.sync;

	if (can_update_sync || update_self_time) {
		if (pose.rootmotion_scale >= 0) {
			// want to match character_speed and speed_of_anim
			float speedup = pose.rootmotion_scale * rt->inv_speed_of_anim_root;
			pose.dt *= speedup;
		}

		rt->anim_time += pose.dt * speed;

		if (rt->anim_time > clip->duration || rt->anim_time < 0.f) {
			if (loop)
				rt->anim_time = fmod(fmod(rt->anim_time, clip->duration) + clip->duration, clip->duration);
			else {
				rt->anim_time = clip->duration - 0.001f;
				rt->stopped_flag = true;
			}
		}

		if (pose.sync && can_be_leader && allow_sync) {
			pose.sync->first_seen = false;
			pose.sync->normalized_frame = rt->anim_time / clip->duration;
		}

	}

	const std::vector<int16_t>* indicies = nullptr;
	if (rt->remap_index != -1)
		indicies = &ctx.get_skeleton()->get_remap(rt->remap_index)->other_to_this;

	util_calc_rotations(ctx.get_skeleton(), clip, rt->anim_time, indicies, *pose.pose);

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

// Inherited via At_Node

 bool Subtract_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {

	Pose* reftemp = Pose_Pool::get().alloc(1);
	ref->get_pose(ctx, pose);
	GetPose_Ctx pose2 = pose;
	pose2.pose = reftemp;
	source->get_pose(ctx, pose2);
	util_subtract(ctx.num_bones(), *reftemp, *pose.pose);
	Pose_Pool::get().free(1);
	return true;
}

 bool Add_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	float lerp = param->get_value<float>(ctx);

	Pose* addtemp = Pose_Pool::get().alloc(1);
	base->get_pose(ctx, pose);

	GetPose_Ctx pose2 = pose;
	pose2.pose = addtemp;

	diff->get_pose(ctx, pose2);
	util_add(ctx.num_bones(), *addtemp, *pose.pose, lerp);
	Pose_Pool::get().free(1);
	return true;
}

 bool Blend_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	 if (!param) {
		 util_set_to_bind_pose(*pose.pose, ctx.get_skeleton());
		 return true;
	 }
	 RT_TYPE* rt = get_rt(ctx);
	 ASSERT(!(rt->lerp_amt != rt->lerp_amt));

	 float value = 0.0;
	 if (store_value_on_reset) {
		 value = rt->saved_f;
	 }
	 else {
		 value = param->get_value<float>(ctx);
		ASSERT(!(rt->lerp_amt != rt->lerp_amt));
	 }


	 bool keep_going = true;

	Pose* addtemp = Pose_Pool::get().alloc(1);
	keep_going &= inp0->get_pose(ctx, pose);
	keep_going &= inp1->get_pose(ctx, pose.set_pose(addtemp));
	util_blend(ctx.num_bones(), *addtemp, *pose.pose, value);
	Pose_Pool::get().free(1);
	return keep_going;
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

		 rt->lerp_amt = damp_dt_independent(1.0f, rt->lerp_amt, damp_factor, pose.dt);

		 if (rt->lerp_amt >= 0.99999)
			 rt->fade_out_i = -1;
	 }
	 /* else, value never changes, so use saved one from reset */

	 bool keep_going = true;

	 if (store_value_on_reset || !rt->fade_out_i) {
		 keep_going &= input[rt->active_i]->get_pose(ctx, pose);
	 }
	 else {
		 Pose* addtemp = Pose_Pool::get().alloc(1);
		 keep_going &= input[rt->active_i]->get_pose(ctx, pose);
		 keep_going &= input[rt->fade_out_i]->get_pose(ctx, pose.set_pose(addtemp));
		 util_blend(ctx.num_bones(), *addtemp, *pose.pose, rt->lerp_amt);
		 Pose_Pool::get().free(1);
	 }
	 return keep_going;
 }


// Inherited via At_Node

 bool Mirror_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{

	auto rt = get_rt(ctx);
	 if (!store_value_on_reset) {

		 float amt = param->get_value<float>(ctx);

		 rt->saved_f = damp_dt_independent(amt, rt->saved_f, damp_time, pose.dt);
	 }
	bool ret = input->get_pose(ctx, pose);

	bool has_mirror_map = ctx.get_skeleton()->has_mirroring_table();

	if (has_mirror_map && rt->saved_f >= 0.000001) {
		const Model* m = ctx.model;
		Pose* posemirrored = Pose_Pool::get().alloc(1);
		// mirror the bones
		for (int i = 0; i <ctx.num_bones(); i++) {


			int from = ctx.get_skeleton()->get_mirrored_bone(i);
			if (from == -1) 
				from = i;	// set to self

			glm::vec3 frompos = pose.pose->pos[from];
			posemirrored->pos[i] = glm::vec3(-frompos.x, frompos.y, frompos.z);
			glm::quat fromquat = pose.pose->q[from];
			posemirrored->q[i] = glm::quat(fromquat.w, fromquat.x, -fromquat.y, -fromquat.z);
		}

		util_blend(ctx.num_bones(), *posemirrored, *pose.pose, rt->saved_f);

		Pose_Pool::get().free(1);

	}
	return ret;
}


// Inherited via At_Node

 bool BlendSpace1d_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
 {
	 return false;
 }

 bool BlendSpace2d_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	//walk_fade_in = g_walk_fade_in;
	//walk_fade_out = g_walk_fade_out;
	//run_fade_in = g_run_fade_in;

	auto rt = get_rt(ctx);

	glm::vec2 relmovedir = glm::vec2(
		xparam->get_value<float>(ctx),
		yparam->get_value<float>(ctx)
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

	return false;
}

Animation_Tree_CFG::Animation_Tree_CFG()
{
	code = std::make_unique<Script>();
}

Animation_Tree_CFG::~Animation_Tree_CFG()
{
	for (int i = 0; i < all_nodes.size(); i++) {
		delete all_nodes[i];
	}
}

 uint32_t Animation_Tree_CFG::get_num_vars() const { return code ? code->num_variables() : 0; }

void Animation_Tree_CFG::construct_all_nodes(NodeRt_Ctx& ctx) const {
	for (int i = 0; i < all_nodes.size(); i++)
		all_nodes[i]->construct(ctx);
}


bool Animation_Tree_CFG::post_load_init()
{
	// initialize script

	code->link_to_native_class();
	
	bool good = code->check_is_valid();
	if (!good) {
		sys_print("!!! script failed 'check_is_valid' for AnimGraph %s\n", get_name().c_str());
	}

	for (int i = 0; i < all_nodes.size(); i++) {
		all_nodes[i]->initialize(this);
	}
	BaseAGNode* rootagnode = serialized_nodecfg_ptr_to_ptr(root, this);
	if (rootagnode) {
		root = rootagnode->cast_to<Node_CFG>();
		ASSERT(root);
	}
	else
		root = nullptr;

	if (!root || !good)
		graph_is_valid = false;

	return good;
}

int Animation_Tree_CFG::get_index_of_node(Node_CFG* ptr)
 {
	 for (int i = 0; i < all_nodes.size(); i++) {
		 if (ptr == all_nodes[i]) return i;
	 }
	 return -1;
 }

 const PropertyInfoList* Animation_Tree_CFG::get_props()
 {
	 START_PROPS(Animation_Tree_CFG)
		REG_STRUCT_CUSTOM_TYPE(root, PROP_SERIALIZE, "AgSerializeNodeCfg"),
		REG_INT(data_used, PROP_SERIALIZE, ""),
		REG_BOOL(graph_is_valid, PROP_SERIALIZE, "")
	END_PROPS()
 }




 bool Animation_Tree_CFG::read_from_dict(DictParser& in)
 {
	 if (!in.expect_string("runtime") || !in.expect_item_start())
		 return false;

	 {
		 if (!in.expect_string("rootdata") || !in.expect_item_start())
			 return false;
		 auto res = read_properties(*get_props(), this, in, {}, {});

		 if (!res.second || !in.check_item_end(res.first))
			 return false;
	 }

	 {
		 if (!in.expect_string("nodes") || !in.expect_list_start())
			 return false;

		bool good =  in.read_list_and_apply_functor([&](StringView view) -> bool
			 {
				BaseAGNode* node = read_object_properties<BaseAGNode>({}, in, view);
				 if (node) {
					 all_nodes.push_back(node);
					 return true;
				 }
				 return false;
			 }
		 );
		if (!good)
			return false;
	 }

	 {
		 if (!in.expect_string("params") || !in.expect_item_start())
			 return false;
		 auto res = read_properties(*Script::get_props(), code.get(), in, {}, {});

		 if (!res.second || !in.check_item_end(res.first))
			 return false;
	 }



	 return in.expect_item_end();
 }


 void Animation_Tree_CFG::write_to_dict(DictWriter& out)
 {

	 // out.set_should_add_indents(true);
	 out.write_key("runtime");
	 out.write_item_start();

	 AgSerializeContext ctx(this);
	 TypedVoidPtr ctxptr(NAME("AgSerializeContext"), &ctx);

	 {
		 out.write_key("rootdata");
		 out.write_item_start();
		 write_properties(*get_props(), this, out, ctxptr);
		 out.write_item_end();
	 }

	 {
		 out.write_key_list_start("nodes");
		 for (int i = 0; i < all_nodes.size(); i++) {
			 auto& node = all_nodes[i];
			 write_object_properties(node, ctxptr, out);
		 }
		 out.write_list_end();
	 }
	 { 
		 out.write_key("params");
		 out.write_item_start();
		 write_properties(*Script::get_props(), code.get(), out, ctxptr);
		 out.write_item_end();
	 }
	 out.write_item_end();
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
			 bits >= 6;
		 }
	 }
 }


 float PropertyInfo::get_float(void* ptr) const
 {
	 ASSERT(type == core_type_id::Float);

	 return *(float*)((char*)ptr + offset);
 }

 void PropertyInfo::set_float(void* ptr, float f) const
 {
	 ASSERT(type == core_type_id::Float);

	 *(float*)((char*)ptr + offset) = f;
 }

 uint64_t PropertyInfo::get_int(void* ptr) const
 {
	 ASSERT(is_integral_type());
	 if (type == core_type_id::Bool || type == core_type_id::Int8 || type == core_type_id::Enum8) {
		 return *(int8_t*)((char*)ptr + offset);
	 }
	 else if (type == core_type_id::Int16 || type == core_type_id::Enum16) {
		 return *(uint16_t*)((char*)ptr + offset);
	 }
	 else if (type == core_type_id::Int32 || type == core_type_id::Enum32) {
		 return *(uint32_t*)((char*)ptr + offset);
	 }
	 else if (type == core_type_id::Int64) {
		 return *(uint64_t*)((char*)ptr + offset);
	 }
	 else {
		 ASSERT(0);
		 return 0;
	 }
 }

 void PropertyInfo::set_int(void* ptr, uint64_t i) const
 {
	 ASSERT(is_integral_type());
	 if (type == core_type_id::Bool || type == core_type_id::Int8 || type == core_type_id::Enum8) {
		 *(int8_t*)((char*)ptr + offset) = i;
	 }
	 else if (type == core_type_id::Int16 || type == core_type_id::Enum16) {
		 *(uint16_t*)((char*)ptr + offset) = i;
	 }
	 else if (type == core_type_id::Int32 || type == core_type_id::Enum32) {
		 *(uint32_t*)((char*)ptr + offset) = i;
	 }
	 else if (type == core_type_id::Int64) {
		 *(uint64_t*)((char*)ptr + offset) = i;	// ERROR NARROWING
	 }
	 else {
		 ASSERT(0);
	 }
 }





 AgSerializeContext::AgSerializeContext(Animation_Tree_CFG* tree)
 {
	 this->tree = tree;
	 for (int i = 0; i < tree->all_nodes.size(); i++) {
		 ptr_to_index[tree->all_nodes[i]] = i;
	 }
 }

 class AgSerializeNodeCfg : public IPropertySerializer
 {
	 // Inherited via IPropertySerializer
	 virtual std::string serialize(DictWriter& out, const PropertyInfo& info, void* inst, TypedVoidPtr user) override
	 {
		 ASSERT(user.name == NAME("AgSerializeContext"));
		 Node_CFG* ptr = *(Node_CFG**)info.get_ptr(inst);

		 int64_t num = (int64_t)ptr;

		 return std::to_string(num);
	 }
	 virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, TypedVoidPtr user) override
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




#include "Game_Engine.h"
#include "imgui.h"
 static float mask_weight = 0.0;
 void somemenulol()
 {
	 ImGui::SliderFloat("w", &mask_weight, 0.0, 1.0);
 }
 static AddToDebugMenu men("daf", somemenulol);

 bool Blend_Masked_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
 {
	 auto rt = get_rt(ctx);
	 if(!rt->mask)
		 return base->get_pose(ctx, pose);


	 float b = param->get_value<float>(ctx);

	 if (b <= 0.000001) {
		 return base->get_pose(ctx, pose);
	 }

#if 1
	
	 if (meshspace_rotation_blend) {
		 Pose* base_layer = Pose_Pool::get().alloc(1);
		 bool ret = base->get_pose(ctx, pose.set_pose(base_layer));
		 ret &= layer->get_pose(ctx, pose);
		 util_global_blend(ctx.get_skeleton(),base_layer, pose.pose, mask_weight, rt->mask->weight);
		 Pose_Pool::get().free(1);
		 return ret;
	 }


	 // nase

	 else {
		 Pose* layer = Pose_Pool::get().alloc(1);
		 bool ret = base->get_pose(ctx, pose);
		 ret &= this->layer->get_pose(ctx, pose.set_pose(layer));
		 util_blend_with_mask(ctx.num_bones(), *layer, *pose.pose, 1.0, rt->mask->weight);
		 Pose_Pool::get().free(1);
		 return ret;
	 }

#endif
 }

bool LocalToMeshspace_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	bool res = input->get_pose(ctx, pose);
	
	return false;
	
}
bool MeshspaceToLocal_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	bool res = input->get_pose(ctx, pose);

	return false;

}