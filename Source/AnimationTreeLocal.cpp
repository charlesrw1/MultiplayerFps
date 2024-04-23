#include "AnimationTreeLocal.h"

#include "AnimationUtil.h"

#include "DictWriter.h"
#include "DictParser.h"

#define DECLARE_PROPERTIES(type) Node_Property_List type::properties;

DECLARE_PROPERTIES(Clip_Node_CFG);
DECLARE_PROPERTIES(Sync_Node_CFG);
DECLARE_PROPERTIES(Mirror_Node_CFG);
DECLARE_PROPERTIES(Statemachine_Node_CFG);
DECLARE_PROPERTIES(Add_Node_CFG);
DECLARE_PROPERTIES(Subtract_Node_CFG);
DECLARE_PROPERTIES(Blend_Node_CFG);
DECLARE_PROPERTIES(Blend2d_CFG);
DECLARE_PROPERTIES(Scale_By_Rootmotion_CFG);




bool ScriptExpression::evaluate(NodeRt_Ctx& ctx) const
{
	return compilied.execute(*ctx.vars).ival;
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

// Inherited via At_Node

 bool Clip_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	Clip_Node_RT* rt = get_rt<Clip_Node_RT>(ctx);


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
		if (rootmotion[i] == Remove) {
			pose.pose->pos[root_index][i] = rt->root_pos_first_frame[i];
		}
	}

	return !rt->stopped_flag;
}

// Inherited via At_Node

 bool Subtract_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {

	Pose* reftemp = Pose_Pool::get().alloc(1);
	input[REF]->get_pose(ctx, pose);
	GetPose_Ctx pose2 = pose;
	pose2.pose = reftemp;
	input[SOURCE]->get_pose(ctx, pose2);
	util_subtract(ctx.model->bones.size(), *reftemp, *pose.pose);
	Pose_Pool::get().free(1);
	return true;
}

 bool Add_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
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

 bool Blend_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
{
	float dest = ctx.vars->get(param).fval;

	Blend_Node_RT* rt = get_rt<Blend_Node_RT>(ctx);

	rt->lerp_amt = damp_dt_independent(dest, rt->lerp_amt, damp_factor, pose.dt);


	Pose* addtemp = Pose_Pool::get().alloc(1);
	input[0]->get_pose(ctx, pose);
	input[1]->get_pose(ctx, pose.set_pose(addtemp));
	util_blend(ctx.num_bones(), *addtemp, *pose.pose,dest);
	Pose_Pool::get().free(1);
	return true;
}

// Inherited via At_Node

 bool Mirror_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const
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

 bool Statemachine_Node_CFG::get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {

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
			input[DIRECTION + pose1]->get_pose(ctx, pose.set_pose(&scratchposes[1]));
			input[DIRECTION + pose2]->get_pose(ctx, pose);
		}
		else {
			input[DIRECTION + pose2]->get_pose(ctx, pose);
			input[DIRECTION + pose1]->get_pose(ctx, pose.set_pose(&scratchposes[1]));
		}
		input[IDLE]->get_pose(ctx, pose.set_pose(&scratchposes[0]));

		util_blend(ctx.num_bones(), scratchposes[1], *pose.pose, anglelerp);
		float speed_lerp = MidLerp(0.0, fade_in, character_ground_speed);
		util_blend(ctx.num_bones(), scratchposes[0], *pose.pose, 1.0 - speed_lerp);
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
	 case script_parameter_type::animvec2:
		 return "vec2";
		 break;
	 case script_parameter_type::animfloat:
		 return "float";
		 break;
	 case script_parameter_type::animint:
		 return "int";
		 break;
	 default:
		 ASSERT(!"no type defined");
		 break;
	 }
}

