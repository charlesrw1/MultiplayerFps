#pragma once
#ifdef EDITOR_BUILD
#include "Framework/Util.h"
#include "Animation/SkeletonData.h"
#include "Physics/Physics2.h"
#include <string>
#include <unordered_map>
#include <vector>
#include "Framework/CurveEditorImgui.h"

enum class AnimImportType_Load {
	File,
	Folder,
	Model,
};
struct AnimImportedSet_Load
{
	AnimImportType_Load type = AnimImportType_Load::File;
	std::string name;
	std::string armature_name;
	bool include_mirror = true;
	bool include_weightlist = true;
	bool retarget = false;
};
enum class SubtractType_Load {
	None,
	FromThis,
	FromAnother,
};

struct ClipCrop
{
	float start = 0.0;
	float end = 10000.0;
	bool has_crop = false;
};



struct AnimationClip_Load
{
	SubtractType_Load sub = SubtractType_Load::None;
	std::string subtract_clipname;
	float fps = 30.0;
	ClipCrop crop;

	bool removeLienarVelocity = false;
	bool fixloop = false;
	bool enableRootMotion = false;
	bool setRootToFirstFrame = false;


	// if non empty, then set origin of clip to this
	std::string make_relative_to_locator;
};

struct LODDef
{
	int lod_num = 0;
	float distance = 1.0;
	bool use_skeleton_lod = false;
};

struct SkeletonLODDef
{
	bool was_defined = false;
	std::vector<std::string> collapsed_bones;
};

struct PhysicsCollisionShapeDefLoad
{
	std::string node_name_target;
	bool is_mesh = false;		// exports tri mesh

	std::string material_name;
	physics_shape_def def;
};

struct WeightlistDef
{
	std::string name;
	std::vector<std::pair<std::string, float>> defs;
};

// interchange format between editor and compilier essentially, serialized to disk
class ModelDefData
{
public:
	bool isLightmapped = false;
	int lightmapSizeX = 0;
	int lightmapSizeY = 0;
	bool worldLmMerge = false;

	bool use_mesh_as_collision = false;

	std::string model_source;
	uint64_t timestamp_of_def = 0;

	// MATERIALS
	std::vector<std::string> directMaterialSet;	// use final index

	// LODS
	std::vector<LODDef> loddefs;
	const int num_lods() const { return loddefs.size(); }

	// SKELETON
	bool merge_meshes_into_skeleton = false;
	SkeletonLODDef skellod;
	std::string armature_name;
	std::unordered_map<std::string, std::string> bone_reparent;
	std::unordered_map<std::string, std::string> bone_rename;
	std::vector<std::string> keepbones;
	std::unordered_map<std::string, RetargetBoneType> bone_retarget_type;
	struct mirror {
		std::string bone1;
		std::string bone2;
	};
	std::vector<mirror> mirrored_bones;
	std::vector< WeightlistDef> weightlists;
	float override_fps = 30.0;

	// PHYSICS
	std::vector<PhysicsCollisionShapeDefLoad> physicsshapes;

	// ANIMATION
	std::unordered_map<std::string, AnimationClip_Load> str_to_clip_def;
	std::vector<AnimImportedSet_Load> imports;

	const AnimationClip_Load* find(const std::string& animname) const {
		if (str_to_clip_def.find(animname) != str_to_clip_def.end())
			return &str_to_clip_def.find(animname)->second;
		return nullptr;
	}
};
#endif