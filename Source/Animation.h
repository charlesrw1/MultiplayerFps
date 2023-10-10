#ifndef ANIMATION_H
#define ANIMATION_H
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <vector>
#include <string>

struct AnimChannel
{
	int num_positions = 0;
	int num_rotations = 0;
	int num_scales = 0;

	// Indicies, not file pointers
	int pos_start = 0;
	int rot_start = 0;
	int scale_start = 0;
};
static_assert(sizeof(AnimChannel) == 24, "AnimChannel wrong size");

template<typename T>
struct KeyframeBase {
	T val;
	float time;
};

typedef KeyframeBase<glm::vec3> PosKeyframe;
typedef KeyframeBase<glm::vec3> ScaleKeyframe;
typedef KeyframeBase<glm::quat> RotKeyframe;
static_assert(sizeof(PosKeyframe) == 16, "PosKeyframe wrong size");
static_assert(sizeof(ScaleKeyframe) == 16, "ScaleKeyframe wrong size");
static_assert(sizeof(RotKeyframe) == 20, "RotKeyframe wrong size");

class Animation
{
public:
	std::string name;
	float total_duration = 0;
	bool looping = false;
	float fps = 24;

	int channel_offset=0;
	int pos_offset=0;
	int num_pos = 0;
	int rot_offset=0;
	int num_rot = 0;
	int scale_offset=0;
	int num_scale = 0;
};

class AnimationSet
{
public:
	int FindClipFromName(const char* name) const;

	// Returns index to use with GetX(), -1 if no keyframes
	int FirstPositionKeyframe(float frame, int channel, int clip) const;
	int FirstRotationKeyframe(float frame, int channel, int clip) const;
	int FirstScaleKeyframe(float frame, int channel, int clip) const;
	const PosKeyframe& GetPos(int channel, int index, int clip) const;
	const RotKeyframe& GetRot(int channel, int index, int clip) const;
	const ScaleKeyframe& GetScale(int channel, int index, int clip) const;
	const AnimChannel& GetChannel(int clip, int channel) const;
public:
	int num_channels = 0;
	std::vector<AnimChannel> channels;
	std::vector<PosKeyframe> positions;
	std::vector<RotKeyframe> rotations;
	std::vector<ScaleKeyframe> scales;
	std::vector<Animation> clips;
};

class Model;
class Animator
{
public:
	void Clear() {
		model = nullptr;
		set = nullptr;
		ResetLayers();
	}
	void Init(const Model* model);
	void SetupBones();
	void ConcatWithInvPose();
	const std::vector<glm::mat4x4> GetBones() const { 
		return cached_bonemats; 
	}

	void AdvanceFrame(float elapsed_time);
	void ResetLayers() {
		mainanim = leganim = -1;
		mainanim_frame = leganim_frame = 0.f;
		leganim_speed = 1.f;
	}
	void SetMainAnim(int anim) {
		mainanim = anim;
		mainanim_frame = 0.f;
	}
	void SetLegAnim(int anim) {
		leganim = anim;
		leganim_frame = 0.f;
		leganim_speed = 1.f;
	}
	void SetLegAnimSpeed(float speed) {
		leganim_speed = speed;
	}

	int mainanim=-1;
	float mainanim_frame;
	int leganim=-1;
	float leganim_frame;
	// only server side vars
	float leganim_speed;
	bool dont_loop = false;

	const Model* model = nullptr;
	const AnimationSet* set = nullptr;
private:
	//void DoHandIK(glm::quat localq[], glm::vec3 localp[], std::vector<glm::mat4x4>& globalbonemats);
	//void DoPlayerHandToGunIK(glm::quat localq[], glm::vec3 localp[], std::vector<glm::mat4x4>& globalbonemats);

	//const Actor* actor_owner = nullptr;

	std::vector<glm::mat4x4> cached_bonemats;		// final transform matricies, meshspace->bonespace->meshspace

	void CalcRotations(glm::quat q[], glm::vec3 pos[], int clip_index, float curframe);

	// lerps 1->2 and outputs to 1
	void LerpTransforms(glm::quat q1[], glm::vec3 p1[], glm::quat q2[], glm::vec3 p2[], float factor, int numbones);

	//void AddPlayerUpperbodyLayer(glm::quat q[], glm::vec3 pos[]);

	void UpdateGlobalMatricies(const glm::quat localq[], const glm::vec3 localp[], std::vector<glm::mat4x4>& out_bone_matricies);
};
#endif