#define DEFINE_ANIMNODE_DEF(name, class_) nodes[(int)animnode_type::name] = {#name, class_::create, class_::register_props}
#define DEFINE_ANIMNODE_DEF_NO_PROP(name, class_) nodes[(int)animnode_type::name] = {#name, class_::create, nullptr}

 static animnode_name_type* get_anim_node_map()
 {
	 static animnode_name_type nodes[(int)animnode_type::COUNT];
	 static bool initialized = false;
	 if (!initialized) {
		 DEFINE_ANIMNODE_DEF(source, Clip_Node_CFG);
		 DEFINE_ANIMNODE_DEF(statemachine, Statemachine_Node_CFG);
		 DEFINE_ANIMNODE_DEF(blend, Blend_Node_CFG);
		 DEFINE_ANIMNODE_DEF(blend2d, Blend2d_CFG);
		 DEFINE_ANIMNODE_DEF_NO_PROP(add, Add_Node_CFG);
		 DEFINE_ANIMNODE_DEF_NO_PROP(subtract, Subtract_Node_CFG);
		 DEFINE_ANIMNODE_DEF_NO_PROP(sync, Sync_Node_CFG);
		 DEFINE_ANIMNODE_DEF_NO_PROP(rootmotion_speed, Scale_By_Rootmotion_CFG);
		 DEFINE_ANIMNODE_DEF(mirror, Mirror_Node_CFG);

		 for (int i = 0; i < (int)animnode_type::COUNT; i++) {
			 if (nodes[i].reg)
				 nodes[i].reg();
		 }

		 initialized = true;
	 }
	 return nodes;
 }
#undef DEFINE_ANIMNODE_DEF

