#pragma once
#include "Base_node.h"
#include "Framework/EnumDefReflection.h"
#include "Animation/Runtime/RuntimeNodesNew.h"
#include <variant>
using std::variant;
opt<int> create_linked_node(CompilationContext& ctx, int input_index, Base_EdNode* node);

class AnimationSeqAsset;

NEWENUM(CommentColors, int32_t)
{
	Gray,
	Red,
	Green,
	Blue,
	Yellow,
	Orange,
};

class CommentNode : public Base_EdNode
{
public:
	CLASS_BODY(CommentNode);
	CommentNode() {
		desc = "A Comment";
	}

	Color32 get_color() const;

	REF string desc;
	REF CommentColors color=CommentColors::Gray;
	REFLECT(hide);
	int sizex = 10;
	REFLECT(hide);
	int sizey = 10;

	bool is_editing = false;

	void draw_imnode() final;
	void on_link_changes() override;
};


class Clip_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(Clip_EdNode);

	Clip_EdNode(bool is_evaluator) :is_evaluator(is_evaluator){
		add_out_port(0, "").type = GraphPinType::LocalSpacePose;
		if (is_evaluator)
			add_in_port(0, "time").type = GraphPinType::Float;
		else {
			auto& p = add_in_port(0, "speed");
			p.type = GraphPinType::Float;
			p.inlineValue = 1.f;
		}
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimSource); }
	string get_subtitle() const override;
	void compile(CompilationContext& ctx) final;

	bool is_evaluator = false;
	REF atClipNodeStruct Data;
};
#include "Animation/Runtime/Statemachine_cfg.h"
class StateTransition_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(StateTransition_EdNode);
	StateTransition_EdNode() {
	}
	void on_link_changes() override;
	bool is_link_attached_node() final { return true; }
	GraphLayerHandle get_owning_sublayer() const override { return transition_graph; }
	void set_owning_sublayer(GraphLayerHandle h) { transition_graph = h; }

	string get_title() const override {
		return "Transition";
	}

	REF float transition_time = 0.2;
	REF Easing blend = Easing::Linear;
	REF int8_t priority = 0;
	REF bool auto_transition = false;
	REF bool interruptable = false;
	REF GraphLayerHandle transition_graph;
};

class State_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(State_EdNode);
	State_EdNode() {
		add_out_port(0, "").type = GraphPinType::StateType;
		add_in_port(0, "").type = GraphPinType::StateType;
	}
	void on_link_changes() override;
	bool draw_links_as_arrows() final { return true; }
	GraphLayerHandle get_owning_sublayer() const override { return state_graph; }
	void set_owning_sublayer(GraphLayerHandle h) { state_graph = h; }
	string get_title() const override {
		if (is_entry_state)
			return "EntryState";
		return statename.empty() ? "State" : (statename);
	}

	REF string statename;
	REF GraphLayerHandle state_graph;
	bool is_entry_state = false;
};

struct SAHandleWithFlag {
	STRUCT_BODY();
	REF GraphNodeHandle handle;
	REF bool flag = false;
};
class StateAlias_EdNode;
struct StateAliasStruct {
	STRUCT_BODY();
	REFLECT(hide);
	vector<SAHandleWithFlag> handles;
	REFLECT(hide);
	bool default_true = false;
};

class StateAlias_EdNode : public Base_EdNode {
public:
	CLASS_BODY(StateAlias_EdNode);
	StateAlias_EdNode() {
		add_out_port(0, "").type = GraphPinType::StateType;
	}
	void fixup_any_extra_references(const unordered_map<int, int>& old_id_to_new_id) override;
	bool draw_links_as_arrows() final { return true; }
	void on_link_changes() override;
	REF StateAliasStruct data;
};

class Statemachine_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(Statemachine_EdNode);
	Statemachine_EdNode() {
		add_out_port(0, "").type = GraphPinType::LocalSpacePose;
	}
	void on_link_changes() override;
	GraphLayerHandle get_owning_sublayer() const override {
		return sublayer;
	}
	void set_owning_sublayer(GraphLayerHandle h);
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimSource); }
	string get_subtitle() const override {
		return sm_name;
	}
	string get_layer_title() const override {
		return sm_name.empty()?get_title():get_subtitle();
	}

	REFLECT(hide);
	GraphLayerHandle sublayer;
	REF string sm_name;

};

