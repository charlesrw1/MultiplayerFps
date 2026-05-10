#ifdef EDITOR_BUILD

#include "ModelCompilierLocal.h"
#include "Animation/SkeletonData.h"
#include "Framework/DictParser.h"
#include "Compiliers.h"
#include "Render/Model.h"
#include "cgltf.h"
#define USE_CGLTF
#include <unordered_set>
#include "Framework/Files.h"
#include "glm/gtc/type_ptr.hpp"
#include <algorithm>

#include "Animation/AnimationUtil.h"

#include "Framework/BinaryReadWrite.h"
#include "Physics/Physics2.h"
#include "AssetCompile/Someutils.h"
#include <stdexcept>

#include "Framework/Config.h"
#include "Assets/AssetDatabase.h"

#include <physx/cooking/PxCooking.h>

#include <fstream>

#include <meshoptimizer.h>

static bool are_all_poskeyframes_equal(float epsilon, const Animation_Set* set, const Animation* a, int LOAD_channel) {
	int offset = a->channel_offset + LOAD_channel;
	const AnimChannel& chan = set->channels[offset];
	int pos_start = chan.pos_start;
	int count = chan.num_positions;

	for (int i = 1; i < count; i++) {
		int index = pos_start + i;
		glm::vec3 first = set->positions[pos_start].val;
		glm::vec3 this_ = set->positions[index].val;
		float sq_dist = glm::dot(first - this_, first - this_);
		if (sq_dist > epsilon)
			return false;
	}
	return true;
}

static bool are_all_rotframes_equal(float epsilon, const Animation_Set* set, const Animation* a, int LOAD_channel) {
	int offset = a->channel_offset + LOAD_channel;
	const AnimChannel& chan = set->channels[offset];
	int rot_start = chan.rot_start;
	int count = chan.num_rotations;

	for (int i = 1; i < count; i++) {
		int index = rot_start + i;
		glm::quat first = set->rotations[rot_start].val;
		glm::quat this_ = set->rotations[index].val;
		float sq_dist = glm::dot(first - this_, first - this_);
		if (sq_dist > epsilon)
			return false;
	}
	return true;
}

static bool are_all_scaleframes_equal(float epsilon, const Animation_Set* set, const Animation* a, int LOAD_channel) {
	int offset = a->channel_offset + LOAD_channel;
	const AnimChannel& chan = set->channels[offset];
	int scale_start = chan.scale_start;
	int count = chan.num_scales;

	for (int i = 1; i < count; i++) {
		int index = scale_start + i;
		glm::vec3 first = set->scales[scale_start].val;
		glm::vec3 this_ = set->scales[index].val;
		float sq_dist = glm::dot(first - this_, first - this_);
		if (sq_dist > epsilon)
			return false;
	}
	return true;
}

static void write_out_to_outseq(float* f, int size, AnimationSeq* seq) {
	for (int i = 0; i < size; i++)
		seq->pose_data.push_back(f[i]);
}
static_assert(alignof(glm::quat) == 4, "a");
static_assert(alignof(glm::vec3) == 4, "a");

inline float lerp_between(float min, float max, float mid_val) {
	return (mid_val - min) / (max - min);
}

