#pragma once

#include "Render/Model.h"
#include "glm/glm.hpp"

#include "Animation.h"
#include "Animation/AnimationTreePublic.h"

#include "Framework/MemArena.h"
#include "Framework/InlineVec.h"
#include "Framework/EnumDefReflection.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Factory.h"
#include "Framework/PoolAllocator.h"
#include "Framework/ClassBase.h"
#include "Animation/SkeletonData.h"

#include "Game/SerializePtrHelpers.h"	// for AssetPtr
#include "AssetCompile/AnimationSeqLoader.h"

#include <type_traits>
#include <cassert>

class Node_CFG;
class Animation_Tree_CFG;

extern Pool_Allocator g_pose_pool;

// only accepted graph values
enum class anim_graph_value
{
	bool_t,
	float_t,
	int_t,
	vec3_t,
	quat_t,
};
ENUM_HEADER(anim_graph_value);


inline anim_graph_value core_type_id_to_anim_graph_value(bool* good, core_type_id type)
{
	*good = true;
	switch (type)
	{
	case core_type_id::Bool: return anim_graph_value::bool_t;
	case core_type_id::Int8: return anim_graph_value::int_t;
	case core_type_id::Int16: return anim_graph_value::int_t;
	case core_type_id::Int32: return anim_graph_value::int_t;
	case core_type_id::Float: return anim_graph_value::float_t;
	case core_type_id::Vec3: return anim_graph_value::vec3_t;
	case core_type_id::Quat: return anim_graph_value::quat_t;
	default:
		*good = false;
		return {};
	}
}



CLASS_H(AgSerializeContext,ClassBase)
public:
	void set_tree(Animation_Tree_CFG* tree);


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
	if (index >= 0xffff && index != -1) { // assume anything larger is a bad pointer error
		sys_print("!!! pointer serialized wrong for node\n");
		return nullptr;
	}
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
	NodeRt_Ctx(AnimatorInstance* inst);

	const Model* model = nullptr;
	AnimatorInstance* anim = nullptr;
	uint16_t tick = 0;


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

	// always returns valid sync group, DONT cache this!!
	// only for short term usage
	SyncGroupData& find_sync_group_data(StringName name) {
		return anim->find_or_create_sync_group(name);
	}

	AnimatorInstance::DirectAnimationSlot& get_slot_for_index(int index) {
		return anim->slots.at(index);
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

	// if > 0, then scale clip by rootmotion
	float rootmotion_scale = -1.0;

	// if true, then clip will return that it is finished when current_time + auto_transition_time >= clip_time
	bool has_auto_transition = false;
	float automatic_transition_time = 0.0;
};

CLASS_H(BaseAGNode, ClassBase)
	// called once per Graph asset
	virtual void initialize(Animation_Tree_CFG* cfg) = 0;
	// called once per instance of Graph
	virtual void construct(NodeRt_Ctx& ctx) const {
	}
protected:
	void fixup_ptrs(Animation_Tree_CFG* cfg) {
		const PropertyInfoList* l = get_type().props;
		if (!l)
			return;

		// auto fixup
		for (int i = 0; i < l->count; i++) {
			if (strcmp(l->list[i].custom_type_str, "AgSerializeNodeCfg") == 0) {
				BaseAGNode** nodea = (BaseAGNode**)l->list[i].get_ptr(this);
				*nodea = serialized_nodecfg_ptr_to_ptr(*nodea, cfg);
			}
		}

	}
};

class NodeRt_Ctx;

CLASS_H(ValueNode, BaseAGNode)
	virtual void initialize(Animation_Tree_CFG* cfg) override {}
	template<typename T>
	T get_value(NodeRt_Ctx& ctx) {
		T val{};
		get_value_internal(ctx, type_trait_animgraph<T>::val, &val);
		return val;
	}
