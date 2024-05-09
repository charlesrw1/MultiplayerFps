#pragma once

#include "Model.h"
#include "ExpressionLang.h"
#include "glm/glm.hpp"
#include "MemArena.h"

#include "AnimationTreePublic.h"
#include "InlineVec.h"

#include "EnumDefReflection.h"
#include "ReflectionProp.h"

#include "ControlParams.h"
#include "TypeInfo.h"

#include "Factory.h"

#include <cassert>


class Node_CFG;
class Animation_Tree_CFG;


struct Rt_Vars_Base
{
	uint16_t last_update_tick = 0;
};

class Program;
struct Animation_Tree_RT;
struct Animation_Tree_CFG;
struct Node_CFG;
struct Control_Params;
class Language;
struct NodeRt_Ctx
{
	const Model* model = nullptr;
	const Animation_Set_New* set = nullptr;
	Animation_Tree_RT* tree = nullptr;
	const ControlParam_CFG* param_cfg = nullptr;
	const Program* script_prog = nullptr;
	uint16_t tick = 0;

	float get_float(ControlParamHandle handle) const {
		return param_cfg->get_float(&tree->vars, handle);
	}
	int get_int(ControlParamHandle handle) const {
		return param_cfg->get_int(&tree->vars, handle);
	}
	bool get_bool(ControlParamHandle handle) const {
		return param_cfg->get_bool(&tree->vars, handle);
	}

	uint32_t num_bones() const { return model->bones.size(); }
};

struct GetPose_Ctx
{
	GetPose_Ctx set_pose(Pose* newpose) {
		GetPose_Ctx gp = *this;
		gp.pose = newpose;
		return gp;
	}
	GetPose_Ctx set_dt(float newdt) {
		GetPose_Ctx gp = *this;
		gp.dt = newdt;
		return gp;
	}
	// used for syncronizing animations
	struct syncval {
		float normalized_frame = 0.0;
		bool first_seen = false;
	};
	GetPose_Ctx set_sync(syncval* s) {
		GetPose_Ctx gp = *this;
		gp.sync = s;
		return gp;
	}
	GetPose_Ctx set_rootmotion(float rm) {
		GetPose_Ctx gp = *this;
		gp.rootmotion_scale = rm;
		return gp;
	}

	Pose* pose = nullptr;
	float dt = 0.0;
	syncval* sync = nullptr;
	// if > 0, then scale clip by rootmotion
	float rootmotion_scale = -1.0;
};


struct Node_CFG
{
	virtual void initialize(Animation_Tree_CFG* cfg) = 0;

	bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {
		set_active(ctx, get_rt<Rt_Vars_Base>(ctx));
		return get_pose_internal(ctx, pose);
	}
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const = 0;
	virtual void construct(NodeRt_Ctx& ctx) const {
		//assert(rt_offset == 0);
	}
	virtual void reset(NodeRt_Ctx& ctx) const = 0;
	virtual bool is_clip_node() const { return false; }

	// serialization helpers, optional
	virtual PropertyInfoList* get_props() = 0;
	virtual const TypeInfo& get_typeinfo() const = 0;

	void set_active(NodeRt_Ctx& ctx, Rt_Vars_Base* base) const {
		base->last_update_tick = ctx.tick;
	}
	bool was_active_last_frame(NodeRt_Ctx& ctx) const {
		return get_rt<Rt_Vars_Base>(ctx)->last_update_tick == ctx.tick;
	}

	template<typename T>
	T* get_rt(NodeRt_Ctx& ctx) const {
		return ctx.tree->get<T>(rt_offset);
	}

	InlineVec<Node_CFG*, 2> input;
protected:
	template<typename T>
	T* construct_this(NodeRt_Ctx& ctx) const {
		return ctx.tree->construct_rt<T>(rt_offset);
	}

	void init_memory_internal(Animation_Tree_CFG* cfg, uint32_t rt_size) {
		rt_offset = cfg->data_used;
		ASSERT(rt_size >= sizeof(Rt_Vars_Base));
		cfg->data_used += rt_size;
	}

private:
	uint32_t rt_offset = 0;
};

