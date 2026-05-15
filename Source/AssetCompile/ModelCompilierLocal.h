#pragma once
#ifdef EDITOR_BUILD
#include "Framework/Util.h"
#include "Animation/SkeletonData.h"
#include "Physics/Physics2.h"
#include <string>
#include <unordered_map>
#include <vector>
#include "Framework/CurveEditorImgui.h"
#include "cgltf.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Render/Model.h"
#include "Animation/AnimationUtil.h"
#include <memory>

enum class AnimImportType_Load
{
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
enum class SubtractType_Load
{
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
	bool is_mesh = false; // exports tri mesh

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
	bool use_mesh_as_cvx_collision = false;
	bool use_mesh_as_collision = false;

	std::string model_source;
	uint64_t timestamp_of_def = 0;

	// MATERIALS
	std::vector<std::string> directMaterialSet; // use final index

	// LODS
	std::vector<LODDef> loddefs;
	const int num_lods() const { return loddefs.size(); }
	bool generate_auto_lods = false;

	// SKELETON
	bool merge_meshes_into_skeleton = false;
	SkeletonLODDef skellod;
	std::string armature_name;
	std::unordered_map<std::string, std::string> bone_reparent;
	std::unordered_map<std::string, std::string> bone_rename;
	std::vector<std::string> keepbones;
	std::unordered_map<std::string, RetargetBoneType> bone_retarget_type;
	struct mirror
	{
		std::string bone1;
		std::string bone2;
	};
	std::vector<mirror> mirrored_bones;
	std::vector<WeightlistDef> weightlists;
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

// ---- Compiler-internal types shared across split translation units ----

struct NodeRef
{
	int index = 0;
	glm::mat4 globaltransform = glm::mat4(1.f);
};

enum CompilierModelAttributes
{
	CMA_POSITION,
	CMA_UV,
	CMA_NORMAL,
	CMA_TANGENT,
	CMA_BONEWEIGHT,
	CMA_BONEINDEX,
	CMA_COLOR,
	CMA_COLOR2,
	CMA_UV2
};

struct FATVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec4 tangent; // xyz=tangent, w = handedness

	glm::vec4 bone_weight;
	glm::ivec4 bone_index;

	glm::vec4 color;
	glm::vec4 color2;
	glm::vec2 uv;
	glm::vec2 uv2;
};

struct LODMesh
{
	NodeRef ref;
	Submesh submesh;
	Bounds bounds;

	int attribute_mask = 0;

	bool mark_for_delete = false;

	bool has_bones() const {
		return (attribute_mask & (1 << CMA_BONEINDEX)) && (attribute_mask & (1 << CMA_BONEWEIGHT));
	}

	bool has_tangents() const { return (attribute_mask & (1 << CMA_TANGENT)); }

	bool has_normals() const { return (attribute_mask & (1 << CMA_NORMAL)); }
	bool has_attribute(CompilierModelAttributes at) const { return (attribute_mask & (1 << at)); }

	ShapeType_e shape_type = ShapeType_e::None;
};

struct CompileModLOD
{
	std::vector<LODMesh> mesh_nodes;
	bool share_verticies_with_lod0 = false;
};

struct SkeletonCompileData
{
	cgltf_skin* using_skin = nullptr;

	int get_num_bones() const { return using_skin->joints_count; }

	void init_skin(cgltf_data* data, cgltf_skin* skin) { using_skin = skin; }

	int get_bone_parent(int bone) const { return bones.at(bone).parent; }

	int get_bone_for_name(std::string name) const {
		for (int i = 0; i < (int)bones.size(); i++)
			if (bones[i].strname == name)
				return i;
		return -1;
	}

	glm::vec3 get_local_position(int bone) const { return bones[bone].localtransform[3]; }
	glm::quat get_local_rotation(int bone) const { return bones[bone].rot; }
	float get_local_scale(int bone) const { return 1.0; }

	glm::mat4 armature_root = glm::mat4(1.f);

	std::vector<int> original_bone_index_to_output;

	std::vector<BoneData> bones;

	std::unique_ptr<Animation_Set> setself;
};

struct ModelCompileData
{
	const cgltf_data* gltf_file = nullptr;

	std::vector<CompileModLOD> lod_where;
	std::vector<LODMesh> physics_nodes;
	std::vector<FATVertex> verticies;
	std::vector<uint32_t> indicies;
};

struct ProcessMeshOutput
{
	std::vector<bool> material_is_used;
	std::vector<int> LOAD_bone_to_FINAL_bone;
	std::vector<int> FINAL_bone_to_LOAD_bone;
	int get_final_count() const { return (int)FINAL_bone_to_LOAD_bone.size(); }
};

struct AnimationSourceToCompile
{
	const std::vector<int>* remap = nullptr;
	const SkeletonCompileData* skel = nullptr;
	int animation_souce_index = 0;
	bool should_retarget_this = false;

	const Animation_Set* get_set() const { return skel->setself.get(); }

