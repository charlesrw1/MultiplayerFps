#include "AnimationTreeLocal.h"
#include "../AnimationUtil.h"
#include "Framework/DictWriter.h"
#include "Framework/DictParser.h"
#include "Framework/ReflectionRegisterDefines.h"
#include "Framework/StdVectorReflection.h"
#include "ControlParams.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/WriteObject.h"


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


PropertyInfoList* State::get_props()
{
	MAKE_INLVECTORCALLBACK_ATOM(uint16_t, transition_idxs, State);
	START_PROPS(State)
		REG_STDVECTOR(transition_idxs,PROP_SERIALIZE),
		REG_FLOAT(state_duration, PROP_DEFAULT, "-1.0"),
		REG_BOOL(wait_until_finish, PROP_DEFAULT, "0"),
		REG_BOOL(is_end_state, PROP_DEFAULT, "0"),
		REG_STRUCT_CUSTOM_TYPE(tree, PROP_SERIALIZE, "AgSerializeNodeCfg"),
	END_PROPS(State)
}

static const char* rm_setting_strs[] = {
	"keep",
	"remove",
	"add_velocity"
};
AutoEnumDef rootmotion_setting_def = AutoEnumDef("rm", 3, rm_setting_strs);

 bool Clip_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	 RT_TYPE* rt = get_rt<RT_TYPE>(ctx);


	const Animation* clip = get_clip(ctx);
	if (!clip) {
		util_set_to_bind_pose(*pose.pose, ctx.model);
		return true;
	}

	if (pose.sync)
		rt->frame = clip->total_duration * pose.sync->normalized_frame;


	if (!pose.sync || pose.sync->first_seen) {
		if (pose.rootmotion_scale >= 0) {
			// want to match character_speed and speed_of_anim
			float speedup = pose.rootmotion_scale * rt->inv_speed_of_anim_root;
			pose.dt *= speedup;
		}

		rt->frame += clip->fps * pose.dt * speed;

		if (rt->frame > clip->total_duration || rt->frame < 0.f) {
			if (loop)
				rt->frame = fmod(fmod(rt->frame, clip->total_duration) + clip->total_duration, clip->total_duration);
			else {
				rt->frame = clip->total_duration - 0.001f;
				rt->stopped_flag = true;
			}
		}

		if (pose.sync) {
			pose.sync->first_seen = false;
			pose.sync->normalized_frame = rt->frame / clip->total_duration;
		}

	}

	const std::vector<int>* indicies = nullptr;
	if (rt->skel_idx != -1)
		indicies = &ctx.set->get_remap(rt->skel_idx);

	util_calc_rotations(ctx.set->get_subset(rt->set_idx), rt->frame, rt->clip_index, ctx.model, indicies , *pose.pose);


	int root_index = ctx.model->root_bone_index;
	for (int i = 0; i < 3; i++) {
		if (rm[i] == rootmotion_setting::remove) {
			pose.pose->pos[root_index][i] = rt->root_pos_first_frame[i];
		}
	}

	return !rt->stopped_flag;
}

