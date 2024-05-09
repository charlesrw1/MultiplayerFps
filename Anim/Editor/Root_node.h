#pragma once
#include "Base_node.h"
class Root_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Root_EdNode);

	void init() override {}
	std::string get_name() const override { return "Output pose"; }
	bool compile_my_data() override { return true; }
	Color32 get_node_color() const override { return ROOT_COLOR; }
	bool has_output_pin() const override { return false; }
	int get_num_inputs() const override { return 1; }
	bool can_delete() const override { return false; }
	bool allow_creation_from_menu() const override { return false; }

	static PropertyInfoList* get_props() {
		return nullptr;
	}


};
