#include "Animation.h"
#include "Util.h"
#include "Model.h"
#include "Game_Engine.h"
#include "Config.h"

#include <fstream>
#include <sstream>

using namespace glm;
#include <iostream>
#include <iomanip>

#include "LispInterpreter.h"

#include "MemArena.h"

#include "AnimationUtil.h"

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

void Animator_Layer::update(float elapsed, const Animation& clip)
{
	frame += clip.fps * elapsed * speed;
	if (frame > clip.total_duration || frame < 0.f) {
		if (loop)
			frame = fmod(fmod(frame, clip.total_duration) + clip.total_duration, clip.total_duration);
		else {
			frame = clip.total_duration - 0.001f;
			finished = true;
		}
	}
	if (blend_anim != -1) {
		blend_remaining -= elapsed;
		if (blend_remaining <= 0) {
			blend_anim = -1;
		}
	}
}

void Animator::AdvanceFrame(float elapsed)
{
	if (!model || !model->animations)
		return;

	int num_clips = model->animations->clips.size();

	if (m.anim < 0 || m.anim >= num_clips)
		m.anim = -1;
	if (legs.anim < 0 || legs.anim >= num_clips)
		legs.anim = -1;

	if (m.anim != -1) {
		const Animation& clip = model->animations->clips[m.anim];
		m.update(elapsed, clip);
	}
	else
		m.finished = true;
	if (legs.anim != -1) {
		const Animation& clip = model->animations->clips[legs.anim];
		legs.update(elapsed, clip);
	}
	else
		legs.finished = true;
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

#if 0
void AnimationController::DoHandIK(glm::quat localq[], glm::vec3 localp[], std::vector<glm::mat4x4>& globalbonemats)
{
	const int hand = model->BoneForName("hand.R");
	const int elbow = model->BoneForName("forearm.R");
	const int shoulder = model->BoneForName("upper_arm.R");
	if (hand == -1 || elbow == -1 || shoulder == -1)
		return;

	const vec3 target = vec3(0, 50.0 + sin(host.realtime) * 10.0, 20.0);
	const vec3 a = globalbonemats[shoulder] * vec4(0.0, 0.0, 0.0, 1.0);
	const vec3 b = globalbonemats[elbow] * vec4(0.0, 0.0, 0.0, 1.0);
	const vec3 c = globalbonemats[hand] * vec4(0.0, 0.0, 0.0, 1.0);

	const glm::quat a_global = glm::quat_cast(globalbonemats[shoulder]);
	const glm::quat b_global = glm::quat_cast(globalbonemats[elbow]);


	SolveTwoBoneIK(a, b, c, target, vec3(0.0), a_global, b_global, localq[shoulder], localq[elbow]);

	UpdateGlobalMatricies(localq, localp, globalbonemats);
}
void AnimationController::DoPlayerHandToGunIK(glm::quat localq[], glm::vec3 localp[], std::vector<glm::mat4x4>& globalbonemats)
{
	const int hand = model->BoneForName("hand.R");
	const int elbow = model->BoneForName("forearm.R");
	const int shoulder = model->BoneForName("upper_arm.R");
	if (hand == -1 || elbow == -1 || shoulder == -1)
		return;


	const GfxModel* gun = ModForName("M16.bmd");
	if (!gun)
		return;
	int grip_idx = -1;
	if (actor_owner->use_front_grip)
		grip_idx = gun->AttachmentForName("ap_frontgrip");
	else
		grip_idx = gun->AttachmentForName("ap_backgrip");
	if (grip_idx == -1)
		return;
	const ModelAttachment& gripAp = gun->attachments[grip_idx];


	int attach_index = actor_owner->model->AttachmentForName("ap_weapon");
	if (attach_index == -1)
		return;
	const ModelAttachment& ap = actor_owner->model->attachments[attach_index];
	glm::mat4 localTransform = ap.transform;
	if (ap.bone_parent != -1)
	{
		// GetBones() hasnt been multipled by the inverse bindpose yet, rn they are bonespace->meshspace (no need to multiply w/ bind pose like later)
		localTransform = mat4(this->GetBones()[ap.bone_parent]) * mat4(ap.transform) * mat4(gripAp.transform);
	}


	const glm::vec3 target = localTransform[3];
	const vec3 a = globalbonemats[shoulder] * vec4(0.0, 0.0, 0.0, 1.0);
	const vec3 b = globalbonemats[elbow] * vec4(0.0, 0.0, 0.0, 1.0);
	const vec3 c = globalbonemats[hand] * vec4(0.0, 0.0, 0.0, 1.0);

	const glm::quat a_global = glm::quat_cast(globalbonemats[shoulder]);
	const glm::quat b_global = glm::quat_cast(globalbonemats[elbow]);


	SolveTwoBoneIK(a, b, c, target, vec3(0.0), a_global, b_global, localq[shoulder], localq[elbow]);

	UpdateGlobalMatricies(localq, localp, globalbonemats);
}
#endif
void Animator::UpdateGlobalMatricies(const glm::quat localq[], const glm::vec3 localp[], std::vector<glm::mat4x4>& out_bone_matricies)
{
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


void Animator::SetupBones()
{
	if (tree) {
		evaluate_new(eng->tick_interval);
		return;
	}
	
	ASSERT(model->animations && model);
	ASSERT(cached_bonemats.size() == model->bones.size());


	if (m.anim < 0 || m.anim >= model->animations->clips.size())
		m.anim = -1;
	if (legs.anim < 0 || legs.anim >= model->animations->clips.size())
		legs.anim = -1;
	if (m.blend_anim < 0 || m.blend_anim >= model->animations->clips.size())
		m.blend_anim = -1;
	if (legs.blend_anim < 0 || legs.blend_anim >= model->animations->clips.size())
		legs.blend_anim = -1;
	
	//static Config_Var* disable_blend = cfg.get_var("disable_blend", "0");
	//if(disable_blend->integer)
	//	m.blend_anim = legs.blend_anim = -1;

	// Just t-pose if no proper animations
	if (m.anim == -1)
	{
		for (int i = 0; i < cached_bonemats.size(); i++)
			cached_bonemats[i] = model->bones[i].posematrix;
		return;
	}

	Pose* pose = Pose_Pool::get().alloc(1);

	util_calc_rotations(set, m.frame, m.anim ,model, *pose);

	update_procedural_bones(*pose);

	// Setup main layer
	//CalcRotations(q, pos, m.anim, m.frame);
	//if (m.blend_anim != -1)
	//{
	//	static glm::quat q2[MAX_BONES];
	//	static glm::vec3 pos2[MAX_BONES];
	//	CalcRotations(q2, pos2, m.blend_anim, m.blend_frame);
	//	float frac = m.blend_remaining / m.blend_time;
	//	LerpTransforms(q, pos, q2, pos2, frac, model->bones.size());
	//}

	//if (legs.anim != -1)
	//	add_legs_layer(q, pos);

	Pose_Pool::get().free(1);

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

	// get global meshspace transforms
	UpdateGlobalMatricies(pose.q, pose.pos, cached_bonemats);
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
	ent_transform = ent_transform * model->skeleton_root_transform;

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

	auto bone_update_functor = [&](Bone_Controller& bc) {
		assert(bc.bone_index >= 0 && bc.bone_index < model->bones.size());

		if (bc.use_two_bone_ik) {
			if (bc.target_relative_bone_index != -1) {
				assert(bc.target_relative_bone_index >= 0 && bc.target_relative_bone_index < model->bones.size());
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



#if 0
void Animator::LerpTransforms(glm::quat q1[], vec3 p1[], glm::quat q2[], glm::vec3 p2[], float factor, int numbones)
{
	for (int i = 0; i < numbones; i++)
	{
		q1[i] = glm::slerp(q1[i], q2[i], factor);
		q1[i] = glm::normalize(q1[i]);
		p1[i] = glm::mix(p1[i], p2[i], factor);
	}
}

void Animator::add_legs_layer(glm::quat finalq[], glm::vec3 finalp[])
{
	static glm::quat q1[MAX_BONES];
	static glm::vec3 p1[MAX_BONES];

	CalcRotations(q1, p1, legs.anim, legs.frame);

	if (legs.blend_anim != -1)
	{
		static glm::quat q2[MAX_BONES];
		static glm::vec3 pos2[MAX_BONES];
		CalcRotations(q2, pos2, legs.blend_anim, legs.blend_frame);
		float frac = legs.blend_remaining / legs.blend_time;
		LerpTransforms(q1, p1, q2, pos2, frac, model->bones.size());
	}

	const int root_loc = model->bone_for_name("mixamorig:Hips");
	const int thigh_loc = model->bone_for_name("mixamorig:LeftUpLeg");
	const int spine_loc = model->bone_for_name("mixamorig:Spine");
	const int toe_end = model->bone_for_name("mixamorig:RightToe_End");



	if (root_loc == -1 || thigh_loc == -1 || spine_loc == -1 || toe_end == -1) {
		printf("Couldn't find spine/root bones\n");
		legs.anim = -1;
		return;
	}

	bool copybones = false;
	// Now overwrite only legs + root + low spine
	for (int i = 0; i < model->bones.size(); i++)
	{
		const Bone& bone = model->bones[i];
		if (i == thigh_loc)
			copybones = true;
		if (copybones || i == root_loc || i == spine_loc) {
			finalq[i] = q1[i];
			finalp[i] = p1[i];
		}
		if (i == toe_end)
			copybones = false;
	}
}
#endif

void Animator::ConcatWithInvPose()
{
	ASSERT(model);
	for (int i = 0; i < model->bones.size(); i++) {
		matrix_palette[i] = cached_bonemats[i] * glm::mat4(model->bones[i].invposematrix);
	}
}


void Animator::set_model(const Model* mod)
{
	// can be called with nullptr model to reset
	if (mod && (!mod->animations || mod->bones.size() != mod->animations->num_channels)) {
		sys_print("Model %s has no animations or invalid skeleton\n", mod->name.c_str());
		mod = nullptr;
	}
	model = mod;
	if (model) {
		set = model->animations.get();
		cached_bonemats.resize(model->bones.size());
		matrix_palette.resize(model->bones.size());
	}

	legs = Animator_Layer();
	m = Animator_Layer();

	set_model_new(mod);
}

void Animator::set_anim(const char* name, bool restart, float blend)
{
	if (!model) return;
	int animation = set->find(name);
	set_anim_from_index(m,animation, restart, blend);
}

void Animator::set_anim_from_index(Animator_Layer& l, int animation, bool restart, float blend)
{
	if (animation != l.anim || restart) {
		if (blend > 0.f) {
			l.blend_anim = l.anim;
			l.blend_frame = l.frame;
			l.blend_remaining = blend;
			l.blend_time = blend;	// FIXME
		}
		l.anim = animation;
		l.frame = 0.0;
		l.finished = false;
		l.speed = 1.f;
	}
}

void Animator::set_leg_anim(const char* name, bool restart, float blend)
{
	if (!model) return;
	int animation = set->find(name);
	set_anim_from_index(legs, animation, restart, blend);
}


//#pragma optimize( "", on )

// source = source-reference


PoseMask::PoseMask()
{

}
Animator::Animator()
	
{

}

static vector<int> get_indicies(const Animation_Set* set, const vector<const char*>& strings)
{
	vector<int> out;
	for (auto s : strings) out.push_back(set->find(s));
	return out;
}

#include "AnimationTreeLocal.h"



#if 0
struct Boolean_Blend_Node : public At_Node
{
	using At_Node::At_Node;

	virtual bool get_pose(Pose& pose, float dt) override {
		//bool b = LispLikeInterpreter::eval(exp.get(), &animator->tree->ctx).asexp().asatom().i;
		bool b = exp.execute().i;
		if (b)
			value += dt / blendin;
		else
			value -= dt / blendin;

		value =glm::clamp(value, 0.0f, 1.0f);

		//printf("boolean value: %f\n", value);

		if (value < 0.00001f)
			return nodes[0]->get_pose(pose, dt);
		else if (value >= 0.99999f)
			return nodes[1]->get_pose(pose, dt);
		else {
			Pose* pose2 = Pose_Pool::get().alloc(1);

			bool r = nodes[0]->get_pose(pose, dt);
			nodes[1]->get_pose(*pose2, dt);

			util_blend(animator->model->bones.size(), *pose2, pose, value);
			Pose_Pool::get().free(1);
			return r;
		}
	}
	virtual void reset() override {
		value = 0.0;
		nodes[0]->reset();
		nodes[1]->reset();
	}
	LispBytecode exp;
	At_Node* nodes[2];
	float value = 0.f;
	float blendin = 0.2;
};
#endif

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

#if 0

struct AnimationTreeLoadContext
{
	Animation_Tree_CFG* tree = nullptr;

	template<typename T>
	T* allocate() {
		return (T*)tree->arena.alloc_bottom(sizeof(T));
	}


	Node_CFG* add_node(Node_CFG* node) {
		tree->all_nodes.push_back(node);
		return node;
	}
	State* find_state(const string& name) {
		if (states.find(name) == states.end()) {
			auto s = allocate<State>();
			s = new(s)State;
			states[name] = s;
			return s;
		}

		return states[name];
	}

	std::unordered_map<std::string, State*> states;
};

static AnimationTreeLoadContext at_ctx;

LispExp root_func(LispArgs args)
{
	for (int i = 0; i < args.count(); i++) {
		if (args.args[i].type == LispExp::animation_tree_node_type) {
			at_ctx.tree->root = (At_Node*)args.args[i].u.ptr;
			break;
		}
	}

	return LispExp(0);
}

LispExp clip_func(LispArgs args)
{
	auto clip = at_ctx.allocate<Clip_Node_CFG>();
	clip = new(clip)Clip_Node_CFG(args.at(0).as_sym().c_str(), at_ctx.tree);

	for (int i = 1; i < args.count(); i++) {
		auto& cmd = args.at(i).as_sym();
		if (cmd == ":rootx") {
			auto& x = args.at(i + 1).as_sym();
			if (x == "del") clip->rootmotion[0] = Clip_Node_CFG::Remove;
			i += 1;
		}
		else if (cmd == ":rooty") {
			auto& x = args.at(i + 1).as_sym();
			if (x == "del") clip->rootmotion[1] = Clip_Node_CFG::Remove;
			i += 1;
		}
		else if (cmd == ":rootz") {
			auto& x = args.at(i + 1).as_sym();
			if (x == "del") clip->rootmotion[2] = Clip_Node_CFG::Remove;
			i += 1;
		}
		else if (cmd == ":loop") {
			auto& x = args.at(i + 1).as_sym();
			if (x == "false") clip->loop = false;
			i += 1;
		}
		else if (cmd == ":rate") {
			clip->speed = args.at(i + 1).cast_to_float();
			i += 1;
		}
	}
	at_ctx.add_node(clip);
	return LispExp(LispExp::animation_tree_node_type, clip);
}

LispExp defstate_func(LispArgs args)
{
	const string& statename = args.at(0).as_sym();
	State* s = at_ctx.find_state(statename);
	s->name = statename.c_str();

	for (int i = 1; i < args.count(); i++) {
		const string& sym = args.at(i).as_sym();
		if (sym == ":tree") {
			s->tree = (Node_CFG*)args.at(i + 1).as_custom_type(LispExp::animation_tree_node_type);
			i += 1;
		}
		else if (sym == ":duration") {
			s->state_duration = args.at(i + 1).cast_to_float();
			i += 1;
		}
		else if (sym == ":transition-done") {
			s->next_state = at_ctx.find_state(args.at(i+1).as_sym());
			i += 1;
		}
		else if (sym == ":transitions") {
			auto& list = args.at(i + 1).as_list();
			for (int j = 0; j < list.size(); j++) {
				s->transitions.push_back(
					*(State_Transition*)list.at(j).as_custom_type(LispExp::animation_transition_type)
				);
				delete list.at(j).u.ptr;
			}
			i += 1;
		}
		else assert(0);
	}

	return LispExp(0);
}

LispExp transition_state(LispArgs args)
{
	State_Transition* st = new State_Transition;
	st->transition_state = at_ctx.find_state(args.at(0).as_sym());
	st->compilied.compile(args.at(1), *at_ctx.tree->control_param_vars);	// evaluate quoted
	return LispExp(LispExp::animation_transition_type, st);
}

LispExp move_directional_blend_func(LispArgs args)
{
	auto db = new(at_ctx.allocate<Directionalblend_node_CFG>())Directionalblend_node_CFG(at_ctx.tree);

	for (int i = 0; i < args.count(); i++) {
		auto& sym = args.args[i].as_sym();
		if (sym == ":walk") {
			auto& list = args.at(++i).as_list();
			for (int j = 0; j < 8; j++) {
				Node_CFG* node = (Node_CFG*)list.at(j).as_custom_type(LispExp::animation_tree_node_type);
				if (!node->is_clip_node()) throw "needs clip node type for db walk";
				db->walk_directions[j] = (Clip_Node_CFG*)node;
			}
		}
		//else if (sym == ":run") {
		//	auto& list = args.at(++i).as_list();
		//	for (int j = 0; j < 8; j++) {
		//		Node_CFG* node = (Node_CFG*)list.at(j).as_custom_type(LispExp::animation_tree_node_type);
		//		if (!node->is_clip_node()) throw "needs clip node type for db run";
		//		db->run_directions[j] = (Clip_Node_CFG*)node;
		//	}
		//}
		else if (sym == ":idle") {
			Node_CFG* node = (Node_CFG*)args.at(++i).as_custom_type(LispExp::animation_tree_node_type);
			if (!node->is_clip_node()) throw "needs clip node type for db idle";
			db->idle = (Clip_Node_CFG*)node;
		}
	}
	return LispExp(LispExp::animation_tree_node_type, at_ctx.add_node(db));
}

LispExp statemachine_node(LispArgs args)
{
	auto sm = new(at_ctx.allocate<Statemachine_Node_CFG>())Statemachine_Node_CFG(at_ctx.tree);


	sm->start_state = at_ctx.find_state(args.at(0).as_sym());
	return LispExp(LispExp::animation_tree_node_type, at_ctx.add_node(sm));
}

LispExp additive_node(LispArgs args)
{
	auto addnode = new(at_ctx.allocate<Add_Node_CFG>())Add_Node_CFG(at_ctx.tree);

	addnode->base_pose = (Node_CFG*)args.at(0).as_custom_type(LispExp::animation_tree_node_type);
	addnode->diff_pose = (Node_CFG*)args.at(1).as_custom_type(LispExp::animation_tree_node_type);

	addnode->compilied.compile( args.at(2) , *at_ctx.tree->control_param_vars);
	
	return LispExp(LispExp::animation_tree_node_type, at_ctx.add_node(addnode));
}

LispExp sub_node_create(LispArgs args)
{
	auto subnode = new(at_ctx.allocate<Subtract_Node_CFG>())Subtract_Node_CFG(at_ctx.tree);

	subnode->source = (Node_CFG*)args.at(0).as_custom_type(LispExp::animation_tree_node_type);
	subnode->ref = (Node_CFG*)args.at(1).as_custom_type(LispExp::animation_tree_node_type);
	return LispExp(LispExp::animation_tree_node_type, at_ctx.add_node(subnode));
}
LispExp blend_node_create(LispArgs args)
{
	auto node = new(at_ctx.allocate<Blend_Node_CFG>())Blend_Node_CFG(at_ctx.tree);

	node->posea = (Node_CFG*)args.at(0).as_custom_type(LispExp::animation_tree_node_type);
	node->poseb = (Node_CFG*)args.at(1).as_custom_type(LispExp::animation_tree_node_type);
	node->compilied.compile( args.at(2) , *at_ctx.tree->control_param_vars);
	return LispExp(LispExp::animation_tree_node_type, at_ctx.add_node(node));
}

LispExp mirror_node_create(LispArgs args)
{
	auto node = new(at_ctx.allocate<Mirror_Node_CFG>())Mirror_Node_CFG(at_ctx.tree);


	for (int i = 0; i < args.argc - 1; i++) {
		string& sym = args.args[i].as_sym();
		if (sym == ":lerp")
			node->mirror_lerp_time = args.at(++i).cast_to_float();
		else if(sym == ":exp")
			node->exp.compile( args.at(++i), &at_ctx.tree->script_vars);
	}

	node->input = (Node_CFG*)args.at(args.argc - 1).as_custom_type(LispExp::animation_tree_node_type);
	return LispExp(LispExp::animation_tree_node_type, at_ctx.add_node(node));
}
#endif


char get_first_token(string& s, char default_='\0')
{
	for (auto c : s) {
		if (c != ' ' && c != '\t' && c != '\n') return c;
	}
	return default_;
}



// LispInterpreter.cpp
extern vector<string> to_tokens(string& input);
void load_mirror_remap(Model* model, const char* path)
{
	std::ifstream infile(path);
	string line;
	for (int i = 0; i < model->bones.size(); i++) model->bones[i].remap_index = i;
	while (std::getline(infile, line)) {
		auto tokens = to_tokens(line);
		int right = model->bone_for_name(tokens.at(0).c_str());
		int left = model->bone_for_name(tokens.at(1).c_str());
		ASSERT(right != -1 && left != -1);
		model->bones[right].remap_index = left;
		model->bones[left].remap_index = right;
	}
}

#if 0
class Animation_Data_Manager
{
public:
	Animation_Data_Manager() {
		def_params = set_params(&def_ext_vars);
	}
	Control_Params set_params(ByteCodeExternalVars_CFG* vars)
	{
		Control_Params param;
		param.crouching		= vars->set("$crouching",	stack_val_type::int_t);
		param.standing		= vars->set("$standing",	stack_val_type::int_t);
		param.relmovedir_x	= vars->set("$relmovex",	stack_val_type::float_t);
		param.relmovedir_y	= vars->set("$relmovey",	stack_val_type::float_t);

		return param;
	}

	Animation_Tree_CFG* load_animation_tree(const char* path) {
		auto find = trees.find(path);
		if (find != trees.end()) return &find->second;
		Animation_Tree_CFG* tree = &trees[path];

		tree->param = def_params;
		tree->control_param_vars = &def_ext_vars;
	
		std::ifstream infile(path);
		std::string line;
		string fullcode;
		while (std::getline(infile, line)) {
			if (get_first_token(line) != ';') {
				fullcode += line;
				fullcode += ' ';
			}
		}

		auto env = LispLikeInterpreter::get_global_env();
		env.symbols["root"].assign(root_func);
		env.symbols["clip"].assign(clip_func);
		env.symbols["move-directional-blend"].assign(move_directional_blend_func);
		env.symbols["def-state"].assign(defstate_func);
		env.symbols["transition"].assign(transition_state);
		env.symbols["statemachine"].assign(statemachine_node);
		env.symbols["additive"].assign(additive_node);
		env.symbols["subtract"].assign(sub_node_create);
		env.symbols["blend"].assign(blend_node_create);
		env.symbols["mirror"].assign(mirror_node_create);

		at_ctx.tree = tree;
		at_ctx.states.clear();

		Interpreter_Ctx ctx = Interpreter_Ctx(64, &env);

		try {
			LispExp root = LispLikeInterpreter::parse(fullcode);
			LispExp val = LispLikeInterpreter::eval(root, &ctx);
		}
		catch (LispError e) {
			printf("error occured %s\n", e.msg);

			return nullptr;
		}

		return tree;
	}
	Animation_Tree_RT* get_runtime_tree(const char* path, Model* model, Animation_Set* set) {
		load_mirror_remap(const_cast<Model*>(model), "./Data/Animations/remap.txt");	// FIXME

		Animation_Tree_CFG* cfg_tree = load_animation_tree(path);

		Animation_Tree_RT* runtime = new Animation_Tree_RT;
		runtime->cfg = cfg_tree;
		runtime->data.resize(cfg_tree->data_used);
		runtime->runtime_vars.vars.resize(cfg_tree->control_param_vars->vals.size());
		NodeRt_Ctx ctx;
		ctx.model = model;
		ctx.set = set;
		ctx.tree = runtime;
		ctx.param = &cfg_tree->param;
		ctx.vars = &runtime->runtime_vars;
		for (int i = 0; i < cfg_tree->all_nodes.size(); i++) {
			cfg_tree->all_nodes[i]->construct(ctx);
		}
		return runtime;
	}

	Control_Params def_params;
	ByteCodeExternalVars_CFG def_ext_vars;
	std::unordered_map<std::string, Animation_Tree_CFG> trees;
};

static Animation_Data_Manager anim_man;
#endif
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


void Animator::postprocess_animation(Pose& pose, float dt)
{
	Pose* source = Pose_Pool::get().alloc(1);
	*source = pose;

	util_localspace_to_meshspace(pose, cached_bonemats, model);
	const int rhand = model->bone_for_name("mixamorig:RightHand");
	const int lhand = model->bone_for_name("mixamorig:LeftHand");
	glm::vec3 rhand_target = cached_bonemats[rhand] * vec4(0.0, 0.0, 0.0, 1.0);
	glm::vec3 lhand_target = cached_bonemats[lhand] * vec4(0.0, 0.0, 0.0, 1.0);
	//hand_target += vec3(0.f, sin(GetTime()*0.2)*5.f, 20.0f * sin(GetTime()*0.5));
	glm::mat4 ent_transform = owner->get_world_transform()*model->skeleton_root_transform;
	{
		glm::vec3 world_hand = ent_transform * glm::vec4(rhand_target, 1.0);
		Debug::add_sphere(world_hand, 0.01, COLOR_RED, 0.0, true);
		world_hand = ent_transform * glm::vec4(lhand_target, 1.0);
		Debug::add_sphere(world_hand, 0.01, COLOR_RED, 0.0, true);
	}

	// procedural acceleration rotation
	//{
		glm::vec3 rotation_dir = glm::normalize(vec3(atan(in.relaccel.y*ym0)*ym1, atan(in.relaccel.x*xm0)*xm1, 1.0f));
		glm::vec3 worldspace = mat3(ent_transform) * vec3(rotation_dir);

		Debug::add_line(owner->position, 
			owner->position + vec3(worldspace.x,0.f,worldspace.z)*1000.f, 
			COLOR_PINK, 0.f);

		glm::quat q = glm::quat(rotation_dir, vec3(0,0,1));
		//in.player_rot_from_accel = glm::slerp(q, in.player_rot_from_accel, lerp_rot);
		if (in.reset_accel) {
			in.reset_accel = false;
			in.player_rot_from_accel = q;
		}
		else
			in.player_rot_from_accel = damp_dt_independent(q, in.player_rot_from_accel, lerp_rot, dt);

		pose.q[model->root_bone_index] = in.player_rot_from_accel * pose.q[model->root_bone_index];
	//}
	util_localspace_to_meshspace(pose, cached_bonemats, model);


	// fix up hand ik
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
	{
		const int relbow = model->bone_for_name("mixamorig:RightForeArm");
		const int rshoulder = model->bone_for_name("mixamorig:RightArm");
		const int lelbow = model->bone_for_name("mixamorig:LeftForeArm");
		const int lshoulder = model->bone_for_name("mixamorig:LeftArm");

		ikfunctor(rhand, relbow, rshoulder, rhand_target);
		ikfunctor(lhand, lelbow, lshoulder, lhand_target);
	}

	// feet ik
	if(!in.ismoving && !in.injump)
	{
		const int rightfoot = model->bone_for_name("mixamorig:RightFoot");
		const int rightleg = model->bone_for_name("mixamorig:RightLeg");
		const int rightlegupper = model->bone_for_name("mixamorig:RightUpLeg");
		const int leftfoot = model->bone_for_name("mixamorig:LeftFoot");
		const int leftleg = model->bone_for_name("mixamorig:LeftLeg");
		const int leftlegupper = model->bone_for_name("mixamorig:LeftUpLeg");

		glm::vec3 worldspace_rfoot = ent_transform * cached_bonemats[rightfoot] * vec4(0.f, 0.f, 0.f, 1.f);
		glm::vec3 worldspace_lfoot = ent_transform * cached_bonemats[leftfoot] * vec4(0.f, 0.f, 0.f, 1.f);
		Ray r;
		r.pos = worldspace_rfoot+vec3(0,2,0);
		r.dir = vec3(0, -1, 0);
		RayHit hit = eng->phys.trace_ray(r, -1, PF_WORLD);
		Debug::add_box(hit.pos, vec3(0.2), COLOR_PINK, 0.f);

		float rfootheight = hit.pos.y - owner->position.y;
		r.pos = worldspace_lfoot + vec3(0, 2, 0);
		hit = eng->phys.trace_ray(r, -1, PF_WORLD);

		Debug::add_box(hit.pos, vec3(0.2), COLOR_PINK, 0.f);

		float lfootheight = hit.pos.y - owner->position.y;
		// now need to offset mesh so that both hiehgts are >= 0
		float add = glm::min(lfootheight, rfootheight);

		//printf("add %f\n", add);
		
		out.meshoffset = glm::vec3(0, add, 0);

		glm::mat4 invent = glm::inverse(ent_transform);

		// now do ik for left and right feet
		glm::vec3 lfoottarget = invent * vec4(worldspace_lfoot + vec3(0, lfootheight - add, 0), 1.f);
		glm::vec3 rfoottarget = invent * vec4(worldspace_rfoot + vec3(0, rfootheight - add, 0), 1.f);

		ikfunctor(rightfoot, rightleg, rightlegupper, rfoottarget);
		ikfunctor(leftfoot, leftleg, leftlegupper, lfoottarget, true);
	}

	
	Pose_Pool::get().free(1);
}

void Animator::tick_tree_new(float dt)
{
	Pose* poses = Pose_Pool::get().alloc(2);

	NodeRt_Ctx ctx;
	ctx.tree = tree;
	ctx.model = model;
	ctx.set = set;
	ctx.vars = &tree->parameters;
	GetPose_Ctx gp_ctx;
	gp_ctx.dt = dt;
	gp_ctx.pose = &poses[0];

	if (tree->cfg->root)
		tree->cfg->root->get_pose(ctx, gp_ctx);
	else
		util_set_to_bind_pose(poses[0], model);

	//postprocess_animation(poses[0], dt);

	UpdateGlobalMatricies(poses[0].q, poses[0].pos, cached_bonemats);

	Pose_Pool::get().free(2);
}

void Animator::evaluate_new(float dt)
{
	if (!owner) return;

	Entity& player = *owner;
	bool mirrored = player.state & PMS_CROUCHING;
	Pose* poses = Pose_Pool::get().alloc(2);

	out.meshoffset = glm::vec3(0.f);


	glm::vec2 next_vel = glm::vec2(player.velocity.x, player.velocity.z);

	in.groundvelocity = next_vel;

	bool moving = glm::length(player.velocity) > 0.001;
	glm::vec2 face_dir = glm::vec2(cos(HALFPI-player.rotation.y), sin(HALFPI-player.rotation.y));
	glm::vec2 side = glm::vec2(-face_dir.y, face_dir.x);
	in.relmovedir = glm::vec2(glm::dot(face_dir, in.groundvelocity), glm::dot(side, in.groundvelocity));

	if (mirrored) in.relmovedir.y *= -1;

	glm::vec2 grndaccel(player.esimated_accel.x, player.esimated_accel.z);
	in.relaccel = vec2(dot(face_dir,grndaccel), dot(side,grndaccel));

	in.ismoving = glm::length(player.velocity) > 0.1;
	in.injump = player.state & PMS_JUMPING;


	tree->parameters.get(p_bMoving).ival = in.ismoving;
	tree->parameters.get(p_bCrouch).ival = int(player.state & PMS_CROUCHING);
	tree->parameters.get(p_bJumping).ival = int(player.state & PMS_JUMPING);
	tree->parameters.get(p_bFalling).ival = !bool(player.state & PMS_GROUND);
	tree->parameters.get(p_flSpeed).fval = glm::length(in.relmovedir);
	tree->parameters.get(p_flMovex).fval = in.relmovedir.x;
	tree->parameters.get(p_flMovey).fval = in.relmovedir.y;


	NodeRt_Ctx ctx;
	ctx.tree = tree;
	ctx.model = model;
	ctx.set = set;
	ctx.vars = &tree->parameters;
	GetPose_Ctx gp_ctx;
	gp_ctx.dt = dt;
	gp_ctx.pose = &poses[0];
	tree->cfg->root->get_pose(ctx, gp_ctx);

	postprocess_animation(poses[0], dt);

	UpdateGlobalMatricies(poses[0].q, poses[0].pos, cached_bonemats);

	Pose_Pool::get().free(2);
}

#include "AnimationTreeLocal.h"
#include "AnimationTreePublic.h"

void Animation_Tree_RT::init_from_cfg(const Animation_Tree_CFG* cfg, const Model* model, const Animation_Set* set)
{
	this->cfg = cfg;
	parameters.parameters.resize(cfg->parameters.types.size());
	data.resize(cfg->data_used);
	NodeRt_Ctx ctx;
	ctx.model = model;
	ctx.set = set;
	ctx.tree = this;
	if(cfg->root)
		cfg->root->construct(ctx);
}

void Animator::set_model_new(const Model* m)
{
	static bool first = true;
	if (first) {
			Debug_Interface::get()->add_hook("animation stuff", menu);
			first = false;
	}

	if (m->name == "player_FINAL.glb") {
		return;

		auto cfg_tree =  anim_tree_man->find_animation_tree("./Data/Animations/testtree.txt");
		tree = new Animation_Tree_RT;
		tree->init_from_cfg(cfg_tree, m, m->animations.get());

		p_flMovex = cfg_tree->find_param("$movex");
		p_flMovey = cfg_tree->find_param("$movey");
		p_flSpeed = cfg_tree->find_param("$speed");
		p_bCrouch = cfg_tree->find_param("$crouch");
		p_bFalling = cfg_tree->find_param("$falling");
		p_bJumping = cfg_tree->find_param("$jumping");
		p_bMoving = cfg_tree->find_param("$moving");
	}

}

Animation_Tree_CFG* Animation_Tree_Manager::find_animation_tree(const char* n) {
	return nullptr;
}

static Animation_Tree_Manager anim_tree_man__;
Animation_Tree_Manager* anim_tree_man = &anim_tree_man__;
