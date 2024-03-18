#include "Animation.h"
#include "Util.h"
#include "Model.h"
#include "Game_Engine.h"
#include "Config.h"

#include <fstream>
#include <sstream>

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

float LawOfCosines(float a, float b, float c)
{
	return (a * a - b * b - c * c) / (-2 * b * c);
}

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
static float MidLerp(float min, float max, float mid_val)
{
	return (mid_val - min) / (max - min);
}

void Animator::CalcRotations(glm::quat q[], vec3 pos[], int clip_index, float curframe)
{
	const Animation_Set* set = model->animations.get();
	const Animation& clip = set->clips[clip_index];

	for (int i = 0; i < set->num_channels; i++) {
		int pos_idx = set->FirstPositionKeyframe(curframe, i, clip_index);
		int rot_idx = set->FirstRotationKeyframe(curframe, i, clip_index);

		vec3 interp_pos{};
		if (pos_idx == -1)
			interp_pos = model->bones.at(i).posematrix[3];
		else if (pos_idx == set->GetChannel(clip_index, i).num_positions - 1)
			interp_pos = set->GetPos(i, pos_idx, clip_index).val;
		else {
			int index0 = pos_idx;
			int index1 = pos_idx + 1;
			//float scale = MidLerp(clip.GetPos(i, index0).time, clip.GetPos(i, index1).time, curframe);
			//interp_pos = glm::mix(clip.GetPos(i, index0).val, clip.GetPos(i, index1).val, scale);
			float scale = MidLerp(set->GetPos(i, index0,clip_index).time, set->GetPos(i, index1,clip_index).time, curframe);
			interp_pos = glm::mix(set->GetPos(i, index0,clip_index).val, set->GetPos(i, index1,clip_index).val, scale);
		}

		glm::quat interp_rot{};
		if (rot_idx == -1)
			interp_rot = model->bones.at(i).rot;
		else if (rot_idx == set->GetChannel(clip_index, i).num_rotations - 1)
			interp_rot = set->GetRot(i, rot_idx, clip_index).val;
		else {
			int index0 = rot_idx;
			int index1 = rot_idx + 1;
			float scale = MidLerp(set->GetRot(i, index0, clip_index).time, set->GetRot(i, index1, clip_index).time, curframe);
			interp_rot = glm::slerp(set->GetRot(i, index0, clip_index).val, set->GetRot(i, index1, clip_index).val, scale);
		}
		interp_rot = glm::normalize(interp_rot);

		q[i] = interp_rot;
		pos[i] = interp_pos;
	}
}
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
		out_bone_matricies[i] = model->skeleton_root_transform * out_bone_matricies[i];
}

void Animator::SetupBones()
{
	if (tree) {
		evaluate_new(eng->frame_time);
		return;
	}
	
	ASSERT(model->animations && model);
	ASSERT(cached_bonemats.size() == model->bones.size());

	static glm::quat q[MAX_BONES];
	static glm::vec3 pos[MAX_BONES];

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

	// Setup main layer
	CalcRotations(q, pos, m.anim, m.frame);
	if (m.blend_anim != -1)
	{
		static glm::quat q2[MAX_BONES];
		static glm::vec3 pos2[MAX_BONES];
		CalcRotations(q2, pos2, m.blend_anim, m.blend_frame);
		float frac = m.blend_remaining / m.blend_time;
		LerpTransforms(q, pos, q2, pos2, frac, model->bones.size());
	}

	if (legs.anim != -1)
		add_legs_layer(q, pos);
	UpdateGlobalMatricies(q, pos, cached_bonemats);
}
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

