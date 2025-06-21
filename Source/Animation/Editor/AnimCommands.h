#pragma once
#include "LevelEditor/CommandMgr.h"
#include "Base_node.h"
#include "Framework/ConsoleCmdGroup.h"
class NodeGraphLayer;
using std::unordered_set;


class GraphCommandUtil
{
public:
	static void remove_link(GraphLink link, EditorNodeGraph& graph);
	static void add_link(GraphLink link, EditorNodeGraph& graph, GraphNodeHandle link_node);
	static GraphPortHandle get_input_port_from_link(int id);
	static opt<GraphLink> get_graph_link_from_linkid(int id, EditorNodeGraph& graph);
	static void get_selected(vector<int>& link_ids, vector<int>& node_ids);
	static Base_EdNode* get_optional_link_object(int linkid, EditorNodeGraph& graph);

	static bool can_connect_these_ports(GraphLink link, EditorNodeGraph& graph);

	struct Clipboard
	{
		unordered_set<int> orig_nodes;
		string serialized;
	};
	static opt<Clipboard> create_clipboard(const vector<int>& node_ids, AnimationGraphEditorNew& ed);
	static vector<GraphNodeHandle> paste_clipboard(const Clipboard& clipboard, opt<GraphLayerHandle> whatLayer, AnimationGraphEditorNew& ed);
	static void undo_paste(vector<GraphNodeHandle>& handles, AnimationGraphEditorNew& ed);
};

class AnimationGraphEditorNew;
class AddLinkCommand : public Command
{
public:
	AddLinkCommand(AnimationGraphEditorNew& ed, GraphPortHandle input, GraphPortHandle output);
	// Inherited via Command
	void execute() override;
	void undo() override;
	std::string to_string() override;
	bool is_valid() override { return is_link_valid; }
	AnimationGraphEditorNew& ed;
	GraphLink link;
	bool is_link_valid = true;
};

class AddNodeCommand : public Command
{
public:
	AddNodeCommand(AnimationGraphEditorNew& ed, const string& creation_prototype, glm::vec2 pos, GraphLayerHandle layer);
	// Inherited via Command
	void execute() override;
	void undo() override;
	std::string to_string() override;

	glm::vec2 pos;
	string creation_name;
	GraphLayerHandle what_layer;
	AnimationGraphEditorNew& ed;
	GraphNodeHandle created_handle;
};

class AddNodeWithLink : public Command {
public:
	AddNodeWithLink(AnimationGraphEditorNew& ed, const string& creation_prototype, glm::vec2 pos, GraphLayerHandle layer,
		GraphPortHandle from_port, int to_port_index);
	void execute() final;
	void undo() final;
	string to_string() final {
		return "AddNodeWithLink";
	}
	bool is_valid() final {
		assert(addNode);
		return addNode->is_valid();
	}
	GraphPortHandle from_port;
	int to_port_index = 0;
	uptr<AddNodeCommand> addNode;
	GraphLink createdLink;
};


class RemoveGraphObjectsCommand : public Command
{
public:
	RemoveGraphObjectsCommand(AnimationGraphEditorNew& ed, vector<int> link_ids, vector<int> node_ids);
	// Inherited via Command
	void execute() override;
	void undo() override;
	std::string to_string() override;
	string serialized;

	vector<GraphLinkWithNode> links;
	vector<int> nodes;
	AnimationGraphEditorNew& ed;
};

class PasteNodeClipboardCommand : public Command
{
public:
	PasteNodeClipboardCommand(AnimationGraphEditorNew& ed, opt<GraphLayerHandle> whatLayer, const GraphCommandUtil::Clipboard& clipboard);
	// Inherited via Command
	void execute() final;
	void undo() final;
	std::string to_string() final { return "PasteNodes"; }

	AnimationGraphEditorNew& ed;
	GraphCommandUtil::Clipboard clipboard;
	vector<GraphNodeHandle> nodes_to_delete;
	opt<GraphLayerHandle> whatLayer;
};
class DuplicateNodesCommand : public Command
{
public:
	DuplicateNodesCommand(AnimationGraphEditorNew& ed, vector<int> node_ids);
	bool is_valid() final { return pasteCommand.get() != nullptr; }
	// Inherited via Command
	void execute() final {
		pasteCommand->execute();
	}
	void undo() final {
		pasteCommand->undo();
	}
	std::string to_string() final;
	uptr<PasteNodeClipboardCommand> pasteCommand;
};