private:
	template<typename T>
	struct type_trait_animgraph { };
	template<>
	struct type_trait_animgraph<int> {
		static const anim_graph_value val = anim_graph_value::int_t;
	};
	template<>
	struct type_trait_animgraph<bool> {
		static const anim_graph_value val = anim_graph_value::bool_t;
	};
	template<>
	struct type_trait_animgraph<float> {
		static const anim_graph_value val = anim_graph_value::float_t;
	};
	template<>
	struct type_trait_animgraph<glm::vec3> {
		static const anim_graph_value val = anim_graph_value::vec3_t;
	};
	template<>
	struct type_trait_animgraph<glm::quat> {
		static const anim_graph_value val = anim_graph_value::quat_t;
	};


	virtual void get_value_internal(NodeRt_Ctx& ctx, anim_graph_value expected_type, void* ptr) = 0;
};



CLASS_H(Node_CFG, BaseAGNode)

	bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {
		set_active(ctx, get_rt_base(ctx));
		return get_pose_internal(ctx, pose);
	}
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const = 0;

	virtual void reset(NodeRt_Ctx& ctx) const = 0;
	virtual bool is_clip_node() const { return false; }


	void set_active(NodeRt_Ctx& ctx, Rt_Vars_Base* base) const {
		base->last_update_tick = ctx.tick;
	}
	bool was_active_last_frame(NodeRt_Ctx& ctx) const {
		return get_rt_base(ctx)->last_update_tick == ctx.tick;
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
	uint32_t rt_offset = 0;
private:
	Rt_Vars_Base* get_rt_base(NodeRt_Ctx& ctx) const {
		return ctx.get<Rt_Vars_Base>(rt_offset);
	}
};

template<typename T>
class NodeCFG_Templated : public Node_CFG
{
public:
	using RT_TYPE = T;
	static_assert(std::is_base_of<Rt_Vars_Base, T>::value, "runtime structs must inherit from Rt_Vars_Base");
	virtual void initialize(Animation_Tree_CFG* tree) override {
		init_memory_internal(tree, sizeof(RT_TYPE));
	}
	virtual void construct(NodeRt_Ctx& ctx) const {
		construct_this<RT_TYPE>(ctx);
	}
	RT_TYPE* get_rt(NodeRt_Ctx& ctx)const {
		return ctx.get<RT_TYPE>(rt_offset);
	}
};


#define NODECFG_HEADER(classname, rt_type) \
	CLASS_H_EXPLICIT_SUPER(classname, NodeCFG_Templated<rt_type>, Node_CFG)



struct Clip_Node_RT : Rt_Vars_Base
{
	glm::vec3 root_pos_first_frame = glm::vec3(0.f);
	float inv_speed_of_anim_root = 1.0;
	float anim_time = 0.0;
	bool stopped_flag = false;
	const AnimationSeq* clip = nullptr;
	int remap_index = -1;
};

enum class rootmotion_setting : uint8_t {
	keep,
	remove,
	add_velocity
};
ENUM_HEADER(rootmotion_setting);


