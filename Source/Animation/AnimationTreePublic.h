#pragma once
#include <cassert>
#include <unordered_map>

#include "Framework/InlineVec.h"
#include "Framework/StringUtil.h"
#include "Framework/StringName.h"
#include "Framework/ClassTypePtr.h"

#include "Assets/IAsset.h"
#include "Animation/Runtime/Animation.h"
#include "Runtime/RuntimeNodesBase.h"
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
using std::unordered_map;
using std::string;
using std::vector;

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
	bool post_load_init();
	const vector<string>& get_slot_names() const { return direct_slot_names; }
	AnimatorInstance* allocate_animator_class() const;
	const ClassTypeInfo& get_animator_class() const {  return *animator_class.ptr;  }
	const unordered_map<int, ClassBase*>& get_nodes() const { return nodes_in_container; };
	int get_root_node_id() const { return rootNodeIdx; }
private:
	bool is_fake_tree = false;
	unordered_map<int, ClassBase*> nodes_in_container;	// both value and pose nodes
	REF int rootNodeIdx = 0;
	REF ClassTypePtr<AnimatorInstance> animator_class;
	REF vector<string> direct_slot_names;

	friend class Animation_Tree_Manager;
	friend class AnimationGraphEditor;
	friend class SerializeNodeCFGRef;
	friend class AgSerializeContext;
	friend class DirectPlaySlot_EdNode;// hack for editor nodes to append direct_slot
	friend class AnimTreeLoadJob;
};

class DictParser;
class AnimationGraphEditor;
