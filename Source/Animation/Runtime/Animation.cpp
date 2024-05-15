#include "Animation/Runtime/Animation.h"
#include "Framework/Util.h"
#include "Model.h"
#include "Game_Engine.h"
#include "Framework/Config.h"

#include <fstream>
#include <sstream>

using namespace glm;
#include <iostream>
#include <iomanip>

#include "Framework/ExpressionLang.h"
#include "Framework/MemArena.h"
#include "../AnimationUtil.h"
#include "../IGraphDriver.h"
#include "AnimationTreeLocal.h"
#include "../AnimationTreePublic.h"

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

void Animator::initialize_animator(
	const Model* model, 
	const Animation_Set_New* set, 
	const Animation_Tree_CFG* graph, 
	IAnimationGraphDriver* driver, 
	Entity* ent)
{
	ASSERT(model);
	ASSERT(set);

	if (!graph)
		return;

	runtime_dat.init_from_cfg(graph, model, set);
	this->driver = driver;
	this->owner = ent;

	if (driver) {
		driver->owner = this;
		driver->on_init();
	}

	cached_bonemats.resize(model->bones.size());
	matrix_palette.resize(model->bones.size());
}

#if 1



//from https://theorangeduck.com/page/simple-two-joint
// (A)
//	|\
//	| \
//  |  (B)
//	| /
//  (C)		(T)
static void SolveTwoBoneIK(const vec3& a, const vec3& b, const vec3& c, const vec3& target, const vec3& pole_vector,
	const glm::quat& a_global_rotation, const glm::quat& b_global_rotation,
	glm::quat& a_local_rotation, glm::quat& b_local_rotation)
{
	float eps = 0.01;
	float len_ab = length(b - a);
	float len_cb = length(b - c);
	float len_at = glm::clamp(length(target - a), eps, len_ab + len_cb - eps);

	// Interior angles of a and b
	float a_interior_angle = acos(dot(normalize(c - a), normalize(b - a)));
	float b_interior_angle = acos(dot(normalize(a - b), normalize(c - b)));
	float c_interior_angle = acos(dot(normalize(c - a), normalize(target - a)));

	// Law of cosines to get the desired angles of the triangle
	float a_desired_angle = acos(LawOfCosines(len_cb, len_ab, len_at));
	float b_desired_angle = acos(LawOfCosines(len_at, len_ab, len_cb));

	// Axis to rotate around
	vec3 axis0 = normalize(cross(c - a, b - a));
	vec3 axis1 = normalize(cross(c - a, target - a));
	glm::quat rot0 = glm::angleAxis(a_desired_angle - a_interior_angle, glm::inverse(a_global_rotation) * axis0);
	glm::quat rot1 = glm::angleAxis(b_desired_angle - b_interior_angle, glm::inverse(b_global_rotation) * axis0);
	glm::quat rot2 = glm::angleAxis(c_interior_angle, glm::inverse(a_global_rotation) * axis1);

	a_local_rotation = a_local_rotation * (rot0 * rot2);
	b_local_rotation = b_local_rotation * rot1;
}
#endif

void Animator::UpdateGlobalMatricies(const glm::quat localq[], const glm::vec3 localp[], std::vector<glm::mat4x4>& out_bone_matricies)
{
	auto model = get_model();

	for (int i = 0; i < model->bones.size(); i++)
	{
		glm::mat4x4 matrix = glm::mat4_cast(localq[i]);
		matrix[3] = glm::vec4(localp[i], 1.0);

		if (model->bones[i].parent == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(model->bones[i].parent < model->bones.size());
			out_bone_matricies[i] = out_bone_matricies[model->bones[i].parent] * matrix;
		}
	}
	for (int i = 0; i < model->bones.size(); i++)
		out_bone_matricies[i] = out_bone_matricies[i];
}

void util_localspace_to_meshspace(const Pose& local, std::vector<glm::mat4x4>& out_bone_matricies, const Model* model)
{
	for (int i = 0; i < model->bones.size(); i++)
	{
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);

		if (model->bones[i].parent == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(model->bones[i].parent < model->bones.size());
			out_bone_matricies[i] = out_bone_matricies[model->bones[i].parent] * matrix;
		}
	}
	for (int i = 0; i < model->bones.size(); i++)
		out_bone_matricies[i] =  out_bone_matricies[i];
}

void util_localspace_to_meshspace_ptr(const Pose& local, glm::mat4* out_bone_matricies, const Model* model)
{
	for (int i = 0; i < model->bones.size(); i++)
	{
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);

		if (model->bones[i].parent == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(model->bones[i].parent < model->bones.size());
			out_bone_matricies[i] = out_bone_matricies[model->bones[i].parent] * matrix;
		}
	}
	for (int i = 0; i < model->bones.size(); i++)
		out_bone_matricies[i] =  out_bone_matricies[i];
}

