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

opt<int> Base_EdNode::find_link_idx_from_port(GraphPortHandle port) {

	for (int i = 0; i < links.size(); i++) {
		const GraphLink& l = links.at(i).link;
		if (l.input == port || l.output == port)
			return i;
	}
	return std::nullopt;
}
opt<GraphLink> Base_EdNode::find_link_from_port(GraphPortHandle port) {
	opt<int> idx = find_link_idx_from_port(port);
	if (!idx.has_value())
		return std::nullopt;
	return links.at(idx.value()).link;
}
void Base_EdNode::get_ports(vector<int>& input, vector<int>& output) {
	for (int i = 0; i < ports.size(); i++) {
		if (ports.at(i).is_input())
			input.push_back(i);
		else
			output.push_back(i);
	}
}
opt<int> Base_EdNode::get_link_index(GraphLink link) {
	for (int i = 0; i < links.size(); i++) {
		auto& l = links.at(i);
		if (l.link == link)
			return i;
	}
	return std::nullopt;
}
bool Base_EdNode::does_input_have_port_already(GraphPortHandle input) {
	assert(input.get_node() == self);
	assert(!input.is_output());
	for (GraphLinkWithNode& link : links) {
		if (link.link.input == input)
			return true;
	}
	return false;
}
void Base_EdNode::add_link(GraphLink link) {
	assert(link.input.get_node() == self || link.output.get_node() == self);
	opt<int> index = get_link_index(link);
	if (index.has_value()) {
		sys_print(Warning, "link already exists\n");
		return;
	}
	links.push_back(GraphLinkWithNode(link));
}
GraphNodeHandle Base_EdNode::remove_link_to_input(GraphPortHandle p) {
	opt<int> index = find_link_idx_from_port(p);
	if (index.has_value()) {
		return remove_link(links.at(index.value()).link);
	}
	else {
		printf("couldnt find link to input\n");
		return GraphNodeHandle();
	}
}

GraphNodeHandle Base_EdNode::remove_link(GraphLink link) {
	opt<int> index = get_link_index(link);
	if (index.has_value()) {
		GraphNodeHandle link_opt_node = links.at(index.value()).opt_link_node;
		links.erase(links.begin() + index.value());
		return link_opt_node;
	}
	else {
		sys_print(Warning, "cant remove link, not found\n");
		return GraphNodeHandle();
	}
}
GraphPort& Base_EdNode::add_in_port(int index, string name) {
	GraphPort p;
	p.index = index;
	p.output_port = false;
	p.name = name;
	ports.push_back(p);
	return ports.back();
}
GraphPort& Base_EdNode::add_out_port(int index, string name) {
	GraphPort p;
	p.output_port = true;
	p.index = index;
	p.name = name;
	ports.push_back(p);
	return ports.back();
}