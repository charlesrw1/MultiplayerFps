#include "Base_node.h"
#include <algorithm>
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
	auto sort_ports = [this](vector<int>& inp) {
		if (inp.size() > 1) {
			std::sort(inp.begin(), inp.end(), [this](int i1, int i2) {
				const GraphPort& p1 = ports.at(i1);
				const GraphPort& p2 = ports.at(i2);
				return p1.index < p2.index;
				});
		}
	};
	sort_ports(input);
	sort_ports(output);
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
GraphNodeHandle Base_EdNode::remove_link_from_idx(int index)
{
	assert(index >= 0 && index < links.size());
	GraphNodeHandle link_opt_node = links.at(index).opt_link_node;
	links.erase(links.begin() + index);
	return link_opt_node;
}
GraphNodeHandle Base_EdNode::remove_link(GraphLink link) {
	opt<int> index = get_link_index(link);
	if (index.has_value()) {
		return remove_link_from_idx(index.value());
	}
	else {
		sys_print(Warning, "cant remove link, not found\n");
		return GraphNodeHandle();
	}
}
GraphPort& Base_EdNode::add_in_port(int index, string name) {
	assert(!find_my_port_idx(index, false).has_value());

	GraphPort p;
	p.index = index;
	p.output_port = false;
	p.name = name;
	ports.push_back(p);
	return ports.back();
}
GraphPort& Base_EdNode::add_out_port(int index, string name) {
	assert(!find_my_port_idx(index, true).has_value());

	GraphPort p;
	p.output_port = true;
	p.index = index;
	p.name = name;
	ports.push_back(p);
	return ports.back();
}
opt<int> Base_EdNode::find_my_port_idx(int index, bool is_output) {
	for (int i = 0; i < ports.size(); i++) {
		GraphPort& p = ports[i];
		if (p.is_output() == is_output && p.get_idx() == index) {
			return i;
		}
	}
	return std::nullopt;
}
GraphPort* Base_EdNode::find_my_port(int index, bool output)
{
	opt<int> idx = find_my_port_idx(index, output);
	if (idx.has_value())
		return &ports.at(idx.value());
	return nullptr;
}
#include "Framework/Serializer.h"
void Base_EdNode::serialize(Serializer& s)
{
	using std::get;
	using std::holds_alternative;
	const int INTEGER_TYPE = 0;
	const int FLOAT_TYPE = 1;
	const int BOOL_TYPE = 2;
	const int VEC3_TYPE = 3;


	if (s.is_saving()) {
		int count = 0;
		for (auto& p : ports) {
			if (!p.is_input())
				continue;
			if (!std::holds_alternative<std::monostate>(p.inlineValue)) {
				count++;
			}
		}
		s.serialize_array("inlineProps",count);
		int written = 0;
		for (auto& p : ports) {
			if (!p.is_input())
				continue;
			auto write_begin = [&](const int type) {
				int tuplesz = 3;
				s.serialize_array_ar(tuplesz);
				int idx = p.index;
				s.serialize_ar(idx);
				int type_val = type;
				s.serialize_ar(type_val);
				written++;
			};

			if (holds_alternative<int>(p.inlineValue)) {
				write_begin(INTEGER_TYPE);
				int i = get<int>(p.inlineValue);
				s.serialize_ar(i);
				s.end_obj();
			}
			else if (holds_alternative<float>(p.inlineValue)) {
				write_begin(FLOAT_TYPE);
				float i = get<float>(p.inlineValue);
				s.serialize_ar(i);
				s.end_obj();
			}
			else if (holds_alternative<bool>(p.inlineValue)) {
				write_begin(BOOL_TYPE);
				bool b = get<bool>(p.inlineValue);
				s.serialize_ar(b);
				s.end_obj();
			}
			else if (holds_alternative<glm::vec3>(p.inlineValue)) {
				write_begin(VEC3_TYPE);
				glm::vec3 b = get<glm::vec3>(p.inlineValue);
				s.serialize_ar(b);
				s.end_obj();
			}
		}
		s.end_obj();
		assert(written == count);
	}
	else {
		int count = 0;
		s.serialize_array("inlineProps", count);
		for (int i = 0; i < count; i++) {
			int tupsize = 0;
			s.serialize_array_ar(tupsize);
			assert(tupsize == 3);
			int index = 0;
			s.serialize_ar(index);
			GraphPort* p = find_my_port(index, false);
			if (!p) {
				p = &add_in_port(index, "@created");
			}

			int type = 0;
			s.serialize_ar(type);
			if (type == INTEGER_TYPE) {
				int integer = 0;
				s.serialize_ar(integer);
				p->inlineValue = integer;
			}
			else if (type == FLOAT_TYPE) {
				float val = 0;
				s.serialize_ar(val);
				p->inlineValue = val;
			}
			else if (type == BOOL_TYPE) {
				bool val = 0;
				s.serialize_ar(val);
				p->inlineValue = val;
			}
			else if (type == VEC3_TYPE) {
				glm::vec3 val = {};
				s.serialize_ar(val);
				p->inlineValue = val;
			}
			else {
				assert(0);
			}
			s.end_obj();
		}
		s.end_obj();
	}
}