#include "imgui.h"

glm::vec3 dbgoffset = vec3(0.f);
void menu_2()
{
	ImGui::DragFloat3("dbg offs", &dbgoffset.x, 0.01);
}


void Animator::update_procedural_bones(Pose& pose)
{
	static bool first = true;
	if (first) {
		Debug_Interface::get()->add_hook("anim", menu_2);
		first = false;
	}

	// global meshspace transforms already calculated
	{
		int i = 0;
		for (; i < (int)bone_controller_type::max_count; i++) {
			if (bone_controllers[i].enabled) break;
		}
		if (i == (int)bone_controller_type::max_count) return;
	}

	Pose* preik = Pose_Pool::get().alloc(1);
	*preik = pose;

	glm::mat4* pre_ik_bonemats = Matrix_Pool::get().alloc(256);
	memcpy(pre_ik_bonemats, cached_bonemats.data(), sizeof(glm::mat4) * cached_bonemats.size());

	mat4 ent_transform = (owner) ? owner->get_world_transform() : mat4(1);
	ent_transform = ent_transform * get_model()->skeleton_root_transform;

	struct global_transform_set {
		int index;
		glm::mat3 rot;
	};
	int global_sets_count = 0;
	global_transform_set global_rot_sets[4];


	auto ikfunctor = [&](int joint0, int joint1, int joint2, vec3 target, bool print = false) {

		const float dist_eps = 0.0001f;
		vec3 a = cached_bonemats[joint2] * vec4(0.0, 0.0, 0.0, 1.0);
		vec3 b = cached_bonemats[joint1] * vec4(0.0, 0.0, 0.0, 1.0);
		vec3 c = cached_bonemats[joint0] * vec4(0.0, 0.0, 0.0, 1.0);
		float dist = length(c - target);
		if (dist <= dist_eps) {
			return;
		}

		Debug::add_sphere(ent_transform*vec4(a,1.0), 0.01, COLOR_GREEN, 0.0, true);
		Debug::add_sphere(ent_transform*vec4(b,1.0), 0.01, COLOR_BLUE, 0.0, true);
		Debug::add_sphere(ent_transform*vec4(c,1.0), 0.01, COLOR_CYAN, 0.0, true);
		glm::quat a_global = glm::quat_cast(cached_bonemats[joint2]);
		glm::quat b_global = glm::quat_cast(cached_bonemats[joint1]);
		util_twobone_ik(a, b, c, target, vec3(0.0, 0.0, 1.0), a_global, b_global, pose.q[joint2], pose.q[joint1]);
	};
	auto ik_find_bones = [&](int joint0_bone, vec3 target, const Model* m) {
		int joint1 = m->bones[joint0_bone].parent;
		assert(joint1 != -1);
		int joint2 = m->bones[joint1].parent;
		assert(joint2 != -1);
		ikfunctor(joint0_bone, joint1, joint2, target);
	};

	auto model = get_model();

	auto bone_update_functor = [&](Bone_Controller& bc) {
		if (bc.bone_index == -1)
			return;

		assert(bc.bone_index >= 0 && bc.bone_index < get_model()->bones.size());

		if (bc.use_two_bone_ik) {
			if (bc.target_relative_bone_index != -1) {
				assert(bc.target_relative_bone_index >= 0 && bc.target_relative_bone_index < get_model()->bones.size());
				// meshspace position of bone
				glm::vec3 meshspace_pos = (bc.use_bone_as_relative_transform) ?
					pre_ik_bonemats[bc.bone_index][3] : bc.position;
				// use global matrix of pre-postprocess
				glm::mat4 inv_relative_bone = glm::inverse(pre_ik_bonemats[bc.target_relative_bone_index]);
				// position of bone relative to the target bone in meshspace
				glm::mat4 rel_transform = inv_relative_bone * pre_ik_bonemats[bc.bone_index];
				glm::mat4 global_transform = cached_bonemats[bc.target_relative_bone_index] * rel_transform;
				// find final meshspace position
				glm::vec3 final_meshspace_pos = global_transform[3];
				
				ik_find_bones(bc.bone_index, final_meshspace_pos, model);
				//pose.q[bc.bone_index] *= glm::inverse(glm::quat_cast(cached_bonemats[bc.bone_index]))*glm::quat_cast(transform);
				global_rot_sets[global_sets_count++] = { bc.bone_index, mat3(global_transform) };
				// want to update local space rotation from global rotation
			}
			else {
				ik_find_bones(bc.bone_index, bc.position, model);
			}
		}
		else
		{
			if (bc.target_relative_bone_index != -1) {
				glm::vec3 meshspace_pos = (bc.use_bone_as_relative_transform) ?
					pre_ik_bonemats[bc.bone_index][3] : bc.position;
				glm::mat4 inv_relative_bone = glm::inverse(pre_ik_bonemats[bc.target_relative_bone_index]);
				glm::mat4 rel_transform = inv_relative_bone * pre_ik_bonemats[bc.bone_index];
				glm::mat4 global_transform = cached_bonemats[bc.target_relative_bone_index] * rel_transform;
				pose.pos[bc.bone_index] =  global_transform[3];
				pose.q[bc.bone_index] = glm::quat_cast(global_transform);
			}
			else if (bc.add_transform_not_replace) {
				pose.pos[bc.bone_index] += bc.position;
				pose.q[bc.bone_index] *= bc.rotation;
			}
			else {
				pose.pos[bc.bone_index] = bc.position;
				pose.q[bc.bone_index] = bc.rotation;
			}
		}
	};

	// firstpass
	for (int i = 0; i < (int)bone_controller_type::max_count; i++) {
		Bone_Controller& bc = bone_controllers[i];
		if (!bc.enabled || bc.evalutate_in_second_pass) continue;
		bone_update_functor(bc);
	}

	util_localspace_to_meshspace(pose, cached_bonemats, model);

	// second pass
	for (int i = 0; i < (int)bone_controller_type::max_count; i++) {
		Bone_Controller& bc = bone_controllers[i];
		if (!bc.enabled || !bc.evalutate_in_second_pass) continue;
		bone_update_functor(bc);
	}


	for (int i = 0; i < model->bones.size(); i++)
	{
		glm::mat4x4 matrix = glm::mat4_cast(pose.q[i]);
		matrix[3] = glm::vec4(pose.pos[i], 1.0);

		if (model->bones[i].parent == ROOT_BONE) {
			cached_bonemats[i] = matrix;
		}
		else {
			assert(model->bones[i].parent < model->bones.size());
			cached_bonemats[i] = cached_bonemats[model->bones[i].parent] * matrix;
			for (int j = 0; j < global_sets_count; j++) {
				if (i == global_rot_sets[j].index) {
					vec4 p = cached_bonemats[i][3];
					global_rot_sets[j].rot = transpose(global_rot_sets[j].rot);
					global_rot_sets[j].rot[0] = normalize(global_rot_sets[j].rot[0]);
					global_rot_sets[j].rot[1] = normalize(global_rot_sets[j].rot[1]);
					global_rot_sets[j].rot[2] = normalize(global_rot_sets[j].rot[2]);
					global_rot_sets[j].rot = transpose(global_rot_sets[j].rot);

					cached_bonemats[i] = global_rot_sets[j].rot;
					cached_bonemats[i][3] = p;
					break;
				}
			}
		}
	}


	Pose_Pool::get().free(1);
	Matrix_Pool::get().free(256);
}



