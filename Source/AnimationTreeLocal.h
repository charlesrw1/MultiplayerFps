#pragma once

#include "Model.h"
#include "LispInterpreter.h"
#include "glm/glm.hpp"
#include "MemArena.h"

#include "AnimationTreePublic.h"

struct NodeRt_Ctx;
struct ScriptExpression
{
	BytecodeExpression compilied;

	bool evaluate(NodeRt_Ctx& rt) const;
};

struct State;
struct State_Transition
{
	State* transition_state;
	ScriptExpression script;
};


struct Animation_Tree_RT;
struct Animation_Tree_CFG;
struct Node_CFG;
struct Control_Params;
struct NodeRt_Ctx
{
	const Model* model = nullptr;
	const Animation_Set* set = nullptr;
	Animation_Tree_RT* tree = nullptr;
	ScriptVars_RT* vars = nullptr;

	uint32_t num_bones() const { return model->bones.size(); }
};

struct GetPose_Ctx
{
	GetPose_Ctx set_pose(Pose* newpose) {
		pose = newpose;
		return *this;
	}
	GetPose_Ctx set_dt(float newdt) {
		dt = newdt;
		return *this;
	}
	// used for syncronizing animations
	struct syncval {
		float normalized_frame = 0.0;
		bool first_seen = false;
	};
	GetPose_Ctx set_sync(syncval* s) {
		sync = s;
		return *this;
	}
	GetPose_Ctx set_rootmotion(float rm) {
		rootmotion_scale = rm;
		return *this;
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
		cfg->data_used += rt_size;
	}

	virtual bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const = 0;
	virtual void construct(NodeRt_Ctx& ctx) const {
		assert(rt_offset == 0);
	}
	virtual void reset(NodeRt_Ctx& ctx) const = 0;
	virtual bool is_clip_node() const { return false; }

	template<typename T>
	T* get_rt(NodeRt_Ctx& ctx) const {
		return ctx.tree->get<T>(rt_offset);
	}
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
	string name;
	Node_CFG* tree = nullptr;
	vector<State_Transition> transitions;
	State* next_state = nullptr;

	float state_duration = -1.0;
	float time_left = 0.0;

	State* get_next_state(NodeRt_Ctx& ctx);
};

// playback speed *= param / (speed of clip's root motion)
struct Scale_By_Rootmotion_CFG : public Node_CFG
{
	Scale_By_Rootmotion_CFG(Animation_Tree_CFG* cfg) : Node_CFG(cfg, 0) {}

	virtual bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override
	{
		float rm = ctx.vars->get(param).fval;
		bool ret = child->get_pose(ctx, pose.set_rootmotion(rm));
		return ret;
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
	}

	Node_CFG* child = nullptr;
	handle<Parameter> param;
};

struct Sync_Node_RT
{
	float normalized_frame = 0.0;
};

struct Sync_Node_CFG : public Node_CFG
{
	Sync_Node_CFG(Animation_Tree_CFG* cfg)
		: Node_CFG(cfg, sizeof(Sync_Node_RT)) {
	}

	virtual void construct(NodeRt_Ctx& ctx) const {
		construct_this<Sync_Node_RT>(ctx);
	}


	virtual bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override
	{
		Sync_Node_RT* rt = get_rt<Sync_Node_RT>(ctx);

		GetPose_Ctx::syncval sv;
		sv.first_seen = true;
		sv.normalized_frame = rt->normalized_frame;

		bool ret = child->get_pose(ctx, pose.set_sync(&sv));

		rt->normalized_frame = sv.normalized_frame;

		return ret;
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		Sync_Node_RT* rt = get_rt<Sync_Node_RT>(ctx);
		rt->normalized_frame = 0.0;
	}

	Node_CFG* child = nullptr;
};

struct Clip_Node_RT
{
	glm::vec3 root_pos_first_frame = glm::vec3(0.f);
	float inv_speed_of_anim_root = 1.0;
	float frame = 0.0;
	uint32_t clip_index = 0;
	bool stopped_flag = false;
};

struct Clip_Node_CFG : public Node_CFG
{
	Clip_Node_CFG(Animation_Tree_CFG* cfg)
		: Node_CFG(cfg, sizeof(Clip_Node_RT)) {
	}

	virtual void construct(NodeRt_Ctx& ctx) const {
		Clip_Node_RT* rt = construct_this<Clip_Node_RT>(ctx);

		rt->clip_index = ctx.set->find(clip_name);
		int root_index = ctx.model->root_bone_index;
		int first_pos = ctx.set->FirstPositionKeyframe(0.0, root_index, rt->clip_index);
		rt->root_pos_first_frame = first_pos != -1 ?
			ctx.set->GetPos(root_index, first_pos, rt->clip_index).val
			: ctx.model->bones[root_index].posematrix[3];

		const Animation& clip = ctx.set->clips[rt->clip_index];
		rt->inv_speed_of_anim_root = 1.0 / glm::length(clip.root_motion_translation) / (clip.total_duration / clip.fps);
	}