const GraphPort* Base_EdNode::get_other_nodes_port(GraphLink whatlink)
{
	auto [othernode, index, is_output] = whatlink.get_other_port(self).break_to_values();
	Base_EdNode* n = editor->get_graph().get_node(othernode);
	if (!n)
		return nullptr;
	return n->find_my_port(index, is_output);
}
opt<int> Base_EdNode::find_port_idx_from_handle(GraphPortHandle handle)
{
	assert(handle.get_node() == self);
	int index = handle.get_index();
	bool is_output = handle.is_output();
	return find_my_port_idx(index, is_output);
}
const GraphPort* Base_EdNode::get_other_nodes_port_from_myport(GraphPortHandle handle)
{
	opt<GraphLink> link;
	link = find_link_from_port(handle);
	if (link.has_value()) {
		return get_other_nodes_port(link.value());
	}
	return nullptr;
}
GraphPort* Base_EdNode::find_port_from_handle(GraphPortHandle handle)
{
	opt<int> idx = find_port_idx_from_handle(handle);
	if (!idx.has_value()) return nullptr;
	return &ports.at(idx.value());
}
GraphPort* GraphPortHandle::get_port_ptr(EditorNodeGraph& graph)
{
	auto n = graph.get_node(get_node());
	if (!n) return nullptr;
	return n->find_port_from_handle(*this);
}
Base_EdNode* GraphPortHandle::get_node_ptr(EditorNodeGraph& graph)
{
	return graph.get_node(get_node());
}
Base_EdNode* Base_EdNode::find_other_node_from_port(GraphPortHandle port)
{
	auto p = find_link_from_port(port);
	if (p.has_value())
		return p->get_other_port(self).get_node_ptr(editor->get_graph());
	return nullptr;
}
GraphPortHandle Base_EdNode::make_my_port_handle(int index, bool is_output) {
	return GraphPortHandle::make(self, index, is_output);
}
void Base_EdNode::on_link_changes()
{
	ImNodes::SetNodeGridSpacePos(self.id,ImVec2(nodex, nodey));
}
#include "AnimCommands.h"
void Base_EdNode::remove_port(int index, bool is_output)
{
	opt<int> portidx = find_my_port_idx(index, is_output);
	if (portidx.has_value()) {
		opt<int> haslink = find_link_idx_from_port(GraphPortHandle::make(self, index, is_output));
		if (haslink.has_value()) {
			//remove_link_from_idx(haslink.value());
			GraphCommandUtil::remove_link(links.at(haslink.value()).link, editor->get_graph());
		}
		ports.erase(ports.begin() + portidx.value());
	}
}

void Base_EdNode::remove_links_without_port()
{
	for (int i = 0; i < links.size(); i++) {
		GraphLinkWithNode gl = links.at(i);
		const GraphPort* myport = get_my_node_port(gl.link);
		const GraphPort* otherport = get_other_nodes_port(gl.link);
		if (!myport || !otherport) {
			const int presize = links.size();
			GraphCommandUtil::remove_link(gl.link, editor->get_graph());
			assert((int)links.size() == presize - 1);
			i--;
		}
	}
}
const GraphPort* Base_EdNode::get_my_node_port(GraphLink whatlink)
{
	auto [othernode, index, is_output] = whatlink.get_self_port(self).break_to_values();
	assert(othernode == self);
	return find_my_port(index, is_output);
}

void CompilationContext::compile_this(Base_EdNode* n)
{
	if (has_compiled_already(n->self))
		return;
	n->compile(*this);
	SetUtil::insert_test_exists(compiled_already, n->self.id);
}
