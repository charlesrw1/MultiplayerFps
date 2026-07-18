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

// Physx needed for cooking meshes:
#include <physx/cooking/PxCooking.h>

#include <fstream>

#include <meshoptimizer.h>

glm::mat4 compute_world_space(glm::mat4 localtransform, int bone, MSkeleton* skel) {
	while (bone != -1) {
		int parent = skel->get_bone_parent(bone);
		localtransform = skel->get_bone_local_transform(parent) * localtransform;
		bone = parent;
	}
	return localtransform;
}

extern std::string turn_gamepath_into_src_path(const std::string& gamepath, const std::string& src_file);

#include "ModelAsset2.h"
ModelDefData new_import_settings_to_modeldef_data(const std::string& gamepath, ModelImportSettings* is) {
	ModelDefData mdd;
	mdd.use_mesh_as_collision = is->meshAsCollision;
	mdd.use_mesh_as_cvx_collision = is->meshAsConvex;
	mdd.export_embedded_textures = is->exportEmbeddedTextures;
	mdd.isLightmapped = is->withLightmap;
	mdd.lightmapSizeX = is->lightmapSizeX;
	mdd.lightmapSizeY = is->lightmapSizeY;
	mdd.worldLmMerge = is->worldLmMerge;
	if (mdd.worldLmMerge)
		mdd.isLightmapped = true;
	mdd.generate_auto_lods = is->generate_auto_lods;
	mdd.prune_disconnected_islands_min_lod = is->prune_disconnected_islands_min_lod;
	mdd.cullDistance = is->cullDistance;
	if (mdd.generate_auto_lods && is->lodScreenSpaceSizes.empty()) {
		is->lodScreenSpaceSizes = {0.1, 0.01, 0.005};
	}

	mdd.model_source = turn_gamepath_into_src_path(gamepath, is->srcGlbFile);
	LODDef lodd;
	mdd.loddefs.push_back(lodd);
	for (int i = 0; i < is->lodScreenSpaceSizes.size(); i++) {
		LODDef lodd;
		lodd.lod_num = i + 1;
		lodd.distance = is->lodScreenSpaceSizes[i];
		mdd.loddefs.push_back(lodd);
	}
	for (int i = 0; i < is->myMaterials.size(); i++) {
		auto mat = is->myMaterials.at(i).get_unsafe();

		mdd.directMaterialSet.push_back((mat) ? mat->get_name() : "eng/fallback.mm");
	}
	mdd.keepbones = is->keepBones;
	mdd.disable_prune_bones = is->disablePruneUnusedBones;
	for (int i = 0; i < is->additionalAnimationGlbFiles.size(); i++) {
		auto& p = is->additionalAnimationGlbFiles[i];
		if (p.rfind('/') != std::string::npos) {
			AnimImportedSet_Load imp;
			imp.name = p;
			imp.type = AnimImportType_Load::Folder;
			imp.retarget = true;
			mdd.imports.push_back(imp);
		} else {
			AnimImportedSet_Load imp;
			imp.name = p;
			imp.type = AnimImportType_Load::File;
			imp.retarget = true;
			mdd.imports.push_back(imp);
		}
	}
	mdd.override_fps = is->animations_set_fps;

	auto& boneRemap = is->bone_rename;
	for (int i = 0; i < (int)boneRemap.remap.size() - 1; i += 2) {
		const auto& str1 = boneRemap.remap.at(i);
		const auto& str2 = boneRemap.remap.at(size_t(i + 1));
		mdd.bone_rename.insert({str1, str2});
	}

	auto& reparent = is->bone_reparent;
	for (int i = 0; i < (int)reparent.remap.size() - 1; i += 2) {
		const auto& str1 = reparent.remap.at(i);
		const auto& str2 = reparent.remap.at(size_t(i + 1));
		mdd.bone_reparent.insert({str1, str2});
	}
	for (auto& [name, type] : is->bone_retargets) {
		mdd.bone_retarget_type[name] = (RetargetBoneType)glm::clamp(type, 0, 2);
	}

	for (int i = 0; i < is->animations.size(); i++) {
		string& str = is->animations[i].clipName;
		auto& isa = is->animations[i];
		AnimationClip_Load acl;
		acl.fixloop = isa.fixLoop;
		acl.setRootToFirstFrame = isa.setRootToFirstPose;
		acl.enableRootMotion = isa.enableRootMotion;
		acl.removeLienarVelocity = isa.removeLinearVelocity;

		if (acl.enableRootMotion && (acl.setRootToFirstFrame || acl.removeLienarVelocity)) {
			sys_print(Warning,
					  "enableRootMotion with setRootToFirstFrame or removeLinearVelocity, will get wrong result\n");
		}

		acl.crop.has_crop = isa.hasEndCrop || isa.hasStartCrop;
		acl.crop.start = isa.cropStart;
		acl.crop.end = isa.cropEnd;
		acl.lengthScale = isa.lengthScale;
		if (!isa.otherClipToSubtract.empty())
			isa.makeAdditive = true;

		if (isa.makeAdditive) {
			if (!isa.otherClipToSubtract.empty()) {
				acl.sub = SubtractType_Load::FromAnother;
				auto find = isa.otherClipToSubtract.rfind("/");
				auto ofs = find == std::string::npos ? 0 : find + 1;
				acl.subtract_clipname = isa.otherClipToSubtract.substr(ofs);
			} else {
				acl.sub = SubtractType_Load::FromThis;
				acl.subtract_frame = isa.additiveSelfFrame; // frame to diff against (additive-from-self)
			}
		} else
			acl.sub = SubtractType_Load::None;

		mdd.str_to_clip_def.insert({str, std::move(acl)});
	}

	return mdd;
}
#include "Framework/SerializerJson.h"
#include "Framework/StringUtils.h"
extern void write_model_import_settings(ModelImportSettings* mis, const std::string& savepath);