#define DECLARE_NODE_CFG(TYPE_NAME) \
virtual PropertyInfoList* get_props() override; \
virtual const TypeInfo& get_typeinfo() const override; \
virtual void initialize(Animation_Tree_CFG* tree) override { \
	init_memory_internal(tree, sizeof(RT_TYPE)); \
} \
TYPE_NAME() {}

#define DECLARE_NO_DEFAULT(TYPE_NAME) \
virtual PropertyInfoList* get_props() override; \
virtual const TypeInfo& get_typeinfo() const override; 

// playback speed *= param / (speed of clip's root motion)
struct Scale_By_Rootmotion_CFG : public Node_CFG
{
	using RT_TYPE = Rt_Vars_Base;
	DECLARE_NODE_CFG(Scale_By_Rootmotion_CFG);
	
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override
	{
		float rm = ctx.get_float(param);
		bool ret = input[0]->get_pose(ctx, pose.set_rootmotion(rm));
		return ret;
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
	}

	ControlParamHandle param;
};

struct Sync_Node_RT : Rt_Vars_Base
{
	float normalized_frame = 0.0;
};

struct Sync_Node_CFG : public Node_CFG
{
	using RT_TYPE = Sync_Node_RT;
	DECLARE_NODE_CFG(Sync_Node_CFG);

	virtual void construct(NodeRt_Ctx& ctx) const {
		construct_this<Sync_Node_RT>(ctx);
	}

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override
	{
		Sync_Node_RT* rt = get_rt<Sync_Node_RT>(ctx);

		GetPose_Ctx::syncval sv;
		sv.first_seen = true;
		sv.normalized_frame = rt->normalized_frame;

		bool ret = input[0]->get_pose(ctx, pose.set_sync(&sv));

		rt->normalized_frame = sv.normalized_frame;

		return ret;
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		Sync_Node_RT* rt = get_rt<Sync_Node_RT>(ctx);
		rt->normalized_frame = 0.0;
	}

	ControlParamHandle param;
};

struct Clip_Node_RT : Rt_Vars_Base
{
	glm::vec3 root_pos_first_frame = glm::vec3(0.f);
	float inv_speed_of_anim_root = 1.0;
	float frame = 0.0;
	int16_t clip_index = -1;
	int16_t set_idx = -1;
	int16_t skel_idx = -1;
	bool stopped_flag = false;
};


extern AutoEnumDef rootmotion_setting_def;
enum class rootmotion_setting : uint8_t {
	keep,
	remove,
	add_velocity
};

struct Clip_Node_CFG : public Node_CFG
{
	using RT_TYPE = Clip_Node_RT;

	DECLARE_NODE_CFG(Clip_Node_CFG);

	virtual void construct(NodeRt_Ctx& ctx) const {
		RT_TYPE* rt = construct_this<RT_TYPE>(ctx);

		ctx.set->find_animation(clip_name.c_str(), &rt->set_idx,&rt->clip_index,&rt->skel_idx);

		if (rt->clip_index != -1) {
			int root_index = ctx.model->root_bone_index;

			if (rt->skel_idx != -1) {
				root_index = ctx.set->get_remap(rt->skel_idx)[root_index];
			}

			auto subset = ctx.set->get_subset(rt->set_idx);
			if (root_index != -1) {
				int first_pos = subset->FirstPositionKeyframe(0.0, root_index, rt->clip_index);
				rt->root_pos_first_frame = first_pos != -1 ?
					subset->GetPos(root_index, first_pos, rt->clip_index).val
					: ctx.model->bones[root_index].posematrix[3];
			}

			const Animation& clip = subset->clips[rt->clip_index];
			rt->inv_speed_of_anim_root = 1.0 / glm::length(clip.root_motion_translation) / (clip.total_duration / clip.fps);
		}
	}


	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	virtual void reset(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);
		rt->frame = 0.0;
		rt->stopped_flag = false;
	}

	virtual bool is_clip_node() const override {
		return true;
	}
	const Animation* get_clip(NodeRt_Ctx& ctx) const {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);
		if (rt->clip_index == -1) {
			return nullptr;
		}
		auto subset = ctx.set->get_subset(rt->set_idx);
		const Animation& clip = subset->clips[rt->clip_index];

		return &clip;
	}
	void set_frame_by_interp(NodeRt_Ctx& ctx, float frac) const {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);

		rt->frame = get_clip(ctx)->total_duration * frac;
	}

	rootmotion_setting rm[3] = { rootmotion_setting::keep ,rootmotion_setting::keep, rootmotion_setting::keep };
	std::string clip_name;
	bool loop = true;
	bool allow_sync = false;
	bool can_be_leader = true;
	float speed = 1.0;
	uint16_t start_frame = 0;
};

