#pragma once
#include "Base_node.h"
#include "State_node.h"
class Root_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Root_EdNode);

	void init() override {
		if (!is_this_node_created()) {
			if (graph_layer == 0) {	// root layer
				Base_EdNode* root = ed.editor_node_for_cfg_node(ed.get_tree()->get_root_node());
				if (root)
					add_input(&ed, root, 0);
			}
			else {
				Base_EdNode* owning_node = ed.get_owning_node_for_layer(graph_layer);
				ASSERT(owning_node->is_state_node());
				State_EdNode* state_owner = (State_EdNode*)owning_node;
				Base_EdNode* root_node_in_layer = ed.editor_node_for_cfg_node(state_owner->self_state.tree);
				if (root_node_in_layer)
					add_input(&ed, root_node_in_layer, 0);
			}
		}
		clear_newly_created();
	}
	std::string get_name() const override { return "Output pose"; }
	bool compile_my_data(const AgSerializeContext* ctx) override { return true; }
	Color32 get_node_color() const override { return ROOT_COLOR; }
	bool has_output_pin() const override { return false; }
	int get_num_inputs() const override { return 1; }
	bool can_delete() const override { return false; }
	bool allow_creation_from_menu() const override { return false; }

	static PropertyInfoList* get_props() {
		return nullptr;
	}


};
