#pragma once
#include "Base_node.h"
#include "imnodes.h"
class Statemachine_EdNode;
class State_EdNode : public Base_EdNode
{
public:
	~State_EdNode() override {
		ASSERT(!is_regular_state_node() && sublayer.context);
		if (sublayer.context)
			ImNodes::EditorContextFree(sublayer.context);
	}

	EDNODE_HEADER(State_EdNode);
public:
	// overrides
	void init() override;
	std::string get_title() const override;
	bool push_imnode_link_colors(int index) override;
	std::string get_input_pin_name(int index) const override;
	void on_remove_pin(int index, bool force) override;
	void remove_reference(Base_EdNode* node) override;
	bool add_input(AnimationGraphEditor* ed, Base_EdNode* input, uint32_t slot) override;
	bool traverse_and_find_errors() override;
	void on_post_remove_pins() override;

	int get_num_inputs() const override { 
		if (is_alias_node() || is_start_node()) 
			return 0;
		return num_inputs;
	}
	std::string get_name() const override { return "State"; }
	// compiling done by statemachine owner
	bool compile_my_data() override { return true; }
	Color32 get_node_color() const override { return STATE_COLOR; }
	const editor_layer* get_layer() {
		if (is_regular_state_node())
			return &sublayer;
		return nullptr;
	}

	bool draw_flat_links() const override { return true; }
	bool is_state_node() const override { return true; }
	ImVec4 get_pin_colors() const override { return ImVec4(0.5, 0.5, 0.5, 1.0); }
	bool has_pin_colors() const override { return true; }
	void get_link_props(std::vector<PropertyListInstancePair>& props, int slot) override {
		ASSERT(inputs[slot]);
		ASSERT(inputs[slot]->is_state_node());
		((State_EdNode*)inputs[slot])->get_transition_props(this, props);
	}

public:

	bool is_regular_state_node() const { return !is_start_node() && !is_alias_node(); }

	bool compile_data_for_statemachine();
	void remove_output_to(State_EdNode* node);
	void get_transition_props(State_EdNode* to, std::vector<PropertyListInstancePair>& props);
	
	virtual bool is_alias_node() const { return false; }
	virtual bool is_start_node() const { return false; }

	void on_output_create(State_EdNode* other) {
		output.push_back({ other });
	}

	State_Transition* get_state_transition_to(State_EdNode* to) {
		for (int i = 0; i < output.size(); i++) 
			if (output[i].output_to == to)
				return &output[i].st;
		return nullptr;
	}

	static PropertyInfoList* get_props() {
		START_PROPS(State_EdNode)
			REG_STDSTRING(name, PROP_SERIALIZE),
			REG_INT(state_handle_internal.id,PROP_SERIALIZE,""),
			REG_INT(sublayer.id, PROP_SERIALIZE, ""),
			REG_STRUCT_CUSTOM_TYPE(sublayer.context, PROP_SERIALIZE, "SerializeImNodeState")
		END_PROPS(State_EdNode)
	}

	struct output_transition_info {
		State_EdNode* output_to = nullptr;
		State_Transition st;
	};

	int num_inputs = 0;

	std::vector<output_transition_info> output;
	State self_state;
	handle<State> state_handle_internal;
	int selected_transition_for_prop_ed = 0;

	editor_layer sublayer;
	Statemachine_EdNode* parent_statemachine = nullptr;

	std::string name = "Unnamed";

};



class StateAlias_EdNode : public State_EdNode
{
public:

	const TypeInfo& get_typeinfo() const override;
	void add_props(std::vector<PropertyListInstancePair>& props) override {
		Base_EdNode::add_props(props);
		// skip State_EdNode
		props.push_back({ StateAlias_EdNode::get_props(), this });
	}

	// Inherited from State_EdNode
	bool is_alias_node() const override { return true; }

	// Inherited from Base_EdNode
	bool allow_creation_from_menu() const { return false; }	// FIXME

	static PropertyInfoList* get_props() {
		START_PROPS(StateAlias_EdNode)
			REG_STDSTRING(name, PROP_SERIALIZE)
		END_PROPS(StateAlias_EdNode);
	}
};

class StateStart_EdNode : public State_EdNode
{
public:

	const TypeInfo& get_typeinfo() const override;
	void add_props(std::vector<PropertyListInstancePair>& props) override {
		Base_EdNode::add_props(props);
		// skip State_EdNode
		props.push_back({ StateStart_EdNode::get_props(), this });
	}

	// Inherited from State_EdNode
	bool is_start_node() const override { return true; }

	// Inherited from Base_EdNode
	std::string get_name() const override { return "State Enter"; }
	bool can_delete() const override { return false; }
	int get_num_inputs() const override { return 0; }
	Color32 get_node_color() const  override { return ROOT_COLOR; }
	std::string get_output_pin_name() const { return "START"; }
	bool allow_creation_from_menu() const { return false; }

	static PropertyInfoList* get_props() {
		return nullptr;
	}
};