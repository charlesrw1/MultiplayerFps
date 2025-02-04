#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <vector>
#include <string>
#include <memory>

class Model;
using std::vector;
using std::unique_ptr;

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

struct Animation_Notify_List;
class Animation
{
public:
	std::string name;
	float total_duration = 0;
	bool looping = false;
	float fps = 24;

	int channel_offset = 0;
	int pos_offset = 0;
	int num_pos = 0;
	int rot_offset = 0;
	int num_rot = 0;
	int scale_offset = 0;
	int num_scale = 0;
	// end - start
	glm::vec3 root_motion_translation;

	// events, ptr can be null, memory is always valid
	const Animation_Notify_List* notify = nullptr;
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

struct Animation_Index
{
	int16_t set = -1;
	int16_t clip = -1;
	int16_t skel = -1;
};


class Pose
{
public:
	const static int MAX_BONES = 256;

	glm::quat q[MAX_BONES];
	glm::vec3 pos[MAX_BONES];
	float scale[MAX_BONES];
};
