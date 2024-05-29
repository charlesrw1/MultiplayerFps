#pragma once

#include "Model.h"
#include "glm/glm.hpp"

#include "Animation/AnimationTreePublic.h"

#include "Framework/ExpressionLang.h"
#include "Framework/MemArena.h"
#include "Framework/InlineVec.h"
#include "Framework/EnumDefReflection.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Factory.h"
#include "Framework/PoolAllocator.h"
#include "Framework/TypeInfo.h"

#include "ControlParams.h"
#include "ControlParamHandle.h"

#include "Animation/SkeletonData.h"


#include <cassert>

class Node_CFG;
class Animation_Tree_CFG;

extern Pool_Allocator g_pose_pool;

struct AgSerializeContext
{
	AgSerializeContext(Animation_Tree_CFG* tree);

	std::unordered_map<Node_CFG*, int> ptr_to_index;
	Animation_Tree_CFG* tree = nullptr;
};


struct Rt_Vars_Base
{
	uint16_t last_update_tick = 0;
};

inline Node_CFG* serialized_nodecfg_ptr_to_ptr(Node_CFG* ptr, Animation_Tree_CFG* cfg) {
	uintptr_t index = (uintptr_t)ptr;	// serialized as indicies
	ASSERT(index < 0xffff || index == -1);	// assume anything larger is a bad pointer error
	return (index == -1) ? nullptr : cfg->all_nodes.at(index);
}

inline Node_CFG* ptr_to_serialized_nodecfg_ptr(Node_CFG* ptr, const AgSerializeContext* ctx) {
	ASSERT(ptr == nullptr || ctx->ptr_to_index.find(ptr) != ctx->ptr_to_index.end());
	uintptr_t index = (ptr) ? ctx->ptr_to_index.find(ptr)->second : -1;
	return (Node_CFG*)index;
}

class Program;
struct Animation_Tree_RT;
struct Animation_Tree_CFG;
struct Node_CFG;
struct Control_Params;
class Language;
struct NodeRt_Ctx
{
	const Model* model = nullptr;

	Animation_Tree_RT* tree = nullptr;
	const ControlParam_CFG* param_cfg = nullptr;
	const Program* script_prog = nullptr;
	uint16_t tick = 0;

	int stack_size = 0;
	script_value_t* stack = nullptr;

	float get_float(ControlParamHandle handle) const {
		return param_cfg->get_float(&tree->vars, handle);
	}
	int get_int(ControlParamHandle handle) const {
		return param_cfg->get_int(&tree->vars, handle);
	}
	bool get_bool(ControlParamHandle handle) const {
		return param_cfg->get_bool(&tree->vars, handle);
	}
	const MSkeleton* get_skeleton() const {
		return model->get_skel();
	}
	uint32_t num_bones() const { return model->get_skel()->get_num_bones(); }
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
	GetPose_Ctx set_automatic_transition(float time) {
		GetPose_Ctx gp = *this;
		gp.automatic_transition_time = time;
		gp.has_auto_transition = true;
		return gp;
	}

	Pose* pose = nullptr;
	float dt = 0.0;
	syncval* sync = nullptr;
	// if > 0, then scale clip by rootmotion
	float rootmotion_scale = -1.0;

	// if true, then clip will return that it is finished when current_time + auto_transition_time >= clip_time
	bool has_auto_transition = false;
	float automatic_transition_time = 0.0;
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

	// serialization helpers
	void add_props(std::vector<PropertyListInstancePair>& props) {
		props.push_back({ Node_CFG::get_props_static(), this });
		props.push_back({ get_props(), this });
	}
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

	static PropertyInfoList* get_props_static();

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

		for (int i = 0; i < input.size(); i++) {
			input[i] = serialized_nodecfg_ptr_to_ptr(input[i], cfg);
		} 
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
		input[0]->reset(ctx);
	}

	ControlParamHandle param;
};

struct Clip_Node_RT : Rt_Vars_Base
{
	glm::vec3 root_pos_first_frame = glm::vec3(0.f);
	float inv_speed_of_anim_root = 1.0;
	float anim_time = 0.0;
	bool stopped_flag = false;
	const AnimationSeq* clip = nullptr;
	int remap_index = -1;
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

	
		rt->clip = ctx.get_skeleton()->find_clip(clip_name, rt->remap_index);

		if (rt->clip) {
			const int root_index = 0;
			rt->root_pos_first_frame = rt->clip->get_keyframe(0, 0, 0.0).pos;
			
			rt->inv_speed_of_anim_root = 1.0 / rt->clip->average_linear_velocity;
		}
	}


	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	virtual void reset(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);
		rt->anim_time = 0.0;
		rt->stopped_flag = false;
	}

	virtual bool is_clip_node() const override {
		return true;
	}
	const AnimationSeq* get_clip(NodeRt_Ctx& ctx) const {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);
		return rt->clip;
	}
	void set_frame_by_interp(NodeRt_Ctx& ctx, float frac) const {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);

		rt->anim_time = get_clip(ctx)->duration * frac;
	}

	rootmotion_setting rm[3] = { rootmotion_setting::keep ,rootmotion_setting::keep, rootmotion_setting::keep };
	std::string clip_name;
	bool loop = true;
	bool allow_sync = false;
	bool can_be_leader = false;
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
	float saved_f = 0.0;
};

struct Mirror_Node_CFG : public Node_CFG
{
	using RT_TYPE = Mirror_Node_RT;
	DECLARE_NO_DEFAULT(Mirror_Node_CFG);
	virtual void initialize(Animation_Tree_CFG* cfg) {
		init_memory_internal(cfg, sizeof(RT_TYPE));
		parameter_type = cfg->params->get_type(param) == control_param_type::float_t ? 0 : 1;
	}

	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	virtual void reset(NodeRt_Ctx& ctx) const override
	{
		auto rt = get_rt< Mirror_Node_RT>(ctx);

		if (parameter_type == 0) {
			rt->saved_f = ctx.get_float(param);
		}
		else {
			rt->saved_f = (float)ctx.get_bool(param);
		}

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


struct Blend_Masked_RT : public Rt_Vars_Base
{
	const BonePoseMask* mask = nullptr;
};

class Blend_Masked_CFG : public Node_CFG
{
public:
	using RT_TYPE = Blend_Masked_RT;
	DECLARE_NODE_CFG(Blend_Masked_CFG);
	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
		auto rt = get_rt<RT_TYPE>(ctx);
		//FIXME
		rt->mask = ctx.get_skeleton()->find_mask(maskname);
	}
	virtual void reset(NodeRt_Ctx& ctx) const override {
		input[0]->reset(ctx);
		input[1]->reset(ctx);
	}

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	bool meshspace_rotation_blend = false;
	int8_t param_type = 0;	// 0 = float,1=bool
	ControlParamHandle param;
	StringName maskname;
};


class LocalToComponent_CFG
{

};

class ComponentToLocal_CFG
{

};

class SolveIkConstraints_CFG
{

};

class PushIKConstraints_CFG
{

};

class PushBoneMask_CFG
{

};

class MotionWarp_CFG
{

};

class BoneModifier_CFG
{

};


extern Factory<std::string, Node_CFG>& get_runtime_node_factory();