#include "Animation/Editor/AnimCommands.h"
#include "AnimationGraphEditor2.h"
#include "GraphUtil.h"
#include "Framework/SerializerJson.h"


void GraphCommandUtil::add_link(GraphLink link, EditorNodeGraph& graph, GraphNodeHandle link_node)
{
	Base_EdNode* outn = graph.get_node(link.output.get_node());
	Base_EdNode* inn = graph.get_node(link.input.get_node());
	assert(inn && outn);
	assert(inn->layer == outn->layer);
	opt<int> index = inn->find_link_idx_from_port(link.input);
	if (!index.has_value()) {
		inn->add_link(link);
	//	inn->on_link_changes();
	}
	if (link_node.is_valid()) {
		index = inn->find_link_idx_from_port(link.input);
		assert(index.has_value());
		auto& l = inn->links.at(index.value());
		assert(!l.opt_link_node.is_valid() || l.opt_link_node == link_node);
		l.opt_link_node = link_node;
	}
	index = outn->find_link_idx_from_port(link.input);
	if (!index.has_value()) {
		outn->add_link(link);
		//outn->on_link_changes();
	}
}

GraphPortHandle GraphCommandUtil::get_input_port_from_link(int id)
{
	return GraphPortHandle(id);
}

opt<GraphLink> GraphCommandUtil::get_graph_link_from_linkid(int id, EditorNodeGraph& graph)
{
	GraphPortHandle inp = get_input_port_from_link(id);
	Base_EdNode* node = graph.get_node(inp.get_node());
	if (!node) return std::nullopt;
	return node->find_link_from_port(inp);
}

void GraphCommandUtil::get_selected(vector<int>& link_ids, vector<int>& node_ids) {
	link_ids.resize(ImNodes::NumSelectedLinks());
	if (link_ids.size() > 0) {
		ImNodes::GetSelectedLinks(link_ids.data());
	}
	node_ids.resize(ImNodes::NumSelectedNodes());
	if (node_ids.size() > 0) {
		ImNodes::GetSelectedNodes(node_ids.data());
	}
}

Base_EdNode* GraphCommandUtil::get_optional_link_object(int linkid, EditorNodeGraph& graph)
{
	opt<GraphLink> link = GraphCommandUtil::get_graph_link_from_linkid(linkid, graph);
	if (!link.has_value())
		return nullptr;
	GraphLink l = link.value();
	Base_EdNode* e = graph.get_node(link.value().input.get_node());
	if (!e)
		return nullptr;
	opt<int> index = e->get_link_index(l);
	if (!index.has_value())
		return nullptr;
	GraphLinkWithNode glwn = e->links.at(index.value());
	if (glwn.opt_link_node.is_valid())
		return graph.get_node(glwn.opt_link_node);
	return nullptr;
}

bool GraphCommandUtil::can_connect_these_ports(GraphLink link, EditorNodeGraph& graph)
{
	auto porta = link.input.get_port_ptr(graph);
	auto portb = link.output.get_port_ptr(graph);
	if (!porta || !portb) return false;
	if (porta->type.type == portb->type.type) return true;
	if (porta->type.type == GraphPinType::Any || portb->type.type == GraphPinType::Any) return true;
	return false;
}

void GraphCommandUtil::remove_link(GraphLink link, EditorNodeGraph& graph)
{
	printf("removing link\n");
	Base_EdNode* inn = graph.get_node(link.input.get_node());
	Base_EdNode* outn = graph.get_node(link.output.get_node());
	if (inn) {
		GraphNodeHandle nh1 = inn->remove_link(link);
		graph.remove_node(nh1);
	}
	if (outn) {
		GraphNodeHandle nh2 = outn->remove_link(link);
		graph.remove_node(nh2);
	}
}