void Animator::ConcatWithInvPose()
{
	ASSERT(get_model());

	auto model = get_model();

	for (int i = 0; i < model->bones.size(); i++) {
		matrix_palette[i] = cached_bonemats[i] * glm::mat4(model->bones[i].invposematrix);
	}
}



//#pragma optimize( "", on )

// source = source-reference


PoseMask::PoseMask()
{

}
Animator::Animator() : slots(1)
{

}

static vector<int> get_indicies(const Animation_Set* set, const vector<const char*>& strings)
{
	vector<int> out;
	for (auto s : strings) out.push_back(set->find(s));
	return out;
}

#include "AnimationTreeLocal.h"


float ym0 = 0.09;
float ym1 = 0.3;
float xm0 = 0.3;
float xm1 = 0.3;
float lerp_rot = 0.2;
float g_fade_out = 0.2;
float g_walk_fade_in = 2.0;
float g_walk_fade_out = 3.0;
float g_run_fade_in = 4.0;
float g_dir_blend = 0.025;

#include "imgui.h"
void menu()
{
	ImGui::DragFloat("ym0", &ym0, 0.001);
	ImGui::DragFloat("ym1", &ym1, 0.001);
	ImGui::DragFloat("xm0", &xm0, 0.001);
	ImGui::DragFloat("xm1", &xm1, 0.001);
	ImGui::DragFloat("lerprot", &lerp_rot, 0.0005, 0.0f,1.f);
	ImGui::DragFloat("g_fade_out", &g_fade_out, 0.005);
	ImGui::DragFloat("g_walk_fade_in", &g_walk_fade_in, 0.05);
	ImGui::DragFloat("g_walk_fade_out", &g_walk_fade_out, 0.05);
	ImGui::DragFloat("g_run_fade_in", &g_run_fade_in, 0.05);
	ImGui::DragFloat("g_dir_blend", &g_dir_blend, 0.01);


//	ImGui::DragFloat("g_frame_force", &g_frame_force, 0.0005);
}