// Inherited via At_Node

 bool Subtract_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {

	Pose* reftemp = Pose_Pool::get().alloc(1);
	input[REF]->get_pose(ctx, pose);
	GetPose_Ctx pose2 = pose;
	pose2.pose = reftemp;
	input[SOURCE]->get_pose(ctx, pose2);
	util_subtract(ctx.model->bones.size(), *reftemp, *pose.pose);
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
	util_add(ctx.model->bones.size(), *addtemp, *pose.pose, lerp);
	Pose_Pool::get().free(1);
	return true;
}

 bool Blend_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	 if (!param.is_valid()) {
		 util_set_to_bind_pose(*pose.pose, ctx.model);
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
		 util_set_to_bind_pose(*pose.pose, ctx.model);
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
	float amt = ctx.get_float(param);

	auto rt = get_rt<Mirror_Node_RT>(ctx);
	rt->lerp_amt = damp_dt_independent(amt, rt->lerp_amt, damp_time, pose.dt);

	bool ret = input[0]->get_pose(ctx, pose);

	bool has_mirror_map = ctx.set->src_skeleton->bone_mirror_map.size() == ctx.model->bones.size();

	if (has_mirror_map && rt->lerp_amt >= 0.000001) {
		const Model* m = ctx.model;
		Pose* posemirrored = Pose_Pool::get().alloc(1);
		// mirror the bones
		for (int i = 0; i < m->bones.size(); i++) {


			int from = ctx.set->src_skeleton->bone_mirror_map.at(i);
			if (from == -1)from = i;

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

 void Statemachine_Node_CFG::initialize(Animation_Tree_CFG* tree) {
	 init_memory_internal(tree, sizeof(RT_TYPE));

	 for (int i = 0; i < states.size(); i++) {
		 states[i].tree = serialized_nodecfg_ptr_to_ptr(states[i].tree, tree);
	 }

	 if (tree->graph_is_valid) {
		 // this can be serialized to a bytestream but for now just compile it on load
		 // since the graph is valid, there shoudnt be and runtime errors
		 // however, graph_is_valid can be true and the script is bad ONLY IF
		 // this statemachine isnt referenced in the final tree, thus a bad script
		 // has no effect on the final graph
		 // theres an assert checking that the script is valid when its checked in the 
		 // graphs real path

		 for (int i = 0; i < transitions.size(); i++) {
			 if (transitions[i].is_a_continue_transition()) 
				 continue;

			 bool bad = false;
			const std::string& code = transitions[i].script_uncompilied;
			try {
				 auto ret = transitions[i].script_condition.compile(
					 tree->graph_program.get(),
					 code,
					 NAME("transition_t"));		// selfname = transition_t, for special transition functions like time_remaining() etc.
			
				 // must return boolean
				 if (ret.out_types.size() != 1 || ret.out_types[0] != script_types::bool_t)
					 bad = true;
			}
			catch (...) {
				bad = true;
			}
			if (bad)
				transitions[i].script_condition.instructions.clear();
		 }
	 }
 }

 bool Statemachine_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {

	auto rt = get_rt<Statemachine_Node_RT>(ctx);
#if 0
	// evaluate state machine
	if (!rt->active_state.is_valid()) {
		rt->active_state = start_state;
		rt->active_weight = 0.0;
	}
	const State* next_state = nullptr;;// = (change_to_next_state) ? active_state->next_state : active_state->get_next_state(animator);
	if (rt->change_to_next) next_state = get_state(get_state(rt->active_state)->next_state);
	else {
		const State* active_state = get_state(rt->active_state);
		next_state = get_state(active_state->get_next_state(ctx));
		const State* next_state2 = get_state(next_state->get_next_state(ctx));
		int infinite_loop_check = 0;
		while (next_state2 != next_state) {
			next_state = next_state2;
			next_state2 = get_state(next_state->get_next_state(ctx));
			infinite_loop_check++;

			ASSERT(infinite_loop_check < 100);
		}
	}

	rt->change_to_next = false;


	auto next_state_handle = get_state_handle(next_state);
	if (rt->active_state.id != next_state_handle.id) {
		if (next_state_handle.id == rt->fading_out_state.id) {
			std::swap(rt->active_state, rt->fading_out_state);
			rt->active_weight = 1.0 - rt->active_weight;
		}
		else {
			rt->fading_out_state = rt->active_state;
			rt->active_state = next_state_handle;

			get_state(rt->active_state)->tree->reset(ctx);
			//fade_in_time = g_fade_out;
			rt->active_weight = 0.f;
		}
		printf("changed to state %s\n", get_state(rt->active_state)->name.c_str());
	}

	rt->active_weight += pose.dt / fade_in_time;
	if (rt->active_weight > 1.f) {
		rt->active_weight = 1.f;
		rt->fading_out_state = { -1 };
	}

	auto active_state_ptr = get_state(rt->active_state);
	auto fading_state_ptr = get_state(rt->fading_out_state);


	bool notdone = active_state_ptr->tree->get_pose(ctx, pose);

	if (fading_state_ptr) {
		Pose* fading_out_pose = Pose_Pool::get().alloc(1);


		fading_state_ptr->tree->get_pose(ctx,
			pose.set_dt(0.f)
			.set_pose(fading_out_pose)
		);
		//printf("%f\n", active_weight);
		assert(rt->fading_out_state.id != rt->active_state.id);
		util_blend(ctx.num_bones(), *fading_out_pose, *pose.pose, 1.0 - rt->active_weight);
		Pose_Pool::get().free(1);
	}

	if (!notdone) {	// if done
		if (active_state_ptr->next_state.is_valid()) {
			rt->change_to_next = true;
			return true;
		}
		else {
			return false;	// bubble up the finished event
		}
	}
#endif
	return true;
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

 PropertyInfoList* Statemachine_Node_CFG::get_props()
 {
	 MAKE_INLVECTORCALLBACK_ATOM(uint16_t, entry_transitions, Statemachine_Node_CFG);
	 MAKE_VECTORCALLBACK(State, states);
	 MAKE_VECTORCALLBACK(State_Transition, transitions);
	 START_PROPS(Statemachine_Node_CFG)
		 REG_STDVECTOR(states,PROP_SERIALIZE),
		 REG_STDVECTOR(transitions, PROP_SERIALIZE),
		 REG_STDVECTOR(entry_transitions, PROP_SERIALIZE)
	END_PROPS(Statemachine_Node_CFG)
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

 int PropertyInfo::get_int(void* ptr) const
 {
	 ASSERT(is_integral_type());
	 if (type == core_type_id::Bool || type == core_type_id::Int8 || type == core_type_id::Enum8) {
		 return *(uint8_t*)((char*)ptr + offset);
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

 void PropertyInfo::set_int(void* ptr, int i)
 {
	 ASSERT(is_integral_type());
	 if (type == core_type_id::Bool || type == core_type_id::Int8 || type == core_type_id::Enum8) {
		 *(uint8_t*)((char*)ptr + offset) = i;
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

 PropertyInfoList* State_Transition::get_props()
 {
	 START_PROPS(State_Transition)
		 REG_INT( transition_state, PROP_SERIALIZE, "-1"),
		 REG_BOOL(automatic_transition_rule, PROP_DEFAULT, "1"),
		 REG_FLOAT( transition_time, PROP_DEFAULT, "0.2"),
		 REG_STDSTRING_CUSTOM_TYPE( script_uncompilied, PROP_DEFAULT, "AG_LISP_CODE"),
		 REG_BOOL(is_continue_transition, PROP_DEFAULT, "0")
	END_PROPS(State_Transition)
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
		 REG_INT(enum_idx, PROP_SERIALIZE,"")
	END_PROPS(AG_ControlParam)
 }

 PropertyInfoList* ControlParam_CFG::get_props()
 {
	 MAKE_VECTORCALLBACK(AG_ControlParam, types);
	 START_PROPS(ControlParam_CFG)
		 REG_STDVECTOR(types, PROP_SERIALIZE)
	 END_PROPS(ControlParam_CFG)
 }
