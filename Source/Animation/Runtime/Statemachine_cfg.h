#pragma once
#include "AnimationTreeLocal.h"

enum class Easing : uint8_t
{
	Linear,
	CubicEaseIn,
	CubicEaseOut,
	CubicEaseInOut,
};

inline float evaluate_easing(Easing type, float t)
{
	switch (type)
	{
	case Easing::Linear:
		return t;
		break;
	case Easing::CubicEaseIn:
		return t * t * t;
		break;
	case Easing::CubicEaseOut: {
		float oneminus = 1 - t;
		return 1.0 - oneminus * oneminus * oneminus;
	} break;
	case Easing::CubicEaseInOut: {
		float othert = -2 * t + 2;
		return (t < 0.5) ? 4 * t * t * t : 1.0 - othert * othert * othert * 0.5;
	}break;
	default:
		break;
	}
}

struct State;
struct State_Transition
{
	static PropertyInfoList* get_props();
	bool is_a_continue_transition() const { return is_continue_transition; }
	bool is_an_auto() const { return automatic_transition_rule; }
	handle<State> transition_state;
	ScriptHandle handle;
	std::string script_uncompilied;		// TODO: save to disk precompilied, compiling is fast though so not a huge deal
	bool is_continue_transition = false;
	float transition_time = 0.2f;
	bool automatic_transition_rule = false;
	bool can_be_interrupted = true;
	int8_t priority = 0;
	Easing easing_type = Easing::Linear;
};


struct State
{
	static PropertyInfoList* get_props();

	Node_CFG* tree = nullptr;	// fixed up at initialization
	bool is_end_state = false;
	bool wait_until_finish = false;
	float state_duration = -1.0;				// when > 0, specifies how long state should be active, then signals a transition end
	InlineVec<uint16_t, 6> transition_idxs;
};

struct Statemachine_Node_RT : Rt_Vars_Base
{
	handle<State> active_state;
	handle<State> fading_out_state;
	const State_Transition* active_transition = nullptr;
	Pose* cached_pose_from_transition = nullptr;

	float blend_duration = 0.0;
	float blend_percentage = 0.0;
};

struct Statemachine_Node_CFG : public Node_CFG
{
	using RT_TYPE = Statemachine_Node_RT;
	CLASS_HEADER();

	virtual void initialize(Animation_Tree_CFG* tree) override;

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	
	static const PropertyInfoList* get_props();

	// Inherited via At_Node
	virtual void reset(NodeRt_Ctx& ctx) const override;

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<Statemachine_Node_RT>(ctx);
	}

	const State* get_state(handle<State> handle) const {
		if (handle.id == -1) return nullptr;
		return &states.at(handle.id);
	}
	handle<State> get_state_handle(const State* state) const {
		return { int((state - states.data()) / sizeof(State)) };
	}

	const State_Transition* find_continue_transition_for_state(const State* s) const;
	handle<State> find_enter_state(Statemachine_Node_RT* rt, NodeRt_Ctx& ctx) const;

	const State_Transition* find_state_transition(NodeRt_Ctx& ctx, const State* s) const;

	handle<State> ptr_to_handle(const State* state) const {
		return { int( (state - states.data()) / sizeof(State) ) };
	}

	std::vector<State> states;
	InlineVec<uint16_t, 6> entry_transitions;
	std::vector<State_Transition> transitions;
};
