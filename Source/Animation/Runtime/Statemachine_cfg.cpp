#include "Statemachine_cfg.h"

#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"

#include "Animation/AnimationUtil.h"

static const char* easing_strs[] = {
	"Linear",
	"CubicEaseIn",
	"CubicEaseOut",
	"CubicEaseInOut",
};

ENUM_HEADER(Easing);
ENUM_START(Easing)
	STRINGIFY_EUNM(Easing::Linear, 0),
	STRINGIFY_EUNM(Easing::CubicEaseIn, 1),
	STRINGIFY_EUNM(Easing::CubicEaseOut, 2),
	STRINGIFY_EUNM(Easing::CubicEaseInOut, 3)
ENUM_IMPL(Easing);


const PropertyInfoList* State_Transition::get_props()
{
	START_PROPS(State_Transition)
		REG_INT(transition_state, PROP_SERIALIZE, "-1"),
		REG_BOOL(automatic_transition_rule, PROP_DEFAULT, "0"),
		REG_FLOAT(transition_time, PROP_DEFAULT, "0.2"),
		REG_STDSTRING_CUSTOM_TYPE(script_uncompilied, PROP_DEFAULT, "AG_LISP_CODE"),
		REG_BOOL(is_continue_transition, PROP_DEFAULT, "0"),
		REG_BOOL(can_be_interrupted, PROP_DEFAULT, "1"),
		REG_INT(priority, PROP_DEFAULT, "0"),
		REG_ENUM(easing_type, PROP_DEFAULT, "Easing::Linear", Easing)
	END_PROPS(State_Transition)
}


const PropertyInfoList* State::get_props()
{
	MAKE_INLVECTORCALLBACK_ATOM(uint16_t, transition_idxs, State);
	START_PROPS(State)
		REG_STDVECTOR(transition_idxs, PROP_SERIALIZE),
		REG_FLOAT(state_duration, PROP_DEFAULT, "-1.0"),
		REG_BOOL(wait_until_finish, PROP_DEFAULT, "0"),
		REG_BOOL(is_end_state, PROP_DEFAULT, "0"),
		REG_STRUCT_CUSTOM_TYPE(tree, PROP_SERIALIZE, "AgSerializeNodeCfg"),
	END_PROPS(State)
}


const PropertyInfoList* Statemachine_Node_CFG::get_props()
{
	MAKE_INLVECTORCALLBACK_ATOM(uint16_t, entry_transitions, Statemachine_Node_CFG);
	MAKE_VECTORCALLBACK(State, states);
	MAKE_VECTORCALLBACK(State_Transition, transitions);
	START_PROPS(Statemachine_Node_CFG)
		REG_STDVECTOR(states, PROP_SERIALIZE),
		REG_STDVECTOR(transitions, PROP_SERIALIZE),
		REG_STDVECTOR(entry_transitions, PROP_SERIALIZE)
	END_PROPS(Statemachine_Node_CFG)
}


void Statemachine_Node_CFG::initialize(Animation_Tree_CFG* tree) {
	init_memory_internal(tree, sizeof(RT_TYPE));

	for (int i = 0; i < states.size(); i++) {
		states[i].tree = (Node_CFG*)serialized_nodecfg_ptr_to_ptr(states[i].tree, tree);
	}

	if (tree->get_graph_is_valid()) {
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
				auto ret = tree->get_script()->compile(transitions[i].handle, code, "anim_inst");

				// must return boolean
				if (ret.out_types.size() != 1 || ret.out_types[0] != script_types::bool_t)
					bad = true;
			}
			catch (...) {
				bad = true;
			}
			if (bad)
				transitions[i].handle.id = -1;
		}
	}
}

const State_Transition* Statemachine_Node_CFG::find_continue_transition_for_state(const State* s) const
{
	for (int i = 0; i < s->transition_idxs.size(); i++) {
		const State_Transition& st = transitions[s->transition_idxs[i]];
		if (st.is_a_continue_transition())
			return &st;
	}
	return nullptr;
}

handle<State> Statemachine_Node_CFG::find_enter_state(Statemachine_Node_RT* rt, NodeRt_Ctx& ctx) const
{
	handle<State> firststate;
	script_value_t stack[32];
	for (int i = 0; i < entry_transitions.size(); i++) {
		uint16_t index = entry_transitions[i];
		const State_Transition& st = transitions[i];
		
		// use this automatically
		if (st.is_a_continue_transition()) {
			firststate = { index };
			break;
		}

		// some error, should assert
		ASSERT(st.handle.is_valid());
		script_state state(stack, 0, 32, nullptr);
		ctx.script->execute(st.handle, &state, &ctx.get_script_inst());
		bool yes = state.pop_int();

		if (yes) {
			firststate = { index };
			break;
		}
	}
	ASSERT(firststate.is_valid());

	rt->blend_duration = 0.0;
	rt->blend_percentage = 0.0;
	rt->active_state = firststate;
	rt->active_transition = nullptr;
	rt->fading_out_state = handle<State>();

	if (rt->cached_pose_from_transition)
		g_pose_pool.free(rt->cached_pose_from_transition);
	rt->cached_pose_from_transition = nullptr;

	return firststate;
}

const State_Transition* Statemachine_Node_CFG::find_state_transition(NodeRt_Ctx& ctx, const State* s) const
{
	for (int i = 0; i < s->transition_idxs.size(); i++) {
		const State_Transition& st = transitions[s->transition_idxs[i]];
		if (st.is_a_continue_transition())
			return &st;

		ASSERT(st.handle.is_valid());
		script_state state(ctx.stack, 0, 32, nullptr);
		ctx.script->execute(st.handle, &state, &ctx.get_script_inst());
		bool yes = state.pop_int();

		if (yes) {
			return &st;
		}
	}
	return nullptr;
}



