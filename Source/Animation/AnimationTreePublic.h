#pragma once
#include <cassert>
#include <unordered_map>

#include "Framework/MemArena.h"
#include "Framework/InlineVec.h"
#include "Framework/StringUtil.h"
#include "Framework/StringName.h"
#include "Framework/ScriptValue.h"

#include "Runtime/ControlParamHandle.h"

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
class Animation_Tree_CFG
{
public:
	Animation_Tree_CFG();
	~Animation_Tree_CFG();

	void init_program_libs();
	void post_load_init();

	bool graph_is_valid = false;
	std::string name;
	Node_CFG* root = nullptr;
	// data in bytes for runtime nodes
	uint32_t data_used = 0;
	std::vector<Node_CFG*> all_nodes;	// for initialization
	std::unique_ptr<Library> graph_var_lib;
	std::unique_ptr<Program> graph_program;
	std::unique_ptr<ControlParam_CFG> params;

	ControlParamHandle find_param(StringName name);
	int get_index_of_node(Node_CFG* ptr);
	void write_to_dict(DictWriter& out);
	bool read_from_dict(DictParser& in);


	static PropertyInfoList* get_props();

private:
	friend class Animation_Tree_Manager;
	bool is_initialized() { return !name.empty(); }
};

class Model;
struct Animation_Tree_RT
{
	const Animation_Tree_CFG* cfg = nullptr;
	const Model* model = nullptr;
	std::vector<uint8_t> data;	// runtime data
	program_script_vars_instance vars;

	void init_from_cfg(const Animation_Tree_CFG* cfg,const Model* model);

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

enum class AnimationNotifyType
{
	FOOTSTEP_LEFT,
	FOOTSTEP_RIGHT,
	SOUND,
	EFFECT,
	LOCK_IK,
	RELEASE_IK,
};

struct Animation_Notify_Def {
	AnimationNotifyType type = AnimationNotifyType::FOOTSTEP_LEFT;
	int param_count = 0;
	const char* params[4];
	float start = 0.0;
	float end = -1.0;

	bool is_oneshot_event() const {
		return end < 0.0;
	}
	bool is_duration_event()  const {
		return !is_oneshot_event();
	}
};

struct Animation_Notify_List
{

	int count = 0;
	Animation_Notify_Def* defs = nullptr;
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