animnode_name_type& get_animnode_typedef(animnode_type type) {
	 return get_anim_node_map()[(int)type];
 }

 int Animation_Tree_CFG::get_index_of_node(Node_CFG* ptr)
 {
	 for (int i = 0; i < all_nodes.size(); i++) {
		 if (ptr == all_nodes[i]) return i;
	 }
	 return -1;
 }

 static void write_properties(Node_Property_List& list, void* ptr, DictWriter& out);

 void Animation_Tree_CFG::write_to_dict(DictWriter& out)
 {
	 out.write_item_start();

	 out.write_key_value("name", this->name.c_str());	
	 out.write_key_value("data_used", string_format("%d", (int)data_used));
	 out.write_key_value("root_node", string_format("%d", get_index_of_node(root)));

	 out.write_key_list_start("params");
	 for (auto& param : parameters.name_to_index) {

		 out.write_item_start();
		 out.write_key_value("name", param.first.c_str());
		 out.write_key_value("type", parameter_type_to_string(parameters.types.at(param.second)));
		 out.write_item_end();
	 }
	 out.write_list_end();

	 out.write_key_list_start("nodes");

	 for (int i = 0; i < all_nodes.size(); i++) {
		 auto& node = all_nodes[i];
		 out.write_item_start();

		 out.write_key_value("type", get_animnode_typedef(node->get_type()).name);

		 out.write_key_value("default_param", string_format("%d", node->param.id));

		 out.write_key_list_start("inputs");
		 for (int i = 0; i < node->input.count; i++) {
			 out.write_value( string_format("%d", get_index_of_node(node->input[i]) ) );
		 }
		 out.write_list_end();

		 write_properties(*node->get_property_list(), node, out);

		 node->write_to_dict(this, out);

		 out.write_item_end();
	 }

	 out.write_list_end();
 }

 static void write_properties(Node_Property_List& list, void* ptr, DictWriter& out)
 {
	 out.write_key_list_start("props");
	 for (int i = 0; i < list.count; i++)
	 {
		 out.write_item_start();
		 auto& prop = list.list[i];
		 if (!prop.serialize) continue;
		 switch (prop.type)
		 {
		 case Property_Type::bool_prop: {
			 bool* b = (bool*)((char*)ptr + prop.offset);
			 out.write_key_value("value", string_format("%d", (int) *b));
		 }break;
		 case Property_Type::int_prop: {
			 int* i = (int*)((char*)ptr + prop.offset);
			 out.write_key_value("value", string_format("%d", *i));
		 }break;

		 case Property_Type::float_prop: {
			 float* i = (float*)((char*)ptr + prop.offset);
			 out.write_key_value("value", string_format("%f", *i));
		 }break;

		 case Property_Type::std_string_prop: {
			 std::string* i = (std::string*)((char*)ptr + prop.offset);
			 out.write_key_value("value", i->c_str());
		 }break;

		 case Property_Type::vec2_prop: {
			 glm::vec2* i = (glm::vec2*)((char*)ptr + prop.offset);
			 out.write_key_value("value", string_format("%f %f", i->x,i->y));
		 }break;

		 default:
			 ASSERT(0);
			 break;
		 }
		 out.write_item_end();
	 }
	 out.write_list_end();
 }



 static bool read_properties(Node_Property_List& list, void* ptr, DictParser& in)
 {
	 StringView tok;

	 in.expect_list_start();

	 for (int i = 0; i < list.count; i++)
	 {
		 bool good = in.check_item_start(tok);
		 if (!good) {
			 printf("read properties mismatch (old format??)\n");
			 return in.check_list_end(tok);
		 }

		 auto& prop = list.list[i];
		 if (!prop.serialize) continue;

		 in.read_string(tok);
		 if (!tok.cmp("value")) return false;

		 switch (prop.type)
		 {
		 case Property_Type::bool_prop: {
			 bool* b = (bool*)((char*)ptr + prop.offset);
			 int i = 0;
			 *b = in.read_int(i);
		 }break;
		 case Property_Type::int_prop: {
			 int* i = (int*)((char*)ptr + prop.offset);
			 in.read_int(*i);
		 }break;

		 case Property_Type::float_prop: {
			 float* i = (float*)((char*)ptr + prop.offset);
			 in.read_float(*i);
		 }break;

		 case Property_Type::std_string_prop: {
			 std::string* i = (std::string*)((char*)ptr + prop.offset);

			 in.read_string(tok);
			 *i = tok.to_stack_string().c_str();
		 }break;

		 case Property_Type::vec2_prop: {
			 glm::vec2* i = (glm::vec2*)((char*)ptr + prop.offset);
			 in.read_float2(i->x, i->y);
		 }break;

		 default:
			 ASSERT(0);
			 break;
		 }

		 in.read_string(tok);
		 if (!in.check_item_end(tok))
			 return false;
	 }
	 in.read_string(tok);
	 return in.check_list_end(tok);
 }

 void Blend2d_CFG::register_props()
 {
	 static Node_Property props[] = {
		 {"xparam", Property_Type::int_prop, offsetof(Blend2d_CFG,xparam.id)},
		 {"yparam", Property_Type::int_prop, offsetof(Blend2d_CFG,yparam.id)},
		 {"fade_in", Property_Type::float_prop, offsetof(Blend2d_CFG,fade_in)},
		 {"weight_damp", Property_Type::float_prop, offsetof(Blend2d_CFG,weight_damp)},
	 };
	 properties = { props, sizeof(props) / sizeof(Node_Property) };
 }

 void Mirror_Node_CFG::register_props()
 {
	 static Node_Property props[] = {
		 {"damp_time", Property_Type::float_prop, offsetof(Mirror_Node_CFG,damp_time)},
	 };
	 properties = { props, sizeof(props) / sizeof(Node_Property) };
 }

 void Blend_Node_CFG::register_props()
 {
	 static Node_Property props[] = {
		 {"damp_time", Property_Type::float_prop, offsetof(Blend_Node_CFG,damp_factor)},
	 };
	 properties = { props, sizeof(props) / sizeof(Node_Property) };
 }

 void Clip_Node_CFG::register_props()
 {
	 static Node_Property props[] = {
		 {"rootmotion_x", Property_Type::int_prop, offsetof(Clip_Node_CFG,rootmotion[0])},
		 {"rootmotion_y", Property_Type::int_prop, offsetof(Clip_Node_CFG,rootmotion[1])},
		 {"rootmotion_z", Property_Type::int_prop, offsetof(Clip_Node_CFG,rootmotion[2])},
		 {"clipname", Property_Type::std_string_prop, offsetof(Clip_Node_CFG,clip_name)},
		 {"loop", Property_Type::bool_prop, offsetof(Clip_Node_CFG,loop)},
		 {"speed", Property_Type::float_prop, offsetof(Clip_Node_CFG,speed)},
		 {"can_be_leader", Property_Type::bool_prop, offsetof(Clip_Node_CFG,can_be_leader)},
	 };
	 properties = { props, sizeof(props) / sizeof(Node_Property) };
 }

 void Statemachine_Node_CFG::register_props()
 {
	 static Node_Property props[] = {
		 {"start_state", Property_Type::int_prop, offsetof(Statemachine_Node_CFG,start_state.id)},
		 {"fade_in_time", Property_Type::float_prop, offsetof(Statemachine_Node_CFG,fade_in_time)},
	 };
	 properties = { props, sizeof(props) / sizeof(Node_Property) };
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