char get_first_token(string& s, char default_='\0')
{
	for (auto c : s) {
		if (c != ' ' && c != '\t' && c != '\n') return c;
	}
	return default_;
}



// LispInterpreter.cpp
extern vector<string> to_tokens(string& input);

const int STREAM_WIDTH = 9;
const int PRECISION_STREAM = 3;
std::ostream& operator<<(std::ostream& out, glm::vec3 v){
	out << std::setw(STREAM_WIDTH) << std::setprecision(PRECISION_STREAM) << v.x  << " " 
		<< std::setw(STREAM_WIDTH) << std::setprecision(PRECISION_STREAM) << v.y << " "
		<< std::setw(STREAM_WIDTH) << std::setprecision(PRECISION_STREAM) << v.z << " ";
	return out;
}
std::ostream& operator<<(std::ostream& out, glm::quat v){
	out << std::setw(STREAM_WIDTH) << std::setprecision(PRECISION_STREAM) << v.w  << " "
		<< std::setw(STREAM_WIDTH) << std::setprecision(PRECISION_STREAM) << v.x << " "
		<< std::setw(STREAM_WIDTH) << std::setprecision(PRECISION_STREAM) << v.y << " "
		<< std::setw(STREAM_WIDTH) << std::setprecision(PRECISION_STREAM) << v.z << " ";
	return out;
}



void Animator::tick_tree_new(float dt)
{
	Pose* poses = Pose_Pool::get().alloc(2);

	script_value_t stack[64];

	NodeRt_Ctx ctx;
	ctx.stack = stack;
	ctx.stack_size = 64;
	ctx.tree = &runtime_dat;
	ctx.model = get_model();
	ctx.set = get_set();
	ctx.param_cfg = runtime_dat.cfg->params.get();
	GetPose_Ctx gp_ctx;
	gp_ctx.dt = dt;
	gp_ctx.pose = &poses[0];

	if(driver)
		driver->on_update(dt);

	if (get_tree()&& get_tree()->root)
		get_tree()->root->get_pose(ctx, gp_ctx);
	else
		util_set_to_bind_pose(poses[0], get_model());

	util_localspace_to_meshspace(poses[0], cached_bonemats, get_model());

	if(driver)
		driver->pre_ik_update(poses[0], dt);

	update_procedural_bones(poses[0]);

	if(driver)
		driver->post_ik_update();

	ConcatWithInvPose();

	Pose_Pool::get().free(2);
}


void Animation_Tree_RT::init_from_cfg(const Animation_Tree_CFG* cfg, const Model* model, const Animation_Set_New* set)
{
	this->cfg = cfg;
	this->model = model;
	this->set = set;

	vars.resize(cfg->graph_program ? cfg->graph_program->num_vars() : 0);

	data.clear();
	data.resize(cfg->data_used, 0);
	NodeRt_Ctx ctx;
	ctx.model = model;
	ctx.set = set;
	ctx.tree = this;
	ctx.param_cfg = cfg->params.get();
	ctx.tick = 0;
	for (int i = 0; i < cfg->all_nodes.size(); i++)
		cfg->all_nodes[i]->construct(ctx);

	if(cfg->root)
		cfg->root->reset(ctx);


}

#include "Framework/DictParser.h"
Animation_Tree_CFG* Animation_Tree_Manager::find_animation_tree(const char* n) {
	if (trees.find(n) != trees.end())
		return &trees[n];
	
	DictParser parser;
	return load_animation_tree_file(n, parser);
}

void Animation_Set_New::find_animation(const char* name, int16_t* out_set, int16_t* out_index, int16_t* skel_idx) const
{
	const auto& find = table.find(name);
	if (find != table.end()) {
		name = find->second.c_str();
	}

	// read backwards to allow overloading inherited
	for (int i = imports.size() - 1; i >= 0; i--) {
		int index = imports[i].mod->animations->find(name);
		if (index != -1) {
			*out_set = i;
			*out_index = index;

			if (imports[i].import_skeleton)
				*skel_idx = imports[i].import_skeleton->find_remap(src_skeleton);

			return;
		}
	}
	*out_set = -1;
	*out_index = -1;
	*skel_idx = -1;
}

const Animation_Set* Animation_Set_New::get_subset(uint32_t index) const {
	return imports[index].mod->animations.get();
}

static Animation_Tree_Manager anim_tree_man__;
Animation_Tree_Manager* anim_tree_man = &anim_tree_man__;
