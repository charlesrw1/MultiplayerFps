#pragma once
#include <cassert>
#include <unordered_map>

#include "Framework/InlineVec.h"
#include "Framework/StringUtil.h"
#include "Framework/StringName.h"
#include "Framework/ClassTypePtr.h"

#include "Assets/IAsset.h"

class DictParser;
class DictWriter;
class Model;
class Node_CFG;
class Animation_Tree_Manager;
struct PropertyInfoList;
class NodeRt_Ctx;
class Script;
class BaseAGNode;
class AnimatorInstance;


class Animation_Tree_CFG : public IAsset {
public:
	CLASS_BODY(Animation_Tree_CFG);

	Animation_Tree_CFG();
	~Animation_Tree_CFG() override;

	void uninstall() override;
	bool load_asset(IAssetLoadingInterface* load);
	void post_load() {}
	void move_construct(IAsset* other);
	void sweep_references(IAssetLoadingInterface* load) const override;

	const Node_CFG* get_root_node() const {
		return root;
	}
	 Node_CFG* get_root_node()  {
		return root;
	}

	bool post_load_init();
	int get_index_of_node(Node_CFG* ptr);
	void write_to_dict(DictWriter& out);
	bool read_from_dict(DictParser& in, IAssetLoadingInterface* load);

	//uint32_t get_data_used() const { return data_used; }

	void construct_all_nodes(NodeRt_Ctx& ctx) const;

	//void add_data_used(uint32_t amt) {
	//	data_used += amt;
	//	uintptr_t modulo = data_used % 16;
	//	if (modulo != 0)
	//		data_used =  data_used + 16 - modulo;
	//}

	BaseAGNode* get_node(uint32_t index) { return all_nodes.at(index); }

	bool get_graph_is_valid() const { return graph_is_valid; }

	const std::vector<std::string>& get_slot_names() const { return direct_slot_names; }

	const PropertyInfo* find_animator_instance_variable(const std::string& var_name) const;
	AnimatorInstance* allocate_animator_class() const;

	int get_num_nodes() const {
		return all_nodes.size();
	}

	static std::unique_ptr<Animation_Tree_CFG> construct_fake_tree();

	bool get_is_fake_tree() const {
		return is_fake_tree;
	}
private:
	REF bool graph_is_valid = false;
	bool is_fake_tree = false;

	// graph root node
	REFLECT(type="AgSerializeNodeCfg")
	Node_CFG* root = nullptr;
	
	// data in bytes for runtime nodes
	//uint32_t data_used = 0;

	std::vector<BaseAGNode*> all_nodes;

	REFLECT(type = "ClassTypePtr");
	ClassTypePtr<AnimatorInstance> animator_class;

	// provides names + indicies for playing animations directly
	// these get provided by adding 'Direct play' nodes and setting their names
	// if 2 nodes share a name, then they reference same index
	// TODO: might want better way than this than tying it to a graph asset
	REF std::vector<std::string> direct_slot_names;

	friend class Animation_Tree_Manager;
	friend class AnimationGraphEditor;
	friend class SerializeNodeCFGRef;
	friend class AgSerializeContext;
	friend class DirectPlaySlot_EdNode;// hack for editor nodes to append direct_slot
	friend class AnimTreeLoadJob;

	//bool is_initialized() { return !path.empty(); }
};

class DictParser;
class AnimationGraphEditor;
