#pragma once
#include "Base_node.h"
#include "State_node.h"
#include "Basic_nodes.h"
class Statemachine_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Statemachine_EdNode);

	~Statemachine_EdNode() override {
		ASSERT(sublayer.context);
		ImNodes::EditorContextFree(sublayer.context);
	}

public:
	// overrides
	void init() override;
	std::string get_title() const override;
	bool compile_my_data() override;
	bool traverse_and_find_errors() override;
	void remove_reference(Base_EdNode* node) override;

	std::string get_name() const override { return "Statemachine"; }
	std::string get_tooltip() const override { return  
			"Contains a statemachine with transitions\n"
			"(double click to open)";
	}

	Color32 get_node_color() const  override { return SM_COLOR; }
	bool is_statemachine() const override { return true; }
	Node_CFG* get_graph_node() override  { return node; }
	const editor_layer* get_layer() const override {
		ASSERT(sublayer.context);
		return &sublayer;
	}

public:
	void add_node_to_statemachine(State_EdNode* node) {
		states.push_back(node);
	}
	State* get_state(handle<State> state);

	static PropertyInfoList* get_props() {
		START_PROPS(Statemachine_EdNode)
			REG_STDSTRING(name,PROP_DEFAULT),
			REG_INT(sublayer.id, PROP_SERIALIZE, ""),
			REG_STRUCT_CUSTOM_TYPE(sublayer.context,PROP_SERIALIZE, "ImNode_PropSerialize"),
			REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef")
		END_PROPS(Statemachine_EdNode)
	}

	std::string name;
	editor_layer sublayer;
	std::vector<State_EdNode*> states;
	Statemachine_Node_CFG* node = nullptr;
};