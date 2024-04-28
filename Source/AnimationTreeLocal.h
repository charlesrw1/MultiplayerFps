#pragma once

#include "Model.h"
#include "ExpressionLang.h"
#include "glm/glm.hpp"
#include "MemArena.h"

#include "AnimationTreePublic.h"
#include "InlineVec.h"

#include "EnumDefReflection.h"
#include "ReflectionProp.h"

#include <cassert>

// to add new node:
// add enum value and edit autoenumdef
// make class with overloaded functions ( incl. get_props() which can be null )
//		remember that Node_CFG constructor has to take in Rt_Vars_Base at minimum, runtime vars inherit from that
// add in impl macro at top of animationtreelocal.cpp to define some metadata

// Modify AutoEnumDef when changing enum!
extern AutoEnumDef animnode_type_def;
enum class animnode_type
{
	source,
	statemachine,

	mask,

	blend,
	blend_by_int,
	blend2d,
	blend1d,
	add,
	subtract,
	aimoffset,

	mirror,
	play_speed,
	rootmotion_speed,
	sync,

	state,

	// used in editor
	root,
	start_statemachine,

	COUNT
};

struct NodeRt_Ctx;
struct ScriptExpression
{
	BytecodeExpression compilied;
	std::string script_str;

	bool evaluate(NodeRt_Ctx& rt) const;
};

struct State;
struct State_Transition
{
	static PropertyInfoList* get_props();

	handle<State> transition_state;
	ScriptExpression script;
	float transition_time = 0.1f;
};

struct Rt_Vars_Base
{
	uint16_t last_update_tick = 0;
};


struct Animation_Tree_RT;
struct Animation_Tree_CFG;
struct Node_CFG;
struct Control_Params;
struct NodeRt_Ctx
{
	const Model* model = nullptr;
	const Animation_Set_New* set = nullptr;
	Animation_Tree_RT* tree = nullptr;
	ScriptVars_RT* vars = nullptr;
	uint16_t tick = 0;

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
	Node_CFG(Animation_Tree_CFG* cfg, uint32_t rt_size) {
		rt_offset = cfg->data_used;
		ASSERT(rt_size >= sizeof(Rt_Vars_Base));
		cfg->data_used += rt_size;

		memset(input.inline_, 0, sizeof(Node_CFG*) * 2);
	}

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
	virtual void write_to_dict(Animation_Tree_CFG* tree, DictWriter& out) {}
	virtual void read_from_dict(Animation_Tree_CFG* tree, DictParser& in) {}
	virtual PropertyInfoList* get_props() = 0;
	virtual animnode_type get_type() = 0;

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
	handle<Parameter> param;	// all nodes have a default parameter for convenience
protected:
	template<typename T>
	T* construct_this(NodeRt_Ctx& ctx) const {
		return ctx.tree->construct_rt<T>(rt_offset);
	}
private:
	uint32_t rt_offset = 0;
};

struct State
{
	static PropertyInfoList* get_props();

	string name;
	Node_CFG* tree = nullptr;
	vector<State_Transition> transitions;
	float time_left = 0.0;

	float state_duration = -1.0;				// when > 0, specifies how long state should be active, then signals a transition end
	bool transition_wait_on_end = false;		// when the state finishes, wait until a valid transition instead of continuing/ending
	handle<State> next_state;					// continue state

	handle<State> get_next_state(NodeRt_Ctx& ctx) const;
};

#define DECLARE_ANIMNODE_CREATOR(TYPE_NAME, ENUM_TYPE) static Node_CFG* create(Animation_Tree_CFG* cfg) { \
TYPE_NAME* clip = (TYPE_NAME*)cfg->arena.alloc_bottom(sizeof(TYPE_NAME)); \
clip = new(clip)TYPE_NAME(cfg); \
return clip; \
}  \
static const animnode_type CONST_TYPE_ENUM = ENUM_TYPE; \
virtual PropertyInfoList* get_props() override;  \
virtual animnode_type get_type() override { return ENUM_TYPE; }

