#pragma once

#include "Model.h"
#include "LispInterpreter.h"
#include "glm/glm.hpp"
#include "MemArena.h"

#include "AnimationTreePublic.h"
#include "InlineVec.h"

#include "EnumDefReflection.h"
#include "ReflectionProp.h"

// Modify AutoEnumDef when changing enum!
extern AutoEnumDef animnode_type_def;
enum class animnode_type
{
	source,
	statemachine,
	selector,

	mask,

	blend,
	blend2d,
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

	// serialization helpers
	virtual void write_to_dict(Animation_Tree_CFG* tree, DictWriter& out) {}
	virtual void read_from_dict(Animation_Tree_CFG* tree, DictParser& in) {}
	virtual PropertyInfoList* get_property_list() = 0;
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
} static PropertyInfoList properties; static void register_props();  \
virtual PropertyInfoList* get_property_list() override { return &properties;}  \
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
	Clip_Node_CFG(Animation_Tree_CFG* cfg)
		: Node_CFG(cfg, sizeof(Clip_Node_RT)) {
		input.count = 0;
	}


	DECLARE_ANIMNODE_CREATOR(Clip_Node_CFG, animnode_type::source)

	virtual void construct(NodeRt_Ctx& ctx) const {
		Clip_Node_RT* rt = construct_this<Clip_Node_RT>(ctx);

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
		Clip_Node_RT* rt = get_rt<Clip_Node_RT>(ctx);
		rt->frame = 0.0;
		rt->stopped_flag = false;
	}

	virtual bool is_clip_node() const override {
		return true;
	}
	const Animation* get_clip(NodeRt_Ctx& ctx) const {
		Clip_Node_RT* rt = get_rt<Clip_Node_RT>(ctx);
		if (rt->clip_index == -1) {
			return nullptr;
		}
		auto subset = ctx.set->get_subset(rt->set_idx);
		const Animation& clip = subset->clips[rt->clip_index];

		return &clip;
	}
	void set_frame_by_interp(NodeRt_Ctx& ctx, float frac) const {
		Clip_Node_RT* rt = get_rt<Clip_Node_RT>(ctx);

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
};

struct Blend_Node_CFG : public Node_CFG
{
	DECLARE_ANIMNODE_CREATOR(Blend_Node_CFG, animnode_type::blend)


	Blend_Node_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(Blend_Node_RT)) {
		input.count = 2;
	}

	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<Blend_Node_RT>(ctx);
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		*get_rt<Blend_Node_RT>(ctx) = Blend_Node_RT();

		input[0]->reset(ctx);
		input[1]->reset(ctx);

	}

	float damp_factor = 0.1;
};

struct Mirror_Node_RT : Rt_Vars_Base
{
	float lerp_amt = 0.0;
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

struct Directionalblend_Node_RT : Rt_Vars_Base
{
	glm::vec2 character_blend_weights = glm::vec2(0.f);
};

struct Blend2d_CFG : public Node_CFG
{
	DECLARE_ANIMNODE_CREATOR(Blend2d_CFG, animnode_type::blend2d)


	Blend2d_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(Directionalblend_Node_RT)) {
		// allocate memory for extra nodes
		Node_CFG** nodes = (Node_CFG**)tree->arena.alloc_bottom(sizeof(Node_CFG**) * 9);
		memset(nodes, 0, sizeof(Node_CFG*)*9);
		input.assign_memory(nodes, 9);
	}

	enum {
		IDLE = 0,
		DIRECTION = 1
	};

	handle<Parameter> xparam;
	handle<Parameter> yparam;
	float fade_in = 1.0;
	float weight_damp = 0.01;

	// Inherited via At_Node
	virtual bool get_pose_internal(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		*get_rt<Directionalblend_Node_RT>(ctx) = Directionalblend_Node_RT();
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<Directionalblend_Node_RT>(ctx);
	}
};


typedef Node_CFG* (*create_func)(Animation_Tree_CFG* tree);
typedef void (*register_func)();


struct animnode_name_type
{
	create_func create = nullptr;
	register_func reg = nullptr;
};

extern animnode_name_type& get_animnode_typedef(animnode_type type);