bool Statemachine_Node_CFG::get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {

	auto rt = get_rt(ctx);

	ASSERT(rt->active_state.is_valid());

	const State* active_ptr = get_state(rt->active_state);
	auto next_transition = find_state_transition(ctx, active_ptr);
	auto active_transition = rt->active_transition;

	/*
	vars: good, 
		next_transition, next_transition.is_continue(), next_transition.is_auto()
		state.is_end(), state.can_be_interrupted(),
		in_transition, active_transition.can_be_interrupted(), active_transition.is_auto()

	if next_transition is auto and (!in_transition or active_transition.can_be_interrupted() )then 
		set_auto(next_transition.duration)
	if in_transition and active_transition is automatic then 
			set_dt( 0 )
	good = active->get_pose()
	
	if in_transition and !active_transition.can_be_interrupted()
		do nothing
	else if in_transition and next_transition
		interrupt current transition
	else if in_transition and !next_transition
		do nothing

	else if !good and !next_transition and state.is_end()
		bubble up end
	else if !good and !next_transition and !state.is_end()
		dont do anything
	else if next_transition and state.is_end()
		start new transition
	else if next_transition and ( !state.is_end() and !state.can_be_interrupted() )
		start new transition

	if starting_new_transition
		save_pose( pose )

	if in_transition
		util_blend( pose, saved_pose, percentage )

	update percentage
	if percentage > 1.0 
		free(saved pose)
		end transition
	*/
	
	GetPose_Ctx posectx = pose;
	
	if (next_transition && next_transition->is_an_auto() && next_transition->is_a_continue_transition() && (!active_transition || active_transition->can_be_interrupted))
		posectx.set_automatic_transition(next_transition->transition_time);
	if (active_transition && active_transition->is_an_auto())
		posectx.set_dt(0);

	bool good = active_ptr->tree->get_pose(ctx,pose);

	bool bubble_up_result = true;

	if (active_transition && !active_transition->can_be_interrupted) {
		// do nothing
	}
	else if (active_transition && next_transition && !next_transition->is_a_continue_transition() && !next_transition->is_an_auto()) {
		
		printf("interrupted transition\n");
		// interrupt current transition

		// dont do any blending or updates this frame
		active_transition = nullptr;

		// save current pose
		ASSERT(rt->cached_pose_from_transition);
		std::memcpy(rt->cached_pose_from_transition->pos, pose.pose->pos, sizeof(glm::vec3) * ctx.num_bones());
		std::memcpy(rt->cached_pose_from_transition->q, pose.pose->q, sizeof(glm::quat) * ctx.num_bones());

		rt->active_transition = next_transition;
		rt->blend_duration = next_transition->transition_time;
		rt->blend_percentage = 0.0;
		rt->fading_out_state = rt->active_state;
		rt->active_state = next_transition->transition_state;

		get_state(next_transition->transition_state)->tree->reset(ctx);
	}
	else if (active_transition) {
		// do nothing
	}
	else if (!good && !next_transition && active_ptr->is_end_state) {
		// bubble up end, do nothing
		bubble_up_result = false;
		printf("bubbled up end state\n");
	}
	else if (!good && !next_transition && !active_ptr->is_end_state) {
		// do nothing/wait for a valid transition
	}
	else if (next_transition && active_ptr->wait_until_finish && good) {	// has a transition but state waits till end
		// do nothing
	}
	else if ( next_transition && ((next_transition->is_a_continue_transition() && !good) 
		||  (!next_transition->is_a_continue_transition()))  )
	{
		printf("started new transition\n");
		// start new transition

		// save current pose
		ASSERT(!rt->cached_pose_from_transition);
		rt->cached_pose_from_transition = (Pose*)g_pose_pool.allocate();

		std::memcpy(rt->cached_pose_from_transition->pos, pose.pose->pos, sizeof(glm::vec3) * ctx.num_bones());
		std::memcpy(rt->cached_pose_from_transition->q, pose.pose->q, sizeof(glm::quat) * ctx.num_bones());

		rt->active_transition = next_transition;
		rt->blend_duration = next_transition->transition_time;
		rt->blend_percentage = 0.0;
		rt->fading_out_state = rt->active_state;
		rt->active_state = next_transition->transition_state;

		get_state(next_transition->transition_state)->tree->reset(ctx);

	}

	if (active_transition) {

		ASSERT(rt->cached_pose_from_transition);

		// blend the cached pose with current pose

		float blend = evaluate_easing(active_transition->easing_type, 1.0 - rt->blend_percentage);

		util_blend(ctx.num_bones(), *rt->cached_pose_from_transition, *pose.pose, blend);

		if (rt->blend_duration >= 0.001) {
			rt->blend_percentage += pose.dt / rt->blend_duration;
		}
		else
			rt->blend_percentage = 1.01;	// a number bigger than 1.0

		// end transition
		if (rt->blend_percentage >= 1.0) {
			g_pose_pool.free(rt->cached_pose_from_transition);
			rt->active_transition = nullptr;
			rt->fading_out_state = handle<State>();
			rt->cached_pose_from_transition = nullptr;
			rt->blend_percentage = 0.0;
			rt->blend_duration = 0.0;

			printf("transition ended\n");
		}
	}

	return bubble_up_result;
}

// Inherited via At_Node

void Statemachine_Node_CFG::reset(NodeRt_Ctx& ctx) const
{
	auto rt = get_rt(ctx);
	// does the reset
	find_enter_state(rt, ctx);

	const State* state = get_state(rt->active_state);
	state->tree->reset(ctx);
}
