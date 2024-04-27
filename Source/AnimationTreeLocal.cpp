#include "AnimationTreeLocal.h"

#include "AnimationUtil.h"

#include "DictWriter.h"
#include "DictParser.h"

#include "GlobalEnumMgr.h"

#include "ReflectionRegisterDefines.h"
#include "StdVectorReflection.h"


struct AutoAnimNodeDef
{
	AutoAnimNodeDef(animnode_type type, create_func create, const char* ed_name, const char* ed_tooltip, Color32 color) {
		get_animnode_typedef(type).create = create;
		get_animnode_typedef(type).editor_name = ed_name;
		get_animnode_typedef(type).editor_tooltip = ed_tooltip;
		get_animnode_typedef(type).editor_color = color;
	}
};
#define IMPL_TOOL_NODE(type, name /* editor name */, tooltip /* editor tooltip */, color /* editor node color*/)   static AutoAnimNodeDef autoanimnode_##type(animnode_type::type,nullptr, name, tooltip, color)
#define IMPL_ANIMNODE(type, name, tooltip, color) static AutoAnimNodeDef autoanimnode_##type(type::CONST_TYPE_ENUM, type::create, name, tooltip, color)


// colors for editor nodes
static const Color32 ROOT_COLOR = { 94, 2, 2 };
static const Color32 SM_COLOR = { 82, 2, 94 };
static const Color32 STATE_COLOR = { 15, 61, 16 };
static const Color32 SOURCE_COLOR = { 1, 0, 74 };
static const Color32 BLEND_COLOR = { 26, 75, 79 };
static const Color32 ADD_COLOR = { 44, 57, 71 };
static const Color32 MISC_COLOR = { 13, 82, 44 };

IMPL_TOOL_NODE(start_statemachine, "START", "State machine enter", ROOT_COLOR);
IMPL_TOOL_NODE(root, "OUTPUT", "Output pose for the blend tree", ROOT_COLOR);
IMPL_TOOL_NODE(state, "State", "A state in a FSM. Use output pin to create transitions to other states\n", STATE_COLOR);

IMPL_ANIMNODE(Clip_Node_CFG, "Clip", "Animation input data", SOURCE_COLOR);
IMPL_ANIMNODE(Sync_Node_CFG, "Sync", "Syncs animations below this node", MISC_COLOR);
IMPL_ANIMNODE(Mirror_Node_CFG, "Mirror", "Mirrors animation across axis", MISC_COLOR);
IMPL_ANIMNODE(Statemachine_Node_CFG, "Statemachine", "Contains a statemachine with transitions\n(double click to open)", SM_COLOR);
IMPL_ANIMNODE(Add_Node_CFG, "Apply Additive", "Apply additive(delta'd) animation to a base pose", ADD_COLOR);
IMPL_ANIMNODE(Subtract_Node_CFG, "Subtract", "Subtract a source pose from a ref pose, use with \'Apply Additive\'", ADD_COLOR);
IMPL_ANIMNODE(Blend_Node_CFG, "Blend", "Blend 2 poses by a boolean or float [0,1]", BLEND_COLOR);
IMPL_ANIMNODE(Blend_Int_Node_CFG, "Blend by int/enum", "Blend n poses by an int or enum\n", BLEND_COLOR);
IMPL_ANIMNODE(Blend2d_CFG, "Blend 2D", "Blend 8 directional poses and an idle pose\nUse for directional locomotion", BLEND_COLOR);
IMPL_ANIMNODE(Scale_By_Rootmotion_CFG, "Rootmotion Scale", "Scale any clip sampling by the velocity parameter. \n(ex: a running clip is at 2 m/s, entity is moving at 4 m/s, so clip is played at 2x speed", MISC_COLOR);


static const char* animnode_strs[] = {
	"source",
	"statemachine",
	"mask",
	"blend",
	"blend_by_int",
	"blend2d",
	"add",
	"subtract",
	"aimoffset",
	"mirror",
	"play_speed",
	"rootmotion_speed",
	"sync",
	"state",
	"root",
	"start_statemachine",
	"COUNT",
};

static_assert((sizeof(animnode_strs) / sizeof(char*)) ==  ((int)animnode_type::COUNT + 1), "string reflection out of sync");
AutoEnumDef animnode_type_def = AutoEnumDef("",sizeof(animnode_strs)/sizeof(char*), animnode_strs);