void ModelCompileHelper::append_animation_seq_to_list(AnimationSourceToCompile source, FinalSkeletonOutput* final_,
													  const std::vector<int>& FINAL_bone_to_LOAD_bone,
													  const std::vector<int>& LOAD_bone_to_FINAL_bone,
													  const SkeletonCompileData* myskel, const ModelDefData& data) {
	if (final_->does_sequence_already_exist(source.get_animation_name()))
		return;

	const int target_count = FINAL_bone_to_LOAD_bone.size();

	const AnimationClip_Load* definition = data.find(source.get_animation_name());

	const Animation* source_a = source.get_animation();
	const Animation_Set* source_set = source.get_set();

	AnimationSeq out_seq;

	const float fps = data.override_fps;
	out_seq.fps = fps;
	if (definition)
		out_seq.has_rootmotion = definition->enableRootMotion;

	int START_keyframe = 0;
	int NUM_keyframes = source_a->total_duration * fps;
	int END_keyframe = NUM_keyframes;
	if (definition && definition->crop.has_crop) {
		if (definition->crop.start >= 0 && definition->crop.start < END_keyframe)
			START_keyframe = definition->crop.start;
		if (definition->crop.end < END_keyframe && definition->crop.end > START_keyframe)
			END_keyframe = definition->crop.end;
		else if (definition->crop.end < 0) {
			END_keyframe = NUM_keyframes + definition->crop.end;
		}
		NUM_keyframes = END_keyframe - START_keyframe;
	}

	out_seq.duration = source_a->total_duration;
	out_seq.num_frames = NUM_keyframes;

	out_seq.channel_offsets.resize(target_count);

	const int clip_index = source.animation_souce_index;

	for (int FINAL_idx = 0; FINAL_idx < target_count; FINAL_idx++) {

		const int LOAD_idx = FINAL_bone_to_LOAD_bone[FINAL_idx];
		assert(LOAD_idx != -1);

		ChannelOffset& offsets = out_seq.channel_offsets[FINAL_idx];

		const int SRC_idx = (source.remap) ? (*source.remap)[LOAD_idx] : LOAD_idx;

#define SET_HIGH_BIT(x) x |= (1u << 31u)

		if (SRC_idx == -1) {
			offsets.pos = out_seq.pose_data.size();
			SET_HIGH_BIT(offsets.pos);
			glm::vec3 pos = myskel->get_local_position(LOAD_idx);
			write_out_to_outseq(&pos.x, 3, &out_seq);

			offsets.rot = out_seq.pose_data.size();
			SET_HIGH_BIT(offsets.rot);
			glm::quat rot = myskel->get_local_rotation(LOAD_idx);
			write_out_to_outseq(&rot.x, 4, &out_seq);

			offsets.scale = out_seq.pose_data.size();
			SET_HIGH_BIT(offsets.scale);
			float scale = myskel->get_local_scale(LOAD_idx);
			write_out_to_outseq(&scale, 1, &out_seq);

			continue;
		}

#define CALC_TIME_INTERP(FuncName, VarName)                                                                            \
	int index0 = VarName;                                                                                              \
	int index1 = VarName + 1;                                                                                          \
	float t0 = source_set->FuncName(SRC_idx, index0, clip_index).time;                                                 \
	float t1 = source_set->FuncName(SRC_idx, index1, clip_index).time;                                                 \
	float scale = lerp_between(t0, t1, TIME);

		// POSITION
		offsets.pos = out_seq.pose_data.size();
		bool all_pos_equal = are_all_poskeyframes_equal(0.0001f, source_set, source_a, SRC_idx);
		auto get_pos_for_time = [&](const float TIME) -> glm::vec3 {
			int pos_idx = source_set->FirstPositionKeyframe(TIME, SRC_idx, source.animation_souce_index);

			glm::vec3 pos{};
			if (pos_idx == -1)
				pos = source.skel->get_local_position(SRC_idx);
			else if (pos_idx == source_set->GetChannel(source.animation_souce_index, SRC_idx).num_positions - 1)
				pos = source_set->GetPos(SRC_idx, pos_idx, source.animation_souce_index).val;
			else {
				CALC_TIME_INTERP(GetPos, pos_idx);
				pos = glm::mix(source_set->GetPos(SRC_idx, index0, clip_index).val,
							   source_set->GetPos(SRC_idx, index1, clip_index).val, scale);
			}
			return pos;
		};

		if (all_pos_equal) {
			SET_HIGH_BIT(offsets.pos);
			glm::vec3 pos = get_pos_for_time(0.0);
			write_out_to_outseq(&pos.x, 3, &out_seq);
		} else {
			for (int frame = 0; frame < out_seq.get_num_keyframes_inclusive(); frame++) {
				const int frame_w_crop = frame + START_keyframe;
				const float t = frame_w_crop / fps;
				ASSERT(t <= out_seq.duration);
				glm::vec3 pos = get_pos_for_time(t);
				write_out_to_outseq(&pos.x, 3, &out_seq);
			}
			assert((out_seq.pose_data.size() - offsets.pos) / 3 == (out_seq.get_num_keyframes_inclusive()));
		}

		// ROTATION
		offsets.rot = out_seq.pose_data.size();
		bool all_rot_equal = are_all_rotframes_equal(0.0001f, source_set, source_a, SRC_idx);
		auto get_rot_for_time = [&](const float TIME) -> glm::quat {
			int rot_idx = source_set->FirstRotationKeyframe(TIME, SRC_idx, source.animation_souce_index);
			glm::quat interp_rot{};
			if (rot_idx == -1) {
				interp_rot = source.skel->get_local_rotation(SRC_idx);
			} else if (rot_idx == source_set->GetChannel(clip_index, SRC_idx).num_rotations - 1)
				interp_rot = source_set->GetRot(SRC_idx, rot_idx, clip_index).val;
			else {
				CALC_TIME_INTERP(GetRot, rot_idx);
				interp_rot = glm::slerp(source_set->GetRot(SRC_idx, index0, clip_index).val,
										source_set->GetRot(SRC_idx, index1, clip_index).val, scale);
			}
			interp_rot = glm::normalize(interp_rot);

			return interp_rot;
		};

		if (all_rot_equal) {
			SET_HIGH_BIT(offsets.rot);
			glm::quat rot = get_rot_for_time(0.0);
			write_out_to_outseq(&rot.x, 4, &out_seq);
		} else {
			for (int frame = 0; frame < out_seq.get_num_keyframes_inclusive(); frame++) {
				const int frame_w_crop = frame + START_keyframe;
				const float t = frame_w_crop / fps;
				ASSERT(t <= out_seq.duration);
				glm::quat rot = get_rot_for_time(t);
				write_out_to_outseq(&rot.x, 4, &out_seq);
			}
			assert((out_seq.pose_data.size() - offsets.rot) / 4 == out_seq.get_num_keyframes_inclusive());
		}

		// SCALE
		offsets.scale = out_seq.pose_data.size();
		bool all_scale_equal = are_all_scaleframes_equal(0.0001f, source_set, source_a, SRC_idx);
		auto get_scale_for_time = [&](const float TIME) -> float {
			int scale_idx = source_set->FirstScaleKeyframe(TIME, SRC_idx, source.animation_souce_index);
			glm::vec3 interp_scale{};
			if (scale_idx == -1) {
				interp_scale = glm::vec3(1.0);
			} else if (scale_idx == source_set->GetChannel(clip_index, SRC_idx).num_scales - 1)
				interp_scale = source_set->GetScale(SRC_idx, scale_idx, clip_index).val;
			else {
				CALC_TIME_INTERP(GetScale, scale_idx);
				interp_scale = glm::mix(source_set->GetScale(SRC_idx, index0, clip_index).val,
										source_set->GetScale(SRC_idx, index1, clip_index).val, scale);
			}
			float uniform_scale = glm::max(glm::max(interp_scale.x, interp_scale.y), interp_scale.z);
			return uniform_scale;
		};

		if (all_scale_equal) {
			SET_HIGH_BIT(offsets.scale);
			float uniform_scale = get_scale_for_time(0.0);
			write_out_to_outseq(&uniform_scale, 1, &out_seq);
		} else {
			for (int frame = 0; frame < out_seq.get_num_keyframes_inclusive(); frame++) {
				const int frame_w_crop = frame + START_keyframe;
				const float t = frame_w_crop / fps;
				ASSERT(t <= out_seq.duration);
				float uniform_scale = get_scale_for_time(t);
				write_out_to_outseq(&uniform_scale, 1, &out_seq);
			}
			assert((out_seq.pose_data.size() - offsets.scale) == out_seq.get_num_keyframes_inclusive());
		}

#undef CALC_TIME_INTERP
#undef SET_HIGH_BIT
	}

	// do reparenting here
	for (int i = 0; i < final_->reparents.size(); i++) {
		auto& r = final_->reparents[i];
		ASSERT(final_->bones[r.FINAL_index].parent != -1);

		auto get_keyframe_matrix = [](AnimationSeq& out_seq, int FINAL_index, int frame) -> glm::mat4 {
			auto keyframe = out_seq.get_keyframe(FINAL_index, frame, 0.0);
			glm::mat4 local_matrix = glm::translate(glm::mat4(1), keyframe.pos);
			local_matrix = local_matrix * glm::mat4_cast(keyframe.rot);
			local_matrix = glm::scale(local_matrix, glm::vec3(keyframe.scale));
			return local_matrix;
		};
		auto get_worldspace_keyframe = [&](FinalSkeletonOutput* final_, AnimationSeq& out_seq, int FINAL_index,
										   int frame) -> glm::mat4 {
			int THEINDEX = FINAL_index;
			glm::mat4 matrix = glm::mat4(1.f);
			while (THEINDEX != -1) {
				matrix = get_keyframe_matrix(out_seq, THEINDEX, frame) * matrix;

				THEINDEX = final_->bones.at(THEINDEX).parent;
			}
			return matrix;
		};

		for (int frame = 0; frame < out_seq.get_num_keyframes_inclusive(); frame++) {
			const int myparent = final_->bones.at(r.FINAL_index).parent;
			ASSERT(myparent != -1);

			auto keyframe = out_seq.get_keyframe(r.FINAL_index, frame, 0.0);

			glm::mat4 worldspace = get_keyframe_matrix(out_seq, r.FINAL_index, frame);
			glm::mat4 parent_worldspace = get_worldspace_keyframe(final_, out_seq, myparent, frame);
			glm::mat4 newlocalspace = glm::inverse(parent_worldspace) * worldspace;

			auto pos = out_seq.get_pos_write_ptr(r.FINAL_index, frame);
			auto rot = out_seq.get_quat_write_ptr(r.FINAL_index, frame);
			auto scale = out_seq.get_scale_write_ptr(r.FINAL_index, frame);
			if (pos)
				*pos = newlocalspace[3];
			if (rot)
				*rot = glm::normalize(glm::quat_cast(newlocalspace));
			if (scale)
				*scale = glm::length(newlocalspace[0]);
		}
	}

#define IS_HIGH_BIT_SET(x) (x & (1u << 31u))
#define CLEAR_HIGH_BIT(x) (x & ~(1u << 31u))

	out_seq.duration = (float)NUM_keyframes / fps;

	// Apply any retargeting
	if (source.should_retarget_this) {
		for (int FINAL_idx = 0; FINAL_idx < target_count; FINAL_idx++) {
			const int LOAD_idx = FINAL_bone_to_LOAD_bone[FINAL_idx];
			assert(LOAD_idx != -1);
			float scale = 1.0;

			const int SRC_idx = (source.remap) ? (*source.remap)[LOAD_idx] : LOAD_idx;
			if (SRC_idx == -1)
				continue;

			glm::mat4 transform_matrix = glm::mat4(1.0);

			if (myskel->get_bone_parent(LOAD_idx) == -1) {
				glm::mat4 local_other = source.skel->bones[SRC_idx].localtransform;
				transform_matrix = glm::inverse(myskel->armature_root) * source.skel->armature_root;
			}
			if (myskel->bones[LOAD_idx].retarget_type == RetargetBoneType::FromAnimationScaled) {
				scale = glm::length(myskel->get_local_position(LOAD_idx));
				scale /= glm::length(source.skel->get_local_position(SRC_idx));
			}

			for (int keyframe = 0; keyframe < out_seq.get_num_keyframes_inclusive(); keyframe++) {
				glm::vec3* pos = out_seq.get_pos_write_ptr(FINAL_idx, keyframe);
				glm::quat* rot = out_seq.get_quat_write_ptr(FINAL_idx, keyframe);
				if (!pos && !rot)
					break;
				if (myskel->get_bone_parent(LOAD_idx) == -1) {

					if (pos)
						*pos = transform_matrix * glm::vec4(*pos, 1.0);

					glm::mat3 justrot2 = transform_matrix;
					justrot2[0] = glm::normalize(justrot2[0]);
					justrot2[1] = glm::normalize(justrot2[1]);
					justrot2[2] = glm::normalize(justrot2[2]);

					auto try2 = glm::quat_cast(justrot2);
					if (rot)
						*rot = try2 * (*rot);
				} else if (myskel->bones[LOAD_idx].retarget_type == RetargetBoneType::FromAnimationScaled) {
					if (pos)
						*pos *= scale;
				} else if (myskel->bones[LOAD_idx].retarget_type == RetargetBoneType::FromTargetBindPose) {
					if (pos)
						*pos = myskel->get_local_position(LOAD_idx);
				}
			}
		}
	}

	assert(myskel->get_bone_parent(FINAL_bone_to_LOAD_bone[0]) == -1);
	// Calculate average linear velocity
	{
		ChannelOffset& chan = out_seq.channel_offsets[0];
		bool is_single_frame = IS_HIGH_BIT_SET(chan.pos);
		uint32_t pose_start = CLEAR_HIGH_BIT(chan.pos);
		glm::vec3 average_linear_vec = glm::vec3(0.f);
		if (is_single_frame)
			out_seq.average_linear_velocity = 0.0;
		else {
			glm::vec3 first = out_seq.get_keyframe(0, 0, 0.0).pos;
			glm::vec3 last = out_seq.get_keyframe(0, out_seq.get_num_keyframes_exclusive(), 0.0).pos;

			glm::vec3 dif = last - first;
			average_linear_vec = dif / out_seq.get_duration();
			out_seq.average_linear_velocity = glm::length(dif) / out_seq.get_duration();
		}

		if (definition && definition->removeLienarVelocity) {

			for (int frame = 0; frame < out_seq.get_num_keyframes_inclusive(); frame++) {
				const int frame_w_crop = frame + START_keyframe;
				const float t = frame / fps;
				ASSERT(t <= out_seq.duration);
				glm::vec3* pos0 = out_seq.get_pos_write_ptr(0, frame);
				if (pos0)
					*pos0 -= t * average_linear_vec;
				else
					sys_print(Warning, "removeLienarVelocity single frame\n");
			}
		}
		if (definition && definition->setRootToFirstFrame) {
			auto keyframe = out_seq.get_keyframe(0, 0, 0.0);
			for (int frame = 0; frame < out_seq.get_num_keyframes_inclusive(); frame++) {
				const int frame_w_crop = frame + START_keyframe;
				const float t = frame / fps;
				ASSERT(t <= out_seq.duration);
				glm::vec3* pos = out_seq.get_pos_write_ptr(0, frame);
				if (pos)
					*pos = keyframe.pos;
			}
		}
	}

	if (definition && definition->fixloop) {
		for (int FINAL_idx = 0; FINAL_idx < target_count; FINAL_idx++) {
			const glm::vec3* pos0 = out_seq.get_pos_write_ptr(FINAL_idx, 0);
			const glm::quat* rot0 = out_seq.get_quat_write_ptr(FINAL_idx, 0);
			const float* scale0 = out_seq.get_scale_write_ptr(FINAL_idx, 0);

			glm::vec3* pos1 = out_seq.get_pos_write_ptr(FINAL_idx, out_seq.get_num_keyframes_inclusive() - 1);
			glm::quat* rot1 = out_seq.get_quat_write_ptr(FINAL_idx, out_seq.get_num_keyframes_inclusive() - 1);
			float* scale1 = out_seq.get_scale_write_ptr(FINAL_idx, out_seq.get_num_keyframes_inclusive() - 1);

			if (!pos1 && !rot1 && !scale1)
				continue;
			if (pos1)
				*pos1 = *pos0;
			if (rot1)
				*rot1 = *rot0;
			if (scale1)
				*scale1 = *scale0;
		}
	}

	final_->add_sequence(source.get_animation_name(), std::move(out_seq));
}

