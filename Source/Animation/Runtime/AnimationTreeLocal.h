#pragma once

#include "Model.h"
#include "glm/glm.hpp"

#include "Animation.h"
#include "Animation/AnimationTreePublic.h"
#include "Framework/ExpressionLang.h"
#include "Framework/MemArena.h"
#include "Framework/InlineVec.h"
#include "Framework/EnumDefReflection.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Factory.h"
#include "Framework/PoolAllocator.h"
#include "Framework/TypeInfo.h"
#include "Framework/ClassBase.h"
#include "Animation/SkeletonData.h"


#include <cassert>

class Node_CFG;
class Animation_Tree_CFG;

extern Pool_Allocator g_pose_pool;

struct AgSerializeContext
{
	AgSerializeContext(Animation_Tree_CFG* tree);

	std::unordered_map<BaseAGNode*, int> ptr_to_index;
	Animation_Tree_CFG* tree = nullptr;
};


struct Rt_Vars_Base
{
	uint16_t last_update_tick = 0;
};

class BaseAGNode;
inline BaseAGNode* serialized_nodecfg_ptr_to_ptr(BaseAGNode* ptr, Animation_Tree_CFG* cfg) {
	uintptr_t index = (uintptr_t)ptr;	// serialized as indicies
	ASSERT(index < 0xffff || index == -1);	// assume anything larger is a bad pointer error
	return (index == -1) ? nullptr : cfg->get_node(index);
}

inline BaseAGNode* ptr_to_serialized_nodecfg_ptr(BaseAGNode* ptr, const AgSerializeContext* ctx) {
	ASSERT(ptr == nullptr || ctx->ptr_to_index.find(ptr) != ctx->ptr_to_index.end());
	uintptr_t index = (ptr) ? ctx->ptr_to_index.find(ptr)->second : -1;
	return (BaseAGNode*)index;
}

struct Animation_Tree_CFG;
struct Node_CFG;
class NodeRt_Ctx
{
public:
	NodeRt_Ctx(AnimatorInstance* inst, script_value_t* stack, int stack_size);

	const Model* model = nullptr;
	AnimatorInstance* anim = nullptr;
	const Script* script = nullptr;
	uint16_t tick = 0;

	ScriptInstance& get_script_inst() { return anim->get_script_inst(); }

	int stack_size = 0;
	script_value_t* stack = nullptr;

	const MSkeleton* get_skeleton() const {
		return model->get_skel();
	}
	uint32_t num_bones() const { return model->get_skel()->get_num_bones(); }

	template<typename T>
	T* get(uint32_t offset) {
		ASSERT(offset + sizeof(T) <= anim->data.size());
		return (T*)(anim->data.data() + offset);
	}
	template<typename T>
	T* construct_rt(uint32_t ofs) {
		T* ptr = get<T>(ofs);
		ptr = new(ptr)(T);
		return ptr;
	}
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

class BaseAGNode : public ClassBase
{
public:
	CLASS_HEADER();

	virtual void initialize(Animation_Tree_CFG* cfg) = 0;
	virtual void construct(NodeRt_Ctx& ctx) const {
	}
protected:
	void fixup_ptrs(Animation_Tree_CFG* cfg) {
		const PropertyInfoList* l = get_type().props;
		if (!l)
			return;

		// auto fixup
		for (int i = 0; i < l->count; i++) {
			if (strcmp(l->list[i].name, "AgSerializeNodeCfg") == 0) {
				BaseAGNode** nodea = (BaseAGNode**)l->list[i].get_ptr(this);
				*nodea = serialized_nodecfg_ptr_to_ptr(*nodea, cfg);
			}
		}

	}
	
};

using AnimGraphVariable = handle<ScriptVariable>;

// only accepted graph values
extern AutoEnumDef anim_graph_value_def;
enum class anim_graph_value
{
	bool_t,
	float_t,
	int_t,
	vec3_t,
	quat_t,
};

class NodeRt_Ctx;
class ValueNode : public BaseAGNode
{
public:
	CLASS_HEADER();