// playback speed *= param / (speed of clip's root motion)
struct Scale_By_Rootmotion_CFG : public Node_CFG
{
	DECLARE_ANIMNODE_CREATOR(Scale_By_Rootmotion_CFG, animnode_type::rootmotion_speed);

	Scale_By_Rootmotion_CFG(Animation_Tree_CFG* cfg) : Node_CFG(cfg, sizeof(Rt_Vars_Base)) { input.count = 1; }

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override
	{
		float rm = ctx.vars->get(param).fval;
		bool ret = input[0]->get_pose(ctx, pose.set_rootmotion(rm));
		return ret;
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
	}
};

struct Sync_Node_RT : Rt_Vars_Base
{
	float normalized_frame = 0.0;
};

struct Sync_Node_CFG : public Node_CFG
{
	Sync_Node_CFG(Animation_Tree_CFG* cfg)
		: Node_CFG(cfg, sizeof(Sync_Node_RT)) {
		input.count = 1;
	}

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

	DECLARE_ANIMNODE_CREATOR(Sync_Node_CFG, animnode_type::sync)
};

struct Clip_Node_RT : Rt_Vars_Base
{
	glm::vec3 root_pos_first_frame = glm::vec3(0.f);
	float inv_speed_of_anim_root = 1.0;
	float frame = 0.0;
	uint32_t clip_index = 0;
	uint32_t set_idx = 0;
	uint32_t skel_idx = 0;
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


	Clip_Node_CFG(Animation_Tree_CFG* cfg)
		: Node_CFG(cfg, sizeof(RT_TYPE)) {
		input.count = 0;
	}


	DECLARE_ANIMNODE_CREATOR(Clip_Node_CFG, animnode_type::source)

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
	bool can_be_leader = false;
	float speed = 1.0;
	uint16_t start_frame = 0;
};

struct Subtract_Node_CFG : public Node_CFG
{
	DECLARE_ANIMNODE_CREATOR(Subtract_Node_CFG, animnode_type::subtract)


	Subtract_Node_CFG(Animation_Tree_CFG* cfg) : Node_CFG(cfg, sizeof(Rt_Vars_Base)) {
		input.count = 2;
	}
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
	Add_Node_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(Rt_Vars_Base)) {
		input.count = 2;
	}

	DECLARE_ANIMNODE_CREATOR(Add_Node_CFG,animnode_type::add)


	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		input[DIFF]->reset(ctx);
		input[BASE]->reset(ctx);
	}

	enum {
		DIFF,
		BASE
	};
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

	DECLARE_ANIMNODE_CREATOR(Blend_Node_CFG, animnode_type::blend)


	Blend_Node_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(RT_TYPE)) {
		input.count = 2;
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
				cur_val = ctx.vars->get(param).fval;
			else
				cur_val =  (float)ctx.vars->get(param).ival;
		}
		rt->lerp_amt = cur_val;
		if (store_value_on_reset)
			rt->saved_f = cur_val;



		input[0]->reset(ctx);
		input[1]->reset(ctx);

	}

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

	DECLARE_ANIMNODE_CREATOR(Blend_Int_Node_CFG, animnode_type::blend_by_int)

	Blend_Int_Node_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(RT_TYPE)) {
		input.count = 1;
	}

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		RT_TYPE* rt = get_rt<RT_TYPE>(ctx);
		
		if (!param.is_valid()) return;

		int val = ctx.vars->get(param).ival;

		rt->active_i = get_actual_index(val);
		rt->lerp_amt = 1.0;
		rt->fade_out_i = -1;

		for (int i = 0; i < input.count; i++)
			input[i]->reset(ctx);
	}

	int get_actual_index(int val_from_param) const {
		if (!uses_jump_table) {
			assert(val_from_param >= 0 && val_from_param < input.count);
			return val_from_param;
		}
		assert(val_from_param >= 0 && val_from_param < jump_table.count);
		int real_idx = jump_table[val_from_param];
		assert(real_idx >= 0 && real_idx < input.count);
		return real_idx;
	}

	float damp_factor = 0.1;
	bool store_value_on_reset = false;

	bool uses_jump_table = false;	// for enums
	InlineVec<int, 2> jump_table;
};

