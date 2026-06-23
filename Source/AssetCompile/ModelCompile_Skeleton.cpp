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

// get_node_transform: also used by ModelCompile_Mesh.cpp
glm::mat4 get_node_transform(cgltf_node* node) {
	glm::mat4 local_transform = glm::mat4(1);
	if (node->has_matrix) {
		float* m = node->matrix;
		local_transform = glm::mat4(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12],
									m[13], m[14], m[15]);
	} else {
		glm::vec3 translation = glm::vec3(0.f);
		glm::vec3 scale = glm::vec3(1.f);
		glm::quat rot = glm::quat(1.f, 0.f, 0.f, 0.f);
		if (node->has_translation)
			translation = glm::make_vec3<float>(node->translation);
		if (node->has_rotation)
			rot = glm::make_quat<float>(node->rotation);
		if (node->has_scale)
			scale = glm::make_vec3<float>(node->scale);
		local_transform = glm::translate(glm::mat4(1), translation);
		local_transform = local_transform * glm::mat4_cast(rot);
		local_transform = glm::scale(local_transform, scale);
	}
	return local_transform;
}

void ModelCompileHelper::addskeleton_R(std::unordered_map<std::string, int>& bone_to_index, cgltf_data* data,
									   std::vector<BoneData>& bones, cgltf_node* node) {
	std::string name = node->name;
	if (bone_to_index.find(name) != bone_to_index.end()) {
		int my_index = bone_to_index[name];
		for (int i = 0; i < node->children_count; i++) {
			cgltf_node* child = node->children[i];
			std::string cname = child->name;
			if (bone_to_index.find(cname) != bone_to_index.end()) {
				bones[bone_to_index[cname]].parent = my_index;
			}
		}
	}
	for (int i = 0; i < node->children_count; i++) {
		addskeleton_R(bone_to_index, data, bones, node->children[i]);
	}
}

void ModelCompileHelper::load_gltf_skeleton(cgltf_data* data, glm::mat4& armature_root, std::vector<BoneData>& bones,
											cgltf_skin* skin) {
	std::unordered_map<std::string, int> bone_to_index;
	ASSERT(skin->inverse_bind_matrices != nullptr);
	cgltf_accessor* invbind_acc = skin->inverse_bind_matrices;
	cgltf_buffer_view* invbind_bv = invbind_acc->buffer_view;

	uint8_t* byte_buffer = (uint8_t*)invbind_bv->buffer->data;
	for (int i = 0; i < skin->joints_count; i++) {
		cgltf_node* node = skin->joints[i];

		BoneData b;

		b.parent = -1;
		float* start = (float*)(&byte_buffer[invbind_bv->offset] + sizeof(float) * 16 * i);
		b.invposematrix =
			glm::mat4(start[0], start[1], start[2], start[3], start[4], start[5], start[6], start[7], start[8],
					  start[9], start[10], start[11], start[12], start[13], start[14], start[15]);
		b.posematrix = glm::inverse(glm::mat4(b.invposematrix));
		b.localtransform = get_node_transform(node);
		b.name = node->name;
		b.strname = node->name;

		b.rot = glm::quat_cast(glm::mat4(b.localtransform));
		bone_to_index.insert({std::string(node->name), (int)bones.size()});
		bones.push_back(b);
	}
	cgltf_scene* defscene = data->scene;
	for (int i = 0; i < defscene->nodes_count; i++) {
		addskeleton_R(bone_to_index, data, bones, defscene->nodes[i]);
	}

	armature_root = glm::mat4(1);

	if (bones[0].parent != -1) {
		sys_print(Error, "root bone not first bone\n");
		std::abort();
	}
	cgltf_node* node = skin->joints[0];
	if (node->parent) {
		armature_root = get_node_transform(node->parent);
	}
}