class Variable_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(Variable_EdNode);
	Variable_EdNode() {
		add_out_port(0, "");
	}
	Variable_EdNode(const string& name, GraphPinType type) {
		add_out_port(0, "").type = type;
		this->variable_name = name;
	}
	void on_link_changes() override;
	bool has_top_bar() override { return false; }
	void compile(CompilationContext& ctx) final;

	REFLECT(hide)
	string variable_name;
	opt<GraphPinType> foundType;
};

class ComposePoses_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(ComposePoses_EdNode);
	ComposePoses_EdNode(bool is_additive) :is_additive(is_additive) {
		add_out_port(0, "").type = GraphPinType::LocalSpacePose;
		add_in_port(0, "0").type = GraphPinType::LocalSpacePose;
		add_in_port(1, "1").type = GraphPinType::LocalSpacePose;
		add_in_port(2, "alpha").type = GraphPinType::Float;
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimBlend); }
	void compile(CompilationContext& ctx) final;
	const bool is_additive = false;
};
class Blend2D_EdNode : public Base_EdNode {
public:
	CLASS_BODY(Blend2D_EdNode);
	Blend2D_EdNode(bool is_additive) :is_additive(is_additive) {
		add_out_port(0,"").type = GraphPinType::LocalSpacePose;
		if (is_additive) {
			add_in_port(0, "in").type = GraphPinType::LocalSpacePose;
		}
		// can skip port
		add_in_port(1, "x").type = GraphPinType::Float;
		add_in_port(2, "y").type = GraphPinType::Float;
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimBlend); }

	bool is_additive = false;
};

class SubtractPoses_EdNode : public Base_EdNode {
public:
	CLASS_BODY(SubtractPoses_EdNode);
	SubtractPoses_EdNode() {
		add_out_port(0, "").type = GraphPinType::LocalSpacePose;
		add_in_port(0, "base").type = GraphPinType::LocalSpacePose;
		add_in_port(1, "delta").type = GraphPinType::LocalSpacePose;
	}
};
class MirrorPose_EdNode : public Base_EdNode {
public:
	CLASS_BODY(MirrorPose_EdNode);
	MirrorPose_EdNode() {
		add_out_port(0, "").type = GraphPinType::LocalSpacePose;
		add_in_port(0, "").type = GraphPinType::LocalSpacePose;
		add_in_port(1, "mirror").type = GraphPinType::Boolean;
	}
	REF bool latch_on_reset = true;
};

class BlendInt_EdNode : public Base_EdNode {
public:
	CLASS_BODY(BlendInt_EdNode);
	BlendInt_EdNode();
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimBlend); }
	void on_link_changes() override;
	void on_property_changes() override;
	void compile(CompilationContext& ctx) final;

	REFLECT();
	int num_blend_cases = 0;

	int get_index_of_value_input() {
		return MAX_INPUTS - 1;
	}
};

class Ik2Bone_EdNode : public Base_EdNode {
public:
	CLASS_BODY(Ik2Bone_EdNode);
	Ik2Bone_EdNode() {
		add_out_port(0, "").type = GraphPinType::LocalSpacePose;
		add_in_port(0, "pose").type = GraphPinType::LocalSpacePose;
		add_in_port(1, "target").type = GraphPinType::Vec3;
		add_in_port(2, "pole").type = GraphPinType::Vec3;
		add_in_port(3, "alpha").type = GraphPinType::Float;
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimBoneModify); }
	void compile(CompilationContext& ctx) final;
};

class ModifyBone_EdNode : public Base_EdNode {
public:
	CLASS_BODY(ModifyBone_EdNode);
	ModifyBone_EdNode() {
		add_out_port(0, "").type = GraphPinType::LocalSpacePose;
		add_in_port(0, "pose").type = GraphPinType::LocalSpacePose;
		add_in_port(1, "translation").type = GraphPinType::Vec3;
		add_in_port(2, "rotation").type = GraphPinType::Quat;
		add_in_port(3, "alpha").type = GraphPinType::Float;
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimBoneModify); }
	void compile(CompilationContext& ctx) final;

	REF ModifyBoneType rotation = ModifyBoneType::None;
	REF ModifyBoneType translation = ModifyBoneType::None;
};

