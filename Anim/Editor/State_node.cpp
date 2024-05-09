#include "State_node.h"
#include "Statemachine_node.h"
#include "AnimationGraphEditor.h"
void State_EdNode::init()
{
	Base_EdNode* parent = ed.get_owning_node_for_layer(graph_layer);
	ASSERT(parent);
	ASSERT(strcmp(parent->get_typeinfo().name, "Statemachine_EdNode") == 0);
	parent_statemachine = (Statemachine_EdNode*)parent;
	parent_statemachine->add_node_to_statemachine(this);

	if (is_regular_state_node()) {
		num_inputs = 1;


		if (is_this_node_created()) {
			ASSERT(!sublayer.context);
			sublayer = ed.create_new_layer(false);
			ed.add_root_node_to_layer(this, sublayer.id, false);
		}
		else {
			ASSERT(state_handle_internal.is_valid());
			self_state = *parent_statemachine->get_state(state_handle_internal);
			
		}
	}
}
std::string State_EdNode::get_title() const
{
	if (name != "Unnamed" || !is_regular_state_node())
		return get_name();

	bool any_non_defaults = false;

	auto startnode = ed.find_first_node_in_layer(sublayer.id,"Root_EdNode");

	if (!startnode->inputs[0] || strcmp(startnode->inputs[0]->get_typeinfo().name, "Clip_EdNode") != 0)
		return get_name();

	// get clip name to use as default state name
	return startnode->inputs[0]->get_title();
}
bool State_EdNode::push_imnode_link_colors(int index) {
	ASSERT(inputs[index]);
	ASSERT(inputs[index]->is_state_node());
	State_EdNode* other = (State_EdNode*)inputs[index];

	auto st = other->get_state_transition_to(this, index);
	ASSERT(st);

	if (st->is_a_continue_transition()) {
		ImNodes::PushColorStyle(ImNodesCol_Link, Color32{ 242, 41, 41 }.to_uint());
		ImNodes::PushColorStyle(ImNodesCol_LinkSelected, Color32{ 245, 211, 211 }.to_uint());
	}
	else {
		ImNodes::PushColorStyle(ImNodesCol_Link, Color32{ 0,0xff,0 }.to_uint());
		ImNodes::PushColorStyle(ImNodesCol_LinkSelected, Color32{ 193, 247, 186 }.to_uint());
	}

	ImNodes::PushColorStyle(ImNodesCol_LinkHovered, Color32{ 0xff,0xff,0xff }.to_uint());

	return true;
}

std::string State_EdNode::get_input_pin_name(int index) const
{
	if (!inputs[index]) return {};

	std::string name = inputs[index]->get_title();
	if (name.size() > 16) {
		name.resize(13);
		name.append("...");
	}
	return name;
}

void State_EdNode::on_remove_pin(int slot, bool force)
{
	ASSERT(inputs[slot]);

	if (inputs[slot]->is_state_node()) {
		((State_EdNode*)inputs[slot])->remove_output_to(this, slot);
		inputs[slot] = nullptr;
	}
	else
		ASSERT(!"not state node in state graph");
}

void State_EdNode::remove_reference(Base_EdNode* node)
{
	if (node->is_state_node()) {
		remove_output_to((State_EdNode*)node, -1);
	}

	// node gets deleted since its in the layer
	Base_EdNode::remove_reference(node);

	if (node == parent_statemachine) {
		//parent_statemachine = nullptr;
	}
}

bool State_EdNode::add_input(AnimationGraphEditor* ed, Base_EdNode* input, uint32_t slot)
{
	ASSERT(input->is_state_node());

	inputs[slot] = input;

	State_EdNode* statenode = (State_EdNode*)input;
	statenode->on_output_create(this, slot);

	if (num_inputs > 0 && inputs[num_inputs - 1] && num_inputs < inputs.size())
		num_inputs++;

	return false;
}

