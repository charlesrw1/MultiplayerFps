#pragma once
#include <cassert>
#include "ScriptVars.h"
#include "MemArena.h"

// this is the thing loaded from disk once

class Model;
class Animation_Set;
class Node_CFG;
struct Animation_Tree_CFG
{
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
};

class Model;
struct Animation_Tree_RT
{
	void init_from_cfg(const Animation_Tree_CFG* cfg,const Model* model, const Animation_Set* set);
	const Animation_Tree_CFG* cfg = nullptr;
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

	std::unordered_map<std::string, Animation_Tree_CFG> trees;
};

extern Animation_Tree_Manager* anim_tree_man;