void ModelCompileHelper::subtract_clips(const int num_bones, AnimationSeq* target, const AnimationSeq* source) {
	Pose ref_pose;
	for (int i = 0; i < num_bones; i++) {
		ScalePositionRot transform = source->get_keyframe(i, 0, 0.0);
		ref_pose.pos[i] = transform.pos;
		ref_pose.q[i] = transform.rot;
		ref_pose.scale[i] = transform.scale;
	}

	for (int i = 0; i < num_bones; i++) {
		for (int j = 0; j < target->get_num_keyframes_inclusive(); j++) {

			glm::vec3* pos = target->get_pos_write_ptr(i, j);
			glm::quat* rot = target->get_quat_write_ptr(i, j);
			float* scale = target->get_scale_write_ptr(i, j);

			if (!pos && !rot && !scale)
				break;
			if (pos)
				*pos = *pos - ref_pose.pos[i];
			if (rot)
				*rot = quat_delta(ref_pose.q[i], *rot);
			if (scale)
				*scale = *scale - ref_pose.scale[i];
		}
	}
}

static std::vector<std::string> get_imported_models(const ModelDefData& def) {
	std::vector<std::string> strs;
	for (int i = 0; i < def.imports.size(); i++) {
		if (def.imports[i].type == AnimImportType_Load::Model)
			strs.push_back(def.imports[i].name);
	}
	return strs;
}

