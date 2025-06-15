#pragma once
#include "LevelEditor/CommandMgr.h"
#include "Base_node.h"
#include "Framework/ConsoleCmdGroup.h"
class NodeGraphLayer;

class GraphCommandUtil
{
public:
	static void remove_link(GraphLink link, EditorNodeGraph& graph);
	static void add_link(GraphLink link, EditorNodeGraph& graph);
	static GraphPortHandle get_input_port_from_link(int id);
	static opt<GraphLink> get_graph_link_from_linkid(int id, EditorNodeGraph& graph);
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
class RemoveGraphObjectsCommand : public Command
{
public:
	RemoveGraphObjectsCommand(AnimationGraphEditorNew& ed, vector<int> link_ids, vector<int> node_ids);
	// Inherited via Command
	void execute() override;
	void undo() override;
	std::string to_string() override;
	string serialized;
	vector<GraphLink> links;
	vector<int> nodes;
	AnimationGraphEditorNew& ed;
};
class MoveNodeCommand : public Command
{
public:
};
class DuplicateNodesCommand : public Command
{
public:
};