bool ScriptExpression::evaluate(NodeRt_Ctx& ctx) const
{
	return compilied.execute(*ctx.vars).ival;
}



PropertyInfoList* State::get_props()
{
	MAKE_VECTORCALLBACK(State_Transition, transitions);
	START_PROPS(State)
		REG_STDVECTOR(transitions, PROP_DEFAULT),
		REG_FLOAT(state_duration, PROP_DEFAULT, ""),
		REG_BOOL(transition_wait_on_end, PROP_DEFAULT, ""),
		REG_INT(next_state.id, PROP_SERIALIZE, ""),
		REG_STDSTRING(name, PROP_SERIALIZE, "")
	END_PROPS(State)
}

handle<State> State::get_next_state(NodeRt_Ctx& ctx) const
{
	for (int i = 0; i < transitions.size(); i++) {
		// evaluate condition
		if (transitions[i].script.evaluate(ctx))
			return  transitions[i].transition_state;
	}
	return { -1 };
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
	float lerp = ctx.vars->get(param).fval;

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

	 float value = 0.0;
	 if (store_value_on_reset) {
		 value = rt->saved_f;
	 }
	 else {
		 if (parameter_type == 0)
			 value = ctx.vars->get(param).fval;
		 else if (parameter_type == 1) // bool
			 value = (float)ctx.vars->get(param).ival;
		rt->lerp_amt = damp_dt_independent(value, rt->lerp_amt, damp_factor, pose.dt);
	 }


	 bool keep_going = true;

	Pose* addtemp = Pose_Pool::get().alloc(1);
	keep_going &= input[0]->get_pose(ctx, pose);
	keep_going &= input[1]->get_pose(ctx, pose.set_pose(addtemp));
	util_blend(ctx.num_bones(), *addtemp, *pose.pose, rt->lerp_amt);
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
		 int val = ctx.vars->get(param).ival;
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
	float amt = ctx.vars->get(param).fval;

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

 bool Statemachine_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {

	auto rt = get_rt<Statemachine_Node_RT>(ctx);

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
	return true;
}

// Inherited via At_Node

 bool Blend2d_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
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
	if (character_ground_speed < 0.9999) {

		if (anglelerp <= 0.5) {
			input[DIRECTION + pose1]->get_pose(ctx, pose.set_pose(&scratchposes[1]));
			input[DIRECTION + pose2]->get_pose(ctx, pose);
		}
		else {
			input[DIRECTION + pose2]->get_pose(ctx, pose);
			input[DIRECTION + pose1]->get_pose(ctx, pose.set_pose(&scratchposes[1]));
		}
		input[IDLE]->get_pose(ctx, pose.set_pose(&scratchposes[0]));

		util_blend(ctx.num_bones(), scratchposes[1], *pose.pose, anglelerp);
		util_blend(ctx.num_bones(), scratchposes[0], *pose.pose, 1.0 - character_ground_speed);
	}
	else {
		if (anglelerp <= 0.5) {
			input[DIRECTION + pose1]->get_pose(ctx, pose.set_pose(&scratchposes[1]));
			input[DIRECTION + pose2]->get_pose(ctx, pose);
		}
		else {
			input[DIRECTION + pose2]->get_pose(ctx, pose);
			input[DIRECTION + pose1]->get_pose(ctx, pose.set_pose(&scratchposes[1]));
		}

		util_blend(ctx.num_bones(), scratchposes[0], *pose.pose, anglelerp);
	}


	Pose_Pool::get().free(3);

	return true;
}

 static const char* parameter_type_to_string(script_parameter_type type)
 {
	 switch (type)
	 {
	 case script_parameter_type::float_t:
		 return "float_t";
	 case script_parameter_type::int_t:
		 return "int_t";
	 case script_parameter_type::bool_t:
		 return "bool_t";
	 case script_parameter_type::enum_t:
		 return "enum_t";
	 default:
		 ASSERT(!"no type defined");
		 break;
	 }
}

