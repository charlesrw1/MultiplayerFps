#include "Animation.h"
#include "Util.h"
#include "Model.h"

#define ROOT_BONE -1
#define INVALID_ANIMATION -1

using glm::vec3;
using glm::quat;
using glm::mat4;
using glm::length;
using glm::dot;
using glm::cross;
using glm::normalize;

int AnimationSet::FirstPositionKeyframe(float frame, int channel_num, int clip) const
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
int AnimationSet::FirstRotationKeyframe(float frame, int channel_num, int clip) const
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
int AnimationSet::FirstScaleKeyframe(float frame, int channel_num, int clip) const
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

const PosKeyframe& AnimationSet::GetPos(int channel, int index, int clip) const {
	ASSERT(clip < clips.size());
	ASSERT(index < channels[clips[clip].channel_offset + channel].num_positions);

	return positions[channels[clips[clip].channel_offset + channel].pos_start + index];
}
const RotKeyframe& AnimationSet::GetRot(int channel, int index, int clip) const {
	ASSERT(clip < clips.size());
	ASSERT(index < channels[clips[clip].channel_offset + channel].num_rotations);
	
	return rotations[channels[clips[clip].channel_offset + channel].rot_start + index];
}
const ScaleKeyframe& AnimationSet::GetScale(int channel, int index, int clip) const {
	ASSERT(clip < clips.size());
	ASSERT(index < channels[clips[clip].channel_offset + channel].num_scales);

	return scales[channels[clips[clip].channel_offset + channel].scale_start + index];
}
const AnimChannel& AnimationSet::GetChannel(int clip, int channel) const {
	ASSERT(clip < clips.size());
	return channels[clips[clip].channel_offset + channel];
}

int AnimationSet::FindClipFromName(const char* name) const
{
	for (int i = 0; i < clips.size(); i++) {
		if (clips[i].name == name)
			return i;
	}
	return -1;
}


void Animator::AdvanceFrame(float elapsed)
{
	if (!model || !model->animations)
		return;

	{
		if (mainanim != -1) {
			ASSERT(mainanim < model->animations->clips.size());
			const Animation& clip = model->animations->clips[mainanim];
			mainanim_frame += clip.fps * elapsed * 1.f;
			if (mainanim_frame > clip.total_duration || mainanim_frame < 0.f) {
				mainanim_frame = fmod(fmod(mainanim_frame, clip.total_duration) + clip.total_duration, clip.total_duration);
			}
		}
	}
	{
		if (leganim != -1) {
			ASSERT(leganim < model->animations->clips.size());
			const Animation& clip = model->animations->clips[leganim];
			leganim_frame += clip.fps * elapsed * leganim_speed;
			if (leganim_frame > clip.total_duration || leganim_frame < 0.f) {
				leganim_frame = fmod(fmod(leganim_frame, clip.total_duration) + clip.total_duration, clip.total_duration);
			}
			if (1)
			{

				static int timer = 0;
				if (timer == 0) {
					timer = 200;
					printf("Anim frame: %f\n", leganim_frame);
				}
				timer--;
			}
		}
	}
}

#if 0

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
	const AnimationSet* set = model->animations.get();
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
			interp_rot = set->GetRot(i, pos_idx, clip_index).val;
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
}