bool ModelCompileHelper::apply_armature_root_to_skeleton(SkeletonCompileData* scd) {
	glm::mat4& armature_root = scd->armature_root;

	if (armature_root == glm::mat4(1.0f))
		return true;

	glm::vec3 arm_scale(glm::length(glm::vec3(armature_root[0])), glm::length(glm::vec3(armature_root[1])),
						glm::length(glm::vec3(armature_root[2])));

	constexpr float scale_epsilon = 0.001f;
	if (glm::abs(arm_scale.x - arm_scale.y) > scale_epsilon ||
		glm::abs(arm_scale.y - arm_scale.z) > scale_epsilon) {
		sys_print(Error,
				  "armature root has non-uniform scale (%.4f, %.4f, %.4f), skipping armature transform bake\n",
				  arm_scale.x, arm_scale.y, arm_scale.z);
		return false;
	}

	glm::mat3 arm_rot(glm::vec3(armature_root[0]) / arm_scale.x, glm::vec3(armature_root[1]) / arm_scale.y,
					   glm::vec3(armature_root[2]) / arm_scale.z);
	glm::quat arm_quat = glm::quat_cast(arm_rot);

	// glTF exporters bake armature_root's inverse into invBindMatrices, so posematrix
	// has an embedded scale (e.g. 0.01) that the raw node local transforms don't have.
	// Strip armature_root from bind poses so they match the local transforms' space.
	// This keeps everything in the raw glTF space (e.g. cm) — no scale mismatch in skinning.
	glm::mat4 inv_armature = glm::inverse(armature_root);
	for (auto& bone : scd->bones) {
		glm::mat4 pose4 = glm::mat4(bone.posematrix);
		pose4[3][3] = 1.0f;
		pose4 = inv_armature * pose4;
		bone.posematrix = glm::mat4x3(pose4);
		bone.invposematrix = glm::mat4x3(glm::inverse(pose4));
	}

	sys_print(Info, "applied armature root transform (uniform scale=%.4f)\n", arm_scale.x);
	armature_root = glm::mat4(1.0);
	return true;
}

static std::unordered_map<int, int> fill_out_node_to_index(cgltf_data* data, cgltf_skin* skin) {
	std::unordered_map<int, int> node_to_index;

	for (int i = 0; i < skin->joints_count; i++) {
		node_to_index[cgltf_node_index(data, skin->joints[i])] = i;
	}
	return node_to_index;
}

