#include "Basic_nodes.h"
#include "AnimCommands.h"
#include "AnimationGraphEditor2.h"
#include "Animation/Runtime/RuntimeNodesNew.h"
#include "ClipNode.h"

opt<int8_t> get_value_type_from_graphpin(GraphPinType type)
{
	switch (type.type)
	{
	case GraphPinType::Boolean: return atValueNode::Bool;
	case GraphPinType::Integer: return atValueNode::Int;
	case GraphPinType::Float: return atValueNode::Float;
	case GraphPinType::Vec3: return atValueNode::Vector3;
	default:
		break;
	}
	return std::nullopt;
}

void Math_EdNode::compile(CompilationContext& ctx)
{
	auto inport0 = find_my_port(0, false);
	auto inport1 = find_my_port(1, false);
	if (!inport0 || !inport1) {
		ctx.add_error(self, "no ports");
		return;
	}
	if (inport0->type.type == GraphPinType::Any || inport1->type.type == GraphPinType::Any) {
		ctx.add_error(self, "any pin type");
		return;
	}
	assert(inport0->type == inport1->type);

	// trust the validation logic in on_link_changes i guess
	atMathNode* out = new atMathNode;
	out->opTypeInt = (int8_t)this->type;
	auto val = get_value_type_from_graphpin(inport0->type);
	if (!val.has_value())
		ctx.add_error(self, "not valid graphpin");
	out->inputTypeInt = val.value_or(0);
	out->leftid = create_linked_node(ctx, 0, this).value_or(0);
	out->rightid = create_linked_node(ctx, 1, this).value_or(0);
	ctx.add_output_node(self, 0, out);
}

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