	// Inherited via At_Node
	virtual bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

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
		const Animation* clip = (rt->clip_index == -1) ? nullptr : &ctx.set->clips.at(rt->clip_index);
		return clip;
	}
	void set_frame_by_interp(NodeRt_Ctx& ctx, float frac) const {
		Clip_Node_RT* rt = get_rt<Clip_Node_RT>(ctx);

		rt->frame = get_clip(ctx)->total_duration * frac;
	}

	enum rootmotion_type {
		None,
		Remove
	}rootmotion[3] = { None,None,None };
	const char* clip_name = "";
	bool loop = true;
	float speed = 1.0;

	int sync_track_idx = 0;
	bool can_be_leader = true;
};

struct Subtract_Node_CFG : public Node_CFG
{
	Subtract_Node_CFG(Animation_Tree_CFG* cfg) : Node_CFG(cfg, 0) {}
	// Inherited via At_Node
	virtual bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		ref->reset(ctx);
		source->reset(ctx);
	}
	Node_CFG* ref = nullptr;
	Node_CFG* source = nullptr;
};

struct Add_Node_CFG : public Node_CFG
{
	Add_Node_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, 0) {}

	virtual bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		diff_pose->reset(ctx);
		base_pose->reset(ctx);
	}

	handle<Parameter> param;

	Node_CFG* diff_pose = nullptr;
	Node_CFG* base_pose = nullptr;
};

struct Blend_Node_RT
{
	float lerp_amt = 0.0;
};

struct Blend_Node_CFG : public Node_CFG
{
	Blend_Node_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(Blend_Node_RT)) {}

	virtual bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<Blend_Node_RT>(ctx);
	}

	virtual void reset(NodeRt_Ctx& ctx) const override {
		*get_rt<Blend_Node_RT>(ctx) = Blend_Node_RT();

		posea->reset(ctx);
		poseb->reset(ctx);
	}

	handle<Parameter> param;
	Node_CFG* posea = nullptr;
	Node_CFG* poseb = nullptr;
	float damp_factor = 0.1;
};

struct Mirror_Node_RT
{
	float lerp_amt = 0.0;
};

struct Mirror_Node_CFG : public Node_CFG
{
	Mirror_Node_CFG(Animation_Tree_CFG* cfg) : Node_CFG(cfg, sizeof Mirror_Node_RT) {}

	// Inherited via At_Node
	virtual bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	virtual void reset(NodeRt_Ctx& ctx) const override
	{
		*get_rt< Mirror_Node_RT>(ctx) = Mirror_Node_RT();
		input->reset(ctx);
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<Mirror_Node_RT>(ctx);
	}

	float damp_time = 0.1;
	Node_CFG* input = nullptr;
	handle<Parameter> param;
};


struct Statemachine_Node_RT
{
	State* active_state = nullptr;
	State* fading_out_state = nullptr;
	float active_weight = 0.0;
	bool change_to_next = false;
};

struct Statemachine_Node_CFG : public Node_CFG
{
	Statemachine_Node_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(Statemachine_Node_RT)) {}

	virtual bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;

	State* start_state = nullptr;

	// Inherited via At_Node
	virtual void reset(NodeRt_Ctx& ctx) const override
	{
		*get_rt<Statemachine_Node_RT>(ctx) = Statemachine_Node_RT();
	}

	virtual void construct(NodeRt_Ctx& ctx) const override
	{
		construct_this<Statemachine_Node_RT>(ctx);
	}

	float fade_in_time = 0.f;
};

struct Directionalblend_Node_RT
{
	glm::vec2 character_blend_weights = glm::vec2(0.f);
};

struct Blend2d_CFG : public Node_CFG
{
	Blend2d_CFG(Animation_Tree_CFG* tree) : Node_CFG(tree, sizeof(Directionalblend_Node_RT)) {
		memset(directions, 0, sizeof(directions));
	}

	Clip_Node_CFG* idle = nullptr;
	Clip_Node_CFG* directions[8];
	handle<Parameter> xparam;
	handle<Parameter> yparam;
	float fade_in = 5.0;

	// Inherited via At_Node
	virtual bool get_pose(NodeRt_Ctx& ctx, GetPose_Ctx pose) const override;
	virtual void reset(NodeRt_Ctx& ctx) const override {
		*get_rt<Directionalblend_Node_RT>(ctx) = Directionalblend_Node_RT();
	}

	virtual void construct(NodeRt_Ctx& ctx) const override {
		construct_this<Directionalblend_Node_RT>(ctx);
	}
};