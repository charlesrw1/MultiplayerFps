#include "Animation/Runtime/Animation.h"
#include "Framework/Util.h"
#include "Render/Model.h"
#include "GameEnginePublic.h"
#include "Framework/Config.h"
#include <fstream>
#include <sstream>
using namespace glm;
#include <iostream>
#include <iomanip>
#include "Framework/MemArena.h"
#include "../AnimationUtil.h"
#include "AnimationTreeLocal.h"

#include "Game/Entity.h"
#include "Game/Components/GameAnimationMgr.h"
#include "RuntimeNodesNew.h"

#include "Debug.h"
#include "Game/Components/GameAnimationMgr.h"
#include "RuntimeNodesNew2.h"

#define ROOT_BONE -1
#define INVALID_ANIMATION -1

using glm::vec3;
using glm::quat;
using glm::mat4;
using glm::length;
using glm::dot;
using glm::cross;
using glm::normalize;


AnimatorObject::~AnimatorObject() {
	g_gameAnimationMgr.remove_from_animating_set(*this);
}
agBaseNode& AnimatorObject::get_root_node() const {
	assert(graph.get_root());
	return *graph.get_root();
}
AnimatorObject::AnimatorObject(const Model& model, AnimGraphConstructed& ingraph, Entity* ent) 
	: model(model),graph(ingraph), simulating_physics_objects(2) {
	if (!model.get_skel()) {
		sys_print(Error, "AnimatorObject(): model doesnt have skeleton\n");
		throw ConstructorError();
	}
	if (!graph.get_root()) {
		sys_print(Error, "AnimatorObject(): graph has no root node set\n");
		throw ConstructorError();
	}

	slots.resize(graph.get_slots().size());
	for (int i = 0; i < slots.size(); i++)
		slots[i].name = slots.at(i).name;
	this->owner = ent;

	get_root_node().reset();	// reset it

	// Init bone arrays
	const int bones = model.get_skel()->get_num_bones();
	cached_bonemats.resize(bones);
	if (using_global_bonemat_double_buffer)
		last_cached_bonemats.resize(bones);

	auto pose = g_pose_pool.allocate_scoped();
	util_set_to_bind_pose(*pose.get(), get_skel());
	util_localspace_to_meshspace(*pose.get(), cached_bonemats, get_skel());
	g_gameAnimationMgr.add_to_animating_set(*this);
}

void AnimatorObject::ConcatWithInvPose()
{
	ASSERT(get_skel());

	auto skel = get_skel();
	glm::mat4* matrix_palette = g_gameAnimationMgr.get_bonemat_ptr(get_matrix_palette_offset());
	for (int i = 0; i < skel->get_num_bones(); i++) {

		matrix_palette[i] = cached_bonemats[i] * (glm::mat4)skel->get_inv_posematrix(i);
	}
}

inline DirectAnimationSlot* AnimatorObject::find_slot_with_name(StringName name) {
	for (int i = 0; i < slots.size(); i++)
		if (slots[i].name == name)
			return &slots[i];
	return nullptr;
}


static std::vector<int> get_indicies(const Animation_Set* set, const std::vector<const char*>& strings)
{
	std::vector<int> out;
	for (auto s : strings) out.push_back(set->find(s));
	return out;
}


ConfigVar force_animation_to_bind_pose("force_animation_to_bind_pose", "0", CVAR_BOOL | CVAR_DEV, "");
void AnimatorObject::update(float dt)
{
	assert(model.get_skel());
	root_motion = RootMotionTransform();
	if(using_global_bonemat_double_buffer)
		last_cached_bonemats.swap(cached_bonemats);
	debug_output_messages.clear();
	
	agGetPoseCtx graphCtx(*this, g_pose_pool,dt);
	auto& pose_base = graphCtx.pose;

	// call into tree
	if (!force_animation_to_bind_pose.get_bool()) {
		get_root_node().get_pose(graphCtx);
	}
	else {
		util_set_to_bind_pose(*pose_base.get(), get_skel());
	}
	for (int i = 0; i < active_sync_groups.size(); i++) {
		const bool delete_group = update_sync_group(i);
		// group did not update, remove it
		if (delete_group) {
			active_sync_groups.erase(active_sync_groups.begin() + i);
			i--;
			continue;
		}
	}
	// update animation slots
	for (int i = 0; i < slots.size(); i++) {
		update_slot(i, dt);
	}
	// add physics driven bones
	// physics bones are in world space, wont cover every bone
	if (simulating_physics_objects.size() > 0) {
		update_physics_bones(*pose_base);
	}
	else {
		util_localspace_to_meshspace(*pose_base, cached_bonemats, get_skel());
	}

	ConcatWithInvPose();
}

