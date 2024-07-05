#pragma once
#include "Base_node.h"
#include "State_node.h"
#include "Basic_nodes.h"
#include "../Runtime/Statemachine_cfg.h"


CLASS_H(Statemachine_EdNode, Base_EdNode)

	~Statemachine_EdNode() override {
		ASSERT(sublayer.context);
		ImNodes::EditorContextFree(sublayer.context);
	}

	MAKE_OUTPUT_TYPE(localspace_pose);

	// overrides
	void init() override;
	std::string get_title() const override;
	bool compile_my_data(const AgSerializeContext* ctx) override;
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

	std::string get_layer_tab_title() const override {
		return "Sm: " + get_title();
	}


public:
	void add_node_to_statemachine(State_EdNode* node) {
		states.push_back(node);
	}
	State* get_state(handle<State> state);

	static const PropertyInfoList* get_props() {
		START_PROPS(Statemachine_EdNode)
			REG_STDSTRING(name,PROP_DEFAULT),
			REG_INT(sublayer.id, PROP_SERIALIZE, ""),
			REG_STRUCT_CUSTOM_TYPE(sublayer.context,PROP_SERIALIZE, "SerializeImNodeState"),
			REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef")
		END_PROPS(Statemachine_EdNode)
	}

	std::string name;
	editor_layer sublayer;
	std::vector<State_EdNode*> states;
	Statemachine_Node_CFG* node = nullptr;
};