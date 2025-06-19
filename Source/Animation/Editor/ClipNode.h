#pragma once
#include "Base_node.h"
#include "Framework/EnumDefReflection.h"
#include "Animation/Runtime/RuntimeNodesNew.h"
#include <variant>
using std::variant;

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

	Clip_EdNode(bool is_evaluator) {
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

	REF stClipNode Data;
};

class StateTransition_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(StateTransition_EdNode);
	StateTransition_EdNode() {

	}
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

	REF string statename;
	bool is_entry_state = false;
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

	REFLECT(hide);
	GraphLayerHandle sublayer;
};

class Variable_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(Variable_EdNode);
	Variable_EdNode() {
		add_out_port(0, "");
	}
	Variable_EdNode(const string& name) {
		add_out_port(0, "");
		this->variable_name = name;
	}
	void on_link_changes() override;
	bool has_top_bar() override { return false; }

	REFLECT(hide)
	string variable_name;
	opt<GraphPinType> foundType;
};

class ComposePoses_EdNode : public Base_EdNode
{
public:
	CLASS_BODY(ComposePoses_EdNode);
	ComposePoses_EdNode(bool is_additive) {
		add_out_port(0, "").type = GraphPinType::LocalSpacePose;
		add_in_port(0, "0").type = GraphPinType::LocalSpacePose;
		add_in_port(1, "1").type = GraphPinType::LocalSpacePose;
		add_in_port(2, "alpha").type = GraphPinType::Float;
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimBlend); }
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
		add_in_port(1, "alpha").type = GraphPinType::Float;
	}
};

class BlendInt_EdNode : public Base_EdNode {
public:
	CLASS_BODY(BlendInt_EdNode);
	BlendInt_EdNode();
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimBlend); }
	void on_link_changes() override;
	void on_property_changes() override;

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
		add_out_port(0, "").type = GraphPinType::MeshSpacePose;
		add_in_port(0, "pose").type = GraphPinType::MeshSpacePose;
		add_in_port(1, "target").type = GraphPinType::Vec3;
		add_in_port(2, "pole").type = GraphPinType::Vec3;
		add_in_port(3, "alpha").type = GraphPinType::Float;
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimBoneModify); }
};

class ModifyBone_EdNode : public Base_EdNode {
public:
	CLASS_BODY(ModifyBone_EdNode);
	ModifyBone_EdNode() {
		add_out_port(0, "").type = GraphPinType::MeshSpacePose;
		add_in_port(0, "pose").type = GraphPinType::MeshSpacePose;
		add_in_port(1, "translation").type = GraphPinType::Vec3;
		add_in_port(2, "rotation").type = GraphPinType::Quat;
		add_in_port(3, "alpha").type = GraphPinType::Float;
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::AnimBoneModify); }
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
		BreakVec3,
		BreakVec2,
		MakeVec3,
		MakeVec2,
		ReturnPose,
		ReturnTransition,
	};
	Func_EdNode(Type t) {
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
		default:
			break;
		}
	}
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
	FloatMathFuncs_EdNode() = default;
	FloatMathFuncs_EdNode(Type t);

	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::Math); }

};

class LogicalOp_EdNode : public Base_EdNode {
public:
	CLASS_BODY(LogicalOp_EdNode);
	LogicalOp_EdNode() = default;
	LogicalOp_EdNode(bool isor) {
		add_out_port(0, "").type = GraphPinType::Boolean;
		add_in_port(0, "").type = GraphPinType::Boolean;
		add_in_port(1, "").type = GraphPinType::Boolean;
		num_inputs = 2;
	}
	Color32 get_node_color() const override { return get_color_for_category(EdNodeCategory::Math); }
	void on_link_changes() override;
	void on_property_changes() override;
	
	REFLECT();
	int num_inputs = 0;
};