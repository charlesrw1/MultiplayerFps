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
#include "../AnimationTreePublic.h"

#include "Game/Entity.h"

#include "Game/Components/GameAnimationMgr.h"

CLASS_IMPL(AnimatorInstance);

#define ROOT_BONE -1
#define INVALID_ANIMATION -1

using glm::vec3;
using glm::quat;
using glm::mat4;
using glm::length;
using glm::dot;
using glm::cross;
using glm::normalize;

int Animation_Set::FirstPositionKeyframe(float frame, int channel_num, int clip) const
{
	const Animation& an = clips[clip];

	assert(channel_num < channels.size());
	//AnimChannel* chan = channels + channel_num;
	const AnimChannel& chan = channels[an.channel_offset + channel_num];
	//const AnimChannel* chan = channels.data() + channel_num;


	if (chan.num_positions == 0)
		return -1;

	for (int i = 0; i < chan.num_positions - 1; i++) {
		if (frame < positions[chan.pos_start + i + 1].time)
			return i;
	}

	return chan.num_positions - 1;
}
int Animation_Set::FirstRotationKeyframe(float frame, int channel_num, int clip) const
{
	const Animation& an = clips[clip];
	assert(channel_num < channels.size());
	const AnimChannel& chan = channels[an.channel_offset + channel_num];

	if (chan.num_rotations == 0)
		return -1;

	for (int i = 0; i < chan.num_rotations - 1; i++) {
		if (frame < rotations[chan.rot_start + i + 1].time)
			return i;
	}

	return chan.num_rotations - 1;
}
int Animation_Set::FirstScaleKeyframe(float frame, int channel_num, int clip) const
{
	const Animation& an = clips[clip];
	assert(channel_num < channels.size());
	const AnimChannel& chan = channels[an.channel_offset + channel_num];


	if (chan.num_scales == 0)
		return -1;

	for (int i = 0; i < chan.num_scales - 1; i++) {
		if (frame < scales[chan.scale_start + i + 1].time)
			return i;
	}

	return chan.num_scales - 1;
}

const PosKeyframe& Animation_Set::GetPos(int channel, int index, int clip) const {
	ASSERT(clip < clips.size());
	ASSERT(index < channels[clips[clip].channel_offset + channel].num_positions);

	return positions[channels[clips[clip].channel_offset + channel].pos_start + index];
}
const RotKeyframe& Animation_Set::GetRot(int channel, int index, int clip) const {
	ASSERT(clip < clips.size());
	ASSERT(index < channels[clips[clip].channel_offset + channel].num_rotations);
	
	return rotations[channels[clips[clip].channel_offset + channel].rot_start + index];
}
const ScaleKeyframe& Animation_Set::GetScale(int channel, int index, int clip) const {
	ASSERT(clip < clips.size());
	ASSERT(index < channels[clips[clip].channel_offset + channel].num_scales);

	return scales[channels[clips[clip].channel_offset + channel].scale_start + index];
}
const AnimChannel& Animation_Set::GetChannel(int clip, int channel) const {
	ASSERT(clip < clips.size());
	return channels[clips[clip].channel_offset + channel];
}

int Animation_Set::find(const char* name) const
{
	for (int i = 0; i < clips.size(); i++) {
		if (clips[i].name == name)
			return i;
	}
	return -1;
}

void util_localspace_to_meshspace(const Pose& local, std::vector<glm::mat4x4>& out_bone_matricies, const MSkeleton* model);

