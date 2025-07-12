#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Framework/StringName.h"
#include "Event.h"
#include "AnimationTypes.h"
#include "Runtime/Easing.h"
#include "Framework/ConsoleCmdGroup.h"

class Node_CFG;
class Pose;

struct ChannelOffset {
	uint32_t pos = 0;	// float[3]
	uint32_t rot = 0;	// float[4]
	uint32_t scale = 0;	// float[1]
};

struct ScalePositionRot {
	float scale;
	glm::vec3 pos;
	glm::quat rot;
};

struct SeqDirectPlayOpt {
	float blend_time = 0.2;
	StringName slotname;
	Easing easing=Easing::Linear;
};
struct SyncMarker {
	StringName name;
	int time = 0;
};
struct AnimCurveData {
	StringName name;
	struct Point {
		glm::vec2 val = glm::vec2(0.0,0.0);
		Easing interp=Easing::Linear;
	};
	void add_point(float time, float value, Easing interp) {
		points.push_back({ {time,value},interp });
	}
	std::vector<Point> points;
};

class AnimationSeq
{
public:
	int num_frames = 0;
	std::vector<ChannelOffset> channel_offsets;
	std::vector<float> pose_data;
	bool is_additive_clip = false;
	float duration = 0.0;
	float fps = 30.0;
	float average_linear_velocity = 0.0;
	bool has_rootmotion = false;
	// store any animation events or curves here
	vector<uptr<AnimationEvent>> events;
	vector<SyncMarker> syncMarkers;
	vector<uptr<AnimDurationEvent>> durationEvents;
	vector<AnimCurveData> curveData;
	SeqDirectPlayOpt directplayopt;
	// creational functions
	void add_event(AnimationEvent* ev);
	void add_duration_event(AnimDurationEvent* ev);
	void add_curve(AnimCurveData& curveData);
	void add_sync_marker(SyncMarker marker);

	int get_num_keyframes_inclusive() const { return num_frames + 1; }
	int get_num_keyframes_exclusive() const { return num_frames; }
	bool is_pose_clip() const { return num_frames == 1; }
	uint32_t get_num_channels() const { return channel_offsets.size(); }
	double get_clip_play_speed_for_linear_velocity(float velocity) const { return (average_linear_velocity >= 0.000001) ? velocity / average_linear_velocity : 0.0; }
	float get_duration() const { return duration; }
	int get_frame_for_time(float time) const { 
		int frame = int(time * fps);
		if (frame < 0)return 0;
		if (frame >= num_frames)return num_frames - 1;
		return frame;
	}
	float get_time_of_keyframe(int keyframe) const { return (float)keyframe / fps; }
	ScalePositionRot get_keyframe(int bone, int keyframe, float lerp) const;
	const AnimationEvent* get_events_for_keyframe(int keyframe, int* out_count) const;
private:
	glm::vec3* get_pos_write_ptr(int channel, int keyframe);
	float* get_scale_write_ptr(int channel, int keyframe);
	glm::quat* get_quat_write_ptr(int channel, int keyframe);

	friend class ModelCompileHelper;
};

// simple index remapping of bones
class MSkeleton;
struct BoneIndexRetargetMap {
	const MSkeleton* who = nullptr;
	// size = bones.size() 
	std::vector<int16_t> my_skeleton_to_who;
	std::vector<glm::quat> my_skelton_to_who_quat_delta;
};

enum class RetargetBoneType : uint8_t
{
	FromAnimation,
	FromTargetBindPose,
	FromAnimationScaled,
};

struct BoneData
{
	StringName name;
	std::string strname;
	int16_t parent;
	RetargetBoneType retarget_type = RetargetBoneType::FromAnimation;
	glm::mat4x3 posematrix;	// bone space -> mesh space
	glm::mat4x3 invposematrix; // mesh space -> bone space
	glm::mat4x3 localtransform;
	glm::quat rot;
};

struct BonePoseMask
{
	std::string strname;
	StringName idname;
	std::vector<float> weight;
};

class Model;
class MSkeleton
{
public:
	MSkeleton() = default;
	~MSkeleton();
	void move_construct(MSkeleton& other);

	bool is_skeleton_the_same(const MSkeleton& other) const;
	int get_num_bones() const { return (int)bone_dat.size(); }
	int get_bone_index(StringName name) const;
	int get_root_bone_index() const { return 0; }
	int get_bone_parent(int bone) const { return (int)bone_dat.at(bone).parent; }
	bool has_mirroring_table() const { return (int)mirroring_table.size() == get_num_bones(); }
	int get_mirrored_bone(int index) const {
		assert(has_mirroring_table());
		return (int)mirroring_table[index];
	}
	const glm::mat4x3& get_bone_local_transform(int index) const { return bone_dat[index].localtransform; }
	const glm::quat& get_bone_local_rotation(int index) const { return bone_dat[index].rot; }
	const glm::mat4x3& get_inv_posematrix(int index) const { return bone_dat[index].invposematrix; }
	const AnimationSeq* find_clip(const std::string& name) const;
	AnimationSeq* find_clip(const std::string& name);
	const BoneIndexRetargetMap* get_remap(const MSkeleton* other);
	const std::vector<BoneData>& get_all_bones() const {
		return bone_dat;
	}
	int get_num_animations() const { return clips.size(); }
	struct refed_clip {
		AnimationSeq* ptr = nullptr;
	};
	const std::unordered_map<std::string, refed_clip>& get_all_clips() const { return clips; }
private:
	// if adding data, update move_construct
	std::vector<std::unique_ptr<BoneIndexRetargetMap>> remaps;
	std::vector<BoneData> bone_dat;
	std::vector<int16_t> mirroring_table;
	std::unordered_map<std::string, refed_clip> clips;

	friend class Animation_Tree_Manager;
	friend class ModelCompileHelper;
	friend class ModelMan;
	friend class Model;
	friend class AgBoneFinder;	// for accessing bones
public:
	// For use with editor
	const std::unordered_map<std::string, refed_clip>& get_clips_hashmap() const { return clips; }
};
