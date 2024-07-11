#pragma once
#include <cassert>
#include <unordered_map>

#include "Framework/MemArena.h"
#include "Framework/InlineVec.h"
#include "Framework/StringUtil.h"
#include "Framework/StringName.h"
#include "Framework/ScriptValue.h"

#include "IAsset.h"

class DictParser;
class DictWriter;
class Model;
class Animation_Set;
class Node_CFG;


// this is the thing loaded from disk once
class Animation_Tree_Manager;
struct PropertyInfoList;
class NodeRt_Ctx;
class Script;
class BaseAGNode;
class Animation_Tree_CFG : public IAsset
{
public:
	Animation_Tree_CFG();
	~Animation_Tree_CFG();

	const Node_CFG* get_root_node() const {
		return root;
	}
	 Node_CFG* get_root_node()  {
		return root;
	}
	const Script* get_script() const {
		return code.get();
	}
	 Script* get_script()  {
		return code.get();
	}
	bool post_load_init();
	int get_index_of_node(Node_CFG* ptr);
	void write_to_dict(DictWriter& out);
	bool read_from_dict(DictParser& in);

	uint32_t get_data_used() const { return data_used; }
	uint32_t get_num_vars() const;

	void construct_all_nodes(NodeRt_Ctx& ctx) const;

	void add_data_used(uint32_t amt) {
		data_used += amt;
		uintptr_t modulo = data_used % 16;
		if (modulo != 0)
			data_used =  data_used + 16 - modulo;
	}

	BaseAGNode* get_node(uint32_t index) { return all_nodes.at(index); }

	bool get_graph_is_valid() const { return graph_is_valid; }

	const std::vector<std::string>& get_slot_names() const { return direct_slot_names; }
private:
	bool graph_is_valid = false;

	// graph root node
	Node_CFG* root = nullptr;
	
	// data in bytes for runtime nodes
	uint32_t data_used = 0;

	std::vector<BaseAGNode*> all_nodes;

	// stores variables and functions for evaulating transitions
	std::unique_ptr<Script> code;

	// provides names + indicies for playing animations directly
	// these get provided by adding 'Direct play' nodes and setting their names
	// if 2 nodes share a name, then they reference same index
	// TODO: might want better way than this than tying it to a graph asset
	std::vector<std::string> direct_slot_names;

	static const PropertyInfoList* get_props();

	friend class Animation_Tree_Manager;
	friend class AnimationGraphEditor;
	friend class SerializeNodeCFGRef;
	friend class AgSerializeContext;
	friend class DirectPlaySlot_EdNode;// hack for editor nodes to append direct_slot

	bool is_initialized() { return !path.empty(); }
};

class DictParser;
class AnimationGraphEditor;
class Animation_Tree_Manager
{
public:
	void init();
	Animation_Tree_CFG* find_animation_tree(const char* filename);
private:
	Animation_Tree_CFG* load_animation_tree_file(const char* filename, DictParser& parser);
	std::unordered_map<std::string, Animation_Tree_CFG> trees;
	friend class AnimationGraphEditor;
};

extern Animation_Tree_Manager* anim_tree_man;