	template<typename T>
	T get_value(NodeRt_Ctx& ctx) {
		T val{};
		get_value_internal(ctx, &val);
		return val;
	}
	virtual anim_graph_value get_value_type() const = 0;
private:
	virtual void get_value_internal(NodeRt_Ctx& ctx, void* ptr) = 0;
};



class Node_CFG : public BaseAGNode
{
public:
	CLASS_HEADER();



	bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {
		set_active(ctx, get_rt<Rt_Vars_Base>(ctx));
		return get_pose_internal(ctx, pose);
	}
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const = 0;

	virtual void reset(NodeRt_Ctx& ctx) const = 0;
	virtual bool is_clip_node() const { return false; }


	void set_active(NodeRt_Ctx& ctx, Rt_Vars_Base* base) const {
		base->last_update_tick = ctx.tick;
	}
	bool was_active_last_frame(NodeRt_Ctx& ctx) const {
		return get_rt<Rt_Vars_Base>(ctx)->last_update_tick == ctx.tick;
	}

	template<typename T>
	T* get_rt(NodeRt_Ctx& ctx) const {
		return ctx.get<T>(rt_offset);
	}
protected:
	template<typename T>
	T* construct_this(NodeRt_Ctx& ctx) const {
		return ctx.construct_rt<T>(rt_offset);
	}

	void init_memory_internal(Animation_Tree_CFG* cfg, uint32_t rt_size) {
		rt_offset = cfg->get_data_used();
		ASSERT(rt_size >= sizeof(Rt_Vars_Base));

		cfg->add_data_used(rt_size);

		fixup_ptrs(cfg);
	}

private:
	uint32_t rt_offset = 0;
};

#define DECLARE_NODE_CFG(TYPE_NAME) \
CLASS_HEADER(); \
virtual void initialize(Animation_Tree_CFG* tree) override { \
	init_memory_internal(tree, sizeof(RT_TYPE)); \
} \
TYPE_NAME() {}


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

		bool ret = input->get_pose(ctx, pose.set_sync(&sv));

		rt->normalized_frame = sv.normalized_frame;

		return ret;
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		Sync_Node_RT* rt = get_rt<Sync_Node_RT>(ctx);
		rt->normalized_frame = 0.0;
		input->reset(ctx);
	}
	static const PropertyInfoList* get_props()
	{
		START_PROPS(Sync_Node_CFG)
			REG_CUSTOM_TYPE_HINT(input, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
		END_PROPS(Sync_Node_CFG)
	}

	Node_CFG* input = nullptr;
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

	static PropertyInfoList* get_props()
	{
		START_PROPS(Clip_Node_CFG)
			REG_ENUM(rm[0], PROP_DEFAULT, "rm::keep", rootmotion_setting_def.id),
			REG_ENUM(rm[1], PROP_DEFAULT, "rm::keep", rootmotion_setting_def.id),
			REG_ENUM(rm[2], PROP_DEFAULT, "rm::keep", rootmotion_setting_def.id),

			REG_BOOL(loop, PROP_DEFAULT, "1"),
			REG_FLOAT(speed, PROP_DEFAULT, "1.0,0.1,10"),
			REG_INT(start_frame, PROP_DEFAULT, "0"),
			REG_BOOL(allow_sync, PROP_DEFAULT, "0"),
			REG_BOOL(can_be_leader, PROP_DEFAULT, "0"),

			REG_STDSTRING_CUSTOM_TYPE(clip_name, PROP_DEFAULT, "AG_CLIP_TYPE")

		END_PROPS(Clip_Node_CFG)
	}

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
		ref->reset(ctx);
		source->reset(ctx);
	}
	static PropertyInfoList* get_props() { return nullptr; }

	Node_CFG* ref = nullptr;
	Node_CFG* source = nullptr;

};