NODECFG_HEADER(Clip_Node_CFG, Clip_Node_RT)

	virtual void initialize(Animation_Tree_CFG* tree) override {
		init_memory_internal(tree, sizeof(RT_TYPE));
		hashed_syncgroupname = SyncGroupName.c_str();
	}

	virtual void construct(NodeRt_Ctx& ctx) const {
		RT_TYPE* rt = construct_this<RT_TYPE>(ctx);

		rt->clip = (Clip.ptr) ? Clip.ptr->seq : nullptr;// ctx.get_skeleton()->find_clip(clip_name, rt->remap_index);
		rt->remap_index = -1;
		if (rt->clip) {
			const int root_index = 0;
			rt->root_pos_first_frame = rt->clip->get_keyframe(0, 0, 0.0).pos;
			
			rt->inv_speed_of_anim_root = 1.0 / rt->clip->average_linear_velocity;
		}
	}


	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	static const PropertyInfoList* get_props()
	{
		START_PROPS(Clip_Node_CFG)
			//REG_STDSTRING_CUSTOM_TYPE(clip_name, PROP_DEFAULT, "AG_CLIP_TYPE"),
			REG_ASSET_PTR(Clip,PROP_DEFAULT),

			REG_ENUM(rm[0], PROP_DEFAULT, "rootmotion_setting::keep", rootmotion_setting),
			REG_ENUM(rm[1], PROP_DEFAULT, "rootmotion_setting::keep", rootmotion_setting),
			REG_ENUM(rm[2], PROP_DEFAULT, "rootmotion_setting::keep", rootmotion_setting),

			REG_BOOL(loop, PROP_DEFAULT, "1"),
			REG_FLOAT(speed, PROP_DEFAULT, "1.0,0.1,10"),
			REG_INT(start_frame, PROP_DEFAULT, "0"),

			REG_ENUM(SyncOption, PROP_DEFAULT, "sync_opt::Default", sync_opt),
			REG_STDSTRING(SyncGroupName, PROP_DEFAULT),

		END_PROPS(Clip_Node_CFG)
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = get_rt(ctx);
		rt->anim_time = 0.0;
		rt->stopped_flag = false;
	}

	virtual bool is_clip_node() const override {
		return true;
	}
	const AnimationSeq* get_clip(NodeRt_Ctx& ctx) const {
		RT_TYPE* rt = get_rt(ctx);
		return rt->clip;
	}
	void set_frame_by_interp(NodeRt_Ctx& ctx, float frac) const {
		RT_TYPE* rt = get_rt(ctx);

		rt->anim_time = get_clip(ctx)->duration * frac;
	}

	rootmotion_setting rm[3] = { rootmotion_setting::keep ,rootmotion_setting::keep, rootmotion_setting::keep };
	AssetPtr<AnimationSeqAsset> Clip;
	//std::string clip_name;
	bool loop = true;

	StringName hashed_syncgroupname;
	std::string SyncGroupName;
	sync_opt SyncOption = sync_opt::Default;

	bool has_sync_group() const {
		return !SyncGroupName.empty();
	}

	float speed = 1.0;
	uint16_t start_frame = 0;
};

struct Frame_Evaluate_RT : public Rt_Vars_Base
{
	const AnimationSeq* clip = nullptr;
	int remap_index = -1;
};
NODECFG_HEADER(Frame_Evaluate_CFG, Frame_Evaluate_RT)
	virtual void construct(NodeRt_Ctx& ctx) const {
		RT_TYPE* rt = construct_this<RT_TYPE>(ctx);
	
	
		rt->clip = ctx.get_skeleton()->find_clip(clip_name, rt->remap_index);
	}
	
	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	
	static const PropertyInfoList* get_props()
	{
		START_PROPS(Frame_Evaluate_CFG)
			REG_STDSTRING_CUSTOM_TYPE(clip_name, PROP_DEFAULT, "AG_CLIP_TYPE"),
			REG_CUSTOM_TYPE_HINT(explicit_time, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),
			REG_BOOL(wrap_around_time, PROP_DEFAULT, "0"),
			REG_BOOL(normalized_time, PROP_DEFAULT, "0")
		END_PROPS(Frame_Evaluate_CFG)
	}
	virtual void reset(NodeRt_Ctx& ctx) const override {

	}
	
	ValueNode* explicit_time = nullptr;
	bool wrap_around_time = false;
	bool normalized_time = false;
	std::string clip_name;
};