void AnimatorObject::add_simulating_physics_object(Entity* e)
{
	simulating_physics_objects.insert(e->get_self_ptr().handle);
}
void AnimatorObject::remove_simulating_physics_object(Entity* e)
{
	simulating_physics_objects.erase(e->get_self_ptr().handle);
}

opt<float> AnimatorObject::get_curve_value(StringName name) const
{
	auto find = MapUtil::get_opt(curve_values, name.get_hash());
	if (find) return *find;
	return std::nullopt;
}

opt<float> AnimatorObject::get_float_variable(StringName name) const
{
	auto find = MapUtil::get_opt(blackboard, name.get_hash());
	if (find && std::holds_alternative<float>(*find))
		return std::get<float>(*find);
	return std::nullopt;
}

opt<bool> AnimatorObject::get_bool_variable(StringName name) const
{
	auto find = MapUtil::get_opt(blackboard, name.get_hash());
	if (find && std::holds_alternative<bool>(*find))
		return std::get<bool>(*find);
	return std::nullopt;
}

opt<int> AnimatorObject::get_int_variable(StringName name) const
{
	auto find = MapUtil::get_opt(blackboard, name.get_hash());
	if (find && std::holds_alternative<int>(*find))
		return std::get<int>(*find);
	return std::nullopt;
}

opt<glm::vec3> AnimatorObject::get_vec3_variable(StringName name) const
{
	auto find = MapUtil::get_opt(blackboard, name.get_hash());
	if (find && std::holds_alternative<glm::vec3>(*find))
		return std::get<glm::vec3>(*find);
	return std::nullopt;
}

void AnimatorObject::set_float_variable(StringName name, float f)
{
	blackboard[name.get_hash()] = f;
}

void AnimatorObject::set_int_variable(StringName name, int f)
{
	blackboard[name.get_hash()] = f;
}

#if 0
class Character
{
public:
	bool is_jumping = false;
	bool is_running = false;
};
#include "RuntimeNodesNew.h"

// basic running and idle
class atMyStatemachine : public atAnimStatemachine {
public:
	enum State {
		Running,
		Jumping,
		Idle,
	};
	bool eval_global_transitions() {
		std::unordered_set<State> transition_to_pose_from = { Running,Jumping };
		if (SetUtil::contains(transition_to_pose_from, state) && c->is_jumping)
			state = Idle;
	}
	bool is_transitioning() const;
	float transition_time_left() const;
	bool can_interrupt_transition() const;
	void set_transition_params(Easing easing, float duration, bool interruptable);	// set transition params to use on next transition. prints warning when this is called, but no transitino occurs

	void update(bool resetMe) final {
		if (resetMe)	// reset state
			state = Idle;
		if (state == Running) {
			set_blend_tree("debug_name",nodes.at(0));
			if (eval_global_transitions()) {
			}
			ctx.duration_remaining();
			else if (c->is_jumping)
				state = Jumping;
			else if (c->is_running && ctx.find_value("flMove") > 0.0)
				state = Idle;
		}
	}
	Character* c = nullptr;
	State state = 0;
	vector<BaseAGNode*> nodes;
};

void f()
{
	atClipNode* c = new atClipNode;
	c->speedId = StringName("speed");
	c->data.loop = true;
	atComposePoses* blend = new atComposePoses;
	blend->alphaId = StringName("Blend")
	atBlendByInt* b = new atBlendByInt;
	atMyStatemachine* sm = new atMyStatemachine;
	sm->character = character;

	atIk2Bone* ik1 = new atIk2Bone;
	ik->bone_name = StringName("handBone");
	ik1->inputId = blend;
	ik1->alphaId = StringName("handIkAlpha");
	atIk2Bone* ik2 = new atIk2Bone;
	ik->bone_name = StringName("footBone");
	ik2->inputId = ik1;
	ik2->alphaId = StringName("legIkAlpha");

	atModifyBone* modify = new atModifyBone;
	modify->inputId = ik2;

	return modify;

	int state = 0;
	if (state == 0) {
		//
		if(anim_time_left() < 0.2)
			state = 1;
	}
	else if (state == 1) {
		if(event_was_fired())
			state = 2;
	}
	set_blend_tree(c);

	set_transition_params();
}
#endif