static std::pair<std::vector<BoneData>, std::vector<FinalSkeletonOutput::ReparentData>> get_final_bone_data(
	const std::vector<int>& FINAL_bone_to_LOAD_bone, const std::vector<int>& LOAD_to_FINAL,
	const SkeletonCompileData* myskel, const ModelDefData& data) {

	std::vector<BoneData> out(FINAL_bone_to_LOAD_bone.size());
	std::vector<FinalSkeletonOutput::ReparentData> reparents;
	for (int i = 0; i < out.size(); i++) {
		int index = FINAL_bone_to_LOAD_bone[i];
		assert(index != -1);
		out[i] = myskel->bones[index];

		if (myskel->bones[index].parent != -1) {
			int FINAL_parent = LOAD_to_FINAL[myskel->bones[index].parent];
			assert(FINAL_parent != -1);
			out[i].parent = FINAL_parent;
		}

		{
			auto find = data.bone_reparent.find(out[i].strname);
			if (find != data.bone_reparent.end()) {
				int parent_to_this = myskel->get_bone_for_name(find->second);
				if (parent_to_this == -1) {
					sys_print(Warning, "couldnt find bone for reparent %s\n", find->second.c_str());
				} else if (out[i].parent != -1) {
					sys_print(Warning, "cant reparent, only parent to\n");
				} else {
					sys_print(Debug, "reparent bone %s %s\n", out[i].strname.c_str(), find->second.c_str());
					int LOAD_parent = parent_to_this;
					auto& parent = myskel->bones.at(LOAD_parent);
					glm::mat4 myworldspace = out[i].posematrix;
					auto parentinv = glm::inverse(glm::mat4(parent.posematrix));
					glm::mat4 mylocalspace = parentinv * myworldspace;
					out[i].localtransform = mylocalspace;
					out[i].rot = glm::quat_cast(glm::mat4(mylocalspace));
					out[i].parent = LOAD_to_FINAL.at(LOAD_parent);

					reparents.push_back({i});
				}
			}
		}

		auto find = data.bone_rename.find(out[i].strname);
		if (find != data.bone_rename.end()) {
			out[i].strname = find->second;
		}
	}
	return {out, reparents};
}