animnode_name_type& get_animnode_typedef(animnode_type type) {
	static animnode_name_type types[(int)animnode_type::COUNT];

	return types[(int)type];
 }

 int Animation_Tree_CFG::get_index_of_node(Node_CFG* ptr)
 {
	 for (int i = 0; i < all_nodes.size(); i++) {
		 if (ptr == all_nodes[i]) return i;
	 }
	 return -1;
 }


 void Animation_Tree_CFG::write_to_dict(DictWriter& out)
 {
	 out.write_item_start();

	 out.write_key_value("name", this->name.c_str());	
	 out.write_key_value("data_used", string_format("%d", (int)data_used));
	 out.write_key_value("root_node", string_format("%d", get_index_of_node(root)));

	 out.write_key_list_start("params");
	 for (auto& param_int : parameters.name_to_index) {

		 auto& param = parameters.types.at(param_int.second);

		 out.write_item_start();
		 out.write_key_value("name", param_int.first.c_str());
		 out.write_key_value("type", parameter_type_to_string(param.default_.type));

		 out.write_key_value("reset_on_tick", string_format("%d", (int)param.reset_after_tick));


		 out.write_item_end();
	 }
	 out.write_list_end();

	 out.write_key_list_start("nodes");

	 for (int i = 0; i < all_nodes.size(); i++) {
		 auto& node = all_nodes[i];
		 out.write_item_start();

		 const char* type_name = GlobalEnumDefMgr::get().get_enum_type_name(animnode_type_def.id);
		 const char* enum_str = GlobalEnumDefMgr::get().get_enum_name(animnode_type_def.id, (int)node->get_type());
		 char* out_str = string_format("%s::%s", type_name, enum_str);

		 out.write_key_value("type", out_str);

		 out.write_key_value("default_param", string_format("%d", node->param.id));

		 out.write_key_list_start("inputs");
		 for (int i = 0; i < node->input.count; i++) {
			 out.write_value( string_format("%d", get_index_of_node(node->input[i]) ) );
		 }
		 out.write_list_end();

		 write_properties(*node->get_props(), node, out);

		 node->write_to_dict(this, out);

		 out.write_item_end();
	 }

	 out.write_list_end();
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

 PropertyInfoList* Add_Node_CFG::get_props()
 {
	 START_PROPS(Add_Node_CFG)
		 REG_INT_W_CUSTOM(param, PROP_DEFAULT, "", "AG_PARAM_FINDER")
	 END_PROPS(Add_Node_CFG)
 }

 PropertyInfoList* Blend_Int_Node_CFG::get_props()
 {
	 START_PROPS(Blend_Int_Node_CFG)
		 REG_INT_W_CUSTOM(param, PROP_DEFAULT,"", "AG_PARAM_FINDER")
	END_PROPS(Blend_Int_Node_CFG)
 }

 PropertyInfoList* Blend2d_CFG::get_props()
 {
	 START_PROPS(Blend2d_CFG)
		 REG_INT_W_CUSTOM(xparam, PROP_DEFAULT, "", "AG_PARAM_FINDER"),
		 REG_INT_W_CUSTOM(yparam, PROP_DEFAULT, "", "AG_PARAM_FINDER"),
		 REG_FLOAT(weight_damp, PROP_DEFAULT, "")
	 END_PROPS(Blend2d_CFG)
 }

 PropertyInfoList* Mirror_Node_CFG::get_props()
 {
	 START_PROPS(Mirror_Node_CFG)
		 REG_FLOAT(damp_time, PROP_DEFAULT, ""),
		 REG_INT_W_CUSTOM(param, PROP_DEFAULT, "", "AG_PARAM_FINDER")
	 END_PROPS(Mirror_Node_CFG)
 }

 PropertyInfoList* Blend_Node_CFG::get_props()
 {
	 START_PROPS(Blend_Node_CFG)
		 REG_INT_W_CUSTOM(param, PROP_DEFAULT, "", "AG_PARAM_FINDER"),
		 REG_FLOAT(damp_factor, PROP_DEFAULT, ""),
		 REG_BOOL(store_value_on_reset, PROP_DEFAULT, ""),
	END_PROPS(Blend_Node_CFG)
 }

 PropertyInfoList* Clip_Node_CFG::get_props()
 {
	 START_PROPS(Clip_Node_CFG)
		 REG_ENUM( rm[0], PROP_DEFAULT, "", rootmotion_setting_def.id),
		 REG_ENUM( rm[1], PROP_DEFAULT, "", rootmotion_setting_def.id),
		 REG_ENUM( rm[2], PROP_DEFAULT, "", rootmotion_setting_def.id),

		 REG_BOOL( loop, PROP_DEFAULT, ""),
		 REG_FLOAT( speed, PROP_DEFAULT, "0,-5,5"),
		 REG_INT( start_frame, PROP_DEFAULT, ""),
		 REG_BOOL( allow_sync, PROP_DEFAULT, ""),
		 REG_BOOL( can_be_leader, PROP_DEFAULT, ""),

		 REG_STDSTRING_CUSTOM_TYPE( clip_name, PROP_DEFAULT, "AG_CLIP_TYPE")

	END_PROPS(Clip_Node_CFG)
 }

 PropertyInfoList* Statemachine_Node_CFG::get_props()
 {
	 START_PROPS(Statemachine_Node_CFG)
		 REG_INT( start_state, PROP_SERIALIZE, "")
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


 void Statemachine_Node_CFG::write_to_dict(Animation_Tree_CFG* tree, DictWriter& out) {
	 out.write_key_list_start("states");

	 std::string buf;

	 for (int i = 0; i < states.size(); i++) {
		 auto& state = states[i];
		 out.write_item_start();

		 out.write_key_value("name", state.name.c_str());
		 out.write_key_value("next_state", string_format("%d", state.next_state.id));
		 out.write_key_value("duration", string_format("%f", state.state_duration));
		 out.write_key_value("node_index", string_format("%d", tree->get_index_of_node(state.tree)));

		 out.write_key_list_start("transitions");
		 for (int j = 0; j < state.transitions.size(); j++) {
			 auto& tr = state.transitions[j];
			 out.write_key_value("transition_state", string_format("%d", tr.transition_state.id));
			 out.write_key_value("script", tr.script.script_str.c_str());
		 }
		 out.write_list_end();

		 out.write_item_end();
	 }
	 out.write_list_end();
 }


 // FIXME BROKEN AS FUCK
 void Statemachine_Node_CFG::read_from_dict(Animation_Tree_CFG* tree, DictParser& in) 
 {
	 ASSERT(0);
	 StringView tok;
	 in.read_string(tok);
	 if (!tok.cmp("states")) {
		 printf("couldnt find states\n");
		 return;
	 }

	 if (!in.expect_list_start()) {
		 printf("nop list start\n");
		 return;
	 }

	 while (1)
	 {
		 if (!in.read_string(tok))
			 break;
		 if (!in.check_item_start(tok))
			 break;

		 State state;

		 in.read_string(tok);
		 while (!in.check_item_end(tok) && !in.is_eof()) {
			 if (tok.cmp("name")) {
				 in.read_string(tok);
				 state.name = tok.to_stack_string().c_str();
			 }
			 else if (tok.cmp("next_state")) {
				 in.read_int(state.next_state.id);
			 }
			 else if (tok.cmp("duration")) {
				 in.read_float(state.state_duration);
			 }
			 else if (tok.cmp("node_index")) {
				 int index;
				 in.read_int(index);
				 state.tree = (Node_CFG*)index;	// fixup later
			 }
			 else if (tok.cmp("transitions")) {
				 if (!in.expect_list_start())
					 return;

				 in.read_string(tok);
				 while (!in.check_list_end(tok) && !in.is_eof())
				 {
					 State_Transition transition;

					 in.read_string(tok);
					 while (!in.check_item_end(tok) && !in.is_eof()) {

						 if (tok.cmp("transition_state")) {
							 in.read_int(transition.transition_state.id);
						 }
						 else if (tok.cmp("script")) {
							 in.read_string(tok);
							 // run lisp interpreter to compile later
							 transition.script.script_str = tok.to_stack_string().c_str();
						 }

						 in.read_string(tok);
					 }

					 in.read_string(tok);
				 }

			 }
		 }
	 }
 }

 float PropertyInfo::get_float(void* ptr)
 {
	 ASSERT(type == core_type_id::Float);

	 return *(float*)((char*)ptr + offset);
 }

 void PropertyInfo::set_float(void* ptr, float f)
 {
	 ASSERT(type == core_type_id::Float);

	 *(float*)((char*)ptr + offset) = f;
 }

 int PropertyInfo::get_int(void* ptr)
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
	 else {
		 ASSERT(0);
	 }
 }

 PropertyInfoList* State_Transition::get_props()
 {
	 START_PROPS(State_Transition)
		 REG_INT( transition_state, PROP_SERIALIZE, ""),
		 REG_FLOAT( transition_time, PROP_DEFAULT, ""),
		 REG_STDSTRING( script.script_str, PROP_SERIALIZE, ""),
		 REG_STRUCT_CUSTOM_TYPE( script, PROP_EDITABLE, "AG_LISP_CODE")
	END_PROPS(State_Transition)
 }
