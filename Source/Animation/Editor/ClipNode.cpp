#include "ClipNode.h"
#include "AnimationGraphEditor2.h"
void Statemachine_EdNode::on_link_changes()  {
	Base_EdNode::on_link_changes();

	if (!sublayer.is_valid()) {
		NodeGraphLayer* layer = editor->get_graph().create_layer();
		layer->set_owner_node(self);
		sublayer = layer->get_id();
		printf("created sublayer %d\n",sublayer.id);
	}
	NodeGraphLayer* layer = editor->get_graph().get_layer(sublayer);
	assert(layer && layer->get_owner_node() == self);
}

void Statemachine_EdNode::set_owning_sublayer(GraphLayerHandle h) {
	this->sublayer = h;
}

void CommentNode::on_link_changes()
{
	Base_EdNode::on_link_changes();
	ImNodes::SetCommentNodeSize(self.id, ImVec2(sizex, sizey));
}
void Variable_EdNode::on_link_changes()
{
	this->name = "Variable";
	this->foundType = editor->get_params().find_value_type(variable_name);
	if (foundType.has_value()) {
		find_my_port(0, true)->type = foundType.value();
	}
	else{
		find_my_port(0, true)->type = GraphPinType::Any;
	}
	find_my_port(0, true)->name = variable_name;
}
#include "AnimCommands.h"
void BlendInt_EdNode::on_link_changes()
{
	Base_EdNode::on_link_changes();
	if (num_blend_cases != ports.size() - 2) {

		for (int i = 0; i < num_blend_cases; i++) {
			GraphPort* p = find_my_port(i, false);
			if (!p)
				add_in_port(i, std::to_string(i)).type = GraphPinType::LocalSpacePose;
		}
		vector<int> inps, outs;
		get_ports(inps, outs);
		vector<int> inp_indicies;
		for (int i : inps)
			inp_indicies.push_back(ports.at(i).index);
		for (int idx : inp_indicies) {
			if (idx >= num_blend_cases && idx != get_index_of_value_input()) {
				remove_port(idx, false);
			}
		}
		remove_links_without_port();
	}


#if 0
	auto set_type_to_other = [&](const int idx) -> bool {
		const auto myport = GraphPortHandle::make(self, enum_info->get_index_of_value_port(), false);
		const GraphPort* other = get_other_nodes_port_from_myport(myport);
		if (other) {
			const auto othertype = other->type;
			bool is_valid = false;
			if (othertype.type==GraphPinType::EnumType) {
				is_valid = true;
			}
			if (is_valid) {
				const EnumTypeInfo* prevenumtype = enum_info->what_enum_type;
				auto myportptr = get_enum_graph_port();
				myportptr->type = other->type;
				const EnumTypeInfo* next = myportptr->type.get_enum_or_set_to_null();
				enum_info->what_enum_type = next;
				return prevenumtype != next;
			}
			else {
				GraphCommandUtil::remove_link(find_link_from_port(myport).value(), editor->get_graph());
				return false;
			}
		}
		return false;
	};

	if (enum_info.has_value()) {
		assert(get_enum_graph_port()->type.type == GraphPinType::EnumType);
		const bool changed = set_type_to_other(0);
		if (changed) {
			auto putbacks = remove_all_input_ports();
			auto enumtype = enum_info->what_enum_type;
			if (enumtype) {
				for (int i = 0; i < enumtype->str_count; i++) {
					auto& poseport = add_in_port(i, enumtype->strs[i].name);
					poseport.type = GraphPinType::LocalSpacePose;
				}
			}
			auto& valueport = add_in_port(enum_info->get_index_of_value_port(), "value");
			valueport.type = GraphPinType::EnumType;
			valueport.type.data = enumtype;
			insert_putbacks(putbacks);
			auto port = get_enum_graph_port();
			assert(port && port->type.type == GraphPinType::EnumType);
		}
	}
#endif
}
void BlendInt_EdNode::on_property_changes() {
	if (num_blend_cases < 0)
		num_blend_cases = 0;
	if (num_blend_cases >= MAX_INPUTS - 1)
		num_blend_cases = MAX_INPUTS - 1;
	on_link_changes();
}

BlendInt_EdNode::BlendInt_EdNode() {
	add_out_port(0, "").type = GraphPinType::LocalSpacePose;
	add_in_port(0, "0").type = GraphPinType::LocalSpacePose;
	add_in_port(1, "1").type = GraphPinType::LocalSpacePose;
	add_in_port(get_index_of_value_input(), "value").type = GraphPinType::Integer;
	num_blend_cases = 2;
	assert(num_blend_cases == ports.size() - 2);
}

void LogicalOp_EdNode::on_link_changes()
{
	Base_EdNode::on_link_changes();
	if (num_inputs != ports.size() - 1) {
		for (int i = 0; i < num_inputs; i++) {
			GraphPort* p = find_my_port(i, false);
			if (!p)
				add_in_port(i, "").type = GraphPinType::Boolean;
		}
		vector<int> inps, outs;
		get_ports(inps, outs);
		vector<int> inp_indicies;
		for (int i : inps)
			inp_indicies.push_back(ports.at(i).index);
		for (int idx : inp_indicies) {
			if (idx >= num_inputs) {
				remove_port(idx, false);
			}
		}
		remove_links_without_port();
	}
}

void LogicalOp_EdNode::on_property_changes()
{
	if (num_inputs < 0)
		num_inputs = 0;
	if (num_inputs >= MAX_INPUTS)
		num_inputs = MAX_INPUTS;
	on_link_changes();
}
