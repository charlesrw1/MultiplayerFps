#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Framework/StringName.h"

#include "Event.h"
#include "AnimationTypes.h"

class Node_CFG;
class Pose;


struct EventIndex {
	uint8_t offset = 0;
	uint8_t count = 0;
};

struct ChannelOffset
{
	uint32_t pos = 0;	// float[3]
	uint32_t rot = 0;	// float[4]
	uint32_t scale = 0;	// float[1]
};

struct ScalePositionRot
{
	float scale;
	glm::vec3 pos;
	glm::quat rot;
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


	// store any animation events or curves here
	std::vector<EventIndex> event_keyframes;
	std::vector<AnimEvent> events;

	int get_num_keyframes_inclusive() const {
		return num_frames + 1;
	}
	int get_num_keyframes_exclusive() const { 
		return num_frames; 
	}
	bool is_pose_clip() const { return num_frames == 1; }
	uint32_t get_num_channels() const { return channel_offsets.size(); }

	float get_clip_play_speed_for_linear_velocity(float velocity) const { return (average_linear_velocity >= 0.000001) ? velocity / average_linear_velocity : 0.0; }
	float get_duration() const { return duration; }
	int get_frame_for_time(float time) const { 
		int frame = time * fps;
		assert(frame >= 0 && frame < num_frames);
		return frame;
	}
	float get_time_of_keyframe(int keyframe) const { return (float)keyframe / fps; }

	ScalePositionRot get_keyframe(int bone, int keyframe, float lerp) const;

	const AnimEvent* get_events_for_keyframe(int keyframe, int* out_count) const;

private:
	glm::vec3* get_pos_write_ptr(int channel, int keyframe);
	glm::quat* get_quat_write_ptr(int channel, int keyframe);

	friend class ModelCompileHelper;
};


// simple index remapping of bones

class MSkeleton;
struct Bone_Index_Remap
{
	const MSkeleton* who = nullptr;
	// size = other.bones.size() 
	std::vector<int16_t> other_to_this;

	bool skeleton_data_is_equivlent() const { return other_to_this.empty(); }
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

	bool is_skeleton_the_same(const MSkeleton& other) const {
		if (get_num_bones() != other.get_num_bones())
			return false;
		for (int i = 0; i < get_num_bones(); i++) {
			if (bone_dat[i].name != other.bone_dat[i].name)
				return false;
		}
		return true;
	}

	int get_num_bones() const { return bone_dat.size(); }
	int get_bone_index(StringName name) const {
		for (int i = 0; i < bone_dat.size(); i++) {
			if (bone_dat[i].name == name)
				return i;
		}
		return -1;
	}
	int get_root_bone_index() const { return 0; }
	int get_bone_parent(int bone) const { return bone_dat[bone].parent; }

	bool has_mirroring_table() const { return mirroring_table.size() == get_num_bones(); }
	int get_mirrored_bone(int index) const {
		assert(has_mirroring_table());
		return mirroring_table[index];
	}

	const glm::mat4x3& get_bone_local_transform(int index) const { return bone_dat[index].localtransform; }
	const glm::quat& get_bone_local_rotation(int index) const { return bone_dat[index].rot; }
	const glm::mat4x3& get_inv_posematrix(int index) const { return bone_dat[index].invposematrix; }

	const AnimationSeq* find_clip(const std::string& name, int& remap_index) const;

	const Bone_Index_Remap* get_remap(int index) const {
		return &remaps[index];
	}
	const BonePoseMask* find_mask(StringName name) const {
		for (int i = 0; i < masks.size(); i++) {
			if (masks[i].idname == name)
				return &masks[i];
		}
		return nullptr;
	}

private:

	std::vector<BonePoseMask> masks;
	std::vector<Bone_Index_Remap> remaps;
	std::vector<BoneData> bone_dat;
	std::vector<int16_t> mirroring_table;

	struct refed_clip {
		AnimationSeq* ptr = nullptr;
		int16_t remap_idx = -1;
		bool skeleton_owns_clip = false;
	};
	std::unordered_map<std::string, refed_clip> clips;

	struct imported_model {
		const Model* model_with_set = nullptr;
		int remap_index_on_skeleton = -1;
		bool has_remap() const { return remap_index_on_skeleton != -1; }
	};
	std::vector<imported_model> imports;

	friend class Animation_Tree_Manager;
	friend class ModelCompileHelper;
	friend class ModelMan;
public:
	// For use with editor
	const std::unordered_map<std::string, refed_clip>& get_clips_hashmap() const { return clips; }
};
