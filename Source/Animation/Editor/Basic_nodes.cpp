#include "Basic_nodes.h"
#include "AnimCommands.h"
#include "AnimationGraphEditor2.h"

void Math_EdNode::on_link_changes() {
	Base_EdNode::on_link_changes();

	find_my_port(0, false)->type = GraphPinType::Any;
	find_my_port(1, false)->type = GraphPinType::Any;
	auto set_type_to_other = [&](const int idx) {
		const auto myport = GraphPortHandle::make(self, idx, false);
		const GraphPort* other = get_other_nodes_port_from_myport(myport);
		if (other) {
			const auto othertype = other->type.type;
			bool is_valid = false;
			if (is_math_node_type_an_equality(type)) {
				is_valid = does_type_have_equality(othertype);
			}
			else if (is_math_node_type_a_comparison(type)) {
				is_valid = does_type_have_comparisons(othertype);
			}
			else {
				is_valid = does_type_have_mathops(othertype);
			}
			if (othertype == GraphPinType::Any)
				is_valid = true;

			if(is_valid)
				find_my_port(idx, false)->type = other->type;
			else {
				GraphCommandUtil::remove_link(find_link_from_port(myport).value(), editor->get_graph());
			}
		}
	};
	set_type_to_other(0);
	set_type_to_other(1);
	if (find_my_port(0, false)->type.type != GraphPinType::Any) {
		find_my_port(1, false)->type = find_my_port(0, false)->type;
	}
	else if (find_my_port(1, false)->type.type != GraphPinType::Any) {
		find_my_port(0, false)->type = find_my_port(1, false)->type;
	}

	if (!is_math_node_type_a_comparison(type)) {
		find_my_port(0, true)->type = find_my_port(0, false)->type;
	}
	else {
		assert(find_my_port(0, true)->type.type == GraphPinType::Boolean);
	}
}