AnimatorInstance::~AnimatorInstance() {
	g_gameAnimationMgr.remove_from_animating_set(this);
}
bool AnimatorInstance::initialize(
	const Model* model, 
	const Animation_Tree_CFG* graph, 
	Entity* ent)
{
	ASSERT(model);

	g_gameAnimationMgr.remove_from_animating_set(this);	// if was already added

	if (!model->get_skel()) {
		sys_print(Error, "model doesnt have skeleton for AnimatorInstance\n");
		return false;
	}

	if (!graph) {
		sys_print(Error, "graph not provided for animator instance\n");
		return false;
	}

	// initialize slots
	slots.clear();
	slots.resize(graph->get_slot_names().size());
	for (int i = 0; i < slots.size(); i++)
		slots[i].name = graph->get_slot_names()[i].c_str();	// set hashed name for gamecode to find

	// Initialize script instance, sets pointer of AnimatorInstance for native variables


	this->cfg = graph;
	this->model = model;
	this->owner = ent;

	// Initialize runtime data, runtime nodes get constructed here
	runtime_graph_data.clear();
	runtime_graph_data.resize(cfg->get_num_nodes());

	// Construct nodes
	NodeRt_Ctx ctx(this);
	cfg->construct_all_nodes(ctx);

	// Reset root node here to kick things off
	if (cfg->get_root_node())
		cfg->get_root_node()->reset(ctx);

	// Callback to inherited class
	if(!get_is_for_editor())
		on_init();

	// Init bone arrays
	const int bones = model->get_skel()->get_num_bones();
	cached_bonemats.resize(bones);
	if (using_global_bonemat_double_buffer)
		last_cached_bonemats.resize(bones);

	ASSERT(is_initialized());

	auto pose = g_pose_pool.allocate_scoped();
	util_set_to_bind_pose(*pose.get(), get_skel());
	util_localspace_to_meshspace(*pose.get(), cached_bonemats, get_skel());

	g_gameAnimationMgr.add_to_animating_set(this);

	return true;
}

