#pragma once
#include "AnimationTreeLocal.h"
#include <string>

NEWENUM(Easing, uint8_t)
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
		return t;
		break;
	}
}

enum class ScriptComparison : int8_t
{
	Eq,
	NotEq,
	Lt,
	LtEq,
	Gt,
	GtEq
};
enum class ScriptValueType : int8_t
{
	None,
	Variable,	// references an AnimatorInstance variable
	Constant,	// references a constant
	Curve,		// references a curve value
	TimeRemaining,
	StateTime,
};

class StateTransitionScript
{
public:
	ScriptComparison comparison = ScriptComparison::NotEq;
	struct ValueData {
		ScriptValueType type = ScriptValueType::None;
		std::string str;
		union {
			float number;
			int curve_handle;
			const PropertyInfo* pi;
		};
		std::string get_str() const {
			if (type == ScriptValueType::Variable)
				return str;
			else
				return std::to_string(number);
		}
	};
	ValueData lhs;
	ValueData rhs;

	static const PropertyInfoList* get_props() {
		START_PROPS(StateTransitionScript)
			make_struct_property("_value",0,PROP_DEFAULT, "StateTransitionScript")
		END_PROPS(StateTransitionScript)
	}
	void init(Animation_Tree_CFG* tree) {
		init_value(lhs, tree);
		init_value(rhs, tree);
	}

	bool evaluate(NodeRt_Ctx& ctx) const;
private:
	double get_value(const ValueData& vd, NodeRt_Ctx& ctx) const;
	void init_value(ValueData& vd, Animation_Tree_CFG* tree);
};

struct State;
struct State_Transition
{
	static const PropertyInfoList* get_props();

	bool is_a_continue_transition() const { return is_continue_transition; }
	bool is_an_auto() const { return automatic_transition_rule; }

	std::vector<StateTransitionScript> conditions;

	handle<State> transition_state;
	bool is_continue_transition = false;
	float transition_time = 0.2f;
	bool automatic_transition_rule = false;
	bool can_be_interrupted = true;
	int8_t priority = 0;
	Easing easing_type = Easing::Linear;
};


struct State
{
	static const PropertyInfoList* get_props();

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


NODECFG_HEADER(Statemachine_Node_CFG, Statemachine_Node_RT)

	virtual void initialize(Animation_Tree_CFG* tree) override;

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	
	static const PropertyInfoList* get_props();

	// Inherited via At_Node
	virtual void reset(NodeRt_Ctx& ctx) const override;

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