NODECFG_HEADER(Subtract_Node_CFG, Rt_Vars_Base)

	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		ref->reset(ctx);
		source->reset(ctx);
	}

	static const PropertyInfoList* get_props()
	{
		START_PROPS(Subtract_Node_CFG)
			REG_CUSTOM_TYPE_HINT(ref, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
			REG_CUSTOM_TYPE_HINT(source, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
		END_PROPS(Subtract_Node_CFG)
	}

	Node_CFG* ref = nullptr;
	Node_CFG* source = nullptr;

};

NODECFG_HEADER(Add_Node_CFG, Rt_Vars_Base)

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


// generic blend by bool or blend by float
NODECFG_HEADER(Blend_Node_CFG, Rt_Vars_Base)


	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	static const PropertyInfoList* get_props()
	{
		START_PROPS(Blend_Node_CFG)
			REG_CUSTOM_TYPE_HINT(inp0, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
			REG_CUSTOM_TYPE_HINT(inp1, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
			REG_CUSTOM_TYPE_HINT(alpha, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),

		END_PROPS(Blend_Node_CFG)
	}


	virtual void reset(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = get_rt(ctx);


		inp0->reset(ctx);
		inp1->reset(ctx);
	}

	Node_CFG* inp0 = nullptr;
	Node_CFG* inp1 = nullptr;

	ValueNode* alpha = nullptr;
};

struct Blend_Int_Node_RT : public Rt_Vars_Base
{
	int fade_out_i = -1;
	float lerp_amt = 0.0;
	int active_i = 0;
};

// blend by int or enum, can handle 1 crossfade (TODO: arbritary n-blends)
NODECFG_HEADER(Blend_Int_Node_CFG, Blend_Int_Node_RT)

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	static const PropertyInfoList* get_props()
	{
		START_PROPS(Blend_Int_Node_CFG)

			REG_CUSTOM_TYPE_HINT(param, PROP_SERIALIZE, "AgSerializeNodeCfg", "int"),

		END_PROPS(Blend_Int_Node_CFG)
	}


	virtual void reset(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = get_rt(ctx);
		
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
	bool store_value_on_reset = false;
	ValueNode* param = nullptr;
};

struct Mirror_Node_RT : Rt_Vars_Base
{
	float saved_f = 0.0;
};

NODECFG_HEADER(Mirror_Node_CFG, Mirror_Node_RT)
	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	static const PropertyInfoList* get_props()
	{
		START_PROPS(Mirror_Node_CFG)
			
			REG_BOOL(store_value_on_reset, PROP_DEFAULT, "0"),

			REG_CUSTOM_TYPE_HINT(input, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
			REG_CUSTOM_TYPE_HINT(param, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),

		END_PROPS(Mirror_Node_CFG)
	}

	virtual void reset(NodeRt_Ctx& ctx) const override
	{
		auto rt = get_rt(ctx);
		input->reset(ctx);
		rt->saved_f = param->get_value<float>(ctx);
	}


	Node_CFG* input = nullptr;
	ValueNode* param = nullptr;
	bool store_value_on_reset = false;	// if true, then parameter value is saved and becomes const until reset again
};

struct NodeRt_Ctx;

struct BlendSpace2d_RT : public Rt_Vars_Base
{
	glm::vec2 blend_weights = glm::vec2(0.f);

	// not using vector here since for now these runtime structs never get cleaned up
	const AnimationSeq* all_sequences[9];
	int all_remap_indicies[9];
	BlendSpace2d_RT() {
		for (int i = 0; i < 9; i++) all_remap_indicies[i] = -1;
		for (int i = 0; i < 9; i++)all_sequences[i] = nullptr;
	}
};

#include "Framework/ArrayReflection.h"
NODECFG_HEADER(BlendSpace2d_CFG, BlendSpace2d_RT)
	ValueNode* xparam = nullptr;
	ValueNode* yparam = nullptr;

	float weight_damp = 0.01;

	struct GridPoint {
		float x = 0.0;
		float y = 0.0;
		
		// grid point corresponds to input source node
		std::string animation_name;

		static const PropertyInfoList* get_props() {
			START_PROPS(GridPoint)
				REG_FLOAT(x,PROP_SERIALIZE,""),
				REG_FLOAT(y, PROP_SERIALIZE,""),
				REG_STDSTRING_CUSTOM_TYPE(animation_name, PROP_DEFAULT, "AG_CLIP_TYPE")
			END_PROPS(GridPoint)
		}
	};

	std::vector<uint16_t> indicies;
	std::vector<GridPoint> verticies;
	bool is_additive_blend_space = false;

	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	static const PropertyInfoList* get_props()
	{
		MAKE_VECTORCALLBACK(GridPoint, verticies);
		MAKE_VECTORCALLBACK_ATOM(uint16_t, indicies);

		START_PROPS(BlendSpace2d_CFG)
			REG_STDVECTOR(verticies, PROP_SERIALIZE),
			REG_STDVECTOR(indicies, PROP_SERIALIZE),
			REG_CUSTOM_TYPE_HINT(xparam, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),
			REG_CUSTOM_TYPE_HINT(yparam, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),

			REG_FLOAT(weight_damp, PROP_DEFAULT, "0.01")
		END_PROPS(BlendSpace2d_CFG)
	}
	virtual void reset(NodeRt_Ctx& ctx) const override {
		get_rt(ctx)->blend_weights = glm::vec2(0.5);
	}

};

struct Blend_Masked_RT : public Rt_Vars_Base
{
	const BonePoseMask* mask = nullptr;
};

NODECFG_HEADER(Blend_Masked_CFG, Blend_Masked_RT)
	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
		auto rt = get_rt(ctx);
		//FIXME
		rt->mask = ctx.get_skeleton()->find_mask(maskname);
		if (!rt->mask)
			sys_print("??? blend_masked_cfg has no valid mask\n");
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
			REG_CUSTOM_TYPE_HINT(alpha, PROP_SERIALIZE, "AgSerializeNodeCfg","float"),
		END_PROPS(Blend_Masked_CFG)
	}

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	bool meshspace_rotation_blend = false;
	ValueNode* alpha = nullptr;
	
	Node_CFG* base = nullptr;
	Node_CFG* layer = nullptr;

	StringName maskname;
};

