#pragma once
#include <cassert>
#include "ScriptVars.h"
#include "MemArena.h"
#include "StringUtil.h"
// this is the thing loaded from disk once

class DictParser;
class DictWriter;

class Model;
class Animation_Set;
class Node_CFG;
class Animation_Tree_CFG
{
public:
	Stack_String<128> name;
	Node_CFG* root = nullptr;
	Memory_Arena arena;
	uint32_t data_used = 0;
	std::vector<Node_CFG*> all_nodes;	// for initialization
	ScriptVars_CFG parameters;

	handle<Parameter> find_param(const char* param)  {
		if (parameters.name_to_index.find(param) != parameters.name_to_index.end())
			return { parameters.name_to_index[param] };
		return { -1 };
	}
	int get_index_of_node(Node_CFG* ptr);

	void write_to_dict(DictWriter& out);
	void read_from_dict(DictParser& in);

};

class Model_Skeleton
{
public:
	const Model* source = nullptr;
	std::vector<int> bone_mirror_map;
	
	struct Remap {
		Model_Skeleton* source = nullptr;
		std::vector<int> skel_to_source;
	};
	std::vector<Remap> remaps;

	int find_remap(const Model_Skeleton* skel) const {
		for (int i = 0; i < remaps.size(); i++) {
			if (remaps[i].source == skel)
				return i;
		}
		return -1;
	}
};

class Animation_Set_New
{
public:
	const Animation_Set_New* parent = nullptr;
	const Model_Skeleton* src_skeleton = nullptr;
	
	struct Import {
		const Model_Skeleton* import_skeleton = nullptr;
		const Animation_Set* set = nullptr;
	};

	std::vector<Import> imports;
	std::unordered_map<std::string, std::string> table;	// fixme: do better

	void find_animation(const char* name, uint32_t* out_set, uint32_t* out_index, uint32_t* out_skel) const;
	const Animation_Set* get_subset(uint32_t index) const {
		return imports[index].set;
	}
	const std::vector<int>& get_remap(uint32_t skel_index) const {
		return src_skeleton->remaps.at(skel_index).skel_to_source;
	}
};

class Model;
struct Animation_Tree_RT
{
	void init_from_cfg(const Animation_Tree_CFG* cfg,const Model* model, const Animation_Set_New* set);
	const Animation_Tree_CFG* cfg = nullptr;
	const Model* model = nullptr;
	const Animation_Set_New* set = nullptr;

	std::vector<uint8_t> data;	// runtime data
	ScriptVars_RT parameters;

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
};

class Animation_Tree_Manager
{
public:
	Animation_Tree_CFG* find_animation_tree(const char* filename);

	const Animation_Set_New* find_set(const char* name);
	const Model_Skeleton* find_skeleton(const char* name) const;
	Model_Skeleton* find_skeleton(const char* name);
private:

	std::unordered_map<std::string, Model_Skeleton> skeletons;
	std::unordered_map<std::string, Animation_Set_New> sets;
	std::unordered_map<std::string, Animation_Tree_CFG> trees;
};

extern Animation_Tree_Manager* anim_tree_man;