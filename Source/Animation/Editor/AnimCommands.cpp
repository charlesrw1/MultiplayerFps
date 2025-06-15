#include "Animation/Editor/AnimCommands.h"
#include "AnimationGraphEditor2.h"
#include "GraphUtil.h"
#include "Framework/SerializerJson.h"


void GraphCommandUtil::add_link(GraphLink link, EditorNodeGraph& graph)
{
	Base_EdNode* outn = graph.get_node(link.output.get_node());
	Base_EdNode* inn = graph.get_node(link.input.get_node());
	opt<int> index = inn->find_link_idx_from_port(link.input);
	if (!index.has_value()) {
		inn->add_link(link);
	}
	index = outn->find_link_idx_from_port(link.input);
	if (!index.has_value()) {
		outn->add_link(link);
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

void GraphCommandUtil::remove_link(GraphLink link, EditorNodeGraph& graph)
{
	Base_EdNode* inn = graph.get_node(link.input.get_node());
	Base_EdNode* outn = graph.get_node(link.output.get_node());
	assert(inn && outn);
	GraphNodeHandle nh1 = inn->remove_link(link);
	GraphNodeHandle nh2 = outn->remove_link(link);
	graph.remove_node(nh1);
	graph.remove_node(nh2);
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
}
void AddLinkCommand::execute()
{
	GraphCommandUtil::add_link(link, ed.get_graph());
}
void AddLinkCommand::undo()
{
	GraphCommandUtil::remove_link(link, ed.get_graph());
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
	assert(node);
	node->editor = &ed;
	ed.get_graph().insert_new_node(*node, what_layer, pos);
	created_handle = node->self;
	assert(created_handle.is_valid());
}
void AddNodeCommand::undo()
{
	assert(created_handle.is_valid());
	ed.get_graph().remove_node(created_handle);
}
std::string AddNodeCommand::to_string()
{
	return "AddNodeCommand";
}
RemoveGraphObjectsCommand::RemoveGraphObjectsCommand(AnimationGraphEditorNew& ed, vector<int> link_ids, vector<int> node_ids) :ed(ed)
{
	// serialize the objects
	SerializeGraphContainer container = SerializeGraphUtils::make_container_from_nodeids(node_ids, link_ids,ed.get_graph());
	serialized = SerializeGraphUtils::serialize_to_string(container, ed.get_graph());
	printf("%s\n", serialized.c_str());
	this->nodes = std::move(node_ids);
	for (int l : link_ids) {
		opt<GraphLink> link = GraphCommandUtil::get_graph_link_from_linkid(l,ed.get_graph());
		if (link.has_value()) {
			this->links.push_back(link.value());
		}
	}
}

void RemoveGraphObjectsCommand::execute()
{
	auto& graph = ed.get_graph();
	// have to remove all objects, including sub graph objects
	for (GraphLink link : this->links) {
		GraphCommandUtil::remove_link(link,graph);
	}
	for (int n : this->nodes) {
		GraphNodeHandle h(n);
		graph.remove_node(h);
	}
}

void RemoveGraphObjectsCommand::undo()
{
	// put back removed objects
	auto unserialized = SerializeGraphUtils::unserialize(serialized);
	if (!unserialized) {
		LOG_WARN("couldnt unserialize");
		return;
	}
	ed.get_graph().insert_nodes(*unserialized);
	for (GraphLink link : links) {
		GraphCommandUtil::add_link(link, ed.get_graph());
	}
}

std::string RemoveGraphObjectsCommand::to_string()
{
	return "RemoveGraphObjectsCommand";
}
