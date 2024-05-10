#pragma once
#include <cassert>
#include <unordered_map>

#include "MemArena.h"
#include "InlineVec.h"
#include "StringUtil.h"
#include "StringName.h"

#include "ControlParamHandle.h"
#include "ScriptValue.h"

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

class Model_Skeleton
{
public:
	bool data_is_valid = false;
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
	bool data_is_valid = false;
	const Model_Skeleton* src_skeleton = nullptr;	
	struct Import {
		Model_Skeleton* import_skeleton = nullptr;
		const Model* mod = nullptr;
	};
	std::vector<Import> imports;
	std::unordered_map<std::string, std::string> table;	// fixme: do better

	void find_animation(const char* name, int16_t* out_set, int16_t* out_index, int16_t* out_skel) const;
	const Animation_Set* get_subset(uint32_t index) const;
	const std::vector<int>& get_remap(uint32_t skel_index) const {
		return src_skeleton->remaps.at(skel_index).skel_to_source;
	}
};

class Model;
struct Animation_Tree_RT
{
	const Animation_Tree_CFG* cfg = nullptr;
	const Model* model = nullptr;
	const Animation_Set_New* set = nullptr;
	std::vector<uint8_t> data;	// runtime data
	program_script_vars_instance vars;

	void init_from_cfg(const Animation_Tree_CFG* cfg,const Model* model, const Animation_Set_New* set);

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
	const Animation_Set_New* find_set(const char* name);
	const Model_Skeleton* find_skeleton(const char* name) const;
	Model_Skeleton* find_skeleton(const char* name);
	void on_new_animation(Model* m, int index);
	const Library* get_std_animation_script_lib();
private:
	void load_notifies();
	void parse_notify_file(DictParser& parser);

	Animation_Tree_CFG* load_animation_tree_file(const char* filename, DictParser& parser);

	struct Animation_Link {
		const Model* model = nullptr;
		int index = 0;
		Animation_Notify_List list;
	};
	Memory_Arena event_arena;
	std::unordered_map<std::string, Animation_Link> notifies;
	std::unordered_map<std::string, Model_Skeleton> skeletons;
	std::unordered_map<std::string, Animation_Set_New> sets;
	std::unordered_map<std::string, Animation_Tree_CFG> trees;

	friend class AnimationGraphEditor;
};

extern Animation_Tree_Manager* anim_tree_man;