AddLinkCommand::AddLinkCommand(AnimationGraphEditorNew& ed, GraphPortHandle input, GraphPortHandle output) : ed(ed)
{
	link = GraphLink(input, output);
	Base_EdNode* inn = ed.get_graph().get_node(link.input.get_node());
	opt<int> index = inn->find_link_idx_from_port(link.input);
	if (index.has_value()) {
		is_link_valid = false;
		sys_print(Warning, "link input already has link\n");
		return;
	}
	if (!GraphCommandUtil::can_connect_these_ports(link, ed.get_graph())) {
		is_link_valid = false;
		sys_print(Warning, "cant connect ports\n");
		return;
	}
}
void AddLinkCommand::execute()
{
	GraphCommandUtil::add_link(link, ed.get_graph(), GraphNodeHandle());
	ed.on_node_changes.invoke();
}
void AddLinkCommand::undo()
{
	GraphCommandUtil::remove_link(link, ed.get_graph());
	ed.on_node_changes.invoke();
}
std::string AddLinkCommand::to_string()
{
	return "AddLinkCommand " + link.to_string();
}
AddNodeCommand::AddNodeCommand(AnimationGraphEditorNew& ed, const string& creation_prototype, glm::vec2 pos, GraphLayerHandle layer) : ed(ed)
{
	this->creation_name = creation_prototype;
	this->pos = pos;
	this->what_layer = layer;
}
void AddNodeCommand::execute()
{
	auto node = ed.get_prototypes().create(creation_name);
	if (!node) {
		node = ed.get_var_prototypes().create(creation_name);
	}
	assert(node);
	ed.get_graph().insert_new_node(*node, what_layer, pos);
	created_handle = node->self;
	assert(created_handle.is_valid());
	ed.on_node_changes.invoke();
}
void AddNodeCommand::undo()
{
	assert(created_handle.is_valid());
	ed.get_graph().remove_node(created_handle);
	ed.on_node_changes.invoke();
}
std::string AddNodeCommand::to_string()
{
	return "AddNodeCommand";
}
RemoveGraphObjectsCommand::RemoveGraphObjectsCommand(AnimationGraphEditorNew& ed, vector<int> link_ids, vector<int> node_ids) :ed(ed)
{
	// serialize the objects
	SerializeGraphContainer container = SerializeGraphUtils::make_container_from_nodeids(node_ids, link_ids,ed.get_graph());
	serialized = SerializeGraphUtils::serialize_to_string(container, ed.get_graph(), ed.get_prototypes());
	printf("%s\n", serialized.c_str());
	this->nodes = std::move(node_ids);
	for (int l : link_ids) {
		opt<GraphLink> link = GraphCommandUtil::get_graph_link_from_linkid(l,ed.get_graph());
		if (link.has_value()) {
			auto node = link->input.get_node_ptr(ed.get_graph());
			assert(node);
			opt<int> idx = node->get_link_index(link.value());
			assert(idx.has_value());
			this->links.push_back(node->links.at(idx.value()));
		}
	}
}

void RemoveGraphObjectsCommand::execute()
{
	auto& graph = ed.get_graph();
	// have to remove all objects, including sub graph objects
	for (GraphLinkWithNode link : this->links) {
		GraphCommandUtil::remove_link(link.link,graph);
	}
	for (int n : this->nodes) {
		GraphNodeHandle h(n);
		graph.remove_node(h);
	}

	ed.on_node_changes.invoke();
}

void RemoveGraphObjectsCommand::undo()
{
	// put back removed objects
	auto unserialized = SerializeGraphUtils::unserialize(serialized, ed.get_prototypes());
	if (!unserialized) {
		LOG_WARN("couldnt unserialize");
		return;
	}
	ed.get_graph().insert_nodes(*unserialized);
	for (GraphLinkWithNode link : links) {
		GraphCommandUtil::add_link(link.link, ed.get_graph(), link.opt_link_node);
	}

	ed.on_node_changes.invoke();
}

std::string RemoveGraphObjectsCommand::to_string()
{
	return "RemoveGraphObjectsCommand";
}

DuplicateNodesCommand::DuplicateNodesCommand(AnimationGraphEditorNew& ed, vector<int> node_ids)
	: ed(ed)
{
	if (node_ids.empty())
		return;
	Base_EdNode* first = ed.get_graph().get_node(node_ids.at(0));
	if (!first)
		return;
	GraphLayerHandle layer = first->layer;
	for (int i = 1; i < node_ids.size(); i++) {
		if (ed.get_graph().get_node(node_ids[i])->layer != layer)
			return;
	}

	SerializeGraphContainer container = SerializeGraphUtils::make_container_from_nodeids(node_ids, {}, ed.get_graph());
	serialized = SerializeGraphUtils::serialize_to_string(container, ed.get_graph(), ed.get_prototypes());
	printf("%s\n", serialized.c_str());
	for (auto i : node_ids)
		orig_nodes.insert(i);
	is_node_valid = true;
}

void DuplicateNodesCommand::execute()
{
	auto unserialized = SerializeGraphUtils::unserialize(serialized, ed.get_prototypes());
	if (!unserialized) {
		LOG_WARN("couldnt unserialize");
		return;
	}
	vector<Base_EdNode*> rootnodes;
	for (auto n : unserialized->nodes) {
		if (SetUtil::contains(orig_nodes, n->self.id)) {
			n->nodex += 50;
			n->nodey += 50;
			rootnodes.push_back(n);
		}
	}

	ed.get_graph().insert_nodes_with_new_id(*unserialized);
	nodes_to_delete.clear();
	for (auto n : unserialized->nodes) {
		nodes_to_delete.push_back(n->self);
	}

	ImNodes::ClearLinkSelection();
	ImNodes::ClearNodeSelection();
	for (auto n : rootnodes) {
		ImNodes::SelectNode(n->self.id);
	}

	ed.on_node_changes.invoke();
}

void DuplicateNodesCommand::undo()
{
	for (auto n : nodes_to_delete)
		ed.get_graph().remove_node(n);
	nodes_to_delete.clear();

	ed.on_node_changes.invoke();
}

std::string DuplicateNodesCommand::to_string()
{
	return "Duplicate";
}

