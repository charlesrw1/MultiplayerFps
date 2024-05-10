#pragma once
#include "Base_node.h"
#include "imnodes.h"
#include "AnimationGraphEditor.h"
class Statemachine_EdNode;
class State_EdNode : public Base_EdNode
{
public:
	~State_EdNode() override {
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

	std::string get_layer_tab_title() const override {
		return "State: " + get_title();
	}

	int get_num_inputs() const override { 
		if (is_alias_node() || is_start_node()) 
			return 0;
		return num_inputs;
	}
	std::string get_name() const override { return "State"; }
	// compiling done by statemachine owner
	bool compile_my_data(const AgSerializeContext* ctx) override { return true; }
	Color32 get_node_color() const override { return STATE_COLOR; }
	const editor_layer* get_layer() const override {
		if (is_regular_state_node())
			return &sublayer;
		return nullptr;
	}
	bool dont_call_compile() const override { return true; }
	bool draw_flat_links() const override { return true; }
	bool is_state_node() const override { return true; }
	ImVec4 get_pin_colors() const override { return ImVec4(0.5, 0.5, 0.5, 1.0); }
	bool has_pin_colors() const override { return true; }
	void get_link_props(std::vector<PropertyListInstancePair>& props, int slot) override {
		ASSERT(inputs[slot]);
		ASSERT(inputs[slot]->is_state_node());
		((State_EdNode*)inputs[slot])->get_transition_props(this, props, slot);
	}

	void add_props(std::vector<PropertyListInstancePair>& props) override {
		Base_EdNode::add_props(props);
		props.push_back({ State_EdNode::get_props(), this });
	}

	void add_props_for_editable_element(std::vector<PropertyListInstancePair>& props) override {
		props.push_back({State::get_props(), &self_state });
	}

public:


	bool is_regular_state_node() const { return !is_start_node() && !is_alias_node(); }

	void init_for_statemachine(Statemachine_EdNode* parent, std::vector<bool>& transition_taken_bitmask, 
		const std::vector<State_EdNode*>& handle_to_ednode);
	bool compile_data_for_statemachine(const AgSerializeContext* ctx);
	void get_transition_props(State_EdNode* to, std::vector<PropertyListInstancePair>& props, int slot);
	
	virtual bool is_alias_node() const { return false; }
	virtual bool is_start_node() const { return false; }

	void reassign_output_slot(State_EdNode* node, int prev, int next) {
		// Warning: N^2 
		for (int i = 0; i < output.size(); i++) {
			if (output[i].output_to == node && output[i].output_to_index == prev) {
				output[i].output_to_index = next;
				return;
			}
		}
		ASSERT(!"missing node");
	}
	void remove_output_to(State_EdNode* node, int slot);
	void on_output_create(State_EdNode* other, int index) {
		output.push_back({ other, index });
		// output vector might become invalidated
		ed.signal_nessecary_prop_ed_reset();
	}

	State_Transition* get_state_transition_to(State_EdNode* to, int index) {
		for (int i = 0; i < output.size(); i++) 
			if (output[i].output_to == to && output[i].output_to_index == index)
				return &output[i].st;
		ASSERT(!"no transition");
		return nullptr;
	}

	static PropertyInfoList* get_props() {
		START_PROPS(State_EdNode)
			REG_STDSTRING(name, PROP_DEFAULT),
			REG_INT(state_handle_internal.id,PROP_SERIALIZE,""),
			REG_INT(sublayer.id, PROP_SERIALIZE, ""),
			REG_STRUCT_CUSTOM_TYPE(sublayer.context, PROP_SERIALIZE, "SerializeImNodeState")
		END_PROPS(State_EdNode)
	}

	struct output_transition_info {
		State_EdNode* output_to = nullptr;
		int output_to_index = 0;
		State_Transition st;
	};

	int num_inputs = 1;

	std::vector<output_transition_info> output;
	State self_state;
	handle<State> state_handle_internal;
	int selected_transition_for_prop_ed = 0;

	editor_layer sublayer;
	Statemachine_EdNode* parent_statemachine = nullptr;

	std::string name = "";

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
	void add_props_for_editable_element(std::vector<PropertyListInstancePair>& props) override {
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
	void add_props_for_editable_element(std::vector<PropertyListInstancePair>& props) override {
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