struct Add_Node_CFG : public Node_CFG
{
	using RT_TYPE = Rt_Vars_Base;
	DECLARE_NODE_CFG(Add_Node_CFG);

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	static const PropertyInfoList* get_props()
	{
		START_PROPS(Add_Node_CFG)
			REG_CUSTOM_TYPE_HINT(diff, PROP_SERIALIZE, "AgSerializeNodeCfg","local"),
			REG_CUSTOM_TYPE_HINT(base, PROP_SERIALIZE, "AgSerializeNodeCfg","local"),
			REG_CUSTOM_TYPE_HINT(param, PROP_SERIALIZE, "AgSerializeNodeCfg","float"),
		END_PROPS(Add_Node_CFG)
	}
	virtual void reset(NodeRt_Ctx& ctx) const override {
		diff->reset(ctx);
		base->reset(ctx);
	}

	Node_CFG* diff = nullptr;
	Node_CFG* base = nullptr;
	ValueNode* param = nullptr;
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
	CLASS_HEADER();

	virtual void initialize(Animation_Tree_CFG* tree) override {
		init_memory_internal(tree, sizeof(RT_TYPE)); 
	} 

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	static const PropertyInfoList* get_props()
	{
		START_PROPS(Blend_Node_CFG)
			REG_FLOAT(damp_factor, PROP_DEFAULT, "0.1"),
			REG_BOOL(store_value_on_reset, PROP_DEFAULT, "0"),


			REG_CUSTOM_TYPE_HINT(inp0, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
			REG_CUSTOM_TYPE_HINT(inp1, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
			REG_CUSTOM_TYPE_HINT(param, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),

		END_PROPS(Blend_Node_CFG)
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);

		float cur_val = 0.0;
		cur_val = param->get_value<float>(ctx);

		rt->lerp_amt = cur_val;
		if (store_value_on_reset)
			rt->saved_f = cur_val;


		inp0->reset(ctx);
		inp1->reset(ctx);
	}

	Node_CFG* inp0 = nullptr;
	Node_CFG* inp1 = nullptr;

	ValueNode* param = nullptr;

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

	static const PropertyInfoList* get_props()
	{
		START_PROPS(Blend_Int_Node_CFG)

			REG_CUSTOM_TYPE_HINT(param, PROP_SERIALIZE, "AgSerializeNodeCfg", "int"),

		END_PROPS(Blend_Int_Node_CFG)
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);
		
		int val = param->get_value<int>(ctx);

		rt->active_i = get_actual_index(val);
		rt->lerp_amt = 1.0;
		rt->fade_out_i = -1;

		for (int i = 0; i < input.size(); i++)
			input[i]->reset(ctx);
	}

	int get_actual_index(int val_from_param) const {
		if (val_from_param < 0 || val_from_param >= input.size()) val_from_param = 0;
		return val_from_param;
	}

	InlineVec<Node_CFG*, 2> input;
	float damp_factor = 0.1;
	bool store_value_on_reset = false;
	ValueNode* param = nullptr;
};

struct Mirror_Node_RT : Rt_Vars_Base
{
	float saved_f = 0.0;
};

struct Mirror_Node_CFG : public Node_CFG
{
	using RT_TYPE = Mirror_Node_RT;
	CLASS_HEADER();
	virtual void initialize(Animation_Tree_CFG* cfg) {
		init_memory_internal(cfg, sizeof(RT_TYPE));
	}

	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	static const PropertyInfoList* get_props()
	{
		START_PROPS(Mirror_Node_CFG)
			REG_FLOAT(damp_time, PROP_DEFAULT, "0.1"),

			REG_BOOL(store_value_on_reset, PROP_DEFAULT, "0"),

			REG_CUSTOM_TYPE_HINT(input, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
			REG_CUSTOM_TYPE_HINT(param, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),

		END_PROPS(Mirror_Node_CFG)
	}

	virtual void reset(NodeRt_Ctx& ctx) const override
	{
		auto rt = get_rt< Mirror_Node_RT>(ctx);
		rt->saved_f = param->get_value<float>(ctx);
		input->reset(ctx);
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<Mirror_Node_RT>(ctx);
	}

	float damp_time = 0.1;
	Node_CFG* input = nullptr;
	ValueNode* param = nullptr;
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

	ValueNode* xparam = nullptr;
	ValueNode* yparam = nullptr;

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
	static const PropertyInfoList* get_props()
	{
		START_PROPS(BlendSpace2d_CFG)

			REG_FLOAT(weight_damp, PROP_DEFAULT, "0.01"),

			REG_CUSTOM_TYPE_HINT(xparam, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),
			REG_CUSTOM_TYPE_HINT(yparam, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),

		END_PROPS(BlendSpace2d_CFG)
	}
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
	static const PropertyInfoList* get_props()
	{
		START_PROPS(BlendSpace1d_CFG)
			REG_CUSTOM_TYPE_HINT(param, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),
			REG_BOOL(is_additive_blend_space, PROP_DEFAULT, "0"),
			END_PROPS(BlendSpace1d_CFG)
	}
	virtual void reset(NodeRt_Ctx& ctx) const override {
		*get_rt<RT_TYPE>(ctx) = RT_TYPE();
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
	}
	ValueNode* param = nullptr;
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
		base->reset(ctx);
		layer->reset(ctx);
	}