// local/meshspace conversion
// 2 bone ik
// modify transform
// copy transform

NODECFG_HEADER(SavePoseToCache_CFG, Rt_Vars_Base)

virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {
	return false;
}

virtual void reset(NodeRt_Ctx& ctx) const {
	pose->reset(ctx);
}
static const PropertyInfoList* get_props()
{
	START_PROPS(SavePoseToCache_CFG)
		REG_STDSTRING(cache_name, PROP_DEFAULT),
		REG_CUSTOM_TYPE_HINT(pose, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
	END_PROPS(SavePoseToCache_CFG)
}

std::string cache_name;
Node_CFG* pose = nullptr;
};


NODECFG_HEADER(GetCachedPose_CFG, Rt_Vars_Base)

virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const {
	return false;
}

virtual void reset(NodeRt_Ctx& ctx) const {

}
static const PropertyInfoList* get_props()
{
	START_PROPS(GetCachedPose_CFG)
		REG_STDSTRING(cache_name, PROP_DEFAULT),
	END_PROPS(GetCachedPose_CFG)
}

std::string cache_name;
};

NODECFG_HEADER(LocalToMeshspace_CFG, Rt_Vars_Base)

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const;
	virtual void reset(NodeRt_Ctx& ctx) const {
		input->reset(ctx);
	}
	static const PropertyInfoList* get_props()
	{
		START_PROPS(LocalToMeshspace_CFG)
			REG_CUSTOM_TYPE_HINT(input, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
		END_PROPS(LocalToMeshspace_CFG)
	}
	
	Node_CFG* input = nullptr;
};

NODECFG_HEADER(MeshspaceToLocal_CFG, Rt_Vars_Base)

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const;
	
	virtual void reset(NodeRt_Ctx& ctx) const {
		input->reset(ctx);
	}
	
	static const PropertyInfoList* get_props()
	{
		START_PROPS(MeshspaceToLocal_CFG)
			REG_CUSTOM_TYPE_HINT(input, PROP_SERIALIZE, "AgSerializeNodeCfg", "mesh"),
		END_PROPS(MeshspaceToLocal_CFG)
	}
	
	Node_CFG* input = nullptr;
};

struct DirectPlaySlot_RT : public Rt_Vars_Base
{
	Pose* fading_out_pose = nullptr;
};