void Animator::ConcatWithInvPose()
{
	ASSERT(model);
	for (int i = 0; i < model->bones.size(); i++) {
		cached_bonemats[i] = cached_bonemats[i] * glm::mat4(model->bones[i].invposematrix);
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

typedef uint32_t bonehandle;

class IK_Controller
{
public:
	virtual void update() = 0;
	bonehandle bone1;
	bonehandle bone2;
	glm::vec3 targetpos;
};

class Single_Bone_Controller
{
public:

	Model* mod;
	bonehandle bone;
};

typedef uint32_t animhandle;


class AnimSeqExt
{
public:
	enum Type
	{
		STANDARD,
		DIRECTIONAL_BLEND,
		AIM_BLEND,
		TURN_BLEND
	}type;
	enum Turn_Blend_Enums {
		TURN_45,
		TURN_NEG45,
		TURN_90,
		TURN_NEG90
	};
	enum Aim_Blend_Enums {
		AIM_UP,
		AIM_DOWN,
		AIM_LEFT,
		AIM_RIGHT,
	};
	enum Directional_Blend_Enums {
		DIR_FORWARD,
		DIR_BACKWARD,
		DIR_LEFT,
		DIR_RIGT,
	}; 
	int type_enum;
	int num_clips = 0;
	float clip_cdf[10];	// type == STANDARD && num_clips > 1
	animhandle clips[10];
};


// source = source-reference
static void util_subtract(int bonecount, const Pose& reference, Pose& source)
{
	for (int i = 0; i < bonecount; i++) {
		source.pos[i] = source.pos[i] - reference.pos[i];
		source.q[i] = source.q[i] - reference.q[i];
	}

}
// b = lerp(a,b,f)
static void util_blend(int bonecount, const Pose& a, Pose& b, float factor)
{
	for (int i = 0; i < bonecount; i++) {
		b.q[i] = glm::slerp(b.q[i], a.q[i], factor);
		b.q[i] = glm::normalize(b.q[i]);
		b.pos[i] = glm::mix(b.pos[i], a.pos[i], factor);
	}
}
// base = lerp(base,base+additive,f)
static void util_add(int bonecount, const Pose& additive, Pose& base, float fac)
{
	for (int i = 0; i < bonecount; i++) {
		base.pos[i] = glm::mix(base.pos[i], base.pos[i] + additive.pos[i], fac);
		base.q[i] = glm::slerp(base.q[i], base.q[i] + additive.q[i], fac);
		base.q[i] = glm::normalize(base.q[i]);
	}
}

static void util_blend_bilinear(const Pose& out, Pose* in, int incount, glm::vec2 interp)
{


}



static void util_twobone_ik(
	const vec3& a, const vec3& b, const vec3& c, 
	const vec3& target, const vec3& pole_vector,
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

PoseMask::PoseMask()
{

}
Animator::Animator()
	: play_layers(8,Anim_Play_Layer(4))
{

}

static vector<int> get_indicies(const Animation_Set* set, const vector<const char*>& strings)
{
	vector<int> out;
	for (auto s : strings) out.push_back(set->find(s));
	return out;
}


#include "LispInterpreter.h"


enum opcode : uint16_t
{
	ADD_F,
	ADD_I,
	SUB_F,
	SUB_I,
	MULT_F,
	MULT_I,
	DIV_F,
	DIV_I,

	LT_F,
	LT_I,
	LEQ_F,
	LEQ_I,
	GT_F,
	GT_I,
	GEQ_F,
	GEQ_I,

	
	EQ_F,
	EQ_I,
	NOTEQ_F,
	NOTEQ_I,

	AND,
	OR,
	NOT,
	PUSH_CONST_F,
	PUSH_CONST_I,
	PUSH_F,
	PUSH_I,
	CAST_0_F,
	CAST_0_I,
	CAST_1_F,
	CAST_1_I,
};

struct stack_val
{
	union {
		int i;
		float f;
	};
};

struct compilied_exp
{
	vector<uint8_t> instructions;

	void push_inst(uint8_t c) {
		instructions.push_back(c);
	}
	void push_4bytes(unsigned int x) {
		instructions.push_back(x & 0xff);
		instructions.push_back((x >> 8) & 0xff);
		instructions.push_back((x >> 16) & 0xff);
		instructions.push_back((x >> 24) & 0xff);
	}
	enum type {
		int_type,
		float_type
	};

	uint32_t read_4bytes(int offset) {
		uint32_t res = (instructions[offset]);
		res |= ((uint32_t)instructions[offset + 1] << 8);
		res |= ((uint32_t)instructions[offset + 2] << 16);
		res |= ((uint32_t)instructions[offset + 3] << 24);
		return res;
	}
	uintptr_t read_8bytes(int offset) {
		uintptr_t low = read_4bytes(offset);
		uintptr_t high = read_4bytes(offset + 4);
		return low | (high << 32);
	}

	type compile(Exp* exp, const Env* env) {
		if (exp->type == Exp::atom_type) {
			if (exp->atom.type == Atom::symbol_type) {
				const auto& find = env->symbols.find(exp->atom.sym);
				if (find->second.type == Atom_Or_Proc::atom_type) {
					if (find->second.a.type == Atom::int_type) {
						push_inst(PUSH_I);
						uintptr_t ptr = (uintptr_t)&find->second.a.i;
						int testi = *(int*)(ptr);
						push_4bytes(ptr);
						push_4bytes(ptr>> 32);
						return type::int_type;
					}
					else if (find->second.a.type == Atom::float_type) {
						push_inst(PUSH_F);
						push_4bytes((unsigned long long) & find->second.a.f);
						push_4bytes(((unsigned long long) & find->second.a.f) >> 32);
						return type::float_type;
					}
					else
						assert(0);
				}
			}
			else {
				if (exp->atom.type == Atom::int_type) {
					push_inst(PUSH_CONST_I);
					push_4bytes(exp->atom.i);
					return type::int_type;
				}
				else if (exp->atom.type == Atom::float_type) {
					push_inst(PUSH_CONST_F);
					float f = exp->atom.f;
					push_4bytes(*(unsigned int*)&f);
					return type::float_type;
				}
				else {
					assert(0);
				}
			}
		}
		else {
			assert(exp->list.at(0)->type == Exp::atom_type);
			string op = exp->list.at(0)->atom.sym;
			if (op == "not") {
				auto type1 = compile(exp->list.at(1).get(), env);
				if (type1 == type::float_type) {
					push_inst(CAST_0_I);
				}
				push_inst(NOT);
				return type::int_type;
			}
			else if (op == "and" || op == "or") {
				auto type1 = compile(exp->list.at(1).get(), env);
				auto type2 = compile(exp->list.at(2).get(), env);
				if (type1 != int_type)
					push_inst(CAST_1_I);
				if (type2 != int_type)
					push_inst(CAST_0_I);
				if (op == "and")
					push_inst(AND);
				else
					push_inst(OR);
				return type::int_type;
			}
			else {
				auto type1 = compile(exp->list.at(1).get(), env);
				auto type2 = compile(exp->list.at(2).get(), env);
				bool isfloat = (type1 == float_type || type2 == float_type);
				if (isfloat && type1 != float_type)
					push_inst(CAST_1_F);
				if (isfloat && type2 != float_type)
					push_inst(CAST_0_F);
				struct pairs {
					const char* name;
					opcode op;
				};
				const static pairs p[] = {
					{"+",ADD_F},
					{"-",SUB_F},
					{"*",MULT_F},
					{"/",DIV_F},
					{"<",LT_F},
					{">",GT_F},
					{"<=",LEQ_F},
					{">=",GEQ_F},
					{"==",EQ_F},
					{"!=",NOTEQ_F}
				};
				int i = 0;
				for (; i < 10; i++) {
					if (p[i].name == op) {
						if (isfloat) push_inst(p[i].op);
						else push_inst(p[i].op + 1);
						break;
					}
				}
				assert(i != 10);

				return (isfloat) ? float_type : int_type;
			}
		}
	}

#define OPONSTACK(op, typein, typeout) stack[sp-2].typeout = stack[sp-2].typein op stack[sp-1].typein; sp-=1; break;
#define FLOAT_AND_INT_OP(opcode_, op) case opcode_: OPONSTACK(op, f, f); \
case (opcode_+1): OPONSTACK(op,i,i);
#define FLOAT_AND_INT_OP_OUTPUT_INT(opcode_, op) case opcode_: OPONSTACK(op,f, i); \
case (opcode_+1): OPONSTACK(op,i, i);
	stack_val run()
	{
		stack_val stack[64];
		int sp = 0;

		for (int pc = 0; pc < instructions.size(); pc++) {
			uint8_t op = instructions[pc];
			switch (op)
			{
				FLOAT_AND_INT_OP(ADD_F, +);
				FLOAT_AND_INT_OP(SUB_F, -);
				FLOAT_AND_INT_OP(MULT_F, *);
				FLOAT_AND_INT_OP(DIV_F, / );
				FLOAT_AND_INT_OP_OUTPUT_INT(LT_F, < );
				FLOAT_AND_INT_OP_OUTPUT_INT(LEQ_F, <= );
				FLOAT_AND_INT_OP_OUTPUT_INT(GT_F, > );
				FLOAT_AND_INT_OP_OUTPUT_INT(GEQ_F, >= );
				FLOAT_AND_INT_OP_OUTPUT_INT(EQ_F, == );
				FLOAT_AND_INT_OP_OUTPUT_INT(NOTEQ_F, != );

			case NOT:
				stack[sp-1].i = !stack[sp-1].i;
				break;
			case AND:
				stack[sp - 2].i = stack[sp - 2].i && stack[sp - 1].i;
				sp -= 1;
				break;
			case OR:
				stack[sp - 2].i = stack[sp - 2].i || stack[sp - 1].i;
				sp -= 1;
				break;

			case CAST_0_F:
				stack[sp-1].f = stack[sp-1].i;
				break;
			case CAST_0_I:
				stack[sp-1].i = stack[sp-1].f;
				break;
			case CAST_1_F:
				stack[sp - 2].f = stack[sp - 2].i;
				break;
			case CAST_1_I:
				stack[sp - 2].i = stack[sp - 2].f;
				break;

			case PUSH_CONST_F: {
				union {
					int i;
					float f;
				};
				i = read_4bytes(pc + 1);
				stack[sp++].f = f;
				pc += 4;
			} break;
			case PUSH_CONST_I: {
				int i = read_4bytes(pc + 1);
				stack[sp++].i = i;
				pc += 4;
			}break;
			case PUSH_F: {
				unsigned long long ptr = read_8bytes(pc + 1);
				stack[sp++].f = *((double*)ptr);
				pc += 8;
			}break;
			case PUSH_I: {
				unsigned long long ptr = read_8bytes(pc + 1);
				stack[sp++].i = *((int*)ptr);
				pc += 8;
			}break;
			}
		}
		assert(sp == 1);
		return stack[0];
	}
};
#undef FLOAT_AND_INT_OP
#undef FLOAT_AND_INT_OP_OUTPUT_INT
#undef OPONSTACK


struct State;
struct State_Transition
{
	State_Transition() {

	}
	~State_Transition() {
		printf("st descructor\n");
	}
	State_Transition(const State_Transition& other) = delete;
	State_Transition(State_Transition&& other) {
		transition_state = other.transition_state;
		compilied = std::move(other.compilied);
	}

	State* transition_state;
	compilied_exp compilied;
};

struct At_Node
{
	At_Node(Animator* a) : animator(a) {}

	virtual ~At_Node() = default;

	virtual bool get_pose(Pose& pose, float dt) = 0;
	virtual void reset() = 0;
	Animator* animator;
};


struct State
{
	string name;
	At_Node* tree = nullptr;
	vector<State_Transition> transitions;

	State* get_next_state(Animator* animator);
};

struct Animation_Tree
{
	~Animation_Tree() {
		printf("at destructor\n");
	}

	At_Node* root = nullptr;
	unordered_map<string, State> states;
	vector<unique_ptr<At_Node>> all_node_list;
	Env script_vars;
	Interpreter_Ctx ctx;
};

class Pose_Pool
{
public:
	Pose_Pool(int n) : poses(n) {}
	static Pose_Pool& get() {
		static Pose_Pool inst(64);
		return inst;
	}

	vector<Pose> poses;
	int head = 0;
	Pose* alloc(int count) {
		assert(count + head < 64);
		head += count;
		return &poses[head - count];
	}
	void free(int count) {
		head -= count;
		assert(head >= 0);
	}
};

State* State::get_next_state(Animator* animator)
{
	for (int i = 0; i < transitions.size(); i++) {
		// evaluate condition
		if (transitions[i].compilied.run().i)
			return transitions[i].transition_state;
	}
	return this;
}



void util_calc_rotations(const Animation_Set* set, 
	float curframe, int clip_index, const Model* model, Pose& pose)
{
	for (int i = 0; i < set->num_channels; i++) {
		int pos_idx = set->FirstPositionKeyframe(curframe, i, clip_index);
		int rot_idx = set->FirstRotationKeyframe(curframe, i, clip_index);

		vec3 interp_pos{};
		if (pos_idx == -1)
			interp_pos = model->bones.at(i).posematrix[3];
		else if (pos_idx == set->GetChannel(clip_index, i).num_positions - 1)
			interp_pos = set->GetPos(i, pos_idx, clip_index).val;
		else {
			int index0 = pos_idx;
			int index1 = pos_idx + 1;
			//float scale = MidLerp(clip.GetPos(i, index0).time, clip.GetPos(i, index1).time, curframe);
			//interp_pos = glm::mix(clip.GetPos(i, index0).val, clip.GetPos(i, index1).val, scale);
			float scale = MidLerp(set->GetPos(i, index0,clip_index).time, set->GetPos(i, index1,clip_index).time, curframe);
			interp_pos = glm::mix(set->GetPos(i, index0,clip_index).val, set->GetPos(i, index1,clip_index).val, scale);
		}

		glm::quat interp_rot{};
		if (rot_idx == -1)
			interp_rot = model->bones.at(i).rot;
		else if (rot_idx == set->GetChannel(clip_index, i).num_rotations - 1)
			interp_rot = set->GetRot(i, rot_idx, clip_index).val;
		else {
			int index0 = rot_idx;
			int index1 = rot_idx + 1;
			float scale = MidLerp(set->GetRot(i, index0, clip_index).time, set->GetRot(i, index1, clip_index).time, curframe);
			interp_rot = glm::slerp(set->GetRot(i, index0, clip_index).val, set->GetRot(i, index1, clip_index).val, scale);
		}
		interp_rot = glm::normalize(interp_rot);

		pose.q[i] = interp_rot;
		pose.pos[i] = interp_pos;
	}
}
void util_set_to_bind_pose(Pose& pose, const Model* model)
{
	for (int i = 0; i < model->bones.size(); i++) {
		pose.pos[i] = model->bones.at(i).posematrix[3];
		pose.q[i] = model->bones.at(i).rot;
	}
}

struct Clip_Node : public At_Node
{
	Clip_Node(const char* clipname, Animator* animator) : At_Node(animator) {
		clip_index = animator->set->find(clipname);
	}

	// Inherited via At_Node
	virtual bool get_pose(Pose& pose, float dt)
	{
		if (clip_index == -1) {
			util_set_to_bind_pose(pose, animator->model);
			return true;
		}

		bool keep_playing = true;
		const Animation& clip = animator->set->clips[clip_index];
		frame += clip.fps * dt;
		if (frame > clip.total_duration || frame < 0.f) {
			if (loop)
				frame = fmod(fmod(frame, clip.total_duration) + clip.total_duration, clip.total_duration);
			else {
				frame = clip.total_duration - 0.001f;
				keep_playing = false;
			}
		}
		util_calc_rotations(animator->set, frame, clip_index,animator->model, pose);

		return true;
	}

	virtual void reset() override {
		frame = 0.f;
	}

	int clip_index = 0;
	float frame = 0.f;
	bool loop = true;
};

struct Subtract_Node : public At_Node
{
	using At_Node::At_Node;
	// Inherited via At_Node
	virtual bool get_pose(Pose& pose, float dt) override
	{
		Pose* reftemp = Pose_Pool::get().alloc(1);
		ref->get_pose(*reftemp, dt);
		source->get_pose(pose, dt);
		util_subtract(animator->GetBones().size(), *reftemp, pose);
		Pose_Pool::get().free(1);
		return true;
	}
	virtual void reset() override {
		ref->reset();
		source->reset();
	}
	At_Node* ref;
	At_Node* source;
};

struct Add_Node : public At_Node
{
	using At_Node::At_Node;

	virtual bool get_pose(Pose& pose, float dt) override
	{
		float lerp = compilied.run().f;
		Pose* addtemp = Pose_Pool::get().alloc(1);
		base_pose->get_pose(pose, dt);
		diff_pose->get_pose(*addtemp, dt);
		util_add(animator->GetBones().size(), *addtemp, pose, lerp);
		Pose_Pool::get().free(1);
		return true;
	}
	virtual void reset() override {
		diff_pose->reset();
		base_pose->reset();
	}

	compilied_exp compilied;
	At_Node* diff_pose;
	At_Node* base_pose;
};

struct Boolean_Blend_Node : public At_Node
{
	using At_Node::At_Node;

	virtual bool get_pose(Pose& pose, float dt) override {
		bool b = LispLikeInterpreter::eval(exp.get(), &animator->tree->ctx).a.cast_to_int();
		if (b)
			value += dt / blendin;
		else
			value -= dt / blendin;
		glm::clamp(value, 0.0f, 1.0f);

		if (value < 0.00001f)
			return nodes[0]->get_pose(pose, dt);
		else if (value >= 0.99999f)
			return nodes[1]->get_pose(pose, dt);
		else {
			Pose* pose2 = Pose_Pool::get().alloc(1);

			bool r = nodes[0]->get_pose(pose, dt);
			nodes[1]->get_pose(*pose2, dt);

			util_blend(animator->model->bones.size(), *pose2, pose, 1 - value);
			Pose_Pool::get().free(1);
			return r;
		}
	}
	virtual void reset() override {
		value = 0.0;
	}
	Pose* temppose = nullptr;
	At_Node* nodes[2];
	unique_ptr<Exp> exp;
	float value = 0.f;
	float blendin = 0.2;
};

struct Statemachine_Node : public At_Node
{
	using At_Node::At_Node;

	virtual bool get_pose(Pose& pose, float dt) override {

		// evaluate state machine
		if (!active_state) active_state = start_state;

		State* next_state = active_state->get_next_state(animator);
		if (active_state != next_state) {
			active_state = next_state;
			active_state->tree->reset();
			printf("changed to state %s\n", active_state->name.c_str());
		}

		return active_state->tree->get_pose(pose, dt);
	}
	State* start_state = nullptr;
	State* active_state = nullptr;

	// Inherited via At_Node
	virtual void reset() override
	{
		active_state = start_state;
		active_state->tree->reset();
	}
};


struct Directionalblend_node : public At_Node
{
	Directionalblend_node(Animator* a) : At_Node(a) {
		memset(directions, 0, sizeof(directions));
	}

	At_Node* directions[8];

	// Inherited via At_Node
	virtual bool get_pose(Pose& pose, float dt) override
	{
		// blend between 2 poses
		glm::vec2 direction = glm::normalize(animator->in.groundvelocity);

		float angle = atan2f(direction.y, direction.x);

		float lerp = 0.0;
		int pose1=0, pose2=1;
		for (int i = 0; i < 8; i++) {
			if (angle <= -PI + PI / 4.0 * (i + 1)) {
				pose1 = i;
				pose2 = (i + 1) % 8;
				lerp = MidLerp(-PI + PI / 4.0 * i, -PI + PI / 4.0 * (i + 1), angle);
				break;
			}
		}
		Pose* scratchpose = Pose_Pool::get().alloc(1);

		directions[pose1]->get_pose(pose, dt);
		directions[pose2]->get_pose(*scratchpose, dt);

		util_blend(animator->model->bones.size(), *scratchpose, pose, lerp);

		Pose_Pool::get().free(1);

		return true;
	}
	virtual void reset() override {
		for (int i = 0; i < 8; i++)
			if (directions[i]) directions[i]->reset();
	}
};

struct Context
{
	Animation_Tree* tree;
	Animator* animator;
	At_Node* add_node(At_Node* node) {
		tree->all_node_list.push_back(unique_ptr<At_Node>(node));
		return tree->all_node_list.back().get();
	}
	State* find_state(string& name) {
		return &tree->states[name];
	}
};

static Context at_ctx;

Atom root_func(Atom* args, int argc)
{
	for (int i = 0; i < argc; i++) {
		if (args[i].type == Atom::animation_tree_node_type) {
			at_ctx.tree->root = (At_Node*)args[i].ptr;	// bubbled up from tree
			break;
		}
	}

	return Atom(0);
}

Atom clip_func(Atom* args, int argc)
{
	if (argc != 1) throw "only 1 clip arg";

	Clip_Node* clip = new Clip_Node(args[0].sym.c_str(), at_ctx.animator);
	return Atom(at_ctx.add_node(clip));
}

Atom tree_func(Atom* args, int argc)
{
	// bubble up
	return args[0];
}

Atom defstate_func(Atom* args, int argc)
{
	if (argc <= 2) throw "bad def state";
	string& statename = args[0].sym;
	State* s = at_ctx.find_state(statename);
	s->name = statename.c_str();

	for (int i = 0; i < argc; i++) {
		if (args[i].type == Atom::animation_transition_type) {
			s->transitions.push_back(std::move(*((State_Transition*)args[i].ptr)));

			delete args[i].ptr;
			args[i].ptr = nullptr;
		}
		else if (args[i].type == Atom::animation_tree_node_type) {
			s->tree = (At_Node*)args[i].ptr;
			args[i].ptr = nullptr;
		}
	}

	return Atom(0);
}

Atom transition_state(Atom* args, int argc)
{
	if (argc != 2)throw "2 args for transition";
	State_Transition* st = new State_Transition;
	st->transition_state = at_ctx.find_state(args[0].sym);
	if (args[1].type != Atom::exp_type) throw "expected expression transition";
	st->compilied.compile(args[1].exp, &at_ctx.tree->script_vars);
	
	Atom a;
	a.type = Atom::animation_transition_type;
	a.ptr = st;
	return a;
}

Atom move_directional_blend_func(Atom* args, int argc)
{
	Directionalblend_node* db = new Directionalblend_node(at_ctx.animator);
	for (int i = 0; i < argc && i < 8; i++) {
		if (args[i].type != Atom::animation_tree_node_type) throw "expected node";
		db->directions[i] = (At_Node*)args[i].ptr;
	}

	return Atom(at_ctx.add_node(db));
}

Atom statemachine_node(Atom* args, int argc)
{
	Statemachine_Node* sm = new Statemachine_Node(at_ctx.animator);
	sm->start_state = at_ctx.find_state(args[0].sym);
	return Atom(at_ctx.add_node(sm));
}

Atom additive_node(Atom* args, int argc)
{
	Add_Node* addnode = new Add_Node(at_ctx.animator);
	addnode->base_pose = (At_Node*)args[0].ptr;
	addnode->diff_pose = (At_Node*)args[1].ptr;
	addnode->compilied.compile(args[2].exp, &at_ctx.tree->script_vars);
	args[2].exp = nullptr;
	return Atom(at_ctx.add_node(addnode));
}

Atom sub_node_create(Atom* args, int argc)
{
	Subtract_Node* subnode = new Subtract_Node(at_ctx.animator);
	subnode->source = (At_Node*)args[0].ptr;
	subnode->ref = (At_Node*)args[1].ptr;
	return Atom(at_ctx.add_node(subnode));
}

Animation_Tree* load_animtion_tree(Animator* anim,const char* path) {

	std::ifstream infile(path);
	std::string line;
	string fullcode;
	while (std::getline(infile, line)) fullcode += line;

	auto env = LispLikeInterpreter::get_global_env();
	env.symbols["root"] = root_func;
	env.symbols["clip"] = clip_func;
	env.symbols["move-directional-blend"] = move_directional_blend_func;
	env.symbols["def-state"] = defstate_func;
	env.symbols["tree"] = tree_func;
	env.symbols["transition"] = transition_state;
	env.symbols["statemachine"] = statemachine_node;
	env.symbols["additive"] = additive_node;
	env.symbols["subtract"] = sub_node_create;

	Animation_Tree* tree = new Animation_Tree;
	tree->ctx.argbuf.resize(128);
	at_ctx.tree = tree;
	at_ctx.animator = anim;
	tree->ctx.arg_head = 0;
	tree->ctx.env = &env;
	tree->script_vars = LispLikeInterpreter::get_global_env();

	tree->script_vars.symbols["$moving"] = Atom(0);
	tree->script_vars.symbols["$alpha"] = Atom(0.f);
	tree->script_vars.symbols["$jumping"] = Atom(0);

	try {
		Atom_Or_Proc val = LispLikeInterpreter::eval(LispLikeInterpreter::parse(fullcode).get(), &tree->ctx);
	}
	catch (const char* str) {
		printf("error occured %s\n", str);
	}

	tree->ctx.env = &tree->script_vars;

	return tree;
}


void Animator::evaluate_new(float dt)
{
	Pose* pose = Pose_Pool::get().alloc(1);

	Entity& player = eng->local_player();

	in.groundvelocity = glm::vec2(player.velocity.x, player.velocity.z);

	tree->script_vars.symbols["$moving"] = Atom(int( glm::length(player.velocity) > 0.1) );
	tree->script_vars.symbols["$alpha"] = Atom(float( sin(GetTime())));
	tree->script_vars.symbols["$jumping"] = Atom(int(player.state & PMS_JUMPING));

	tree->root->get_pose(*pose, dt);

	UpdateGlobalMatricies(pose->q, pose->pos, cached_bonemats);

	Pose_Pool::get().free(1);
}

void Animator::set_model_new(const Model* m)
{
	if (m->name == "player_FINAL.glb") {
		tree =
			load_animtion_tree(this,"./Data/Animations/testtree.txt");
	}
}