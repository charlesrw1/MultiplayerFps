#pragma once
#include "LevelEditor/CommandMgr.h"
#include "Base_node.h"
#include "Framework/ConsoleCmdGroup.h"
class NodeGraphLayer;
using std::unordered_map;

class SerializeGraphContainer
{
public:
	unordered_map<int, Base_EdNode*> nodes;
	unordered_map<int, NodeGraphLayer*> layers;
};

class SerializeGraphUtils
{
public:
	static uptr<SerializeGraphContainer> unserialize();
	static void serialize(SerializeGraphContainer& container);
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
class RemoveLinkCommand : public Command
{
public:
	RemoveLinkCommand(AnimationGraphEditorNew& ed, GraphPortHandle input);

	// Inherited via Command
	void execute() override;
	void undo() override;
	std::string to_string() override;
	AnimationGraphEditorNew& ed;
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
};
class MoveNodeCommand : public Command
{
public:
};
class DuplicateNodesCommand : public Command
{
public:
};