static Animation_Set* load_animation_set_for_gltf_skin(cgltf_data* data, cgltf_skin* skin) {
	Animation_Set* set = new Animation_Set;
	std::unordered_map<int, int> node_to_index = fill_out_node_to_index(data, skin);
	set->num_channels = skin->joints_count;

	for (int a = 0; a < data->animations_count; a++) {
		cgltf_animation* gltf_anim = &data->animations[a];

		Animation my_anim{};
		my_anim.name = gltf_anim->name;
		my_anim.channel_offset = set->channels.size();
		my_anim.pos_offset = set->positions.size();
		my_anim.rot_offset = set->rotations.size();
		my_anim.scale_offset = set->scales.size();
		set->channels.resize(set->channels.size() + skin->joints_count);
		for (int c = 0; c < gltf_anim->channels_count; c++) {
			cgltf_animation_channel* gltf_channel = &gltf_anim->channels[c];

			int node_of_target = cgltf_node_index(data, gltf_channel->target_node);

			if (node_to_index.find(node_of_target) == node_to_index.end())
				continue;

			int channel_idx = node_to_index[node_of_target];
			AnimChannel& my_channel = set->channels[my_anim.channel_offset + channel_idx];

			int type = -1;
			if (gltf_channel->target_path == cgltf_animation_path_type_translation)
				type = 0;
			else if (gltf_channel->target_path == cgltf_animation_path_type_rotation)
				type = 1;
			else if (gltf_channel->target_path == cgltf_animation_path_type_scale)
				type = 2;
			else
				continue;

			cgltf_animation_sampler* sampler = gltf_channel->sampler;
			cgltf_accessor* timevals = sampler->input;
			cgltf_accessor* vals = sampler->output;
			ASSERT(timevals->count == vals->count);
			cgltf_buffer_view* time_bv = timevals->buffer_view;
			cgltf_buffer_view* val_bv = vals->buffer_view;
			ASSERT(time_bv->buffer == val_bv->buffer);
			cgltf_buffer* buffer = time_bv->buffer;
			ASSERT(timevals->component_type == cgltf_component_type_r_32f);
			ASSERT(time_bv->stride == 0);

			my_anim.total_duration = glm::max(my_anim.total_duration, (float)timevals->max[0]);

			char* buffer_byte_data = (char*)buffer->data;

			float* time_buffer = (float*)&buffer_byte_data[time_bv->offset];
			if (type == 0) {
				my_channel.pos_start = set->positions.size();
				ASSERT(vals->component_type == cgltf_component_type_r_32f && vals->type == cgltf_type_vec3);
				ASSERT(val_bv->stride == 0);

				glm::vec3* pos_buffer = (glm::vec3*)&buffer_byte_data[val_bv->offset];

				for (int t = 0; t < timevals->count; t++) {
					PosKeyframe pkf;
					pkf.time = time_buffer[t];
					pkf.val = pos_buffer[t];
					set->positions.push_back(pkf);
				}
				my_channel.num_positions = set->positions.size() - my_channel.pos_start;

			} else if (type == 1) {
				my_channel.rot_start = set->rotations.size();
				ASSERT(vals->component_type == cgltf_component_type_r_32f && vals->type == cgltf_type_vec4);
				ASSERT(val_bv->stride == 0);
				glm::quat* rot_buffer = (glm::quat*)&buffer_byte_data[val_bv->offset];
				for (int t = 0; t < timevals->count; t++) {
					RotKeyframe rkf;
					rkf.time = time_buffer[t];
					rkf.val = rot_buffer[t];
					set->rotations.push_back(rkf);
				}
				my_channel.num_rotations = set->rotations.size() - my_channel.rot_start;
			} else if (type == 2) {
				my_channel.scale_start = set->scales.size();
				ASSERT(vals->component_type == cgltf_component_type_r_32f && vals->type == cgltf_type_vec3);
				ASSERT(val_bv->stride == 0);
				glm::vec3* scale_buffer = (glm::vec3*)&buffer_byte_data[val_bv->offset];
				for (int t = 0; t < timevals->count; t++) {
					ScaleKeyframe skf;
					skf.time = time_buffer[t];
					skf.val = scale_buffer[t];
					set->scales.push_back(skf);
				}
				my_channel.num_scales = set->scales.size() - my_channel.scale_start;
			}
		}

		my_anim.num_pos = set->positions.size() - my_anim.pos_offset;
		my_anim.num_rot = set->rotations.size() - my_anim.rot_offset;
		my_anim.num_scale = set->scales.size() - my_anim.scale_offset;
		my_anim.fps = 1.0;
		set->clips.push_back(my_anim);
	}

	return set;
}

cgltf_and_binary load_cgltf_data(const std::string& path) {
	cgltf_and_binary out;

	cgltf_options options = {};
	cgltf_data* data = NULL;

	auto sourceFile = FileSys::open_read_game(path.c_str());
	if (!sourceFile) {
		sys_print(Error, "couldn't open souce file %s\n", path.c_str());
		return {};
	}
	out.bin_file = new uint8_t[sourceFile->size()];
	out.bin_len = sourceFile->size();
	sourceFile->read(out.bin_file, out.bin_len);
	sourceFile.reset();

	cgltf_result result = cgltf_parse(&options, out.bin_file, out.bin_len, &data);

	if (result != cgltf_result_success) {
		sys_print(Error, "cgltf failed to parse file\n");
		delete[] out.bin_file;
		return {};
	}
	cgltf_load_buffers(&options, data, path.c_str());

	out.data = data;

	return out;
}