NODECFG_HEADER(DirectPlaySlot_CFG, DirectPlaySlot_RT)
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const;
	virtual void reset(NodeRt_Ctx& ctx) const {
		input->reset(ctx);
	}
	static const PropertyInfoList* get_props()
	{
		START_PROPS(DirectPlaySlot_CFG)
			REG_BOOL(update_children_when_playing,PROP_DEFAULT, "1"),
			REG_CUSTOM_TYPE_HINT(input, PROP_SERIALIZE, "AgSerializeNodeCfg", "local"),
			REG_INT(slot_index, PROP_SERIALIZE, "")
		END_PROPS(DirectPlaySlot_CFG)
	}
	
	bool update_children_when_playing = true;
	int slot_index = -1;

	Node_CFG* input = nullptr;
};

struct ModifyBone_RT : public Rt_Vars_Base
{
	int bone_index = -1;
};

NODECFG_HEADER(ModifyBone_CFG, ModifyBone_RT)

	void construct(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = construct_this<RT_TYPE>(ctx);
		rt->bone_index = ctx.get_skeleton()->get_bone_index(bone_name.c_str());
	}
	
	bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override ;
	void reset(NodeRt_Ctx& ctx) const override {
		input->reset(ctx);
	}
	static const PropertyInfoList* get_props()
	{
		START_PROPS(ModifyBone_CFG)
			REG_STDSTRING_CUSTOM_TYPE(bone_name, PROP_DEFAULT, "AgBoneFinder"),
			REG_CUSTOM_TYPE_HINT(alpha, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),
			REG_CUSTOM_TYPE_HINT(input, PROP_SERIALIZE, "AgSerializeNodeCfg", "mesh"),
			REG_CUSTOM_TYPE_HINT(position, PROP_SERIALIZE, "AgSerializeNodeCfg", "vec3"),
			REG_CUSTOM_TYPE_HINT(rotation, PROP_SERIALIZE, "AgSerializeNodeCfg", "quat"),
			REG_BOOL(apply_rotation,PROP_DEFAULT,"0"),
			REG_BOOL(apply_rotation_additive, PROP_DEFAULT,"0"),
			REG_BOOL(apply_rotation_meshspace, PROP_DEFAULT, "0"),
			REG_BOOL(apply_position, PROP_DEFAULT, "0"),
			REG_BOOL(apply_position_additive, PROP_DEFAULT, "0"),
			REG_BOOL(apply_position_meshspace, PROP_DEFAULT, "0"),
		END_PROPS(ModifyBone_CFG)
	}
	
	
	Node_CFG* input = nullptr;
	ValueNode* alpha = nullptr;
	ValueNode* position = nullptr;
	ValueNode* rotation = nullptr;
	std::string bone_name;
	bool apply_rotation = false;
	bool apply_position = false;
	bool apply_rotation_meshspace = false;
	bool apply_position_meshspace = false;
	bool apply_rotation_additive = false;
	bool apply_position_additive = false;
};


struct TwoBoneIK_RT : public Rt_Vars_Base
{
	int bone_index = -1;
	int target_bone_index = -1;
};