struct Mirror_Node_RT : Rt_Vars_Base
{
	float lerp_amt = 0.0;
	union {
		int saved_boolean = 0;
		float saved_f;
	};
};

struct Mirror_Node_CFG : public Node_CFG
{
	DECLARE_ANIMNODE_CREATOR(Mirror_Node_CFG, animnode_type::mirror)


	Mirror_Node_CFG(Animation_Tree_CFG* cfg) : Node_CFG(cfg, sizeof Mirror_Node_RT) {
		input.count = 1;
	}

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

	bool store_value_on_reset = false;	// if true, then parameter value is saved and becomes const until reset again
	// below are computed on init
	uint8_t parameter_type = 1;// 0 = float, 1 = bool
};


struct Statemachine_Node_RT : Rt_Vars_Base
{
	handle<State> active_state;
	handle<State> fading_out_state;
	float active_weight = 0.0;
	bool change_to_next = false;
};

struct Statemachine_Node_CFG : public Node_CFG
{
	DECLARE_ANIMNODE_CREATOR(Statemachine_Node_CFG, animnode_type::statemachine)


	Statemachine_Node_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(Statemachine_Node_RT))
	{
		input.count = 0;
	}

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	// Inherited via At_Node
	virtual void reset(NodeRt_Ctx& ctx) const override
	{
		*get_rt<Statemachine_Node_RT>(ctx) = Statemachine_Node_RT();
	}

	virtual void construct(NodeRt_Ctx& ctx) const override
	{
		construct_this<Statemachine_Node_RT>(ctx);
	}

	virtual void write_to_dict(Animation_Tree_CFG* tree, DictWriter& out) override;
	virtual void read_from_dict(Animation_Tree_CFG* tree, DictParser& in) override;

	const State* get_state(handle<State> handle) const {
		if (handle.id == -1) return nullptr;
		return &states.at(handle.id);
	}
	handle<State> get_state_handle(const State* state) const {
		return { int((state - states.data()) / sizeof(State)) };
	}

	handle<State> start_state;
	float fade_in_time = 0.f;
	std::vector<State> states;
};

struct BlendSpace2d_RT : Rt_Vars_Base
{
	glm::vec2 character_blend_weights = glm::vec2(0.f);
};

struct BlendSpace2d_CFG : public Node_CFG
{
	DECLARE_ANIMNODE_CREATOR(BlendSpace2d_CFG, animnode_type::blend2d)
	using RT_TYPE = BlendSpace2d_RT;

	BlendSpace2d_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(RT_TYPE)) {
		// allocate memory for extra nodes
		Node_CFG** nodes = (Node_CFG**)tree->arena.alloc_bottom(sizeof(Node_CFG**) * 9);
		memset(nodes, 0, sizeof(Node_CFG*)*9);
		input.assign_memory(nodes, 9);
	}

	handle<Parameter> xparam;
	handle<Parameter> yparam;
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
	DECLARE_ANIMNODE_CREATOR(BlendSpace1d_CFG, animnode_type::blend1d)
	using RT_TYPE = BlendSpace1d_RT;

	BlendSpace1d_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(RT_TYPE)) {
		// allocate memory for extra nodes
		Node_CFG** nodes = (Node_CFG**)tree->arena.alloc_bottom(sizeof(Node_CFG**) * 9);
		memset(nodes, 0, sizeof(Node_CFG*) * 9);
		input.assign_memory(nodes, 9);
	}

	InlineVec<float, 3> blend1d_verts;
	bool is_additive_blend_space = false;

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		*get_rt<RT_TYPE>(ctx) = RT_TYPE();
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<RT_TYPE>(ctx);
	}
};


typedef Node_CFG* (*create_func)(Animation_Tree_CFG* tree);
typedef void (*register_func)();


struct animnode_name_type
{
	create_func create = nullptr;
	register_func reg = nullptr;
	Color32 editor_color = { 0,0,0,0xff };
	const char* editor_name = "Unnamed";
	const char* editor_tooltip = "No tooltip";
};

extern animnode_name_type& get_animnode_typedef(animnode_type type);
BytecodeContext& get_global_anim_bytecode_ctx();