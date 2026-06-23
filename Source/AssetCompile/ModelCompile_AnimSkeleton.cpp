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

static std::vector<std::string> get_imported_models(const ModelDefData& def) {
	ASSERT(!def.imports.empty() || def.imports.empty()); // def is always valid
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

	ASSERT(myskel != nullptr);
	ASSERT(!FINAL_bone_to_LOAD_bone.empty());

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
	ASSERT(myskel != nullptr);
	ASSERT(FINAL_bones_count >= 0);

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
	ASSERT(myskel != nullptr);
	ASSERT(FINAL_bones_count >= 0);

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
	ASSERT(!outputName.empty());

	if (!compile_data)
		return nullptr;

	const std::vector<ImportedSkeleton> imports = read_animation_imports(LOAD_bone_to_FINAL_bone, compile_data, data);

	FinalSkeletonOutput* final_out = new FinalSkeletonOutput;
	{
		auto res = get_final_bone_data(FINAL_bone_to_LOAD_bone, LOAD_bone_to_FINAL_bone, compile_data, data);
		final_out->bones = res.first;
		final_out->reparents = res.second;
	}
	// After bake, armature_root is identity. Runtime get_root_transform() is currently unused (zero callers).
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

#endif