	static const PropertyInfoList* get_props()
	{
		START_PROPS(Blend_Masked_CFG)
			REG_BOOL(meshspace_rotation_blend, PROP_DEFAULT, "0"),
			REG_INT(maskname, PROP_SERIALIZE, "0"),

			REG_CUSTOM_TYPE_HINT(base, PROP_SERIALIZE, "AgSerializeNodeCfg","local"),
			REG_CUSTOM_TYPE_HINT(layer, PROP_SERIALIZE, "AgSerializeNodeCfg","local"),
			REG_CUSTOM_TYPE_HINT(param, PROP_SERIALIZE, "AgSerializeNodeCfg","float"),
		END_PROPS(Blend_Masked_CFG)
	}

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	bool meshspace_rotation_blend = false;
	ValueNode* param = nullptr;
	Node_CFG* base = nullptr;
	Node_CFG* layer = nullptr;

	StringName maskname;
};

// local/meshspace conversion
// 2 bone ik
// modify transform
// copy transform

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


#define VALUENODE_HEADER(type) virtual anim_graph_value get_value_type() const override { return anim_graph_value::type; } \
CLASS_HEADER();

class CurveNode : public ValueNode
{
public:
	VALUENODE_HEADER(float_t);
	virtual void get_value_internal(NodeRt_Ctx& ctx, void* ptr) override {
		*(float*)ptr = 0.f;
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(CurveNode) REG_STDSTRING(curve_name, PROP_DEFAULT) END_PROPS(CurveNode)
	}
	std::string curve_name;
};
class VectorConstant : public ValueNode
{
public:
	VALUENODE_HEADER(vec3_t);
	virtual void get_value_internal(NodeRt_Ctx& ctx, void* ptr) override {
		*(glm::vec3*)ptr = v;
	}
	static const PropertyInfoList* get_props() {
		return nullptr;
	}
	glm::vec3 v;
};
class VariableNode : public ValueNode
{
public:
	CLASS_HEADER();

	static const PropertyInfoList* get_props() {
		START_PROPS(VariableNode) REG_INT(handle, PROP_DEFAULT, "") END_PROPS(VariableNode)
	}
	AnimGraphVariable handle;
};

class RotationConstant : public ValueNode
{
public:
	VALUENODE_HEADER(quat_t);
	virtual void get_value_internal(NodeRt_Ctx& ctx, void* ptr) override {
		*(glm::quat*)ptr = rotation;
	}
	glm::quat rotation;
};
class FloatConstant : public ValueNode
{
public:
	VALUENODE_HEADER(float_t);
	virtual void get_value_internal(NodeRt_Ctx& ctx, void* ptr) override {
		*(float*)ptr = f;
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(FloatConstant) REG_FLOAT(f, PROP_DEFAULT, "") END_PROPS(FloatConstant)
	}
	float f = 0.0;
};
class BoolAsFloatConstant : public ValueNode
{
public:
	VALUENODE_HEADER(float_t);
	virtual void get_value_internal(NodeRt_Ctx& ctx, void* ptr) override {

	}
	bool b = false;
};