#include "Assets/AssetDatabase.h"
#include "UI/UILoader.h"
#include "Render/RenderWindow.h"
#include "UI/GUISystemPublic.h"
void AnimatorObject::debug_print(int start_y)
{
	auto font = g_assets.find_sync<GuiFont>("eng/fonts/monospace12.fnt").get();
	int start = start_y;
	auto draw_text = [&](const char* s) {
		string str = s;
		TextShape shape;
		Rect2d size = GuiHelpers::calc_text_size(std::string_view(str), font);
		shape.rect.x = 20;
		shape.rect.y = start + size.h;
		shape.font = font;
		shape.color = COLOR_WHITE;
		shape.with_drop_shadow = true;
		shape.drop_shadow_ofs = 1;
		shape.text = str;
		UiSystem::inst->window.draw(shape);
		start += size.h;
	};
	for (auto& msg : debug_output_messages)
		draw_text(msg.c_str());
}

bool AnimatorObject::update_sync_group(int idx)
{
	SyncGroupData& data = active_sync_groups[idx];
	if (!data.update_owner) {
		assert(!data.is_first_update);
		return false;
	}
	// clear first update flag
	data.is_first_update = false;

	// set updated data into data used for next tree tick
	data.time = data.update_time;
	data.has_sync_marker = data.update_has_sync_marker;
	data.sync_marker_name = data.update_sync_marker_name;

	// clear update data
	data.update_time = Percentage();
	data.update_weight = 0.0;
	data.update_owner = nullptr;
	data.update_owner_synctype = sync_opt::Default;
	data.update_sync_marker_name = StringName();
	data.update_has_sync_marker = false;

	return true;
}

void AnimatorObject::update_slot(int idx, float dt)
{
	auto& slot = slots.at(idx);
	if (!slot.active)
		return;
	auto seq = slot.active;
	slot.time += dt*slot.playspeed;	// also update time
	if (slot.time > seq->seq->get_duration()) {
		if (slot.on_finished) {
			slot.on_finished(true);
			slot.on_finished = {};
		}
		slot.active = nullptr;
	}
}

void AnimatorObject::update_physics_bones(const Pose& inpose)
{
	std::vector<bool> is_simulating(num_bones(), 0);	// fixme

	for (auto it = simulating_physics_objects.begin(); it != simulating_physics_objects.end();)
	{
		auto e = eng->get_entity(*it);
		if (!e) {
			it = simulating_physics_objects.erase(it);
			return;
		}
		else
			++it;


		int parent = get_skel()->get_bone_index(e->get_parent_bone());
		if (parent == -1) continue;
		is_simulating[parent] = true;
		cached_bonemats[parent] = e->get_ws_transform();
	}
	if (update_owner_position_to_root) {
		glm::mat4 root = cached_bonemats[0];
		auto inv = glm::inverse(root);
		for (int i = 0; i < num_bones(); i++) {
			if (is_simulating[i])
				cached_bonemats[i] = inv * cached_bonemats[i];
		}
		owner->set_ws_transform(root);
	}
	else {
		glm::mat4 obj_worldspace = owner->get_ws_transform();
		auto invobj = glm::inverse(obj_worldspace);
		for (int i = 0; i < num_bones(); i++) {
			if (is_simulating[i])
				cached_bonemats[i] = invobj * cached_bonemats[i];
		}
	}

	util_localspace_to_meshspace_with_physics(inpose, cached_bonemats, is_simulating, get_skel());
}

SyncGroupData& AnimatorObject::find_or_create_sync_group(StringName name)
{
	for (int i = 0; i < active_sync_groups.size(); i++) {
		if (active_sync_groups[i].name == name)
			return active_sync_groups[i];
	}

	// no sync group active, create one
	SyncGroupData sync;
	sync.name = name;
	sync.is_first_update = true;
	
	active_sync_groups.push_back(sync);
	return active_sync_groups.back();
}


bool AnimatorObject::play_animation(
	const AnimationSeqAsset* seqAsset,
	float play_speed,
	float start_pos
)
{
	if (!seqAsset || !seqAsset->seq) {
		sys_print(Warning, "play_animation: sequence invalid\n");
		return false;
	}
	auto seq = seqAsset->seq;

	auto slot_to_play_in = find_slot_with_name(seq->directplayopt.slotname);
	if (!slot_to_play_in) {
		sys_print(Warning, "no slot with name\n");
		return false;
	}
	

	slot_to_play_in->active = seqAsset;
	slot_to_play_in->time = 0.0;
	slot_to_play_in->playspeed = play_speed;
	return true;
}

float DirectAnimationSlot::time_remaining() const {
	if (!active || !active->seq) return 0.f;
	return (active->seq->get_duration() - time)/playspeed;
}