	const Animation* get_animation() const { return &skel->setself->clips.at(animation_souce_index); }
	const std::string& get_animation_name() const { return get_animation()->name; }
	bool is_self() const { return remap == nullptr; }
};

struct FinalSkeletonOutput
{
	std::unordered_map<std::string, AnimationSeq> allseqs;
	std::vector<BoneData> bones;
	std::vector<int16_t> mirror_table;
	std::vector<BonePoseMask> masks;
	glm::mat4 armature_root_transform;
	std::vector<std::string> imported_models;

	struct ReparentData
	{
		int FINAL_index = 0;
	};
	std::vector<ReparentData> reparents;

	bool does_sequence_already_exist(const std::string& name) const { return find_sequence(name) != nullptr; }

	void add_sequence(const std::string& name, AnimationSeq&& seq) { allseqs.insert({name, std::move(seq)}); }

	const AnimationSeq* find_sequence(const std::string& name) const {
		if (allseqs.find(name) == allseqs.end())
			return nullptr;
		return &allseqs.find(name)->second;
	}

	AnimationSeq* find_sequence(const std::string& name) {
		if (allseqs.find(name) == allseqs.end())
			return nullptr;
		return &allseqs.find(name)->second;
	}
};

struct ImportedSkeleton
{
	std::unique_ptr<SkeletonCompileData> skeleton = nullptr;
	std::vector<int> remap_from_LOAD_to_THIS;
	bool retarget_this = false;
};

#include "physx/extensions/PxDefaultStreams.h"
struct FinalPhysicsData
{
	std::vector<physics_shape_def> shapes;
	std::vector<std::unique_ptr<physx::PxDefaultMemoryOutputStream>> output_streams;
};

struct FinalModelData
{
	FinalPhysicsData final_physics;
	std::vector<ModelVertex> verticies;
	std::vector<uint16_t> indicies;
	std::vector<MeshLod> lods;
	std::vector<Submesh> submeshes;
	std::vector<std::string> material_names;
	Bounds AABB;
	std::vector<ModelTag> tags;

	bool get_is_lightmapped_bool() const { return isLightmapped != Model::LightmapType::None; }
	Model::LightmapType isLightmapped = Model::LightmapType::None;
	int lightmapX = 0;
	int lightmapY = 0;
};

struct ProcessNodesAndMeshOutput
{
	ModelCompileData mcd;
	ProcessMeshOutput meshout;
};

constexpr int MODEL_VERSION = 18;

struct cgltf_and_binary
{
	uint8_t* bin_file = nullptr;
	size_t bin_len = 0;
	cgltf_data* data = nullptr;

	void free() {
		delete[] bin_file;
		cgltf_free(data);
	}
};

// Forward declarations for functions used across split files
glm::mat4 get_node_transform(cgltf_node* node);
cgltf_and_binary load_cgltf_data(const std::string& path);
unique_ptr<SkeletonCompileData> get_skin_from_file(cgltf_data* dat, const char* name, const std::string armature);
std::vector<ImportedSkeleton> read_animation_imports(const std::vector<int>& LOAD_bone_to_FINAL_bone,
													 const SkeletonCompileData* compile_data,
													 const ModelDefData& data);
ProcessNodesAndMeshOutput process_nodes_and_mesh(cgltf_data* data, const SkeletonCompileData* scd,
												 const cgltf_skin* using_skin, const ModelDefData& def);

// ModelCompileHelper: static methods implemented across split files
#include "Compiliers.h"
class ModelCompileHelper
{
public:
	static ModelDefData parse_definition_file(const std::string& deffile);

	static ModelCompilier::Ret compile_model(const std::string& defname, const ModelDefData& data);

	static void load_gltf_skeleton(cgltf_data* data, glm::mat4& armature_root, std::vector<BoneData>& bones,
								   cgltf_skin* skin);

	static void addskeleton_R(std::unordered_map<std::string, int>& bone_to_index, cgltf_data* data,
							  std::vector<BoneData>& bones, cgltf_node* node);

	static ProcessMeshOutput process_mesh(ModelCompileData& comp, const SkeletonCompileData* scd,
										  const ModelDefData& data);

	static unique_ptr<FinalSkeletonOutput> create_final_skeleton(std::string outputName,
																 const std::vector<int>& LOAD_bone_to_FINAL_bone,
																 const std::vector<int>& FINAL_bone_to_LOAD_bone,
																 const SkeletonCompileData* compile_data,
																 const ModelDefData& data);

	static void subtract_clips(const int num_bones, AnimationSeq* target, const AnimationSeq* source);
	static std::vector<std::string> create_final_material_names(const std::string& modelname,
																const ModelCompileData& comp, const ModelDefData& data,
																const std::vector<bool>& mats_refed);
	static void append_animation_seq_to_list(AnimationSourceToCompile source, FinalSkeletonOutput* final_,
											 const std::vector<int>& FINAL_bone_to_LOAD_bone,
											 const std::vector<int>& LOAD_bone_to_FINAL_bone,
											 const SkeletonCompileData* myskel, const ModelDefData& data);
};

#endif