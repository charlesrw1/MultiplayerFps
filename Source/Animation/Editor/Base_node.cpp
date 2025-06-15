#include "Base_node.h"
#include "AnimationGraphEditor2.h"

GraphNodeHandle GraphPortHandle::get_node() const
{
	if (is_output()) {
		return Base_EdNode::get_nodeid_from_output_id(id);
	}
	else {
		return Base_EdNode::get_nodeid_from_input_id(id);
	}
}

int GraphPortHandle::get_index() const
{
	return Base_EdNode::get_slot_from_id(id);
}

bool GraphPortHandle::is_output() const
{
	assert(id >= INPUT_START && id < LINK_START);
	return id >= OUTPUT_START && id < LINK_START;
}
void Base_EdNode::remove_node_from_other_ports() {
	for (int i = 0; i < links.size(); i++) {
		GraphLink l = links.at(i).link;
		Base_EdNode* other_node = editor->get_graph().get_node(l.get_other_node(self));
		if (other_node) {
			other_node->remove_link(l);
		}
	}
	links.clear();
}

bool Base_EdNode::vaildate_links() {
	// rules: has to be on same layer, cant be duplicate to same input
	std::unordered_set<int> seen;
	for (int i = 0; i < links.size(); i++) {
		GraphLink l = links.at(i).link;
		GraphNodeHandle other = l.get_other_node(self);
		Base_EdNode* other_node = editor->get_graph().get_node(other);
		if (!other_node)
			return false;
		if (other_node->layer != layer)
			return false;
		if (l.input.get_node() == self) {
			// is input
			if (SetUtil::contains(seen, l.input.id))
				return false;
			seen.insert(l.input.id);
		}
	}
	return true;
}