void Animator::SetupBones()
{
	ASSERT(model->animations && model);
	//assert(animations->animdata);
	//(animations->animdata[playinganimation].data->channels.size() == model->bones.size());

	assert(cached_bonemats.size() == model->bones.size());

	glm::quat q[MAX_BONES];
	glm::vec3 pos[MAX_BONES];
	glm::vec3 scl[MAX_BONES];

	if (mainanim < 0 || mainanim >= model->animations->clips.size())
		mainanim = INVALID_ANIMATION;

	// Just t-pose if no proper animations
	if (mainanim == INVALID_ANIMATION)
	{
		for (int i = 0; i < cached_bonemats.size(); i++)
			cached_bonemats[i] = model->bones[i].posematrix;
		return;
	}

	// Setup base layer
	CalcRotations(q, pos, mainanim, mainanim_frame);

	//if (actor_owner && actor_owner->IsPlayer())
	//{
	//	AddPlayerUpperbodyLayer(q, pos);
	//}

	UpdateGlobalMatricies(q, pos, cached_bonemats);

	// Inverse kinematics
	//if (!actor_owner)
	//{
	//	DoHandIK(q, pos, cached_bonemats);
	//}
	//if (actor_owner && actor_owner->IsPlayer() && !actor_owner->is_dead)
	//{
	//	DoPlayerHandToGunIK(q, pos, cached_bonemats);
	//}
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

#if 0
void AnimationController::AddPlayerUpperbodyLayer(glm::quat finalq[], glm::vec3 finalp[])
{
	assert(actor_owner);

	// Dead actors dont move upper body
	if (actor_owner->is_dead)
		return;

	const AnimationLayer& layer = GetLayer(UPPERBODY_LAYER);
	if (layer.set_index == -1 || layer.set_index >= animations->anims.size())
		return;

	const AnimDesc& desc = animations->anims[layer.set_index];
	assert(desc.clips.size() == 3);

	glm::quat q1[MAX_BONES];
	glm::vec3 p1[MAX_BONES];
	glm::quat q2[MAX_BONES];
	glm::vec3 p2[MAX_BONES];

	// assume: character angles are oriented "correctly" already: oriented towards velocity
	//			upper body then needs to be oriented to the angle between view vector and velocity

	const float lowerbodyangle = actor_owner->angles.y;
	const vec3 lowerbodyvector = vec3(cos(lowerbodyangle), 0, sin(lowerbodyangle));

	const float upperbodyangle = -actor_owner->viewangles.y + PI * 0.5;
	const vec3 upperbodyvector = vec3(cos(upperbodyangle), 0, sin(upperbodyangle));

	const float anglebetween = acos(dot(upperbodyvector, lowerbodyvector));
	const bool leftside = cross(upperbodyvector, lowerbodyvector).y > 0;
	const float interpfactor = anglebetween / (PI * 0.5);

	//	assert(anglebetween >= -0.0001 && anglebetween <= PI*0.5+0.1);

		// First get center angles
	CalcRotations(q1, p1, *desc.clips[1], layer.cur_frame);

	const AnimClip* otherclip = (leftside) ? desc.clips[0] : desc.clips[2];
	// Next get other sides angles
	CalcRotations(q2, p2, *otherclip, layer.cur_frame);

	if (abs(anglebetween) > 0.001) {
		// Now interpolate
		LerpTransforms(q1, p1, q2, p2, interpfactor, otherclip->channels.size());
	}

	const int root_loc = model->BoneForName("spine");
	const int spine_loc = model->BoneForName("spine.001");

	if (root_loc == -1 || spine_loc == -1) {
		printf("Couldn't find spine/root bones\n");
		return;
	}


	bool copybones = false;
	// Now overwrite only upperbody
	for (int i = 0; i < model->bones.size(); i++)
	{
		const GfxBone& bone = model->bones[i];
		if (i == spine_loc)
			copybones = true;
		else if (bone.parent == root_loc)
			copybones = false;
		if (copybones) {
			finalq[i] = q1[i];
			finalp[i] = p1[i];
		}
	}

}
#endif

void Animator::ConcatWithInvPose()
{
	ASSERT(model);
	for (int i = 0; i < model->bones.size(); i++) {
		cached_bonemats[i] = cached_bonemats[i] * glm::mat4(model->bones[i].invposematrix);
	}
}


void Animator::Init(const Model* mod)
{
	ASSERT(mod);
	if (!(mod->bones.size() > 0 && mod->animations && mod->bones.size() == mod->animations->num_channels)) {
		printf("Animator::Init called with invalid model\n");
		ASSERT(0);
	}
	model = mod;
	set = model->animations.get();
	cached_bonemats.resize(model->bones.size());
}