unique_ptr<SkeletonCompileData> get_skin_from_file(cgltf_data* dat, const char* name, const std::string armature) {
	cgltf_skin* s = nullptr;
	if (dat->skins_count == 0) {
		return nullptr;
	} else if (dat->skins_count == 1)
		s = &dat->skins[0];
	else {
		if (!armature.empty()) {
			for (int i = 0; i < dat->skins_count; i++) {
				if (armature == dat->skins[i].name) {
					s = &dat->skins[i];
					break;
				}
			}
		}
		if (s == nullptr) {
			sys_print(Warning, "multiple skins in %s, trunacting to first", name);
			s = &dat->skins[0];
		}
	}

	sys_print(Info, "found animation skin\n");

	SkeletonCompileData* scd = new SkeletonCompileData;

	scd->init_skin(dat, s);
	scd->setself.reset(load_animation_set_for_gltf_skin(dat, s));
	ModelCompileHelper::load_gltf_skeleton(dat, scd->armature_root, scd->bones, s);

	return unique_ptr<SkeletonCompileData>(scd);
}

unique_ptr<SkeletonCompileData> open_file_and_read_skeleton(const std::string& path) {
	cgltf_and_binary out = load_cgltf_data(path.c_str());

	if (!out.data)
		return nullptr;

	auto skeleton_data = get_skin_from_file(out.data, path.c_str(), "");
	out.free();

	return std::move(skeleton_data);
}

static std::vector<int> create_remap_table(const SkeletonCompileData* source, const SkeletonCompileData* target) {
	std::vector<int> remap;
	remap.resize(target->get_num_bones(), -1);
	for (int i = 0; i < target->get_num_bones(); i++) {
		int index_in_source = source->get_bone_for_name(target->bones[i].strname);
		if (index_in_source == -1)
			continue;
		remap[i] = index_in_source;
	}

	return remap;
}

std::vector<ImportedSkeleton> read_animation_imports(const std::vector<int>& LOAD_bone_to_FINAL_bone,
													 const SkeletonCompileData* compile_data,
													 const ModelDefData& data) {
	std::vector<ImportedSkeleton> imports;

	for (int i = 0; i < data.imports.size(); i++) {
		if (data.imports[i].type == AnimImportType_Load::Model)
			continue;
		else if (data.imports[i].type == AnimImportType_Load::File) {
			ImportedSkeleton is;
			is.retarget_this = data.imports[i].retarget;

			std::string gamepath = data.imports[i].name;

			is.skeleton = open_file_and_read_skeleton(gamepath);

			if (!is.skeleton) {
				sys_print(Error, "import animation failed %s\n", gamepath.c_str());
			} else {
				is.remap_from_LOAD_to_THIS = create_remap_table(is.skeleton.get(), compile_data);
				imports.push_back(std::move(is));
			}
		} else if (data.imports[i].type == AnimImportType_Load::Folder) {

			std::string game_folder_path = data.imports[i].name;
			std::string full_path_folder = FileSys::get_game_path();
			full_path_folder += "/";
			full_path_folder += game_folder_path;
			if (!full_path_folder.empty() && full_path_folder.back() == '/')
				full_path_folder.pop_back();

			FileTree tree = FileSys::find_files(full_path_folder.c_str());

			for (const auto& file : tree) {

				const std::string& full_path = file;
				if (get_extension(full_path) != ".glb")
					continue;
				ImportedSkeleton is;
				is.retarget_this = data.imports[i].retarget;

				auto pathToUse = FileSys::get_game_path_from_full_path(full_path);

				is.skeleton = open_file_and_read_skeleton(pathToUse);

				if (!is.skeleton) {
					sys_print(Error, "import animation failed %s\n", full_path.c_str());
				} else {
					is.remap_from_LOAD_to_THIS = create_remap_table(is.skeleton.get(), compile_data);
					imports.push_back(std::move(is));
				}
			}
		}
	}
	sys_print(Info, "imported %d files\n", (int)imports.size());
	return imports;
}

#endif
