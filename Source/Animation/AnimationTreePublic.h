#pragma once
#include <cassert>
#include <unordered_map>

#include "Framework/MemArena.h"
#include "Framework/InlineVec.h"
#include "Framework/StringUtil.h"
#include "Framework/StringName.h"
#include "Framework/ScriptValue.h"

#include "Runtime/ControlParamHandle.h"

#include "IAsset.h"

class DictParser;
class DictWriter;
class Model;
class Animation_Set;
class Node_CFG;
class Program;
class Library;
class ControlParam_CFG;


// this is the thing loaded from disk once
class Animation_Tree_Manager;
struct PropertyInfoList;
class NodeRt_Ctx;
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
	const ControlParam_CFG* get_control_params() const {
		return params.get();
	}
	void post_load_init();
	ControlParamHandle find_param(StringName name);
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

	Node_CFG* get_node(uint32_t index) { return all_nodes.at(index); }

	const Program* get_program() const { return graph_program.get(); }

	bool get_graph_is_valid() const { return graph_is_valid; }
private:
	void init_program_libs();

	bool graph_is_valid = false;
	Node_CFG* root = nullptr;
	// data in bytes for runtime nodes
	uint32_t data_used = 0;
	std::vector<Node_CFG*> all_nodes;	// for initialization
	std::unique_ptr<Library> graph_var_lib;
	std::unique_ptr<Program> graph_program;
	std::unique_ptr<ControlParam_CFG> params;

	static PropertyInfoList* get_props();

	// :(
	friend class Animation_Tree_Manager;
	friend class AnimationGraphEditor;
	friend class SerializeNodeCFGRef;
	friend class AgSerializeContext;

	bool is_initialized() { return !path.empty(); }
};

class Model;
class Animation_Tree_RT
{
public:
	void init_from_cfg(const Animation_Tree_CFG* cfg,const Model* model);

	const Animation_Tree_CFG* cfg = nullptr;
	const Model* model = nullptr;
	program_script_vars_instance vars;

	template<typename T>
	T* get(uint32_t offset) {
		ASSERT(offset + sizeof(T) <= data.size());
		return (T*)(data.data() + offset);
	}

	template<typename T>
	T* construct_rt(uint32_t ofs) {
		T* ptr = get<T>(ofs);
		ptr = new(ptr)(T);
		return ptr;
	}
private:
	std::vector<uint8_t> data;	// runtime data
};

class DictParser;
class AnimationGraphEditor;
class Animation_Tree_Manager
{
public:
	void init();
	Animation_Tree_CFG* find_animation_tree(const char* filename);
	const Library* get_std_animation_script_lib();
private:
	Animation_Tree_CFG* load_animation_tree_file(const char* filename, DictParser& parser);
	std::unordered_map<std::string, Animation_Tree_CFG> trees;
	friend class AnimationGraphEditor;
};

extern Animation_Tree_Manager* anim_tree_man;