struct Subtract_Node_CFG : public Node_CFG
{
	using RT_TYPE = Rt_Vars_Base;
	DECLARE_NODE_CFG(Subtract_Node_CFG);

	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		input[REF]->reset(ctx);
		input[SOURCE]->reset(ctx);
	}
	enum {
		REF,
		SOURCE
	};
};

struct Add_Node_CFG : public Node_CFG
{
	using RT_TYPE = Rt_Vars_Base;
	DECLARE_NODE_CFG(Add_Node_CFG);

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		input[DIFF]->reset(ctx);
		input[BASE]->reset(ctx);
	}

	enum {
		DIFF,
		BASE
	};
	ControlParamHandle param;
};

struct Blend_Node_RT : Rt_Vars_Base
{
	float lerp_amt = 0.0;
	float saved_f = 0.0;
};

// generic blend by bool or blend by float
struct Blend_Node_CFG : public Node_CFG
{
	using RT_TYPE = Blend_Node_RT;
	DECLARE_NO_DEFAULT(Blend_Node_CFG);

	virtual void initialize(Animation_Tree_CFG* tree) override {
		init_memory_internal(tree, sizeof(RT_TYPE)); 
		if (param.is_valid()) {
			parameter_type = (tree->params->get_type(param) == control_param_type::float_t) ? 0 : 1;
		}
	} 

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);

		float cur_val = 0.0;
		if (param.is_valid()) {
			if (parameter_type == 0)	// float
				cur_val = ctx.get_float(param);
			else // boolean
				cur_val = (float)ctx.get_bool(param);
		}
		rt->lerp_amt = cur_val;
		if (store_value_on_reset)
			rt->saved_f = cur_val;



		input[0]->reset(ctx);
		input[1]->reset(ctx);

	}

	ControlParamHandle param;
	float damp_factor = 0.1;
	bool store_value_on_reset = false;	// if true, then parameter value is saved and becomes const until reset again
	// below are computed on init
	uint8_t parameter_type = 0;// 0 = float, 1 = bool
};

struct Blend_Int_Node_RT : public Rt_Vars_Base
{
	int fade_out_i = -1;
	float lerp_amt = 0.0;
	int active_i = 0;
};

// blend by int or enum, can handle 1 crossfade (TODO: arbritary n-blends)
class Blend_Int_Node_CFG : public Node_CFG
{
public:
	using RT_TYPE = Blend_Int_Node_RT;

	DECLARE_NODE_CFG(Blend_Int_Node_CFG);

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);
		
		if (!param.is_valid()) return;

		int val = ctx.get_int(param);

		rt->active_i = get_actual_index(val);
		rt->lerp_amt = 1.0;
		rt->fade_out_i = -1;

		for (int i = 0; i < input.size(); i++)
			input[i]->reset(ctx);
	}

	int get_actual_index(int val_from_param) const {
		if (!uses_jump_table) {
			assert(val_from_param >= 0 && val_from_param <input.size());
			return val_from_param;
		}
		assert(val_from_param >= 0 && val_from_param < jump_table.size());
		int real_idx = jump_table[val_from_param];
		assert(real_idx >= 0 && real_idx < input.size());
		return real_idx;
	}

	float damp_factor = 0.1;
	bool store_value_on_reset = false;
	ControlParamHandle param;
	bool uses_jump_table = false;	// for enums
	InlineVec<unsigned char, 10> jump_table;
};

struct Mirror_Node_RT : Rt_Vars_Base
{
	float lerp_amt = 0.0;
	float saved_f;
	bool saved_boolean = 0;
};