static std::vector<int16_t> get_mirror_table(const SkeletonCompileData* myskel,
									  const std::vector<int>& LOAD_bone_to_FINAL_bone, const int FINAL_bones_count,
									  const ModelDefData& def) {
	if (def.mirrored_bones.empty())
		return {};
	std::vector<int16_t> out_mirror(FINAL_bones_count, -1);
	for (int i = 0; i < def.mirrored_bones.size(); i++) {
		auto& mir = def.mirrored_bones[i];

		int index0 = myskel->get_bone_for_name(mir.bone1);
		int index1 = myskel->get_bone_for_name(mir.bone2);
		if (index0 == -1 || index1 == -1) {
			sys_print(Error, "mirrored bone not found %s %s\n", mir.bone1.c_str(), mir.bone2.c_str());
			continue;
		}
		int final_index0 = LOAD_bone_to_FINAL_bone[index0];
		int final_index1 = LOAD_bone_to_FINAL_bone[index1];
		if (final_index0 == -1 || final_index1 == -1) {
			sys_print(Error, "mirrored bone was pruned %s %s\n", mir.bone1.c_str(), mir.bone2.c_str());
			continue;
		}

		out_mirror[final_index0] = final_index1;
		out_mirror[final_index1] = final_index0;
	}

	return out_mirror;
}