class Func_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(Func_EdNode);
	Func_EdNode() = default;
	enum Type {
		GetCurve,
		DidEventStart,
		DidEventEnd,
		IsEventActive,
		StateTimeRemaining,
		StateDuration,
		BreakVec3,
		BreakVec2,
		MakeVec3,
		MakeVec2,
		ReturnPose,
		ReturnTransition,
		EntryState,
	};
	Func_EdNode(Type t) :myType(t){
		switch (t)
		{
		case Func_EdNode::GetCurve:
			add_out_port(0, "").type = GraphPinType::Float;
			add_in_port(0, "name").type = GraphPinType::StringName;
			break;
		case Func_EdNode::DidEventStart:
			add_out_port(0, "").type = GraphPinType::Boolean;
			add_in_port(0, "name").type = GraphPinType::StringName;
			break;
		case Func_EdNode::DidEventEnd:
			add_out_port(0, "").type = GraphPinType::Boolean;
			add_in_port(0, "name").type = GraphPinType::StringName;
			break;
		case Func_EdNode::IsEventActive:
			add_out_port(0, "").type = GraphPinType::Boolean;
			add_in_port(0, "name").type = GraphPinType::ClassInfoType;
			break;
		case Func_EdNode::MakeVec3:
			add_in_port(0, "x").type = GraphPinType::Float;
			add_in_port(1, "y").type = GraphPinType::Float;
			add_in_port(2, "z").type = GraphPinType::Float;
			add_out_port(0, "").type = GraphPinType::Vec3;
			break;
		case Func_EdNode::BreakVec3:
			add_out_port(0, "x").type = GraphPinType::Float;
			add_out_port(1, "y").type = GraphPinType::Float;
			add_out_port(2, "z").type = GraphPinType::Float;
			add_in_port(0, "").type = GraphPinType::Vec3;
			break;
		case Func_EdNode::ReturnPose:
			add_in_port(0, "").type = GraphPinType::LocalSpacePose;
			break;
		case Func_EdNode::ReturnTransition:
			add_in_port(0, "").type = GraphPinType::Boolean;
			break;
		case Func_EdNode::EntryState:
			add_out_port(0, "").type = GraphPinType::StateType;
			break;

		case Func_EdNode::StateTimeRemaining:
			add_out_port(0, "").type = GraphPinType::Float;
			break;
		case Func_EdNode::StateDuration:
			add_out_port(0, "").type = GraphPinType::Float;
			break;
		default:
			break;
		}
	}
	Type myType{};
	bool draw_links_as_arrows() override { return myType == Func_EdNode::EntryState; }
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::Function); }
};
class FloatMathFuncs_EdNode : public Base_EdNode {
public:
	CLASS_BODY(FloatMathFuncs_EdNode);
	enum Type {
		ScaleBias,
		Clamp,
		InRange,
		Abs,
		Remap,
	};
	const Type funcType = Type::ScaleBias;
	FloatMathFuncs_EdNode() = default;
	FloatMathFuncs_EdNode(Type t);

	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::Math); }
	void compile(CompilationContext& ctx) final;
};

class Not_EdNode : public Base_EdNode {
public:
	CLASS_BODY(Not_EdNode);
	Not_EdNode() {
		add_out_port(0, "").type = GraphPinType::Boolean;
		add_in_port(0, "").type = GraphPinType::Boolean;
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::Math); }
	void compile(CompilationContext& ctx) final;
};

class LogicalOp_EdNode : public Base_EdNode {
public:
	CLASS_BODY(LogicalOp_EdNode);
	LogicalOp_EdNode() = default;
	LogicalOp_EdNode(bool isor)  : is_or(isor) {
		add_out_port(0, "").type = GraphPinType::Boolean;
		add_in_port(0, "").type = GraphPinType::Boolean;
		add_in_port(1, "").type = GraphPinType::Boolean;
		num_inputs = 2;
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::Math); }
	void on_link_changes() override;
	void on_property_changes() override;
	void compile(CompilationContext& ctx) final;
	
	REFLECT();
	int num_inputs = 0;
	const bool is_or = false;
};