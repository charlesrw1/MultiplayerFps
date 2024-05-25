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


class Animation_Clip_New
{
public:
	std::string source_name;
	StringName idname;
	int num_frames = 0;
	std::vector<uint32_t> channel_offsets;
	// pose_data.size() >= num_channels && pose_data.size() <= num_frames*num_channels
	std::vector<SRT> pose_data;
	bool is_additive_clip = false;
	float duration = 0.0;
	float fps = 24.0;
	float average_linear_velocity = 0.0;


	// store any animation events or curves here
	std::vector<EventIndex> event_keyframes;
	std::vector<AnimEvent> events;

	int get_num_keyframes() const { return num_frames; }
	bool is_pose_clip() const { return num_frames == 1; }
	uint32_t get_num_channels() const { return channel_offsets.size(); }

	float get_clip_play_speed_for_linear_velocity(float velocity) const { return (average_linear_velocity >= 0.000001) ? velocity / average_linear_velocity : 0.0; }
	float get_duration() const { return duration; }
	int get_frame_for_time(float time) const { 
		int frame = time * fps;
		ASSERT(frame >= 0 && frame < num_frames);
		return frame;
	}
	float get_time_of_keyframe(int keyframe) const { return (float)keyframe / fps; }

	const SRT& get_keyframe(int bone, int keyframe/* get_frame_for_time(), not time!*/, bool& is_single_pose) const;
	SRT& get_keyframe(int bone, int keyframe/* get_frame_for_time(), not time!*/, bool& is_single_pose);

	const AnimEvent* get_events_for_keyframe(int keyframe, int* out_count) const;
};


// simple index remapping of bones
struct Skeleton_New;
struct Bone_Index_Remap
{
	const Skeleton_New* who = nullptr;
	// size = other.bones.size() 
	std::vector<int16_t> other_to_this;

	bool skeleton_data_is_equivlent() const { return other_to_this.empty(); }
};


class BonePoseMask
{
public:
	StringName name;
	std::vector<float> weight;

	void set_weights_from_definition(const Skeleton_New* skel);

	struct Definition {
		std::string name;
		float weight;
	};
	std::vector<Definition> mask_definition;
};

enum class RetargetBoneType : uint8_t
{
	FromAnimation,
	FromTargetBindPose,
	FromAnimationScaled,
};

class Model;
class Skeleton_New
{
public:
	Skeleton_New() = default;
	~Skeleton_New();

	bool is_skeleton_the_same(const Skeleton_New& other) const {
		if (get_num_bones() != other.get_num_bones())
			return false;
		for (int i = 0; i < get_num_bones(); i++) {
			if (bone_names[i] != other.bone_names[i])
				return false;
		}
		return true;
	}

	int get_num_bones() const { return inv_pose_matrix.size(); }
	int get_bone_index(StringName name) const {
		for (int i = 0; i < bone_names.size(); i++) {
			if (bone_names[i] == name)
				return i;
		}
		return -1;
	}
	int get_root_bone_index() const { return 0; }
	int get_bone_parent(int bone) const { return parents[bone]; }

	bool has_mirroring_table() const { return mirroring_table.size() == get_num_bones(); }
	const glm::mat4x4& get_bone_local_transform(int index) const { return local_transform[index]; }
	const glm::quat& get_bone_local_rotation(int index) const { return local_rot[index]; }
	const glm::mat4& get_inv_posematrix(int index) const { return inv_pose_matrix[index]; }

	bool is_valid = false;

	// soa bone data
	std::vector<glm::quat> local_rot;
	std::vector<glm::mat4x4> local_transform;
	std::vector<glm::mat4x4> inv_pose_matrix;
	std::vector<glm::mat4x4> pose_matrix;
	std::vector<int16_t> parents;
	std::vector<StringName> bone_names;
	std::vector<RetargetBoneType> retarget_type;

	std::string name = "";
	glm::mat4x4 armature_root_transform;
	std::vector<BonePoseMask> masks;
	std::vector<Bone_Index_Remap> remaps;
	std::vector<int16_t> mirroring_table;

	const Animation_Clip_New* find_clip(const std::string& name, int& remap_index) const;
private:
	friend class SkeletonLoadHelper;

	std::unordered_map<std::string, Animation_Clip_New*> clips;
	struct Import {
		
		const Model* model_with_set = nullptr;
		int remap_index_on_skeleton = -1;

		bool has_remap() const { return remap_index_on_skeleton != -1; }
		const Skeleton_New* get_skeletal_data() const {
			ASSERT(model_with_set && model_with_set->get_skeleton());
			return model_with_set->get_skeleton();
		}
	};
	std::vector<Import> imports;

	friend class Animation_Tree_Manager;
	friend class SkeletonLoadHelper;
};