ModelDefData new_import_settings_to_modeldef_data(const string& mis_path, IFile* file) {
	std::unique_ptr<ModelImportSettings> impSettings = nullptr;

	std::string to_str(file->size(), ' ');
	file->read(to_str.data(), file->size());
	if (to_str.find("!json") == 0) {
		to_str = to_str.substr(6);
		MakeObjectFromPathGeneric objmaker;
		ReadSerializerBackendJson reader("read_modeldef", to_str, objmaker);
		if (reader.get_root_obj()) {
			impSettings.reset(reader.get_root_obj()->cast_to<ModelImportSettings>());
		}
		if (!impSettings) {
			throw std::runtime_error("couldn't open dict");
		}
	} else {
		sys_print(Warning, "new_import_settings_to_modeldef_data: loading old version %s\n", mis_path.c_str());
		if (!impSettings) {
			throw std::runtime_error("couldn't open dict");
		}
	}
	ModelDefData mdd = new_import_settings_to_modeldef_data(mis_path, impSettings.get());
	return mdd;
}

ModelDefData ModelCompileHelper::parse_definition_file(const std::string& game_path) {
	{
		std::string pathNew = strip_extension(game_path);
		pathNew += ".mis"; // model import settings
		auto filenew = FileSys::open_read_game(pathNew.c_str());
		if (!filenew)
			throw std::runtime_error("couldn't open dict");

		return new_import_settings_to_modeldef_data(pathNew, filenew.get());
	}
}

static bool compile_everything = false;

bool ModelCompilier::does_model_need_compile(const char* game_path, ModelDefData& def_data, bool needs_def) {
	auto file = FileSys::open_read_game(game_path);
	if (!file) {
		sys_print(Error, "coudln't open model def file %s\n", game_path);
		return false;
	}
	uint64_t timestamp_of_def = file->get_timestamp();
	file.reset();

	uint64_t timestamp_of_cmdl = 0;

	std::string compilied = strip_extension(game_path) + ".cmdl";
	file = FileSys::open_read_game(compilied.c_str());

	bool needs_compile = false;
	if (file) {
		timestamp_of_cmdl = file->get_timestamp();
		if (file->size() <= 8)
			needs_compile = true;
		else {
			uint32_t magic = 0;
			uint32_t version = 0;
			file->read(&magic, 4);
			file->read(&version, 4);
			if (magic != 'CMDL') {
				needs_compile = true;
			} else if (version != MODEL_VERSION) {
				sys_print(Info, ".cmdl version out of data (found %d, current %d), recompiling\n", version,
						  MODEL_VERSION);
				needs_compile = true;
			}
		}
	}
	file.reset();

	if (!needs_compile && timestamp_of_cmdl <= timestamp_of_def) {
		sys_print(Info, "*** .def newer than .cmdl, recompiling\n");
		needs_compile = true;
	}

	if (needs_compile && !needs_def)
		return true;

	def_data = ModelCompileHelper::parse_definition_file(game_path);

	auto check_timestamp_file = [](uint64_t cmdl_time, const std::string& full_path) -> bool {
		auto input_file = FileSys::open_read_game(full_path.c_str());
		if (!input_file)
			return false;

		uint64_t time = input_file->get_timestamp();
		return time >= cmdl_time;
	};
	auto check_timestamp_folder = [&](uint64_t cmdl_time, const std::string& folder) -> bool {
		auto tree = FileSys::find_game_files_path(folder);

		for (const auto& file : tree) {
			if (get_extension(file) != ".glb")
				continue;
			bool res = check_timestamp_file(cmdl_time, FileSys::get_game_path_from_full_path(file));
			if (res)
				return true;
		}
		return false;
	};

	bool needs_compile_before_src = needs_compile;

	if (!needs_compile)
		needs_compile |= check_timestamp_file(timestamp_of_cmdl, def_data.model_source);
	for (int i = 0; i < def_data.imports.size() && !needs_compile; i++) {
		if (def_data.imports[i].type == AnimImportType_Load::File)
			needs_compile |= check_timestamp_file(timestamp_of_cmdl, def_data.imports[i].name);
		else if (def_data.imports[i].type == AnimImportType_Load::Folder)
			needs_compile |= check_timestamp_folder(timestamp_of_cmdl, def_data.imports[i].name);
	}
	if (!needs_compile_before_src && needs_compile)
		sys_print(Info, "source files out of data, recompiling\n");

	return needs_compile;
}

ModelCompilier::Ret ModelCompilier::compile_from_settings(const std::string& output, ModelImportSettings* settings) {
	ModelDefData def_data = new_import_settings_to_modeldef_data(output, settings);
	return ModelCompileHelper::compile_model(output, def_data);
}

ModelCompilier::Ret ModelCompilier::compile(const char* game_path) {

	ModelDefData def;
	try {
		bool needs_compile = does_model_need_compile(game_path, def, true);
		if (!needs_compile)
			return Ret::Skipped;
	}
	catch (std::runtime_error er) {
		sys_print(Error, "ModelCompilier::compile: when model compiling %s: %s\n", game_path, er.what());
		return Ret::CompileErr;
	}

	return ModelCompileHelper::compile_model(game_path, def);
}

#endif
