#include "Animation/Editor/AnimCommands.h"
#include "AnimationGraphEditor2.h"
#include "GraphUtil.h"
#include "Framework/SerializerJson.h"



AddLinkCommand::AddLinkCommand(AnimationGraphEditorNew& ed, GraphPortHandle input, GraphPortHandle output) : ed(ed)
{
	link = GraphLink(input, output);
	Base_EdNode* inn = ed.get_graph().get_node(link.input.get_node());
	opt<int> index = inn->find_link_idx(link.input);
	if (index.has_value()) {
		is_link_valid = false;
		sys_print(Warning, "link input already has link\n");
		return;
	}
}

void AddLinkCommand::execute()
{
	Base_EdNode* outn = ed.get_graph().get_node(link.output.get_node());
	Base_EdNode* inn = ed.get_graph().get_node(link.input.get_node());
	opt<int> index = inn->find_link_idx(link.input);
	if (index.has_value()) {
		LOG_WARN("addlinkcmd invalid");
		return;
	}
	inn->add_link(link);
	outn->add_link(link);
}

void AddLinkCommand::undo()
{
	Base_EdNode* inn = ed.get_graph().get_node(link.input.get_node());
	Base_EdNode* outn = ed.get_graph().get_node(link.output.get_node());
	assert(inn && outn);
	inn->remove_link(link);
	outn->remove_link(link);
}

std::string AddLinkCommand::to_string()
{
	return "AddLinkCommand " + link.to_string();
}

RemoveLinkCommand::RemoveLinkCommand(AnimationGraphEditorNew& ed, GraphPortHandle input) : ed(ed)
{
}

void RemoveLinkCommand::execute()
{
}

void RemoveLinkCommand::undo()
{
}

std::string RemoveLinkCommand::to_string()
{
	return "RemoveLinkCommand";
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
	ed.get_graph().insert_new_node(*node, what_layer);
	ed.get_imnodes().set_node_position(node->self, pos);
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

RemoveGraphObjectsCommand::RemoveGraphObjectsCommand(AnimationGraphEditorNew& ed, vector<int> link_ids, vector<int> node_ids)
{
	// serialize the objects


	WriteSerializerBackendJson json(;
}

void RemoveGraphObjectsCommand::execute()
{
	// have to remove all objects, including sub graph objects
}

void RemoveGraphObjectsCommand::undo()
{
	// put back removed objects
}

std::string RemoveGraphObjectsCommand::to_string()
{
	return "RemoveGraphObjectsCommand";
}