NODECFG_HEADER(TwoBoneIK_CFG, TwoBoneIK_RT)

	bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const;
	void construct(NodeRt_Ctx& ctx) const override {
		auto rt = construct_this<RT_TYPE>(ctx);
		rt->bone_index = ctx.get_skeleton()->get_bone_index(bone_name.c_str());
		rt->target_bone_index = ctx.get_skeleton()->get_bone_index(other_bone.c_str());
	
		if (!rt->bone_index)
			sys_print("??? no bone index for twoboneik node\n");
		if (ik_in_bone_space && !rt->target_bone_index)
			sys_print("??? no target bone when its required for twoboneik\n");
	}
	virtual void reset(NodeRt_Ctx& ctx) const {
		input->reset(ctx);
	}
	static const PropertyInfoList* get_props()
	{
		START_PROPS(TwoBoneIK_CFG)
			REG_CUSTOM_TYPE_HINT(input, PROP_SERIALIZE, "AgSerializeNodeCfg", "mesh"),
			REG_CUSTOM_TYPE_HINT(alpha, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),
			REG_CUSTOM_TYPE_HINT(position, PROP_SERIALIZE, "AgSerializeNodeCfg", "vec3"),
			REG_STDSTRING_CUSTOM_TYPE(bone_name, PROP_DEFAULT, "AgBoneFinder"),
			REG_BOOL(ik_in_bone_space, PROP_DEFAULT, "0"),
			REG_STDSTRING_CUSTOM_TYPE(other_bone, PROP_DEFAULT, "AgBoneFinder"),
			REG_BOOL(take_rotation_of_other_bone, PROP_DEFAULT, "0")
		END_PROPS(TwoBoneIK_CFG)
	}
	
	
	Node_CFG* input = nullptr;
	ValueNode* alpha = nullptr;
	ValueNode* position = nullptr;
	std::string bone_name;
	
	bool take_rotation_of_other_bone = false;
	bool ik_in_bone_space = false;
	std::string other_bone;
};

struct CopyBone_RT : public Rt_Vars_Base
{
	int src_bone_index = -1;
	int target_bone_index = -1;

};

NODECFG_HEADER(CopyBone_CFG, CopyBone_RT)

	bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const;
	void construct(NodeRt_Ctx& ctx) const override {
		auto rt = construct_this<RT_TYPE>(ctx);
		rt->src_bone_index = ctx.get_skeleton()->get_bone_index(src_bone.c_str());
		rt->target_bone_index = ctx.get_skeleton()->get_bone_index(dest_bone.c_str());
	
		if (rt->src_bone_index==-1||rt->target_bone_index==-1)
			sys_print("??? no bone index for copybone node\n");
	}
	virtual void reset(NodeRt_Ctx& ctx) const {
		input->reset(ctx);
	}

	
	static const PropertyInfoList* get_props()
	{
		START_PROPS(CopyBone_CFG)
			REG_CUSTOM_TYPE_HINT(input, PROP_SERIALIZE, "AgSerializeNodeCfg", "mesh"),
			REG_CUSTOM_TYPE_HINT(alpha, PROP_SERIALIZE, "AgSerializeNodeCfg", "float"),
			REG_STDSTRING_CUSTOM_TYPE(src_bone, PROP_DEFAULT, "AgBoneFinder"),
			REG_STDSTRING_CUSTOM_TYPE(dest_bone, PROP_DEFAULT, "AgBoneFinder"),
			REG_BOOL(copy_position,PROP_DEFAULT, "0"),
			REG_BOOL(copy_rotation, PROP_DEFAULT, "0"),
			REG_BOOL(copy_bonespace, PROP_DEFAULT, "0"),

		END_PROPS(CopyBone_CFG)
	}
	
	
	Node_CFG* input = nullptr;
	ValueNode* alpha = nullptr;
	
	bool copy_bonespace = false;
	bool copy_position = false;
	bool copy_rotation = false;
	
	std::string src_bone;
	std::string dest_bone;

};

CLASS_H(CurveNode, ValueNode)
	virtual void get_value_internal(NodeRt_Ctx& ctx, anim_graph_value type, void* ptr) override {
		if(type == anim_graph_value::float_t)
			*(float*)ptr = 0.f;
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(CurveNode) REG_STDSTRING(curve_name, PROP_DEFAULT) END_PROPS(CurveNode)
	}
	std::string curve_name;
};

CLASS_H(VectorConstant, ValueNode)
	virtual void get_value_internal(NodeRt_Ctx& ctx, anim_graph_value type, void* ptr) override {
		if(type == anim_graph_value::vec3_t)
			*(glm::vec3*)ptr = vector;
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(VectorConstant)
			REG_VEC3(vector, PROP_DEFAULT, "")
		END_PROPS(VectorConstant)
	}
	glm::vec3 vector = {};
};