void util_localspace_to_meshspace(const Pose& local, std::vector<glm::mat4x4>& out_bone_matricies, const MSkeleton* model)
{
	for (int i = 0; i < model->get_num_bones(); i++)
	{
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (model->get_bone_parent(i) == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(model->get_bone_parent(i) < model->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[model->get_bone_parent(i)] * matrix;
		}
	}
}

void util_localspace_to_meshspace_with_physics(const Pose& local, std::vector<glm::mat4x4>& out_bone_matricies, const std::vector<bool>& phys_bitmask, const MSkeleton* model)
{
	for (int i = 0; i < model->get_num_bones(); i++)
	{
		if (phys_bitmask[i])
			continue;

		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (model->get_bone_parent(i) == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(model->get_bone_parent(i) < model->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[model->get_bone_parent(i)] * matrix;
		}
	}

}


void util_localspace_to_meshspace_ptr(const Pose& local, glm::mat4* out_bone_matricies, const MSkeleton* model)
{
	for (int i = 0; i < model->get_num_bones(); i++)
	{
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (model->get_bone_parent(i) == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(model->get_bone_parent(i) < model->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[model->get_bone_parent(i)] * matrix;
		}
	}
	//for (int i = 0; i < model->bones.size(); i++)
	//	out_bone_matricies[i] =  out_bone_matricies[i];
}

#include "Game/Components/GameAnimationMgr.h"

void AnimatorInstance::ConcatWithInvPose()
{
	ASSERT(get_skel());

	auto skel = get_skel();
	glm::mat4* matrix_palette = g_gameAnimationMgr.get_bonemat_ptr(get_matrix_palette_offset());
	for (int i = 0; i < skel->get_num_bones(); i++) {

		matrix_palette[i] = cached_bonemats[i] * (glm::mat4)skel->get_inv_posematrix(i);
	}
}

AnimatorInstance::AnimatorInstance() : slots(1), simulating_physics_objects(2)
{
}

static std::vector<int> get_indicies(const Animation_Set* set, const std::vector<const char*>& strings)
{
	std::vector<int> out;
	for (auto s : strings) out.push_back(set->find(s));
	return out;
}

char get_first_token(string& s, char default_='\0')
{
	for (auto c : s) {
		if (c != ' ' && c != '\t' && c != '\n') return c;
	}
	return default_;
}

NodeRt_Ctx::NodeRt_Ctx(AnimatorInstance* inst)
{
	this->anim = inst;
	this->model = inst->get_model();
}
#include "Debug.h"

ConfigVar force_animation_to_bind_pose("force_animation_to_bind_pose", "0", CVAR_BOOL | CVAR_DEV, "");
void AnimatorInstance::update(float dt)
{
	root_motion = RootMotionTransform();

	if (!cfg)
		return;

	if(using_global_bonemat_double_buffer)
		last_cached_bonemats.swap(cached_bonemats);

	auto pose_base = g_pose_pool.allocate_scoped();

	NodeRt_Ctx ctx(this);

	GetPose_Ctx gp_ctx;
	gp_ctx.dt = dt;
	gp_ctx.pose = pose_base.get();
	gp_ctx.accumulated_root_motion = &root_motion;

	// callback
	if(!get_is_for_editor())
		on_update(dt);

	// call into tree
	if (!force_animation_to_bind_pose.get_bool() && get_tree()&& get_tree()->get_root_node())
		get_tree()->get_root_node()->get_pose(ctx, gp_ctx);
	else
		util_set_to_bind_pose(*pose_base.get(), get_skel());

	// update sync groups
	for (int i = 0; i < active_sync_groups.size(); i++) {

		// group did not update, remove it
		if (!active_sync_groups[i].update_owner) {
			ASSERT(!active_sync_groups[i].is_first_update);
			active_sync_groups.erase(active_sync_groups.begin() + i);
			i--;
			continue;
		}
		SyncGroupData& data = active_sync_groups[i];
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
	}

	// update animation slots
	for (int i = 0; i < slots.size(); i++) {
		if (!slots[i].active)
			continue;
		auto seq = slots[i].active;
		auto& slot = slots[i];
		if (slot.state == DirectAnimationSlot::FadingIn) {
			slot.fade_percentage += dt / slot.fade_duration;
			if (slot.fade_percentage > 1.0)
				slot.state = DirectAnimationSlot::Full;
			slot.time += dt;	// also update time
			if (slot.time > seq->seq->get_duration())	// just exit out
				slot.active = nullptr;
		}
		else if (slot.state == DirectAnimationSlot::Full) {
			slot.time += dt;
			if (slot.time > seq->seq->get_duration()) {
				slot.time = seq->seq->get_duration() - 0.0001;
				slot.fade_percentage = 1.0;
				slot.state = DirectAnimationSlot::FadingOut;
			}
		}
		else if (slot.state == DirectAnimationSlot::FadingOut) {
			slot.fade_percentage -= dt / slot.fade_duration;
			if (slot.fade_percentage < 0)
				slot.active = nullptr;
		}
	}


	// add physics driven bones
	// physics bones are in world space, wont cover every bone
	// 

	if (simulating_physics_objects.size() > 0) {
		std::vector<bool> is_simulating(num_bones(),0);	// fixme

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

		util_localspace_to_meshspace_with_physics(*pose_base.get(), cached_bonemats, is_simulating, get_skel());

	}
	else {
		util_localspace_to_meshspace(*pose_base.get(), cached_bonemats, get_skel());
	}
	
	// Callback
	if(!get_is_for_editor())
		on_post_update();

	ConcatWithInvPose();
}

void AnimatorInstance::add_simulating_physics_object(Entity* e)
{
	simulating_physics_objects.insert(e->get_self_ptr().handle);
}
void AnimatorInstance::remove_simulating_physics_object(Entity* e)
{
	simulating_physics_objects.erase(e->get_self_ptr().handle);
}


#include "Framework/DictParser.h"


SyncGroupData& AnimatorInstance::find_or_create_sync_group(StringName name)
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


bool AnimatorInstance::play_animation_in_slot(
	const AnimationSeqAsset* seq,
	StringName slot,
	float play_speed,
	float start_pos
)
{
	if (!model || !cfg || !seq)
		return false;

	auto slot_to_play_in = find_slot_with_name(slot);
	if (!slot_to_play_in) {
		sys_print(Warning, "no slot with name\n");
		return false;
	}
	
	slot_to_play_in->state = DirectAnimationSlot::FadingIn;
	slot_to_play_in->fade_percentage = 0.0;
	slot_to_play_in->active = seq;
	slot_to_play_in->time = 0.0;
	slot_to_play_in->playspeed = play_speed;

	return true;
}

DECLARE_ENGINE_CMD(print_animation_pools)
{
	//sys_print(Info, "Pose: %d/%d\n", Pose_Pool::get().head,Pose_Pool::get().poses.size());
	//sys_print(Info, "Matrix: %d/%d\n", Matrix_Pool::get().head, Matrix_Pool::get().matricies.size());
}