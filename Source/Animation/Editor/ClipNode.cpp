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
	this->foundType = editor->get_params().find_value_type(name);
	if (foundType.has_value()) {
		find_my_port(0, true)->type.type = foundType.value();
	}
	else{
		find_my_port(0, true)->type.type = GraphPinType::Any;
	}
}