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

template<typename T>
struct KeyframeBase 
{
	T val;
	float time;
};

typedef KeyframeBase<glm::vec3> PosKeyframe;
typedef KeyframeBase<glm::vec3> ScaleKeyframe;
typedef KeyframeBase<glm::quat> RotKeyframe;


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

class Animation_Set
{
public:
	int find(const char* animation_name) const;

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

struct Animator_Layer
{
	int anim = -1;
	float frame = 0;
	float speed = 1;
	bool finished = false;
	bool loop = false;
	int blend_anim = -1;
	float blend_frame = 1;
	float blend_remaining = 0;
	float blend_time = 0;

	// horrible hacky fix for networking
	int staging_anim = -1;
	float staging_frame = 0;
	bool staging_loop = false;
	float staging_speed = 1.f;

	void update(float dt, const Animation& clip);
};

class Model;
class Animator
{
public:
	void set_model(const Model* model);
	void set_anim(const char* name, bool restart, float blend = 0.1f);
	void set_anim_from_index(Animator_Layer& l, int animation, bool restart, float blend = 0.1f);
	void set_leg_anim(const char* name, bool restart, float blend = 0.1f);

	void SetupBones();
	void ConcatWithInvPose();
	const std::vector<glm::mat4x4> GetBones() const { 
		return cached_bonemats; 
	}

	void AdvanceFrame(float elapsed_time);

	Animator_Layer m;		// main animimation, upper body
	Animator_Layer legs;	// legs

	const Model* model = nullptr;
	const Animation_Set* set = nullptr;
private:
	//void DoHandIK(glm::quat localq[], glm::vec3 localp[], std::vector<glm::mat4x4>& globalbonemats);
	//void DoPlayerHandToGunIK(glm::quat localq[], glm::vec3 localp[], std::vector<glm::mat4x4>& globalbonemats);

	std::vector<glm::mat4x4> cached_bonemats;		// final transform matricies, meshspace->bonespace->meshspace

	void CalcRotations(glm::quat q[], glm::vec3 pos[], int clip_index, float curframe);

	// lerps 1->2 and outputs to 1
	void LerpTransforms(glm::quat q1[], glm::vec3 p1[], glm::quat q2[], glm::vec3 p2[], float factor, int numbones);

	void add_legs_layer(glm::quat q[], glm::vec3 pos[]);

	void UpdateGlobalMatricies(const glm::quat localq[], const glm::vec3 localp[], std::vector<glm::mat4x4>& out_bone_matricies);
};
#endif