static std::vector<BonePoseMask> get_bone_masks(const std::vector<int>& FINAL_to_LOAD_bone,
									 const std::vector<int>& LOAD_to_FINAL_bone,
									 const int FINAL_bones_count, const ModelDefData& def,
									 const SkeletonCompileData* myskel) {
	std::vector<int> num_children_per_bone(FINAL_bones_count, 0);

	for (int i = 0; i < FINAL_bones_count; i++) {
		int count = 1;

		for (int j = i + 1; j < FINAL_bones_count; j++) {

			assert(FINAL_to_LOAD_bone[j] != -1);
			int parent_LOAD = myskel->get_bone_parent(FINAL_to_LOAD_bone[j]);
			int parent_FINAL = (parent_LOAD != -1) ? LOAD_to_FINAL_bone[parent_LOAD] : -1;
			if (parent_FINAL < i)
				break;
			count++;
		}
		num_children_per_bone[i] = count;
	}

	std::vector<BonePoseMask> masks;

	for (int i = 0; i < def.weightlists.size(); i++) {
		BonePoseMask bpm;
		auto& weightlist_def = def.weightlists[i];
		bpm.strname = weightlist_def.name;
		bpm.weight.resize(FINAL_bones_count);
		for (int j = 0; j < weightlist_def.defs.size(); j++) {

			int bone_index_LOAD = myskel->get_bone_for_name(weightlist_def.defs[j].first);
			if (bone_index_LOAD == -1) {
				printf("!!! no bone for mask %s !!!\n", weightlist_def.defs[j].first.c_str());
				continue;
			}
			int bone_index_FINAL = LOAD_to_FINAL_bone[bone_index_LOAD];
			if (bone_index_FINAL == -1) {
				printf("!!! bone mask was pruned %s !!! \n", weightlist_def.defs[j].first.c_str());
				continue;
			}

			int count = num_children_per_bone[bone_index_FINAL];
			for (int k = 0; k < count; k++) {
				bpm.weight[bone_index_FINAL + k] = weightlist_def.defs[j].second;
			}
		}

		masks.push_back(bpm);
	}

	return masks;
}