struct Mirror_Node_CFG : public Node_CFG
{
	using RT_TYPE = Mirror_Node_RT;
	DECLARE_NODE_CFG(Mirror_Node_CFG);

	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	virtual void reset(NodeRt_Ctx& ctx) const override
	{
		*get_rt< Mirror_Node_RT>(ctx) = Mirror_Node_RT();
		input[0]->reset(ctx);
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<Mirror_Node_RT>(ctx);
	}

	float damp_time = 0.1;
	ControlParamHandle param;
	bool store_value_on_reset = false;	// if true, then parameter value is saved and becomes const until reset again
	// below are computed on init
	uint8_t parameter_type = 1;// 0 = float, 1 = bool
};

struct NodeRt_Ctx;

struct State;
struct State_Transition
{
	static PropertyInfoList* get_props();
	bool is_a_continue_transition() const { return is_continue_transition; }
	handle<State> transition_state;
	BytecodeExpression script_condition;
	std::string script_uncompilied;		// TODO: save to disk precompilied, compiling is fast though so not a huge deal
	bool is_continue_transition = false;
	float transition_time = 0.2f;
	bool automatic_transition_rule = true;
};


struct State
{
	static PropertyInfoList* get_props();

	Node_CFG* tree = nullptr;	// fixed up at initialization
	uint16_t tree_index = 0;
	bool is_end_state = false;
	bool wait_until_finish = false;
	float state_duration = -1.0;				// when > 0, specifies how long state should be active, then signals a transition end
	InlineVec<uint16_t, 6> transition_idxs;
};

struct Statemachine_Node_RT : Rt_Vars_Base
{
	handle<State> active_state;
	handle<State> fading_out_state;
	State_Transition* active_transition = nullptr;
	Pose* cached_pose_from_transition  = nullptr;

	float blend_duration = 0.0;
	float blend_percentage = 0.0;
};

struct Statemachine_Node_CFG : public Node_CFG
{
	using RT_TYPE = Statemachine_Node_RT;
	DECLARE_NODE_CFG(Statemachine_Node_CFG);

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	handle<State> find_start_state(Statemachine_Node_RT* rt) { return {}; }

	// Inherited via At_Node
	virtual void reset(NodeRt_Ctx& ctx) const override
	{
		*get_rt<Statemachine_Node_RT>(ctx) = Statemachine_Node_RT();
	}

	virtual void construct(NodeRt_Ctx& ctx) const override
	{
		construct_this<Statemachine_Node_RT>(ctx);
	}

	const State* get_state(handle<State> handle) const {
		if (handle.id == -1) return nullptr;
		return &states.at(handle.id);
	}
	handle<State> get_state_handle(const State* state) const {
		return { int((state - states.data()) / sizeof(State)) };
	}

	std::vector<State> states;
	InlineVec<uint16_t, 6> entry_transitions;
	std::vector<State_Transition> transitions;
};

struct BlendSpace2d_RT : Rt_Vars_Base
{
	glm::vec2 character_blend_weights = glm::vec2(0.f);
};

struct BlendSpace2d_CFG : public Node_CFG
{
	using RT_TYPE = BlendSpace2d_RT;
	DECLARE_NODE_CFG(BlendSpace2d_CFG);

	ControlParamHandle xparam;
	ControlParamHandle yparam;

	float weight_damp = 0.01;

	struct GridPoint {
		float x = 0.0;
		float y = 0.0;
		// grid point corresponds to input source node
	};

	// either 5,9,11, or 15 vertex topology
	InlineVec<GridPoint, 5> blend2d_verts;
	bool is_additive_blend_space = false;

	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		*get_rt<RT_TYPE>(ctx) = RT_TYPE();
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
	}
};

struct BlendSpace1d_RT : public Rt_Vars_Base
{
	float weight = 0.0;
};

class BlendSpace1d_CFG : public Node_CFG
{
public:
	using RT_TYPE = BlendSpace1d_RT;
	DECLARE_NODE_CFG(BlendSpace1d_CFG);

	InlineVec<float, 3> blend1d_verts;
	bool is_additive_blend_space = false;

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		*get_rt<RT_TYPE>(ctx) = RT_TYPE();
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
	}
	ControlParamHandle param;
};

extern Factory<std::string, Node_CFG>& get_runtime_node_factory();