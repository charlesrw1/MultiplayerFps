#include "Statemachine_node.h"
#include <algorithm>
#include "Animation/Runtime/AnimationTreeLocal.h"
 void Statemachine_EdNode::init() {
	bool is_create = util_create_or_ensure(node);
	if (is_create) {
		sublayer = ed.create_new_layer(true);
		ed.add_root_node_to_layer(this, sublayer.id, true);
	}
	else {

		for (int i = 0; i < ed.nodes.size(); i++) {
			if (ed.nodes[i]->graph_layer == sublayer.id) {
				ASSERT(ed.nodes[i]->is_state_node());
				add_node_to_statemachine((State_EdNode*)ed.nodes[i]);
			}
		}
		// now have a list of all ed state nodes
		std::vector<bool> taken_transitions(node->transitions.size(),false);
		std::vector<State_EdNode*> handle_to_ednode;
		handle_to_ednode.resize(node->states.size(),nullptr);
		for (int i = 0; i < states.size(); i++) {
			if (states[i]->state_handle_internal.is_valid()) {
				handle_to_ednode[states[i]->state_handle_internal.id] = states[i];
			}
		}
#ifdef _DEBUG
		for (auto p : handle_to_ednode)
			ASSERT(p);
#endif _DEBUG

		// TODO:
		// evaluate alias transitions first

		for (int i = 0; i < states.size(); i++) {
			states[i]->init_for_statemachine(this, taken_transitions, handle_to_ednode);
		}
	}
}


 template<uint32_t COUNT>
 void sort_indicies(InlineVec<uint16_t, COUNT>& transition_idxs, const std::vector<State_Transition>& transitions)
 {
	 // fuck it
	 std::vector<uint16_t> transition_idxs_vec(transition_idxs.size());
	 memcpy(transition_idxs_vec.data(), transition_idxs.data(), transition_idxs.size() * sizeof(uint16_t));
	 std::sort(transition_idxs_vec.begin(), transition_idxs_vec.end(), [&](const uint16_t a, const uint16_t b)->bool {
		 return transitions[a].priority < transitions[b].priority && !transitions[a].is_continue_transition;
	});
	 memcpy((void*)transition_idxs.data(), transition_idxs_vec.data(), transition_idxs.size() * sizeof(uint16_t));

 }

bool Statemachine_EdNode::compile_my_data(const AgSerializeContext* ctx)
{
	node->states.clear();
	int node_state_count = 0;
	for (int i = 0; i < states.size(); i++) {
		ASSERT(states[i]);
		if (states[i]->is_regular_state_node()) {
			states[i]->state_handle_internal = { node_state_count++ };
		}
	}
	node->entry_transitions.resize(0);
	node->transitions.clear();

	node->initialize(ed.editing_tree);

	bool has_errors = false;

	for (int i = 0; i < states.size(); i++) {
		has_errors |= !states[i]->compile_data_for_statemachine(ctx);

	}

	for (int j = 0; j < node->states.size(); j++)
		sort_indicies(node->states[j].transition_idxs, node->transitions);
	sort_indicies(node->entry_transitions, node->transitions);


	ASSERT(node_state_count == node->states.size());

	if (has_errors)
		append_fail_msg("[ERROR] state machine states contain errors\n");


	auto state_enter = ed.find_first_node_in_layer<StateStart_EdNode>(sublayer.id);
	ASSERT(state_enter);	// should never be deleted

	bool found_default_entry = false;
	for (int i = 0; i < node->entry_transitions.size(); i++) {

		auto st = node->transitions[node->entry_transitions[i]];
		if (st.is_a_continue_transition()) {
			if (found_default_entry) {
				append_fail_msg("[ERROR] state machine contains more than one default entry condition");
				break;
			}
			found_default_entry = true;

		}

	}
	if (!found_default_entry) {
		append_fail_msg("[ERROR] state machine does not have a default entry transition");
	}

	return true;
}

bool Statemachine_EdNode::traverse_and_find_errors()
{
	children_have_errors = false;

	for (int i = 0; i < states.size(); i++) {
		children_have_errors |= !states[i]->traverse_and_find_errors();
	}

	return !children_have_errors && compile_error_string.empty();
}

std::string Statemachine_EdNode::get_title() const
{
	if (!name.empty())
		return this->name;

	std::string name;
	for (int i = 0; i < states.size(); i++) {

		// states can be null...
		if (!states[i]) continue;

		if (!states[i]->is_regular_state_node())
			continue;

		name += states[i]->get_title();
		name += '/';

		if (name.size() > 22) {
			name += "...";
			return name;
		}
	}
	if (name.empty())
		return get_name();

	name.pop_back();
	return name;

}

void Statemachine_EdNode::remove_reference(Base_EdNode* node)
{
	Base_EdNode::remove_reference(node);
	bool already_erased = false;
	for (int i = 0; i < states.size(); i++) {
		if (states[i] == node) {
			ASSERT(!already_erased);	// just a check
			states.erase(states.begin() + i);
			i--;
			already_erased = true;
		}
	}
}

State* Statemachine_EdNode::get_state(handle<State> state)
{
	ASSERT(state.is_valid() && state.id < node->states.size());
	return &node->states.at(state.id);
}