bool State_EdNode::compile_data_for_statemachine()
{
	compile_error_string.clear();
	compile_info_string.clear();

	Statemachine_Node_CFG* sm_cfg = parent_statemachine->node;
	if (is_regular_state_node()) {

		self_state.transition_idxs.resize(0);
	}
	for (int i = 0; i < output.size(); i++) {
		State_EdNode* out_state = output[i].output_to;
		State_Transition* st = &output[i].st;

		st->transition_state = output[i].output_to->state_handle_internal;

		// compile transition script
		if (!st->script_uncompilied.empty() && st->is_a_continue_transition()) {
			append_info_msg(string_format("[INFO] is_continue_transition == true, but script is not empty.\n"));
		}
		else if (!st->is_a_continue_transition()) {

			const std::string& code = st->script_uncompilied;
			std::string err_str;
			try {

				auto ret = st->script_condition.compile(
					ed.editing_tree->graph_program.get(),
					code,
					NAME("transition_t"));		// selfname = transition_t, for special transition functions like time_remaining() etc.

				// must return boolean
				if (ret.out_types.size() != 1 || ret.out_types[0] != script_types::bool_t)
					err_str = "script must return boolean";

			}
			catch (CompileError err) {
				err_str = std::move(err.str);
			}
			catch (...) {
				err_str = "unknown error";

			}

			if (!err_str.empty()) {
				append_fail_msg(string_format("[ERROR] script (-> %s) compile failed ( %s )\n", out_state->get_title().c_str(), err_str.c_str()));
			}
		}

		sm_cfg->transitions.push_back(*st);
		int idx = sm_cfg->transitions.size() - 1;
		if (is_regular_state_node()) {
			self_state.transition_idxs.push_back(idx);
		}
		else if (is_start_node()) {// entry state
			sm_cfg->entry_transitions.push_back(idx);
		}
	}

	// append tree
	if (is_regular_state_node()) {
		Base_EdNode* startnode = ed.find_first_node_in_layer(sublayer.id, "Root_EdNode");
		ASSERT(startnode);

		if (!startnode->inputs[0]) {
			append_fail_msg(string_format("[ERROR] missing start state in blend tree \n"));
		}
		else {

			Base_EdNode* rootnode = startnode->inputs[0];
			ASSERT(rootnode->get_graph_node());

			self_state.tree = rootnode->get_graph_node();
		}

		sm_cfg->states.push_back(self_state);
	}

	return compile_error_string.empty();	// empty == no errors generated
}

void State_EdNode::remove_output_to(State_EdNode* node, int slot)
{
	bool already_seen = false;
	for (int i = 0; i < output.size(); i++) {

		// WARNING transitions invalidation potentially!
		if (output[i].output_to == node && (slot == -1 || slot == output[i].output_to_index)) {
			output.erase(output.begin() + i);
			already_seen = true;
			i--;
		}
	}
	ASSERT(already_seen);
	// output array might become invalidated
	ed.signal_nessecary_prop_ed_reset();
}

 void State_EdNode::get_transition_props(State_EdNode* to, std::vector<PropertyListInstancePair>& props, int slot)
{
	for (int i = 0; i < output.size(); i++) {
		if (output[i].output_to == to && output[i].output_to_index == slot) {
			// WARNING: this pointer becomes invalid if output is resized
			props.push_back({ State_Transition::get_props(), &output[i].st });
			return;
		}
	}
	ASSERT(0);
}

 bool State_EdNode::traverse_and_find_errors() {
	children_have_errors = false;
	if (is_regular_state_node()) {
		auto startnode = ed.find_first_node_in_layer(sublayer.id, "Root_EdNode");

		if (startnode->inputs[0])
			children_have_errors |= !startnode->inputs[0]->traverse_and_find_errors();
		// else is an error too, but its already built into compile_error_string
	}
	return !children_have_errors && compile_error_string.empty();
}

 void State_EdNode::on_post_remove_pins()
 {
	 ASSERT(num_inputs >= 1);
	 ASSERT(inputs[num_inputs - 1] == nullptr);

	 int count = 0;
	 for (int i = 0; i < num_inputs - 1; i++) {
		 if (inputs[i]) {
			 ASSERT(inputs[i]->is_state_node());

			 State_EdNode* statenode = (State_EdNode*)inputs[i];
			 statenode->reassign_output_slot(this, i, count);

			 inputs[count++] = inputs[i];
		 }
	 }
	 num_inputs = count + 1;
	 ASSERT(num_inputs >= 1 && num_inputs < MAX_INPUTS);
	 inputs[num_inputs - 1] = nullptr;


#ifdef _DEBUG
	 ASSERT(num_inputs >= 1);
	 ASSERT(inputs[num_inputs - 1] == nullptr);
	 if (num_inputs > 1) {
		 for (int i = 0; i < num_inputs - 1; i++) {
			 ASSERT(inputs[i] != nullptr);
		 }
	 }
#endif // _DEBUG
 }
