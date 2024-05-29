#include "AnimationTreeLocal.h"
#include "../AnimationUtil.h"
#include "Framework/DictWriter.h"
#include "Framework/DictParser.h"
#include "Framework/ReflectionRegisterDefines.h"
#include "Framework/StdVectorReflection.h"
#include "ControlParams.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/WriteObject.h"

#include "Statemachine_cfg.h"

Pool_Allocator g_pose_pool = Pool_Allocator(sizeof(Pose), 8);

#define IMPL_NODE_CFG(type_name) \
const TypeInfo& type_name::get_typeinfo() const { \
	static TypeInfo ti = { #type_name, sizeof(type_name) };\
	return ti;\
}\
AddClassToFactory<type_name, Node_CFG> nodecreator##type_name(get_runtime_node_factory(),#type_name);

IMPL_NODE_CFG(Clip_Node_CFG);
IMPL_NODE_CFG(Sync_Node_CFG);
IMPL_NODE_CFG(Mirror_Node_CFG);
IMPL_NODE_CFG(Statemachine_Node_CFG);
IMPL_NODE_CFG(Add_Node_CFG);
IMPL_NODE_CFG(Subtract_Node_CFG);
IMPL_NODE_CFG(Blend_Node_CFG);
IMPL_NODE_CFG(Blend_Int_Node_CFG);
IMPL_NODE_CFG(BlendSpace2d_CFG);
IMPL_NODE_CFG(BlendSpace1d_CFG);
IMPL_NODE_CFG(Scale_By_Rootmotion_CFG);
IMPL_NODE_CFG(Blend_Masked_CFG);


static const char* rm_setting_strs[] = {
	"keep",
	"remove",
	"add_velocity"
};
AutoEnumDef rootmotion_setting_def = AutoEnumDef("rm", 3, rm_setting_strs);

 bool Clip_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	 RT_TYPE* rt = get_rt<RT_TYPE>(ctx);
	 //util_set_to_bind_pose(*pose.pose, ctx.get_skeleton());
	 //return true;

	const AnimationSeq* clip = get_clip(ctx);
	if (!clip) {
		util_set_to_bind_pose(*pose.pose, ctx.get_skeleton());
		return true;
	}

	if (pose.sync)
		rt->anim_time = clip->duration * pose.sync->normalized_frame;


	if (!pose.sync || pose.sync->first_seen) {
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

		if (pose.sync) {
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
	input[REF]->get_pose(ctx, pose);
	GetPose_Ctx pose2 = pose;
	pose2.pose = reftemp;
	input[SOURCE]->get_pose(ctx, pose2);
	util_subtract(ctx.num_bones(), *reftemp, *pose.pose);
	Pose_Pool::get().free(1);
	return true;
}

 bool Add_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	float lerp = ctx.get_float(param);

	Pose* addtemp = Pose_Pool::get().alloc(1);
	input[BASE]->get_pose(ctx, pose);

	GetPose_Ctx pose2 = pose;
	pose2.pose = addtemp;

	input[DIFF]->get_pose(ctx, pose2);
	util_add(ctx.num_bones(), *addtemp, *pose.pose, lerp);
	Pose_Pool::get().free(1);
	return true;
}

 bool Blend_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	 if (!param.is_valid()) {
		 util_set_to_bind_pose(*pose.pose, ctx.get_skeleton());
		 return true;
	 }
	 RT_TYPE* rt = get_rt<RT_TYPE>(ctx);
	 ASSERT(!(rt->lerp_amt != rt->lerp_amt));

	 float value = 0.0;
	 if (store_value_on_reset) {
		 value = rt->saved_f;
	 }
	 else {
		 if (parameter_type == 0)
			 value = ctx.get_float(param);
		 else if (parameter_type == 1) // bool
			 value = (float)ctx.get_bool(param);
		//rt->lerp_amt = damp_dt_independent(value, rt->lerp_amt, damp_factor, pose.dt);
		ASSERT(!(rt->lerp_amt != rt->lerp_amt));
	 }


	 bool keep_going = true;

	Pose* addtemp = Pose_Pool::get().alloc(1);
	keep_going &= input[0]->get_pose(ctx, pose);
	keep_going &= input[1]->get_pose(ctx, pose.set_pose(addtemp));
	util_blend(ctx.num_bones(), *addtemp, *pose.pose, value);
	Pose_Pool::get().free(1);
	return keep_going;
}


 bool Blend_Int_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
 {
	 if (!param.is_valid()) {
		 util_set_to_bind_pose(*pose.pose, ctx.get_skeleton());
		 return true;
	 }
	 RT_TYPE* rt = get_rt<RT_TYPE>(ctx);

	 // param never changes
	 if (!store_value_on_reset) {
		 int val = ctx.get_int(param);
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

	auto rt = get_rt<Mirror_Node_RT>(ctx);
	 if (!store_value_on_reset) {

		 float amt{};
		 if (parameter_type == 0) {
			 amt = ctx.get_float(param);
		 }
		 else {
			 amt = (float)ctx.get_bool(param);
		 }

		 rt->saved_f = damp_dt_independent(amt, rt->saved_f, damp_time, pose.dt);
	 }
	bool ret = input[0]->get_pose(ctx, pose);

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

	auto rt = get_rt<RT_TYPE>(ctx);

	glm::vec2 relmovedir = glm::vec2(
		ctx.get_float(xparam),
		ctx.get_float(yparam)
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
	graph_var_lib = std::make_unique<Library>();
	graph_program = std::make_unique<Program>();
	params = std::make_unique<ControlParam_CFG>();
}

Animation_Tree_CFG::~Animation_Tree_CFG()
{
	for (int i = 0; i < all_nodes.size(); i++) {
		delete all_nodes[i];
	}
}

void Animation_Tree_CFG::init_program_libs()
{
	graph_var_lib->clear();
	params->set_library_vars(graph_var_lib.get());

	graph_program->clear();
	graph_program->push_library(anim_tree_man->get_std_animation_script_lib());
	graph_program->push_library(graph_var_lib.get());
}

void Animation_Tree_CFG::post_load_init()
{
	init_program_libs();

	for (int i = 0; i < all_nodes.size(); i++) {
		all_nodes[i]->initialize(this);
	}
	root = serialized_nodecfg_ptr_to_ptr(root, this);

	params->name_to_index.clear();
	for (int i = 0; i < params->types.size(); i++)
		params->name_to_index[StringName(params->types[i].name.c_str()).get_hash()] = i;
}

 ControlParamHandle Animation_Tree_CFG::find_param(StringName name) {
	return params->find(name);
}

int Animation_Tree_CFG::get_index_of_node(Node_CFG* ptr)
 {
	 for (int i = 0; i < all_nodes.size(); i++) {
		 if (ptr == all_nodes[i]) return i;
	 }
	 return -1;
 }

 PropertyInfoList* Animation_Tree_CFG::get_props()
 {
	 START_PROPS(Animation_Tree_CFG)
		REG_STRUCT_CUSTOM_TYPE(root, PROP_SERIALIZE, "AgSerializeNodeCfg"),
		REG_INT(data_used, PROP_SERIALIZE, ""),
		REG_BOOL(graph_is_valid, PROP_SERIALIZE, "")
	END_PROPS()
 }


 struct getter_nodecfg
 {
	 static void get(std::vector<PropertyListInstancePair>& props, Node_CFG* node) {
		 node->add_props(props);
	 }
 };

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
				 Node_CFG* node = read_object_properties<Node_CFG, getter_nodecfg>(get_runtime_node_factory(), {}, in, view);
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
		 auto res = read_properties(*ControlParam_CFG::get_props(), params.get(), in, {}, {});

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
			 write_object_properties<Node_CFG, getter_nodecfg>(node, ctxptr, out);
		 }
		 out.write_list_end();
	 }
	 { 
		 out.write_key("params");
		 out.write_item_start();
		 write_properties(*ControlParam_CFG::get_props(), params.get(), out, ctxptr);
		 out.write_item_end();
	 }
	 out.write_item_end();
 }

 PropertyInfoList* Blend_Masked_CFG::get_props()
 {
	 START_PROPS(Blend_Masked_CFG)
		 REG_BOOL(meshspace_rotation_blend, PROP_DEFAULT, "0"),
		 REG_INT_W_CUSTOM(param, PROP_DEFAULT, "-1", "AG_PARAM_FINDER"),
		 REG_INT(maskname, PROP_SERIALIZE, "0")
	 END_PROPS(Blend_Masked_CFG)
 }

 PropertyInfoList* Scale_By_Rootmotion_CFG::get_props()
 {
	 return nullptr;
 }

 PropertyInfoList* Sync_Node_CFG::get_props()
 {
	 return nullptr;
 }

 PropertyInfoList* Subtract_Node_CFG::get_props()
 {
	 return nullptr;
 }

 PropertyInfoList* BlendSpace1d_CFG::get_props()
 {
	 START_PROPS(BlendSpace1d_CFG)
		 REG_INT_W_CUSTOM(param, PROP_DEFAULT, "-1", "AG_PARAM_FINDER"),
		 REG_BOOL(is_additive_blend_space, PROP_DEFAULT, "0"),
	END_PROPS(BlendSpace1d_CFG)
 }

 PropertyInfoList* Add_Node_CFG::get_props()
 {
	 START_PROPS(Add_Node_CFG)
		 REG_INT_W_CUSTOM(param, PROP_DEFAULT, "-1", "AG_PARAM_FINDER")
	 END_PROPS(Add_Node_CFG)
 }

 PropertyInfoList* Blend_Int_Node_CFG::get_props()
 {
	 START_PROPS(Blend_Int_Node_CFG)
		 REG_INT_W_CUSTOM(param, PROP_DEFAULT,"-1", "AG_PARAM_FINDER")
	END_PROPS(Blend_Int_Node_CFG)
 }

 PropertyInfoList* BlendSpace2d_CFG::get_props()
 {
	 START_PROPS(BlendSpace2d_CFG)
		 REG_INT_W_CUSTOM(xparam, PROP_DEFAULT, "-1", "AG_PARAM_FINDER"),
		 REG_INT_W_CUSTOM(yparam, PROP_DEFAULT, "-1", "AG_PARAM_FINDER"),
		 REG_FLOAT(weight_damp, PROP_DEFAULT, "0.01")
	 END_PROPS(BlendSpace2d_CFG)
 }

 PropertyInfoList* Mirror_Node_CFG::get_props()
 {
	 START_PROPS(Mirror_Node_CFG)
		 REG_FLOAT(damp_time, PROP_DEFAULT, "0.1"),
		 REG_INT_W_CUSTOM(param, PROP_DEFAULT, "-1", "AG_PARAM_FINDER"),
		 REG_BOOL(store_value_on_reset, PROP_DEFAULT, "0"),
	 END_PROPS(Mirror_Node_CFG)
 }

 PropertyInfoList* Blend_Node_CFG::get_props()
 {
	 START_PROPS(Blend_Node_CFG)
		 REG_INT_W_CUSTOM(param, PROP_DEFAULT, "-1", "AG_PARAM_FINDER"),
		 REG_FLOAT(damp_factor, PROP_DEFAULT, "0.1"),
		 REG_BOOL(store_value_on_reset, PROP_DEFAULT, "0"),
	END_PROPS(Blend_Node_CFG)
 }

 PropertyInfoList* Clip_Node_CFG::get_props()
 {
	 START_PROPS(Clip_Node_CFG)
		 REG_ENUM( rm[0], PROP_DEFAULT, "rm::keep", rootmotion_setting_def.id),
		 REG_ENUM( rm[1], PROP_DEFAULT, "rm::keep", rootmotion_setting_def.id),
		 REG_ENUM( rm[2], PROP_DEFAULT, "rm::keep", rootmotion_setting_def.id),

		 REG_BOOL( loop, PROP_DEFAULT, "1"),
		 REG_FLOAT( speed, PROP_DEFAULT, "1.0,0.1,10"),
		 REG_INT( start_frame, PROP_DEFAULT, "0"),
		 REG_BOOL( allow_sync, PROP_DEFAULT, "0"),
		 REG_BOOL( can_be_leader, PROP_DEFAULT, "1"),

		 REG_STDSTRING_CUSTOM_TYPE( clip_name, PROP_DEFAULT, "AG_CLIP_TYPE")

	END_PROPS(Clip_Node_CFG)
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

 void PropertyInfo::set_float(void* ptr, float f)
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

 void PropertyInfo::set_int(void* ptr, uint64_t i)
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

 void ControlParam_CFG::set_library_vars(Library* lib)
 {
	 for (int i = 0; i < types.size(); i++) {
		 AG_ControlParam& param = types[i];
		 lib->push_global_def(param.name.c_str(), get_script_type_for_control_param(param.type));
	 }
 }


 namespace Script {
	 static void time_remaining(script_state* state)
	 {

	 }

	 static void is_tag_active(script_state* state)
	 {

	 }

	 static void is_tag_entered(script_state* state)
	 {

	 }

	 static void is_tag_exited(script_state* state)
	 {

	 }

	 static void get_curve(script_state* state)
	 {

	 }
 }


 const Library* Animation_Tree_Manager::get_std_animation_script_lib()
 {
	 static bool init = true;
	 static Library lib;
	 if (init) {
		 lib.push_function_def("time_remaining", "float", "transition_t", Script::time_remaining);
		 lib.push_function_def("is_tag_active", "bool", "transition_t,int", Script::is_tag_active);
		 lib.push_function_def("is_tag_entered", "bool", "transition_t,int", Script::is_tag_entered);
		 lib.push_function_def("is_tag_exited", "bool", "transition_t,int", Script::is_tag_exited);
		 lib.push_function_def("get_curve", "float", "transition_t,name", Script::get_curve);
		 init = false;
	 }
	 return &lib;
 }

 const char* cpt_strs[] = {
	"int_t",
	"enum_t",
	"bool_t",
	"float_t",
 };
 AutoEnumDef control_param_type_def = AutoEnumDef("cpt", 4, cpt_strs);

 Factory<std::string, Node_CFG>& get_runtime_node_factory()
 {
	static Factory<std::string, Node_CFG> factory;
	return factory;
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

 static AddClassToFactory<AgSerializeNodeCfg, IPropertySerializer> abc(get_property_serializer_factory(), "AgSerializeNodeCfg");

 PropertyInfoList* get_nodecfg_ptr_type()
 {
	 static PropertyInfo props[] = {
		 make_struct_property("_value",0,PROP_SERIALIZE,"AgSerializeNodeCfg")
	 };
	 static PropertyInfoList list = { props,1,"nodecfg_ptr" };
	 return &list;
 }


 PropertyInfoList* Node_CFG::get_props_static()
 {
	 MAKE_INLVECTORCALLBACK_TYPE(get_nodecfg_ptr_type(), input, Node_CFG);
	 START_PROPS(Node_CFG)
		 REG_STDVECTOR(input, PROP_SERIALIZE)
	END_PROPS(Node_CFG);
 }

 PropertyInfoList* AG_ControlParam::get_props()
 {
	 START_PROPS(AG_ControlParam)
		 REG_BOOL(reset_after_tick, PROP_SERIALIZE, ""),
		 REG_ENUM(type, PROP_SERIALIZE, "", control_param_type_def.id),
		 REG_STDSTRING(name, PROP_SERIALIZE),
		 REG_INT(default_i, PROP_SERIALIZE,""),
		 REG_FLOAT(default_f, PROP_SERIALIZE, ""),
		 REG_INT(enum_idx, PROP_SERIALIZE,""),
	END_PROPS(AG_ControlParam)
 }

 PropertyInfoList* ControlParam_CFG::get_props()
 {
	 MAKE_VECTORCALLBACK(AG_ControlParam, types);
	 START_PROPS(ControlParam_CFG)
		 REG_STDVECTOR(types, PROP_SERIALIZE)
	 END_PROPS(ControlParam_CFG)
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
	 auto rt = get_rt<RT_TYPE>(ctx);
	 if(!rt->mask)
		 return input[0]->get_pose(ctx, pose);


	 bool b = ctx.get_bool(param);

	 if (!b) {
		 return input[0]->get_pose(ctx, pose);
	 }

#if 1
	
	 if (meshspace_rotation_blend) {
		 Pose* base_layer = Pose_Pool::get().alloc(1);
		 bool ret = input[1]->get_pose(ctx, pose);
		 ret &= input[0]->get_pose(ctx, pose.set_pose(base_layer));
		 util_global_blend(ctx.get_skeleton(),base_layer, pose.pose, mask_weight, rt->mask->weight);
		 Pose_Pool::get().free(1);
		 return ret;
	 }


	 // nase

	 else {
		 Pose* layer = Pose_Pool::get().alloc(1);
		 bool ret = input[0]->get_pose(ctx, pose);
		 ret &= input[1]->get_pose(ctx, pose.set_pose(layer));
		 util_blend_with_mask(ctx.num_bones(), *layer, *pose.pose, 1.0, rt->mask->weight);
		 Pose_Pool::get().free(1);
		 return ret;
	 }

#endif
 }