unique_ptr<FinalSkeletonOutput> ModelCompileHelper::create_final_skeleton(
	std::string outputName, const std::vector<int>& LOAD_bone_to_FINAL_bone,
	const std::vector<int>& FINAL_bone_to_LOAD_bone, const SkeletonCompileData* compile_data,
	const ModelDefData& data) {
	if (!compile_data)
		return nullptr;

	const std::vector<ImportedSkeleton> imports = read_animation_imports(LOAD_bone_to_FINAL_bone, compile_data, data);

	FinalSkeletonOutput* final_out = new FinalSkeletonOutput;
	{
		auto res = get_final_bone_data(FINAL_bone_to_LOAD_bone, LOAD_bone_to_FINAL_bone, compile_data, data);
		final_out->bones = res.first;
		final_out->reparents = res.second;
	}
	final_out->armature_root_transform = compile_data->armature_root;

	for (int i = 0; i < imports.size(); i++) {

		auto& imp = imports[i];
		for (int j = 0; j < imp.skeleton->setself->clips.size(); j++) {
			AnimationSourceToCompile astc;
			astc.animation_souce_index = j;
			astc.remap = &imp.remap_from_LOAD_to_THIS;
			astc.skel = imp.skeleton.get();
			astc.should_retarget_this = imp.retarget_this;

			append_animation_seq_to_list(astc, final_out, FINAL_bone_to_LOAD_bone, LOAD_bone_to_FINAL_bone,
										 compile_data, data);
		}
	}

	for (int j = 0; j < compile_data->setself->clips.size(); j++) {
		AnimationSourceToCompile astc;
		astc.animation_souce_index = j;
		astc.remap = nullptr;
		astc.skel = compile_data;

		append_animation_seq_to_list(astc, final_out, FINAL_bone_to_LOAD_bone, LOAD_bone_to_FINAL_bone, compile_data,
									 data);
	}

	for (const auto& clip : data.str_to_clip_def) {

		AnimationSeq* a = final_out->find_sequence(clip.first);
		if (!a) {
			sys_print(Warning, "clip defintion was not applied to any animations %s\n", clip.first.c_str());
			continue;
		}

		if (clip.second.sub != SubtractType_Load::None) {
			AnimationSeq* other = a;
			if (clip.second.sub == SubtractType_Load::FromAnother) {
				other = final_out->find_sequence(clip.second.subtract_clipname);
				if (!other) {
					sys_print(Error, "subtract clip not found (%s from %s)\n", clip.first.c_str(),
							  clip.second.subtract_clipname.c_str());
				}
			}
			if (other) {
				subtract_clips(FINAL_bone_to_LOAD_bone.size(), a, other);
				a->is_additive_clip = true;
			}
		}
	}

	final_out->mirror_table =
		get_mirror_table(compile_data, LOAD_bone_to_FINAL_bone, FINAL_bone_to_LOAD_bone.size(), data);
	final_out->imported_models = get_imported_models(data);
	final_out->masks = get_bone_masks(FINAL_bone_to_LOAD_bone, LOAD_bone_to_FINAL_bone, FINAL_bone_to_LOAD_bone.size(),
									  data, compile_data);

	{
		std::string outname = FileSys::get_full_path_from_game_path(strip_extension(outputName) + ".anims");
		sys_print(Debug, "writing .anims file: %s\n", outname.c_str());
		std::ofstream outfile(outname);
		for (auto& o : final_out->allseqs) {
			outfile << o.first << "\n";
		}
		outfile.close();
	}

	return unique_ptr<FinalSkeletonOutput>(final_out);
}

#undef IS_HIGH_BIT_SET
#undef CLEAR_HIGH_BIT

#endif