CLASS_H(VariableNode, ValueNode)
	static const PropertyInfoList* get_props() {
		START_PROPS(VariableNode) 
			REG_STDSTRING(var_name, PROP_SERIALIZE, ""),
			REG_FLOAT(scale, PROP_DEFAULT, "1"),
			REG_FLOAT(bias, PROP_DEFAULT, "0"),
			REG_BOOL(apply_clamp, PROP_DEFAULT, "0"),
			REG_FLOAT(min_bounds, PROP_DEFAULT, "0"),
			REG_FLOAT(max_bounds, PROP_DEFAULT, "1")

		END_PROPS(VariableNode)
	}
	virtual void initialize(Animation_Tree_CFG* cfg) override {
		if (!var_name.empty())
			sys_print("??? invalid handle for variable node on initialization\n");
		else {
			pi = cfg->find_animator_instance_variable(var_name);
			
			if (!pi)
				sys_print("??? variable wasnt linked to native class\n");
			else {
				bool good = false;
				var_type = core_type_id_to_anim_graph_value(&good, pi->type);
				if (!good) {
					pi = nullptr;
					sys_print("??? variable type wasn't found right\n");
				}
			}
		}
	}
	virtual void get_value_internal(NodeRt_Ctx& ctx, anim_graph_value type, void* ptr) override {
		if (!pi)
			return;
		if (type == anim_graph_value::float_t) {
			float f = 0.0;
			if (var_type == anim_graph_value::float_t)
				f = pi->get_float(ctx.anim);
			else if (var_type == anim_graph_value::bool_t)
				f = (float)(bool)pi->get_int(ctx.anim);
			f = f * scale + bias;
			if (apply_clamp)
				f = glm::min(glm::max(f, min_bounds), max_bounds);
			*(float*)ptr = f;
		}
		else if (type == anim_graph_value::bool_t && var_type == anim_graph_value::bool_t) {
			*(bool*)ptr = pi->get_int(ctx.anim);
		}
		else if(type == anim_graph_value::int_t && var_type == anim_graph_value::int_t)
			*(int*)ptr = pi->get_int(ctx.anim);
		else if (type == anim_graph_value::quat_t && var_type == anim_graph_value::quat_t)
			*(glm::quat*)ptr = *(glm::quat*)pi->get_ptr(ctx.anim);
		else if (type == anim_graph_value::vec3_t && var_type == anim_graph_value::vec3_t)
			*(glm::vec3*)ptr = *(glm::vec3*)pi->get_ptr(ctx.anim);

		// if it failed, ptr is set already to T{} by default, should have already caught the error before running
	}

	float scale = 1.0;
	float bias = 0.0;
	bool apply_clamp = false;
	float min_bounds = 0.0;
	float max_bounds = 1.0;

	anim_graph_value var_type = {};
	std::string var_name;
	const PropertyInfo* pi = nullptr;
};

CLASS_H(RotationConstant, ValueNode)
	virtual void get_value_internal(NodeRt_Ctx& ctx, anim_graph_value type, void* ptr) override {
		*(glm::quat*)ptr = rotation;
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(RotationConstant)
			REG_QUAT(rotation, PROP_DEFAULT, "") 
		END_PROPS(RotationConstant)
	}
	glm::quat rotation{};
};
CLASS_H(FloatConstant, ValueNode)

	virtual void get_value_internal(NodeRt_Ctx& ctx, anim_graph_value type, void* ptr) override {
		*(float*)ptr = f;
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(FloatConstant) REG_FLOAT(f, PROP_DEFAULT, "") END_PROPS(FloatConstant)
	}
	float f = 0.0;
};

CLASS_H(BoolConstant, ValueNode)
	virtual void get_value_internal(NodeRt_Ctx& ctx, anim_graph_value type, void* ptr) override {
	if (type == anim_graph_value::bool_t)
		*(bool*)ptr = (bool)b;
	else if(type == anim_graph_value::float_t)
		*(float*)ptr = (float)b;
	}
	bool b = false;
};
