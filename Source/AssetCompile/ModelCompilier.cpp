#include "Animation/SkeletonData.h"
#include "Framework/DictParser.h"
#include "Compiliers.h"
#include "Model.h"
#define CGLTF_IMPLEMENTATION
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
// MODEL FORMAT:

static const int MODEL_VERSION = 3;



// HEADER
// int magic 'C' 'M' 'D' 'L'
// int version 1
// 
// mat4 root_transform
// 
// Bounds aabb
// 
// int num_lods
// LOD lods[num_lods]
// 
// int num_meshes
// Submesh meshes[num_meshes]
// 
// int num_materials
// string materials[num_materials]
// 
// int num_locators
// LOCATOR locators[num_locators]
// 
// VBIB vbib
// 
// int num_bones
// mat4 bindpose[num_bones]
// mat4 invbindpose[num_bones]
// string names[num_bones]
// mat4 localtransform[num_bones]
// 
// int num_anims
// ANIMATION anims[num_anims]
// 
// int num_includes
// string included_model_names[num_includes]
// 
// int num_bone_masks 
// uint8_t bone_masks[num_bone_masks][num_bones] -> turned into 0-1 floats
//
// int b_has_mirror_map
// int16_t mirror_map[ (b_has_mirror_map) ? num_bones : 0 ]
//
// int num_masks
// MASK masks[num_mask]

// MASK
// string name
// float mask_data[num_bones]

// MESH
// int base_vertex
// int element_offset
// int element_count
// int material_index

// LOD
// float screen_size
// int num_meshes
// MESH meshes[num_meshes] 

// VERTEX
// vec3 pos
// vec2 uv
// int16 normals
// int16 tangents
// int8[4] color
// int8[4] color2

// VBIB
// int num_verticies
// int num_indicies
// int16 indicies[num_indicies]
// VERTEX verticies[num_verticies]

// LOCATOR
// vec3 position
// quat rotation
// int bone_index
// string name


// EVENT
// int type
// string cmd

// if high bit set, then channel has only one frame
// CHANNEL_OFS
// int pos_ofs
// int rot_ofs
// int scale_ofs

// float* packed_data[ pos_ofs + 3*keyframe ] 

// ANIMATION 
// string name
// float duration
// float linear_velocity
// int num_keyframes
// bool is_delta
// 
// CHANNEL_OFS channels[num_bones]
// 
// int packed_size
// float packed_data[packed_size]
// 
// KEYFRAME_EVENT events[num_keyframes]
// 
// int num_events
// EVENT events[num_events]
// 



struct AnimEvent_Load
{
	float time = 0.0;
	std::string type;
	std::string arg;
};
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
struct ClipStart
{
	float new_start = 0.0;
	bool has_start = false;
};

struct AnimationClip_Load
{
	SubtractType_Load sub = SubtractType_Load::None;
	std::string subtract_clipname;
	float fps = 30.0;
	ClipCrop crop;
	ClipStart start;
	bool fixloop = false;
	std::vector<AnimEvent_Load> events;

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
	PhysicsBodyConstraintDef constraint_def;
};

struct WeightlistDef
{
	std::string name;
	std::vector<std::pair<std::string, float>> defs;
};



struct ModelDefData
{
	std::string model_source;
	uint64_t timestamp_of_def = 0;

	// MATERIALS
	std::string root_material_dir;
	std::unordered_map<std::string, std::string> material_rename;

	// LODS
	std::vector<LODDef> loddefs;
	const int num_lods() const { return loddefs.size(); }

	// SKELETON
	bool merge_meshes_into_skeleton = false;
	SkeletonLODDef skellod;
	std::string armature_name;
	std::unordered_map<std::string, std::string> bone_rename;
	std::vector<std::string> keepbones;
	std::unordered_map<std::string, RetargetBoneType> bone_retarget_type;
	struct mirror {
		std::string bone1;
		std::string bone2;
	};
	std::vector<mirror> mirrored_bones;
	std::vector< WeightlistDef> weightlists;

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

struct NodeRef
{
	int index = 0;
	glm::mat4 globaltransform=glm::mat4(1.f);
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

// For compilier, compilied version is slimmed down obvs
struct FATVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 tangent;
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
		return (attribute_mask & (1 << CMA_BONEINDEX)) && (attribute_mask & (1<<CMA_BONEWEIGHT));
	}

	bool has_tangents() const {
		return (attribute_mask & (1 << CMA_TANGENT));
	}

	bool has_normals() const {
		return (attribute_mask & (1 << CMA_NORMAL));
	}
};

struct CompileModLOD
{
	std::vector<LODMesh> mesh_nodes;
};



// Animation related data

struct SkeletonCompileData
{
	cgltf_skin* using_skin = nullptr;

	int get_num_bones() const {
		return using_skin->joints_count;
	}

	void init_skin(cgltf_data* data, cgltf_skin* skin) {
		using_skin = skin;

	}

	int get_bone_parent(int bone) const {
		return bones.at(bone).parent;
	}

	int get_bone_for_name(std::string name) const {

		for (int i = 0; i < bones.size(); i++)
			if (bones[i].strname == name)
				return i;
		return -1;
	}

	glm::vec3 get_local_position(int bone) const {
		return bones[bone].localtransform[3];
	}
	glm::quat get_local_rotation(int bone) const {
		return bones[bone].rot;
	}
	float get_local_scale(int bone) const {
		return 1.0;
	}

	glm::mat4 armature_root = glm::mat4(1.f);

	std::vector<int> original_bone_index_to_output;

	std::vector<BoneData> bones;


	unique_ptr<Animation_Set> setself;
};

struct ModelCompileData
{
	const cgltf_data* gltf_file = nullptr;

	// LODS
	std::vector<CompileModLOD> lod_where;

	// PHYSICS NODES
	std::vector<LODMesh> physics_nodes;

	// RENDER+PHYSICS VERTEX DATA
	std::vector<FATVertex> verticies;
	std::vector<uint32_t> indicies;
};

struct ProcessMeshOutput
{
	std::vector<bool> material_is_used;
	std::vector<int> LOAD_bone_to_FINAL_bone;
	std::vector<int> FINAL_bone_to_LOAD_bone;
	int get_final_count() const { return FINAL_bone_to_LOAD_bone.size(); }
};
struct AnimationSourceToCompile
{
	std::vector<int>* remap = nullptr;
	const SkeletonCompileData* skel = nullptr;
	int animation_souce_index = 0;
	bool should_retarget_this = false;

	const Animation_Set* get_set() const {
		return skel->setself.get();
	}

	const Animation* get_animation() const {
		return &skel->setself->clips.at(animation_souce_index);
	}
	const std::string& get_animation_name() const {
		return get_animation()->name;
	}
	bool is_self() const {
		return remap == nullptr;
	}
};

struct FinalSkeletonOutput;
class ModelCompileHelper
{
public:

	static ModelDefData parse_definition_file(const std::string& deffile);

	static bool compile_model(const std::string& defname, const ModelDefData& data);

	static void load_gltf_skeleton(cgltf_data* data, glm::mat4& armature_root,std::vector<BoneData>& bones, cgltf_skin* skin);

	static void addskeleton_R(std::unordered_map<std::string, int>& bone_to_index, cgltf_data* data, std::vector<BoneData>& bones, cgltf_node* node);

	static std::vector<int> create_bonemap_between(const MSkeleton* src, const MSkeleton* target) {
		std::vector<int> bones(target->get_num_bones());
		for (int i = 0; i < bones.size(); i++) {
			bones[i] = src->get_bone_index(target->bone_dat[i].name);
		}
		return bones;
	}

	static ProcessMeshOutput process_mesh(ModelCompileData& comp, const SkeletonCompileData* scd, const ModelDefData& data);

	static unique_ptr<FinalSkeletonOutput> create_final_skeleton(
		const std::vector<int>& LOAD_bone_to_FINAL_bone,
		const std::vector<int>& FINAL_bone_to_LOAD_bone,
		const SkeletonCompileData* compile_data, 
		const ModelDefData& data);

	static void process_physics(const ModelCompileData& comp, const ModelDefData& data);
	static void subtract_clips(const int num_bones, AnimationSeq* target, const AnimationSeq* source);
	static std::vector<std::string> create_final_material_names(const std::string& modelname, const ModelCompileData& comp, const ModelDefData& data, const std::vector<bool>& mats_refed);
	static void append_animation_seq_to_list(
		AnimationSourceToCompile source,
		FinalSkeletonOutput* final_,
		const std::vector<int>& FINAL_bone_to_LOAD_bone,
		const std::vector<int>& LOAD_bone_to_FINAL_bone,
		const SkeletonCompileData* myskel,
		const ModelDefData& data);

};




#define CAST_TO_AND_INDEX(index, type, buffer) ((type*)(buffer))[index]
class FormatConverter
{
public:

	static uint32_t convert_integer(const uint8_t* input_buf, cgltf_component_type type) {
		assert(type != cgltf_component_type_r_32f);
		if (type == cgltf_component_type_r_8)
			return CAST_TO_AND_INDEX(0, int8_t, input_buf);
		else if (type == cgltf_component_type_r_8u)
			return CAST_TO_AND_INDEX(0, uint8_t, input_buf);
		else if (type == cgltf_component_type_r_16)
			return CAST_TO_AND_INDEX(0, int16_t, input_buf);
		else if (type == cgltf_component_type_r_16u)
			return CAST_TO_AND_INDEX(0, uint16_t, input_buf);
		else if (type == cgltf_component_type_r_32u)
			return CAST_TO_AND_INDEX(0, uint32_t, input_buf);
		assert(0);
		return 0;
	}

	static glm::vec4 convert_to_floatvec(const uint8_t* input_buf, cgltf_component_type type, cgltf_type count, bool normalized, float default_=0.0) {
		assert(count < cgltf_type_mat2);
		assert(type == cgltf_component_type_r_32f || normalized);
		glm::vec4 out_vec=glm::vec4(default_);

		if (type == cgltf_component_type_r_32f) {
			float* input_buf_f = (float*)input_buf;
			for (int i = 0; i < count; i++) {
				out_vec[i] = input_buf_f[i];
			}
		}
		else if(normalized) {
			for (int i = 0; i < count; i++) {
				if (type == cgltf_component_type_r_8u) {
					int normalized = CAST_TO_AND_INDEX(i, uint8_t, input_buf);
					out_vec[i] = normalized * 255.f;
				}
				else if (type == cgltf_component_type_r_16u) {
					int normalized = CAST_TO_AND_INDEX(i, uint16_t, input_buf);
					out_vec[i] = normalized * (float)UINT16_MAX;
				}
				else
					assert(0);
			}
		}
		return out_vec;
	}
	static glm::ivec4 convert_to_intvec(const uint8_t* input_buf,cgltf_component_type type, cgltf_type count, bool normalized) {
		assert(type != cgltf_component_type_r_32f && !normalized);	// cant be float
		assert(count < cgltf_type_mat2);
		glm::vec4 input;
		for (int i = 0; i < count; i++) {
			if (type == cgltf_component_type_r_8)
				input[i] = CAST_TO_AND_INDEX(i, int8_t, input_buf);
			else if (type == cgltf_component_type_r_8u)
				input[i] = CAST_TO_AND_INDEX(i, uint8_t, input_buf);
			else if (type == cgltf_component_type_r_16)
				input[i] = CAST_TO_AND_INDEX(i, int16_t, input_buf);
			else if (type == cgltf_component_type_r_16u)
				input[i] = CAST_TO_AND_INDEX(i, uint16_t, input_buf);
			else if (type == cgltf_component_type_r_32u)
				input[i] = CAST_TO_AND_INDEX(i, uint32_t, input_buf);
			else
				assert(0);
		}
		return input;
	}

};
#undef CAST_TO_AND_INDEX


template<typename FUNCTOR, typename T>
void convert_format_verts(FUNCTOR&& f, size_t start, std::vector<T>& verts, cgltf_accessor* ac)
{
	uint8_t* buffer = (uint8_t*)ac->buffer_view->buffer->data + ( ac->buffer_view->offset + ac->offset );
	assert(verts.size() == start+ac->count);
	assert(ac->stride != 0);
	for (int i = 0; i < ac->count; i++) {
		uint8_t* ptr = buffer + i * ac->stride;
		f(verts[start+i], ptr, ac->component_type, ac->type, ac->normalized);
	}
}


void append_a_found_mesh_node(
	const ModelDefData& def,
	ModelCompileData& mcd,
	cgltf_node* node,
	const glm::mat4& transform, bool is_collision_node)
{
	ASSERT(node->mesh);

	cgltf_mesh* mesh = node->mesh;

	std::string node_name = node->name;

	int lod = 0;
	// :<
	if (node_name.find("LOD0_") == 0)
		lod = 0;
	else if (node_name.find("LOD1_") == 0)
		lod = 1;
	else if (node_name.find("LOD2_") == 0)
		lod = 2;
	else if (node_name.find("LOD3_") == 0)
		lod = 3;
	else if (node_name.find("LOD4_") == 0)
		lod = 4;


	if (mcd.lod_where.size() <= lod)
		mcd.lod_where.resize(lod + 1);

	for (int i = 0; i < mesh->primitives_count; i++)
	{
		const cgltf_primitive& prim = mesh->primitives[i];

		Submesh part;
		Bounds bounds;

		 cgltf_accessor* indicies_accessor = prim.indices;

		// get base vertex of this new mesh part, uses position vertex buffer to determine size
		part.base_vertex = mcd.verticies.size();
		part.element_offset = mcd.indicies.size() * sizeof(uint32_t);
		part.element_count = indicies_accessor->count;

		const size_t index_start = mcd.indicies.size();
		const int index_count = indicies_accessor->count;
		mcd.indicies.resize(index_start + index_count);

		convert_format_verts([](uint32_t& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized)
			{
				v = FormatConverter::convert_integer(ptr, ct);
			}, index_start, mcd.indicies, indicies_accessor);


		part.material_idx = -1;
		if (prim.material)
			part.material_idx = cgltf_material_index(mcd.gltf_file, prim.material);

		assert(prim.attributes_count >= 1 && prim.attributes[0].type == cgltf_attribute_type_position);
		const int vert_count = prim.attributes[0].data->count;
		const size_t vert_start = part.base_vertex;
		part.vertex_count = vert_count;
		mcd.verticies.resize(vert_start + vert_count);

		int attrib_mask = 0;

		for (int at_index = 0; at_index < prim.attributes_count; at_index++)
		{
			cgltf_attribute& attribute = prim.attributes[at_index];
			cgltf_accessor& accessor = *attribute.data;
			int byte_stride = accessor.stride;

			int location = -1;
			if (strcmp(attribute.name, "POSITION") == 0) {

				location = CMA_POSITION;

				bounds = bounds_union(bounds, glm::vec3(accessor.min[0], accessor.min[1], accessor.min[2]));
				bounds = bounds_union(bounds, glm::vec3(accessor.max[0], accessor.max[1], accessor.max[2]));

				convert_format_verts([](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized)
					{
						v.position = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					}, vert_start, mcd.verticies, &accessor);
			}
			else if (strcmp(attribute.name, "TEXCOORD_0") == 0) {
				location = CMA_UV;
				convert_format_verts([](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized)
					{
						v.uv = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					}, vert_start, mcd.verticies, &accessor);
			}
			else if (strcmp(attribute.name, "TEXCOORD_1") == 0) {
				location = CMA_UV2;
				convert_format_verts([](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized)
					{
						v.uv2 = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					}, vert_start, mcd.verticies, &accessor);
			}
			else if (strcmp(attribute.name, "NORMAL") == 0) {
				location = CMA_NORMAL;
				convert_format_verts([](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized)
					{
						v.normal = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					}, vert_start, mcd.verticies, &accessor);
			}
			else if (strcmp(attribute.name, "JOINTS_0") == 0) {
				location = CMA_BONEINDEX;
				convert_format_verts([](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized)
					{
						v.bone_index = FormatConverter::convert_to_intvec(ptr, ct, type, normalized);
					}, vert_start, mcd.verticies, &accessor);
			}
			else if (strcmp(attribute.name, "WEIGHTS_0") == 0) {
				location = CMA_BONEWEIGHT;
				convert_format_verts([](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized)
					{
						v.bone_weight = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					}, vert_start, mcd.verticies, &accessor);
			}
			else if (strcmp(attribute.name, "COLOR_0") == 0) {
				convert_format_verts([](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized)
					{
						v.color = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized,1.f);
					}, vert_start, mcd.verticies, &accessor);
				location = CMA_COLOR;
			}
			else if (strcmp(attribute.name, "TANGENT") == 0) {
				convert_format_verts([](FATVertex& v, uint8_t* ptr, cgltf_component_type ct, cgltf_type type, bool normalized)
					{
						v.tangent = FormatConverter::convert_to_floatvec(ptr, ct, type, normalized);
					}, vert_start, mcd.verticies, &accessor);
				location = CMA_TANGENT;
			}
			if (location == -1) continue;

			attrib_mask |= (1 << location);

		}

		LODMesh m;
		m.submesh = part;
		m.bounds = bounds;
		m.ref.globaltransform = transform;
		m.ref.index = cgltf_node_index(mcd.gltf_file, node);
		m.attribute_mask = attrib_mask;
		m.mark_for_delete = false;

		if (is_collision_node)
			mcd.physics_nodes.push_back(m);
		else
			mcd.lod_where[lod].mesh_nodes.push_back(m);
	}

}


glm::mat4 get_node_transform(cgltf_node* node)
{
	glm::mat4 local_transform = glm::mat4(1);
	if (node->has_matrix) {
		float* m = node->matrix;
		local_transform = glm::mat4(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
			m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
	}
	else {
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


 void ModelCompileHelper::addskeleton_R(std::unordered_map<std::string, int>& bone_to_index, cgltf_data* data,std::vector<BoneData>& bones, cgltf_node* node)
{
	std::string name = node->name;
	if (bone_to_index.find(name) != bone_to_index.end())
	{
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


void ModelCompileHelper::load_gltf_skeleton(cgltf_data* data, glm::mat4& armature_root, std::vector<BoneData>& bones,cgltf_skin* skin)
{
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
		b.invposematrix = glm::mat4(start[0], start[1], start[2], start[3],
			start[4], start[5], start[6], start[7],
			start[8], start[9], start[10], start[11],
			start[12], start[13], start[14], start[15]);
		b.posematrix = glm::inverse(glm::mat4(b.invposematrix));
		b.localtransform = get_node_transform(node);
		b.name = node->name;
		b.strname = node->name;

		// needed when animations dont have any keyframes, cause gltf exports nothing when its euler angles ???
		b.rot = glm::quat_cast(glm::mat4(b.localtransform));
		bone_to_index.insert({ std::string(node->name),bones.size() });
		bones.push_back(b);
	}
	cgltf_scene* defscene = data->scene;
	for (int i = 0; i < defscene->nodes_count; i++) {
		addskeleton_R(bone_to_index, data, bones, defscene->nodes[i]);
	}

	armature_root = glm::mat4(1);

	if (bones[0].parent != -1) {
		sys_print("!!! root bone not first bone\n");
		std::abort();
	}
	cgltf_node* node = skin->joints[0];
	if (node->parent) {
		armature_root = get_node_transform(node->parent);
	}
	

}


std::unordered_map<int, int> fill_out_node_to_index(cgltf_data* data, cgltf_skin* skin) {
	std::unordered_map<int, int> node_to_index;

	for (int i = 0; i < skin->joints_count; i++) {
		node_to_index[cgltf_node_index(data, skin->joints[i])] = i;
	}
	return node_to_index;
}

static Animation_Set* load_animation_set_for_gltf_skin(cgltf_data* data, cgltf_skin* skin)
{
	Animation_Set* set = new Animation_Set;
	std::unordered_map<int, int> node_to_index = fill_out_node_to_index(data, skin);
	set->num_channels = skin->joints_count;// skin.joints.size();
	for (int a = 0; a < data->animations_count; a++)
	{
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
			
			if (node_to_index.find(node_of_target) == node_to_index.end())	// channel is associated with a different armature or something
				continue;

			int channel_idx = node_to_index[node_of_target];
			AnimChannel& my_channel = set->channels[my_anim.channel_offset + channel_idx];

			int type = -1;
			if (gltf_channel->target_path == cgltf_animation_path_type_translation) type = 0;
			else if (gltf_channel->target_path == cgltf_animation_path_type_rotation) type = 1;
			else if (gltf_channel->target_path == cgltf_animation_path_type_scale) type = 2;
			else continue;

			cgltf_animation_sampler* sampler = gltf_channel->sampler;
			cgltf_accessor* timevals = sampler->input;
			cgltf_accessor* vals = sampler->output;
			ASSERT(timevals->count == vals->count);
			cgltf_buffer_view* time_bv = timevals->buffer_view;
			cgltf_buffer_view* val_bv = vals->buffer_view;
			ASSERT(time_bv->buffer == val_bv->buffer);
			cgltf_buffer* buffer = time_bv->buffer;
			ASSERT(timevals->component_type == cgltf_component_type_r_32f);// just make life easier
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

			}
			else if (type == 1) {
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
			}
			else if (type == 2) {
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
		auto& anim = set->clips.back();
#if 0
		{
			int clip_index = set->clips.size() - 1;
			int start_index = set->FirstPositionKeyframe(0.0, root_index, clip_index);
			int end_index = set->FirstPositionKeyframe(my_anim.total_duration, root_index, clip_index);

			if (start_index != -1 && end_index != -1) {
				glm::vec3 pstart = set->GetPos(root_index, start_index, clip_index).val;
				glm::vec3 pend = set->GetPos(root_index, end_index, clip_index).val;
				anim.root_motion_translation = root_transform * glm::vec4(pend - pstart, 1.0);
			}
		}
#endif
	}

	return set;
}


ModelDefData ModelCompileHelper::parse_definition_file(const std::string& name) {
	auto file = FileSys::open_read_os(name.c_str());
	if (!file)
		throw std::runtime_error("couldn't open dict");
	DictParser in;
	in.load_from_file(file.get());

	ModelDefData out;

	StringView tok;
	while (in.read_string(tok) && !in.is_eof()) {
		if (tok.cmp("source")) {
			in.read_string(tok);
			out.model_source = to_std_string_sv(tok);
		}
		else if (tok.cmp("armature_name")) {
			in.read_string(tok);
			out.armature_name = to_std_string_sv(tok);
		}
		else if (tok.cmp("animation")) {
			AnimationClip_Load acl;
			in.read_string(tok);
			auto name = to_std_string_sv(tok);

			if (!in.expect_item_start()) throw std::runtime_error("expected {");
			while (in.read_string(tok) && !in.is_eof() && !in.check_item_end(tok)) {
				if (tok.cmp("event")) {

				}
				else if (tok.cmp("rename")) {

				}
				else if (tok.cmp("subtract")) {
					in.read_string(tok);
					auto str1 = to_std_string_sv(tok);
					if (str1 == "this") {
						acl.sub = SubtractType_Load::FromThis;
					}
					else {
						acl.sub = SubtractType_Load::FromAnother;
						acl.subtract_clipname = str1;
					}
				}
				else if (tok.cmp("fps")) {

				}
				else if (tok.cmp("crop")) {
					float start = 0.0;
					in.read_float(start);
					in.read_string(tok);
					float end = 10000.0;
					if (!tok.cmp("END")) {
						end = atof(tok.to_stack_string().c_str());
					}
					acl.crop.has_crop = true;
					acl.crop.start = start;
					acl.crop.end = end;
				}
				else if (tok.cmp("start")) {

				}
				else if (tok.cmp("fixloop")) {
					acl.fixloop = true;
				}
			}

			out.str_to_clip_def[name] = acl;
		}
		else if (tok.cmp("rename_bone")) {
			in.read_string(tok);
			auto str1 = to_std_string_sv(tok);
			in.read_string(tok);
			auto str2 = to_std_string_sv(tok);
			out.bone_rename[str1] = str2;
		}
		else if (tok.cmp("bone_retarget")) {
			in.read_string(tok);
			std::string bone = to_std_string_sv(tok);
			in.read_string(tok);
			RetargetBoneType type{};
			if (tok.cmp("AScaled"))type = RetargetBoneType::FromAnimationScaled;
			else if (tok.cmp("Keep")) type = RetargetBoneType::FromAnimation;
			else if (tok.cmp("TBind")) type = RetargetBoneType::FromTargetBindPose;
			else throw std::runtime_error("unknown bone_retarget type");
			out.bone_retarget_type[bone] = type;
		}
		else if (tok.cmp("weightlist")) {
			in.read_string(tok);
			WeightlistDef wd;
			wd.name = to_std_string_sv(tok);
			if (!in.expect_list_start()) throw std::runtime_error("expected [");
			while (in.read_string(tok)&&!in.is_eof() && !in.check_list_end(tok)) {
				auto bone = to_std_string_sv(tok);
				float weight = 0.0;
				in.read_float(weight);
				wd.defs.push_back({ bone,weight });
			}
			out.weightlists.push_back(wd);
		}
		else if (tok.cmp("mirror")) {
			ModelDefData::mirror m;
			in.read_string(tok);
			m.bone1 = to_std_string_sv(tok);
			in.read_string(tok);
			m.bone2 = to_std_string_sv(tok);
			out.mirrored_bones.push_back(m);
		}
		else if (tok.cmp("lod_skeleton")) {

		}
		else if (tok.cmp("LOD")) {
			int level = 0;
			in.read_int(level);
			float dist = 1.0;
			in.read_float(dist);

			LODDef l;
			l.distance = dist;
			l.lod_num = level;
			out.loddefs.push_back(l);
		}
		else if (tok.cmp("physics")) {

		}
		else if (tok.cmp("collider")) {

		}
		else if (tok.cmp("material_dir")) {
			in.read_string(tok);
			auto str1 = to_std_string_sv(tok);
			out.root_material_dir = str1;
		}
		else if (tok.cmp("keep_bone")) {
			in.read_string(tok);
			auto str1 = to_std_string_sv(tok);
			out.keepbones.push_back(str1);
		}
		else if (tok.cmp("rename_mat")) {
			in.read_string(tok);
			auto str1 = to_std_string_sv(tok);
			in.read_string(tok);
			auto str2 = to_std_string_sv(tok);
			out.material_rename[str1] = str2;
		}
		else if (tok.cmp("include") || tok.cmp("include_ex")) {
			bool extended = tok.cmp("include_ex");
			in.read_string(tok);
			AnimImportType_Load type{};
			if (tok.cmp("file")) type = AnimImportType_Load::File;
			else if (tok.cmp("folder")) type = AnimImportType_Load::Folder;
			else if (tok.cmp("model")) type = AnimImportType_Load::Model;
			else throw std::runtime_error("bad include type");
			in.read_string(tok);
			std::string name = to_std_string_sv(tok);
			AnimImportedSet_Load ais;
			ais.name = name;
			ais.type = type;

			if (extended) {
				if (!in.expect_item_start()) throw std::runtime_error("expected { for include_ex");

				while (in.read_string(tok) && !in.is_eof() && !in.check_item_end(tok)) {
					if (tok.cmp("retarget")) {
						ais.retarget = true;
					}
				}
			}


			out.imports.push_back(ais);

		}
		else {
			throw std::runtime_error("unknown key" + to_std_string_sv(tok));
		}


	}
	return out;
}

glm::mat4 compute_world_space(glm::mat4 localtransform, int bone, MSkeleton* skel)
{
	while (bone != -1) {
		int parent = skel->get_bone_parent(bone);
		localtransform = skel->get_bone_local_transform(parent) * localtransform;
		bone = parent;
	}
	return localtransform;
}


MSkeleton::~MSkeleton() {
	for (auto clip : clips) {
		if (clip.second.skeleton_owns_clip)
			delete clip.second.ptr;
	}
}
const AnimationSeq* MSkeleton::find_clip(const std::string& name, int& remap_index) const
{
	remap_index = -1;
	auto findthis = clips.find(name);
	if (findthis != clips.end()) {
		remap_index = findthis->second.remap_idx;
		return findthis->second.ptr;
	}
	return nullptr;
}

static const char* MODELDIR = "./Data/Models/";
static std::string modelpath_to_fullpath(const std::string& m) {
	return MODELDIR + m;
}

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


cgltf_and_binary load_cgltf_data(const std::string& path)
{
	cgltf_and_binary out;

	cgltf_options options = {};
	cgltf_data* data = NULL;

	auto sourceFile = FileSys::open_read_os(path.c_str());
	if (!sourceFile) {
		sys_print("!!! couldn't open souce file %s\n",path.c_str());
		return {};
	}
	out.bin_file = new uint8_t[sourceFile->size()];
	out.bin_len = sourceFile->size();
	sourceFile->read(out.bin_file, out.bin_len);
	sourceFile.reset();	// close file


	cgltf_result result = cgltf_parse(&options, out.bin_file, out.bin_len, &data);

	if (result != cgltf_result_success) {
		sys_print("!!! cgltf failed to parse file\n");
		delete[] out.bin_file;
		return {};
	}
	cgltf_load_buffers(&options, data, path.c_str());

	out.data = data;

	return out;;
}

unique_ptr<SkeletonCompileData> get_skin_from_file(cgltf_data* dat, const char* name, const std::string armature)
{
	cgltf_skin* s = nullptr;
	if (dat->skins_count == 0) {
		// blank
		return nullptr;
	}
	else if (dat->skins_count == 1)
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
			sys_print("??? multiple skins in %s, trunacting to first", name);
			s = &dat->skins[0];
		}
	}

	SkeletonCompileData* scd = new SkeletonCompileData;
	
	scd->init_skin(dat, s);
	scd->setself.reset(load_animation_set_for_gltf_skin(dat, s));
	ModelCompileHelper::load_gltf_skeleton(dat, scd->armature_root, scd->bones, s);

	return unique_ptr<SkeletonCompileData>(scd);
}


static void traverse_model_nodes(
	const ModelDefData& def,
	ModelCompileData& mcd,
	const cgltf_skin* using_skin, 
	cgltf_node* node, 
	glm::mat4 transform)
{
	// decide what to do with this
	glm::mat4 this_transform = transform * get_node_transform(node);

	// Collision prefixes
	// BOX_
	// CAP_
	// SPH_
	// CVX_
	// TRI_

	std::string node_name = node->name;
	int col_index = -1;
	if (node_name.find("BOX_")== 0)
		col_index = 0;
	else if (node_name.find("CAP_") == 0)
		col_index = 1;
	else if (node_name.find("SPH_") == 0)
		col_index = 2;
	else if (node_name.find("CVX_") == 0)
		col_index = 3;
	else if (node_name.find("TRI_") == 0)
		col_index = 4;

	bool has_mesh = node->mesh;
	if (col_index != -1) {
		if (has_mesh)
			sys_print("*** found collision item %s\n", node_name.c_str());
		else
			sys_print("??? node has collision name but no mesh %s\n", node_name.c_str());
	}
	else if (using_skin != node->skin && has_mesh) {
		has_mesh = false;
		sys_print("??? this model is skinned but found a mesh node that isn't parented to it, skipping it\n");
	}

	if (has_mesh) {
		 append_a_found_mesh_node(def, mcd, node, this_transform, col_index!=-1);
	}

	for (int i = 0; i < node->children_count; i++)
		traverse_model_nodes(def, mcd, using_skin, node->children[i], this_transform);
}

void mark_used_bones_R(int this_index, const SkeletonCompileData* scd, std::vector<bool>& bone_refed)
{
	int parent = scd->get_bone_parent(this_index);
	if (bone_refed[this_index] && parent != -1) {
		bone_refed[parent] = true;
		mark_used_bones_R(parent, scd, bone_refed);
	}
}

ProcessMeshOutput ModelCompileHelper::process_mesh(ModelCompileData& mcd, const SkeletonCompileData* scd, const ModelDefData& def)
{
	std::vector<bool> material_is_used(mcd.gltf_file->materials_count + 1, false);
	
	std::vector<bool> bone_is_referenced;

	if (scd) {
		bone_is_referenced.resize(scd->get_num_bones(), false);

		for (int i = 0; i < def.keepbones.size(); i++) {

			int index = scd->get_bone_for_name(def.keepbones[i]);
			if (index == -1) {
				sys_print("!!! keepbone does not name a skeleton bone %s\n", def.keepbones[i].c_str());
			}
			else
				bone_is_referenced[index] = true;
		}
	}

	 for (int i = 0; i < mcd.lod_where.size(); i++) {
		 auto& lod = mcd.lod_where[i];
		 for (auto& mesh : lod.mesh_nodes) {

			 if (!mesh.has_bones() && scd != nullptr) {
				 sys_print("??? nobone mesh made it past filters?\n");
				 mesh.mark_for_delete = true;
				 continue;
			 }

			 if (!mesh.has_normals()) {
				 sys_print("??? mesh was exported without normals, skipping it...\n");
				 mesh.mark_for_delete = true;
				 continue;
			 }

			 // Add any material refs
			 if (mesh.submesh.material_idx != -1)
				 material_is_used.at(mesh.submesh.material_idx) = true;
			 else
				 material_is_used.at(material_is_used.size() - 1) = true;	// null material needed

			 // mark bones that should be kept
			 if (scd) {
				 const int num_bones = scd->get_num_bones();
				 for (int j = 0; j < mesh.submesh.vertex_count; j++) {
					 int index = mesh.submesh.base_vertex + j;

					 FATVertex& fv = mcd.verticies.at(index);

					 for (int x = 0; x < 4; x++) {
						 assert(fv.bone_index[x] >= -1 && fv.bone_index[x] < num_bones);
						 if (fv.bone_index[x] != -1) {

							 bone_is_referenced.at(fv.bone_index[x]) = true;
						 
						 }
					 }
				 }
			 }
		 }
	 }

	 // mark used bone parents

	 int FINAL_bone_count = 0;
	std::vector<int> FINAL_to_LOAD_bones;
	 std::vector<int> LOAD_to_FINAL_bones;
	 if (scd) {
		 const int num_bones = scd->get_num_bones();
		 for (int i = 0; i < num_bones; i++) {
			 mark_used_bones_R(i, scd, bone_is_referenced);
		 }
		 LOAD_to_FINAL_bones.resize(num_bones, -1);
		 int count = 0;
		 for (int i = 0; i < num_bones; i++) {
			 if (bone_is_referenced[i]) {
				 LOAD_to_FINAL_bones[i] = count++;
			 }
			 else {
				 sys_print("*** bone will be pruned %s\n", scd->bones[i].strname.c_str());
			 }
		 }
		 FINAL_to_LOAD_bones.resize(count);
		for (int i = 0; i < LOAD_to_FINAL_bones.size(); i++)
			if(LOAD_to_FINAL_bones[i]!=-1)
				FINAL_to_LOAD_bones.at(LOAD_to_FINAL_bones[i]) = i;

		 FINAL_bone_count = count;
		 sys_print("*** final bone count %d\n", FINAL_bone_count);

		 for (int i = 0; i < mcd.lod_where.size(); i++) {
			 for (int j = 0; j < mcd.lod_where[i].mesh_nodes.size(); j++) {
				 auto& mesh = mcd.lod_where[i].mesh_nodes[j];
				 if (mesh.mark_for_delete)
					 continue;
				 for (int k = 0; k < mesh.submesh.vertex_count; k++) {
					 int index = mesh.submesh.base_vertex + k;
					 FATVertex& fv = mcd.verticies.at(index);
					 for (int l = 0; l < 4; l++) {
						 if (fv.bone_index[l] == -1)
							 continue;
						 assert(LOAD_to_FINAL_bones[fv.bone_index[l]] != -1);	// should always be kept
						 fv.bone_index[l] = LOAD_to_FINAL_bones[fv.bone_index[l]];
					 }
				 }
			 }
		 }

		 // sanity checks
		 assert(FINAL_bone_count > 0 && scd->get_bone_parent(LOAD_to_FINAL_bones[0]) == -1);
		 for (int i = 0; i < FINAL_bone_count; i++) {
			 const int FINAL_bone = i;
			 const int LOAD_bone = FINAL_to_LOAD_bones[i];
			 assert(LOAD_bone != -1);
			 const int LOAD_parent = scd->get_bone_parent(LOAD_bone);
			 const int FINAL_parent = (LOAD_parent==-1) ? -1 : LOAD_to_FINAL_bones[LOAD_parent];
			 assert(FINAL_parent < FINAL_bone);
		 }

	 }


	 ProcessMeshOutput output;
	 output.FINAL_bone_to_LOAD_bone = std::move(FINAL_to_LOAD_bones);
	 output.LOAD_bone_to_FINAL_bone = std::move(LOAD_to_FINAL_bones);
	 output.material_is_used = std::move(material_is_used);

	 return output;
}


struct ImportedSkeleton
{
	unique_ptr<SkeletonCompileData> skeleton = nullptr;
	std::vector<int> remap_from_LOAD_to_THIS;
	bool retarget_this = false;
};

unique_ptr<SkeletonCompileData> open_file_and_read_skeleton(const std::string& path)
{
	cgltf_and_binary out = load_cgltf_data(path.c_str());

	if (!out.data)
		return nullptr;

	auto skeleton_data = get_skin_from_file(out.data, path.c_str(), "");
	out.free();

	return std::move( skeleton_data );
}

static std::vector<int> create_remap_table(const SkeletonCompileData* source, const SkeletonCompileData* target)
{
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

std::vector<ImportedSkeleton> read_animation_imports(
	const std::vector<int>& LOAD_bone_to_FINAL_bone,
	const SkeletonCompileData* compile_data,
	const ModelDefData& data)
{
	std::vector<ImportedSkeleton> imports;

	for (int i = 0; i < data.imports.size(); i++) {
		if (data.imports[i].type == AnimImportType_Load::Model)
			continue;
		else if (data.imports[i].type == AnimImportType_Load::File) {
			ImportedSkeleton is;
			is.retarget_this = data.imports[i].retarget;

			std::string full_path = modelpath_to_fullpath(data.imports[i].name);

			is.skeleton = open_file_and_read_skeleton(full_path);

			if (!is.skeleton) {
				sys_print("!!! import animation failed %s\n", full_path.c_str());
			}
			else {
				is.remap_from_LOAD_to_THIS = create_remap_table(is.skeleton.get(), compile_data);
				imports.push_back(std::move(is));
			}
		}
		else if (data.imports[i].type == AnimImportType_Load::Folder) {

			std::string full_path_to_folder = modelpath_to_fullpath(data.imports[i].name);
			FileTree tree = FileSys::find_files(full_path_to_folder.c_str());

			for (const auto& file : tree) {

				const std::string& full_path = file;
				if (get_extension(full_path) != ".glb")
					continue;
				ImportedSkeleton is;
				is.retarget_this = data.imports[i].retarget;
				is.skeleton = open_file_and_read_skeleton(full_path);

				if (!is.skeleton) {
					sys_print("!!! import animation failed %s\n", full_path.c_str());
				}
				else {
					is.remap_from_LOAD_to_THIS = create_remap_table(is.skeleton.get(), compile_data);
					imports.push_back(std::move(is));
				}
			}
		}
	}
	sys_print("*** imported %d files\n", (int)imports.size());
	return imports;
}

struct FinalSkeletonOutput
{
	// These are all using the FINAL bones
	std::unordered_map<std::string, AnimationSeq> allseqs;
	std::vector<BoneData> bones;
	std::vector<int16_t> mirror_table;
	std::vector<BonePoseMask> masks;
	glm::mat4 armature_root_transform;
	std::vector<std::string> imported_models;

	bool does_sequence_already_exist(const std::string& name) const {
		return find_sequence(name) != nullptr;
	}

	void add_sequence(const std::string& name, AnimationSeq&& seq) {
		allseqs.insert({ name,std::move(seq) });
	}

	const AnimationSeq* find_sequence(const std::string& name) const {
		if (allseqs.find(name) == allseqs.end())
			return nullptr;
		return &allseqs.find(name)->second;
	}

	 AnimationSeq* find_sequence(const std::string& name)  {
		if (allseqs.find(name) == allseqs.end())
			return nullptr;
		return &allseqs.find(name)->second;
	}
};

std::vector<std::string> get_imported_models(
	const ModelDefData& def)
{
	std::vector<std::string> strs;
	for (int i = 0; i < def.imports.size(); i++) {
		if (def.imports[i].type == AnimImportType_Load::Model)
			strs.push_back(def.imports[i].name);
	}
	return strs;
}

std::vector<BoneData> get_final_bone_data(
	const std::vector<int>& FINAL_bone_to_LOAD_bone,
	const std::vector<int>& LOAD_to_FINAL,

	const SkeletonCompileData* myskel)
{
	std::vector<BoneData> out(FINAL_bone_to_LOAD_bone.size());
	for (int i = 0; i < out.size(); i++) {
		int index = FINAL_bone_to_LOAD_bone[i];
		assert(index != -1);
		out[i] = myskel->bones[index];
		if (myskel->bones[index].parent != -1) {
			int FINAL_parent = LOAD_to_FINAL[myskel->bones[index].parent];
			assert(FINAL_parent != -1);
			out[i].parent = FINAL_parent;
		}
	}
	return out;
}

std::vector<int16_t> get_mirror_table(const SkeletonCompileData* myskel,
	const std::vector<int>& LOAD_bone_to_FINAL_bone,
	const int FINAL_bones_count,
	const ModelDefData& def
)
{
	if (def.mirrored_bones.empty())
		return {};
	std::vector<int16_t> out_mirror(FINAL_bones_count, -1);
	for (int i = 0; i < def.mirrored_bones.size(); i++) {
		auto& mir = def.mirrored_bones[i];
		
		int index0 = myskel->get_bone_for_name(mir.bone1);
		int index1 = myskel->get_bone_for_name(mir.bone2);
		if (index0 == -1 || index1 == -1) {
			sys_print("!!! mirrored bone not found %s %s\n", mir.bone1.c_str(), mir.bone2.c_str());
			continue;
		}
		int final_index0 = LOAD_bone_to_FINAL_bone[index0];
		int final_index1 = LOAD_bone_to_FINAL_bone[index1];
		if (final_index0 == -1 || final_index1 == -1) {
			sys_print("!!! mirrored bone was pruned %s %s\n", mir.bone1.c_str(), mir.bone2.c_str());
			continue;
		}

		out_mirror[final_index0] = final_index1;
		out_mirror[final_index1] = final_index0;
	}

	return out_mirror;
}

std::vector<BonePoseMask> get_bone_masks(
	const std::vector<int>& FINAL_to_LOAD_bone,
	const std::vector<int>& LOAD_to_FINAL_bone,

	const int FINAL_bones_count,
	const ModelDefData& def,
	const SkeletonCompileData* myskel
)
{
	std::vector<int> num_children_per_bone(FINAL_bones_count,0);


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



bool are_all_poskeyframes_equal(float epsilon, const Animation_Set* set, const Animation* a, int LOAD_channel)
{
	int offset = a->channel_offset + LOAD_channel;
	const AnimChannel& chan = set->channels[offset];
	int pos_start = chan.pos_start;
	int count = chan.num_positions;

	for (int i = 1; i < count; i++) {
		int index = pos_start + i;
		glm::vec3 first = set->positions[pos_start].val;
		glm::vec3 this_ = set->positions[index].val;
		float sq_dist = glm::dot(first-this_, first-this_);
		if (sq_dist > epsilon)
			return false;
	}
	return true;
}

bool are_all_rotframes_equal(float epsilon, const Animation_Set* set, const Animation* a, int LOAD_channel)
{
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

bool are_all_scaleframes_equal(float epsilon, const Animation_Set* set, const Animation* a, int LOAD_channel)
{
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
	for(int i=0;i<size;i++)
		seq->pose_data.push_back(f[i]);
}
static_assert(alignof(glm::quat) == 4, "a");
static_assert(alignof(glm::vec3) == 4, "a");

inline float lerp_between(float min, float max, float mid_val)
{
	return (mid_val - min) / (max - min);
}

static glm::quat quat_delta(const glm::quat& from, const glm::quat& to)
{
	return to * glm::inverse(from);
}

void ModelCompileHelper::append_animation_seq_to_list(
	AnimationSourceToCompile source,
	FinalSkeletonOutput* final_,
	const std::vector<int>& FINAL_bone_to_LOAD_bone,
	const std::vector<int>& LOAD_bone_to_FINAL_bone,
	const SkeletonCompileData* myskel,
	const ModelDefData& data)
{
	if (final_->does_sequence_already_exist(source.get_animation_name()))
		return;

	const int target_count = FINAL_bone_to_LOAD_bone.size();

	const AnimationClip_Load* definition = data.find(source.get_animation_name());

	const Animation* source_a = source.get_animation();
	const Animation_Set* source_set = source.get_set();

	AnimationSeq out_seq;

	const float fps = 30.0;
	out_seq.fps = fps;

	int START_keyframe = 0;
	int NUM_keyframes = source_a->total_duration * fps;
	int END_keyframe = NUM_keyframes;
	if (definition && definition->crop.has_crop) {
		if(definition->crop.start >= 0 && definition->crop.start < END_keyframe)
			START_keyframe = definition->crop.start;
		if (definition->crop.end < END_keyframe && END_keyframe > START_keyframe)
			END_keyframe = definition->crop.end;
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

#define SET_HIGH_BIT(x) x |= (1u<<31u)

		// All keyframes set to target bind pose
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

#define CALC_TIME_INTERP(FuncName, VarName) \
		int index0 = VarName; \
		int index1 = VarName + 1; \
		float t0 = source_set->FuncName(SRC_idx, index0, clip_index).time; \
		float t1 = source_set->FuncName(SRC_idx, index1, clip_index).time; \
		float scale = lerp_between(t0, t1, TIME); \
		//assert(scale >= 0 && scale <= 1.f);
		//if (index0 == 0)t0 = 0.f;

		// POSITION
		offsets.pos = out_seq.pose_data.size();
		bool all_pos_equal = are_all_poskeyframes_equal(0.0001f, source_set, source_a, SRC_idx);
		auto get_pos_for_time = [&](const float TIME) -> glm::vec3
		{
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
		}
		else {
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
			}
			else if (rot_idx == source_set->GetChannel(clip_index, SRC_idx).num_rotations - 1)
				interp_rot = source_set->GetRot(SRC_idx, rot_idx, clip_index).val;
			else {
				CALC_TIME_INTERP(GetRot, rot_idx);
				interp_rot = glm::slerp(
					source_set->GetRot(SRC_idx, index0, clip_index).val,
					source_set->GetRot(SRC_idx, index1, clip_index).val, scale);
			}
			interp_rot = glm::normalize(interp_rot);

			return interp_rot;
		};

		if (all_rot_equal) {
			SET_HIGH_BIT(offsets.rot);
			glm::quat rot = get_rot_for_time(0.0);
			write_out_to_outseq(&rot.x, 4, &out_seq);
		}
		else {
			for (int frame = 0; frame < out_seq.get_num_keyframes_inclusive(); frame++) {
				const int frame_w_crop = frame + START_keyframe;
				const float t = frame_w_crop / fps;
				ASSERT(t <= out_seq.duration);
				glm::quat rot = get_rot_for_time(t);
				write_out_to_outseq(&rot.x, 4, &out_seq);
			}
			assert(( out_seq.pose_data.size() - offsets.rot)/4 == out_seq.get_num_keyframes_inclusive());
		}

		// SCALE
		offsets.scale = out_seq.pose_data.size();
		bool all_scale_equal = are_all_scaleframes_equal(0.0001f, source_set, source_a, SRC_idx);
		auto get_scale_for_time = [&](const float TIME) -> float {
			int scale_idx = source_set->FirstScaleKeyframe(TIME, SRC_idx, source.animation_souce_index);
			glm::vec3 interp_scale{};
			if (scale_idx == -1) {
				interp_scale = glm::vec3(1.0);	// fixme
			}
			else if (scale_idx == source_set->GetChannel(clip_index, SRC_idx).num_scales - 1)
				interp_scale = source_set->GetScale(SRC_idx, scale_idx, clip_index).val;
			else {
				CALC_TIME_INTERP(GetScale, scale_idx);
				interp_scale = glm::mix(
					source_set->GetScale(SRC_idx, index0, clip_index).val,
					source_set->GetScale(SRC_idx, index1, clip_index).val, scale);
			}
			float uniform_scale = glm::length(interp_scale);
			return uniform_scale;
		};

		if (all_scale_equal) {
			SET_HIGH_BIT(offsets.scale);
			float uniform_scale = get_scale_for_time(0.0);
			write_out_to_outseq(&uniform_scale, 1, &out_seq);
		}
		else {
			for (int frame = 0; frame < out_seq.get_num_keyframes_inclusive(); frame++) {
				const int frame_w_crop = frame + START_keyframe;
				const float t = frame_w_crop / fps;
				ASSERT(t <= out_seq.duration);
				float uniform_scale = get_scale_for_time(t);
				write_out_to_outseq(&uniform_scale, 1, &out_seq);
			}
			assert(( out_seq.pose_data.size()-offsets.scale) == out_seq.get_num_keyframes_inclusive());
		}



#undef CALC_TIME_INTERP
#undef SET_HIGH_BIT
	}

#define IS_HIGH_BIT_SET(x) (x & (1u<<31u))
#define CLEAR_HIGH_BIT(x) ( x & ~(1u<<31u) )
	
	// Set actual duration here
	out_seq.duration = (float)NUM_keyframes / fps;

	// Apply any retargeting
	if (source.should_retarget_this) {
		for (int FINAL_idx = 0; FINAL_idx < target_count; FINAL_idx++) {
			const int LOAD_idx = FINAL_bone_to_LOAD_bone[FINAL_idx];
			assert(LOAD_idx != -1);
			float scale = 1.0;

			const int SRC_idx = (source.remap) ? (*source.remap)[LOAD_idx] : LOAD_idx;
			if (SRC_idx == -1)	// already added local pose
				continue;

			glm::mat4 transform_matrix = glm::mat4(1.0);

			if (myskel->get_bone_parent(LOAD_idx) == -1) {
				glm::mat4 local_other = source.skel->bones[SRC_idx].localtransform;
				transform_matrix = glm::inverse(myskel->armature_root)*source.skel->armature_root;
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

					if(pos)
						*pos =  transform_matrix* glm::vec4(*pos, 1.0);

					glm::mat3 justrot2 = transform_matrix;
					justrot2[0] = glm::normalize(justrot2[0]);
					justrot2[1] = glm::normalize(justrot2[1]);
					justrot2[2] = glm::normalize(justrot2[2]);

					auto try2 = glm::quat_cast(justrot2);
					if(rot)
						*rot = try2 * (*rot);
				}
				else if (myskel->bones[LOAD_idx].retarget_type == RetargetBoneType::FromAnimationScaled) {
					if(pos)
						*pos *= scale;
				}
				else if (myskel->bones[LOAD_idx].retarget_type == RetargetBoneType::FromTargetBindPose) {
					if(pos)
						*pos = myskel->get_local_position(LOAD_idx);
				}
			}
		}
	}


	
	assert(myskel->get_bone_parent(FINAL_bone_to_LOAD_bone[0]) == -1);	// ROOT
	// Calculate average linear velocity
	{
		ChannelOffset& chan = out_seq.channel_offsets[0];	// ROOT
		bool is_single_frame = IS_HIGH_BIT_SET(chan.pos);
		uint32_t pose_start = CLEAR_HIGH_BIT(chan.pos);
		if (is_single_frame)
			out_seq.average_linear_velocity = 0.0;	// no movement
		else {
			glm::vec3 first = out_seq.get_keyframe(0, 0, 0.0).pos;
			glm::vec3 last = out_seq.get_keyframe(0, out_seq.get_num_keyframes_exclusive(), 0.0).pos;

			glm::vec3 dif = last - first;

			out_seq.average_linear_velocity = glm::length(dif) / out_seq.get_duration();
		}
	}

	if (definition && definition->fixloop) {
		for (int FINAL_idx = 0; FINAL_idx < target_count; FINAL_idx++) {
			const glm::vec3* pos0 = out_seq.get_pos_write_ptr(FINAL_idx, 0);
			const glm::quat* rot0 = out_seq.get_quat_write_ptr(FINAL_idx, 0);
			glm::vec3* pos1 = out_seq.get_pos_write_ptr(FINAL_idx, out_seq.get_num_keyframes_inclusive()-1);
			glm::quat* rot1 = out_seq.get_quat_write_ptr(FINAL_idx, out_seq.get_num_keyframes_inclusive() - 1);
			if (!pos1 && !rot1)	//indicate a "single" pose frame
				continue;
			if(pos1)
				*pos1 = *pos0;
			if(rot1)
				*rot1 = *rot0;
		}
	}

	out_seq.event_keyframes.resize(out_seq.get_num_keyframes_inclusive());

	// Output any events
	if (definition) {
		
		std::vector<int> indicies;
		for (int i = 0; i < definition->events.size(); i++)
			indicies[i] = i;
		// sort by time
		std::sort(indicies.begin(), indicies.end(), [&](const int& a, const int& b) {
			return definition->events[a].time < definition->events[b].time;
		});

		EventIndex cur;
		int current_index = -1;
		for (int i = 0; i < indicies.size(); i++) {
			const auto& event = definition->events[indicies[i]];
			int frame_of_event = event.time;	// already in keyframes
			if (frame_of_event < 0)
				frame_of_event += out_seq.get_num_keyframes_inclusive();
			if (frame_of_event >= out_seq.get_num_keyframes_inclusive())
				frame_of_event = out_seq.get_num_keyframes_inclusive() - 1;

			AnimEvent ae;
			ae.str = event.type;
			out_seq.events.push_back(ae);
			if (frame_of_event != current_index) {
				if (current_index != -1 && cur.count > 0) {
					out_seq.event_keyframes.at(current_index) = cur;
					cur = EventIndex();

					cur.offset = out_seq.events.size();
					cur.count = 1;
					current_index = frame_of_event;
				}
			}
			else {
				cur.count++;
			}
		}
		if (current_index != -1 && cur.count > 0) {
			out_seq.event_keyframes.at(current_index) = cur;
		}
	}


	final_->add_sequence(source.get_animation_name(), std::move(out_seq));
}

// these are all final bones, no remapping needed
 void ModelCompileHelper::subtract_clips(const int num_bones, AnimationSeq* target, const AnimationSeq* source)
{
	 Pose ref_pose;
	 for (int i = 0; i < num_bones; i++) {
		ScalePositionRot transform = source->get_keyframe(i, 0, 0.0);
		ref_pose.pos[i] = transform.pos;
		ref_pose.q[i] = transform.rot;
	 }

	 for (int i = 0; i < num_bones; i++) {
		 for (int j = 0; j < target->get_num_keyframes_inclusive(); j++) {
			
			 glm::vec3* pos = target->get_pos_write_ptr(i, j);
			 glm::quat* rot = target->get_quat_write_ptr(i, j);
			 if (!pos && !rot)	//indicate a "single" pose frame
				 break;
			 if(pos)
				 *pos = *pos - ref_pose.pos[i];
			 if (rot)
				 *rot = quat_delta(ref_pose.q[i], *rot);
		 }
	 }
}

unique_ptr<FinalSkeletonOutput> ModelCompileHelper::create_final_skeleton(
	const std::vector<int>& LOAD_bone_to_FINAL_bone, 
	const std::vector<int>& FINAL_bone_to_LOAD_bone,
	const SkeletonCompileData* compile_data, 
	const ModelDefData& data)
{
	if (!compile_data)
		return nullptr;

	std::vector<ImportedSkeleton> imports = read_animation_imports(LOAD_bone_to_FINAL_bone, compile_data, data);
	
	FinalSkeletonOutput* final_out = new FinalSkeletonOutput;

	final_out->armature_root_transform = compile_data->armature_root;

	for (int i = 0; i < imports.size(); i++) {

		auto& imp = imports[i];
		for (int j = 0; j < imp.skeleton->setself->clips.size(); j++) {
			AnimationSourceToCompile astc;
			astc.animation_souce_index = j;
			astc.remap = &imp.remap_from_LOAD_to_THIS;
			astc.skel = imp.skeleton.get();
			astc.should_retarget_this = imp.retarget_this;

			append_animation_seq_to_list(astc, final_out, FINAL_bone_to_LOAD_bone, LOAD_bone_to_FINAL_bone, compile_data, data);
		}
	}

	for (int j = 0; j < compile_data->setself->clips.size(); j++) {
		AnimationSourceToCompile astc;
		astc.animation_souce_index = j;
		astc.remap = nullptr;
		astc.skel = compile_data;

		append_animation_seq_to_list(astc, final_out, FINAL_bone_to_LOAD_bone, LOAD_bone_to_FINAL_bone, compile_data, data);
	}

	for (const auto& clip : data.str_to_clip_def) {

		// warn about unused definitions
		AnimationSeq* a = final_out->find_sequence(clip.first);
		if (!a) {
			sys_print("??? clip defintion was not applied to any animations %s\n", clip.first.c_str());
			continue;
		}

		if (clip.second.sub != SubtractType_Load::None) {
			AnimationSeq* other = a;	// subtract from self
			if (clip.second.sub == SubtractType_Load::FromAnother) {
				other = final_out->find_sequence(clip.second.subtract_clipname);
				if (!other) {
					sys_print("!!! subtract clip not found (%s from %s)\n", clip.first.c_str(), clip.second.subtract_clipname.c_str());
				}
			}
			if (other) {
				subtract_clips(FINAL_bone_to_LOAD_bone.size(), a, other);
				a->is_additive_clip = true;
			}
		}
	}

	// create final bones/masks/mirrors
	final_out->mirror_table = get_mirror_table(compile_data, LOAD_bone_to_FINAL_bone, FINAL_bone_to_LOAD_bone.size(), data);
	final_out->bones = get_final_bone_data(FINAL_bone_to_LOAD_bone,LOAD_bone_to_FINAL_bone, compile_data);
	final_out->imported_models = get_imported_models(data);
	final_out->masks = get_bone_masks(FINAL_bone_to_LOAD_bone, LOAD_bone_to_FINAL_bone, FINAL_bone_to_LOAD_bone.size(), data, compile_data);

	return unique_ptr<FinalSkeletonOutput>(final_out);
}

#include <cstring>
std::string make_non_proplematic_name(const std::string& name)
{
	std::string out;
	if (name.empty()) return "";
	for (int i = 0; i < name.size(); i++) {
		if (isalnum(name[i]))
			out.push_back(name[i]);
	}
	return out;
}
#include "Framework/DictWriter.h"
#include <fstream>
static void output_embedded_texture(const std::string& outputname, const cgltf_image* i, const cgltf_data* d)
{
	sys_print("*** writing out embedded texture %s\n", outputname.c_str());

	std::string image_path = "./Data/Textures/" + outputname;
	std::ofstream outfile(image_path.c_str(), std::ios::binary);
	if (!outfile) {
		sys_print("!!! couldn't open file to output embedded texture %s\n", image_path.c_str());
		return;
	}

	const cgltf_buffer_view& bv = *i->buffer_view;
	const cgltf_buffer& b = *bv.buffer;

	ASSERT(bv.stride == 0);
	uint8_t* buffer_bytes = (uint8_t*)b.data;

	const char* name = i->name;
	if (!name) name = "";


	outfile.write((char*)(buffer_bytes + bv.offset), bv.size);

	outfile.close();
}


static std::string create_material_and_export(const std::string& generated_name, const cgltf_data* data, const cgltf_material* mat)
{
	std::string material_dir_path = "./Data/Materials/" + generated_name + ".txt";
	bool does_exist = FileSys::does_os_file_exist(material_dir_path.c_str());

	// update it in case
	if (does_exist) {
		bool good = MaterialCompilier::compile(generated_name.c_str());
		if (!good) {
			sys_print("!!! MaterialCompilier failed on generated material %s even though it already exists\n", generated_name.c_str());
		}
		return generated_name;
	}

	// create it
	
	DictWriter out;
	out.write_key(generated_name.c_str());
	out.write_item_start();

	if (mat->has_pbr_metallic_roughness) {
		const cgltf_pbr_metallic_roughness& base = mat->pbr_metallic_roughness;
		out.write_key_value("metal_val", string_format("%f", base.metallic_factor));
		out.write_key_value("rough_val", string_format("%f", base.roughness_factor));
		out.write_key_value("tint", 
			string_format("%f %f %f",
				base.base_color_factor[0],
				base.base_color_factor[1],
				base.base_color_factor[2])
		);

		if (base.base_color_texture.texture) {
			std::string output_name =  base.base_color_texture.texture->image->name;
			output_name += ".png";
			output_embedded_texture(output_name, base.base_color_texture.texture->image, data);
			
			out.write_key_value("albedo", output_name.c_str());
		}
		if (base.metallic_roughness_texture.texture) {
			std::string output_name =  base.metallic_roughness_texture.texture->image->name;
			output_name += ".png";
			output_embedded_texture(output_name, base.metallic_roughness_texture.texture->image, data);

			out.write_key_value("rough", output_name.c_str());
		}

	}
	if (mat->normal_texture.texture) {
		std::string output_name =  mat->normal_texture.texture->image->name;
		output_name += ".png";
		output_embedded_texture(output_name, mat->normal_texture.texture->image, data);

		out.write_key_value("normal", output_name.c_str());
	}

	if (mat->double_sided)
		out.write_key("showbackface\n");

	if (mat->alpha_mode == cgltf_alpha_mode_blend)
		out.write_key("alpha blend\n");
	else if (mat->alpha_mode == cgltf_alpha_mode_mask)
		out.write_key("alpha test\n");

	out.write_item_end();
	
	std::ofstream outfile(material_dir_path.c_str());
	if (!outfile) {
		sys_print("!!! couldn't write out generated material %s\n", material_dir_path.c_str());
		return material_dir_path;
	}
	size_t count = out.get_output().size();
	outfile.write(out.get_output().c_str(), count);
	outfile.close();

	bool good = MaterialCompilier::compile(generated_name.c_str());

	if (!good) {
		sys_print("!!! MaterialCompilier failed on material that was just generated\n");
	}

	return generated_name;
}

std::vector<std::string> ModelCompileHelper::create_final_material_names(
	const std::string& modelname,
	const ModelCompileData& comp, 
	const ModelDefData& def, 
	const std::vector<bool>& materials_used)
{


	const int num_materials = comp.gltf_file->materials_count;
	std::vector<std::string> final_mats(num_materials+1);

	const cgltf_data* d = comp.gltf_file;
	for (int i = 0; i < num_materials; i++) {
		if (!materials_used[i])
			continue;
		const auto& cgltf_mat = d->materials[i];
		std::string mat_name = cgltf_mat.name;

		if (def.material_rename.find(mat_name) != def.material_rename.end())
			mat_name = def.material_rename.find(mat_name)->second;

		if (!def.root_material_dir.empty())
			mat_name = def.root_material_dir + mat_name;

		bool good = MaterialCompilier::compile(mat_name.c_str());

		if (good)
			continue;

		// not good, this means the material does not exist, thus we should create one
		std::string justname = strip_extension(modelname);
		get_filename(justname);

		std::string generated_mat_name = justname + cgltf_mat.name;
		generated_mat_name = make_non_proplematic_name(generated_mat_name);

		final_mats[i] = create_material_and_export(generated_mat_name, d, &cgltf_mat);
	}
	final_mats[num_materials] = "_NULL";
	return final_mats;
}

struct ProcessNodesAndMeshOutput
{
	ModelCompileData mcd;
	ProcessMeshOutput meshout;
};

ProcessNodesAndMeshOutput process_nodes_and_mesh(cgltf_data* data, const SkeletonCompileData* scd, const cgltf_skin* using_skin, const ModelDefData& def)
{
	ModelCompileData mcd;
	mcd.gltf_file = data;
	cgltf_scene* scene = data->scene;
	for (int i = 0; i < scene->nodes_count; i++) {
		cgltf_node* node = scene->nodes[i];
		traverse_model_nodes(def, mcd, using_skin, node, glm::mat4(1.f));
	}

	ProcessMeshOutput post_mesh_process = ModelCompileHelper::process_mesh(mcd, scd, def);

	ProcessNodesAndMeshOutput output;
	output.mcd = std::move(mcd);
	output.meshout = std::move(post_mesh_process);
	return output;
}

// Last stop for stuff thats getting written out
struct FinalModelData
{
	std::vector<ModelVertex> verticies;
	std::vector<uint16_t> indicies;
	std::vector<MeshLod> lods;
	std::vector<Submesh> submeshes;
	std::vector<std::string> material_names;
	Bounds AABB;
	std::vector<ModelTag> tags;
};

ModelVertex fatvert_to_mv_skinned(const FATVertex& v)
{
	ModelVertex mv;
	mv.pos = v.position;
	mv.uv = v.uv;

	// quantize
	for (int i = 0; i < 3; i++) {
		mv.normal[i] = v.normal[i] * INT16_MAX;
	}
	for (int i = 0; i < 3; i++) {
		mv.tangent[i] = v.tangent[i] * INT16_MAX;
	}

	for (int i = 0; i < 4; i++) {
		mv.color[i] = v.bone_index[i];
	}

	for (int i = 0; i < 4; i++) {
		int qu = v.bone_weight[i] * 255.0;
		if (qu > 255)qu = 255;
		if (qu < 0)qu = 0;
		mv.color2[i] = qu;
	}

	return mv;
}


Submesh make_final_submesh_from_existing(
	const Submesh& in,
	FinalModelData& finalmod, 
	const ModelCompileData& compile, 
	const std::vector<int>& indirect_mats_out)
{
	Submesh out;
	out.material_idx = (in.material_idx == -1) ? indirect_mats_out[indirect_mats_out.size() - 1] : indirect_mats_out[in.material_idx];
	out.element_count = in.element_count;
	out.vertex_count = in.vertex_count;

	const int index_start = in.element_offset / sizeof(uint32_t);
	const int new_index_start = finalmod.indicies.size();
	for (int i = 0; i < in.element_count; i++) {
		finalmod.indicies.push_back(compile.indicies[index_start + i]);
	}
	const int vertex_start = in.base_vertex;
	const int new_vertex_start = finalmod.verticies.size();
	for (int i = 0; i < in.vertex_count; i++) {
		finalmod.verticies.push_back(fatvert_to_mv_skinned(compile.verticies[vertex_start + i]));
	}

	out.base_vertex = new_vertex_start;
	out.element_offset = new_index_start * sizeof(uint16_t);

	return out;
}

FinalModelData create_final_model_data(
	const std::vector<std::string>& final_mat_names, 
	const std::vector<bool>& mat_is_used,

	const ModelCompileData& compile, 
	const std::vector<int>& LOAD_to_FINAL_bones,
	const ModelDefData& def)
{
	FinalModelData final_mod;

	std::vector<int> indirect_mats_out(final_mat_names.size(),-1);
	int count = 0;
	for (int i = 0; i < mat_is_used.size(); i++) {
		if (mat_is_used[i]) {
			indirect_mats_out[i] = count++;
			final_mod.material_names.push_back(final_mat_names[i]);
		}
	}


	// determine lods
	const int num_actual_lods = compile.lod_where.size();
	std::vector<int> lods_to_def(num_actual_lods,-1);
	for (int i = 0; i < def.loddefs.size(); i++) {
		assert(def.loddefs[i].lod_num >= 0);
		if (def.loddefs[i].lod_num >= num_actual_lods) continue;
		lods_to_def[def.loddefs[i].lod_num] = i;
	}

	Bounds total_bounds;
	// sort
	for (int i = 0; i < compile.lod_where.size(); i++) {
		MeshLod out_lod;

		if (i != 0) {
			if (lods_to_def[i] == -1) {
				sys_print("!!! mesh has LOD_%d parts, but distance wasn't definied in .def, skipping...\n", i);
				continue;
			}
			out_lod.end_percentage = def.loddefs[lods_to_def[i]].distance;
		}
		else
			out_lod.end_percentage = 1.0;

		out_lod.part_ofs = final_mod.submeshes.size();
		auto& lod = compile.lod_where[i];
		for (int j = 0; j < lod.mesh_nodes.size(); j++) {
			if (lod.mesh_nodes[j].mark_for_delete)
				continue;

			const LODMesh& lm = lod.mesh_nodes[j];
			total_bounds = bounds_union(total_bounds, lm.bounds);

			final_mod.submeshes.push_back(
				make_final_submesh_from_existing(
					lm.submesh,
					final_mod,
					compile,
					indirect_mats_out
				)
			);
		}
		out_lod.part_count = final_mod.submeshes.size()- out_lod.part_ofs;

		final_mod.lods.push_back(out_lod);
	}

	if (final_mod.lods.size() == 0) {
		sys_print("??? model has no lods to output, creating an empty default one\n");
		MeshLod loddefault;
		loddefault.part_count = 0;
		loddefault.part_ofs = 0;
		final_mod.lods.push_back(loddefault);
		total_bounds = Bounds(glm::vec3(-0.5), glm::vec3(0.5));
	}

	final_mod.AABB = total_bounds;

	return final_mod;
}


bool write_out_compilied_model(const std::string& path, const FinalModelData* model, const FinalSkeletonOutput* skel)
{
	FileWriter out;
	out.write_int32('CMDL');
	out.write_int32(MODEL_VERSION);

	glm::mat4 roottransform = glm::mat4(1.0);
	if (skel)
		roottransform = skel->armature_root_transform;
	out.write_struct(&roottransform);
	
	out.write_struct(&model->AABB);

	out.write_int32(model->lods.size());
	for (int i = 0; i < model->lods.size(); i++)
		out.write_struct(&model->lods[i]);
	out.write_int32(model->submeshes.size());
	for (int i = 0; i < model->submeshes.size(); i++)
		out.write_struct(&model->submeshes[i]);
	out.write_int32('HELP');
	out.write_int32(model->material_names.size());
	for (int i = 0; i < model->material_names.size(); i++)
		out.write_string(model->material_names[i]);
	out.write_int32(model->tags.size());
	for (int i = 0; i < model->tags.size(); i++) {
		out.write_string(model->tags[i].name);
		out.write_struct(&model->tags[i].transform);
		out.write_int32(model->tags[i].bone_index);
	}


	size_t marker = out.tell();
	out.write_int32(model->indicies.size());
	out.write_bytes_ptr((uint8_t*)model->indicies.data(), model->indicies.size()*sizeof(uint16_t));
	size_t index_size = out.tell() - marker;


	marker = out.tell();
	sys_print("*** MARKER: %d\n", (int)marker);
	out.write_int32(model->verticies.size());
	out.write_bytes_ptr((uint8_t*)model->verticies.data(), model->verticies.size() * sizeof(ModelVertex));
	size_t vert_size = out.tell()- marker;

	out.write_int32('HELP');

	size_t skel_size = 0;
	size_t animation_size = 0;

	if (!skel)
		out.write_int32(0);
	else {

		marker = out.tell();
		out.write_int32(skel->bones.size());
		for (int i = 0; i < skel->bones.size(); i++) {
			out.write_string(skel->bones[i].strname);
			out.write_int32(skel->bones[i].parent);
			out.write_int32((int)skel->bones[i].retarget_type);
			out.write_struct(&skel->bones[i].posematrix);
			out.write_struct(&skel->bones[i].invposematrix);
			out.write_struct(&skel->bones[i].localtransform);
			out.write_struct(&skel->bones[i].rot);
		}
		skel_size = out.tell()- marker;

		marker = out.tell();
		out.write_int32(skel->allseqs.size());	
		for (const auto& seq : skel->allseqs) {
			out.write_int32('HELP');

			out.write_string(seq.first);
			out.write_float(seq.second.duration);
			out.write_float(seq.second.average_linear_velocity);
			out.write_int32(seq.second.num_frames);
			out.write_byte(seq.second.is_additive_clip);
			
			assert(seq.second.channel_offsets.size() == skel->bones.size());
			out.write_bytes_ptr(
				(uint8_t*)seq.second.channel_offsets.data(),
				seq.second.channel_offsets.size() * sizeof(ChannelOffset));

			out.write_int32(seq.second.pose_data.size());
			out.write_bytes_ptr(
				(uint8_t*)seq.second.pose_data.data(), 
				seq.second.pose_data.size() * sizeof(float));

			assert(seq.second.event_keyframes.size() == seq.second.num_frames + 1);
			out.write_bytes_ptr(
				(uint8_t*)seq.second.event_keyframes.data(),
				seq.second.event_keyframes.size() * sizeof(EventIndex)
			);

			out.write_int32(seq.second.events.size());
			for (int i = 0; i < seq.second.events.size(); i++)
				out.write_string(seq.second.events[i].str);
		}
		animation_size = out.tell()-marker;

		out.write_int32(skel->imported_models.size());
		for (int i = 0; i < skel->imported_models.size(); i++)
			out.write_string(skel->imported_models[i]);


		out.write_byte(skel->mirror_table.size() == skel->bones.size());
		if (skel->mirror_table.size() == skel->bones.size()) {
			out.write_bytes_ptr(
				(uint8_t*)skel->mirror_table.data(),
				skel->bones.size() * sizeof(int16_t)
			);
		}

		out.write_int32(skel->masks.size());
		for (int i = 0; i < skel->masks.size(); i++) {
			out.write_string(skel->masks[i].strname);
			assert(skel->masks[i].weight.size() == skel->bones.size());
			out.write_bytes_ptr((uint8_t*)skel->masks[i].weight.data(), skel->bones.size() * sizeof(float));
		}

		out.write_int32('E');
	}


	const std::string fullpath = path;// modelpath_to_fullpath(path);
	std::ofstream outfile(fullpath, std::ios::binary);
	if (!outfile) {
		sys_print("!!! Couldn't open file to write out model %s\n", path.c_str());
		return false;
	}
	sys_print("*** Writing out model (%s) (size: %d)\n", path.c_str(), (int)out.get_size());
	sys_print("***     -vert bytes: %d\n", (int)vert_size);
	sys_print("***     -index bytes: %d\n", (int)index_size);
	sys_print("***     -bone bytes: %d\n", (int)skel_size);
	sys_print("***     -anim bytes: %d\n", (int)animation_size);




	outfile.write(out.get_buffer(), out.get_size());
	outfile.close();

	return true;
}

void add_bone_def_data_to_skeleton(const ModelDefData& def, SkeletonCompileData* skel)
{
	for (int i = 0; i < skel->get_num_bones(); i++) {
		auto name = skel->bones[i].strname;
		if (def.bone_retarget_type.find(name) != def.bone_retarget_type.end())
			skel->bones[i].retarget_type = def.bone_retarget_type.find(name)->second;
		else
			skel->bones[i].retarget_type = RetargetBoneType::FromTargetBindPose;
	}
}

bool ModelCompileHelper::compile_model(const std::string& defname, const ModelDefData& def)
{
	cgltf_and_binary out = load_cgltf_data(modelpath_to_fullpath(def.model_source));

	if (!out.data)
		return false;

	unique_ptr<SkeletonCompileData> skeleton_data = get_skin_from_file(out.data, defname.c_str(), def.armature_name);
	if(skeleton_data)
		add_bone_def_data_to_skeleton(def, skeleton_data.get());

	const ProcessNodesAndMeshOutput post_traverse = process_nodes_and_mesh(
		out.data,
		skeleton_data.get(),
		(skeleton_data) ? skeleton_data->using_skin : nullptr,
		def);

	const std::vector<std::string> final_material_names = create_final_material_names(
		defname,
		post_traverse.mcd, 
		def, 
		post_traverse.meshout.material_is_used
	);

	unique_ptr<FinalSkeletonOutput> final_skeleton = ModelCompileHelper::create_final_skeleton(
		post_traverse.meshout.LOAD_bone_to_FINAL_bone, 
		post_traverse.meshout.FINAL_bone_to_LOAD_bone,
		skeleton_data.get(), 
		def
	);

	const FinalModelData final_model = create_final_model_data(
		final_material_names, 
		post_traverse.meshout.material_is_used, 
		post_traverse.mcd, 
		post_traverse.meshout.LOAD_bone_to_FINAL_bone,
		def);

	std::string finalpath = strip_extension(defname);
	finalpath += ".cmdl";

	bool res =  write_out_compilied_model(finalpath, &final_model, final_skeleton.get());
	out.free();
	return res;
}

static bool compile_everything = false;

bool ModelCompilier::compile(const char* name)
{
	sys_print("----- Compiling Model %s -----\n", name);

	auto file = FileSys::open_read_os(name);
	if (!file) {
		sys_print("!!! coudln't open model def file %s\n", name);
		return false;
	}
	uint64_t timestamp_of_def = file->get_timestamp();
	file.reset();	// close it

	uint64_t timestamp_of_cmdl = 0;

	std::string compilied = strip_extension(name) + ".cmdl";
	file = FileSys::open_read_os(compilied.c_str());


	bool needs_compile = compile_everything;
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
			}
			else if (version != MODEL_VERSION) {
				sys_print("*** .cmdl version out of data (found %d, current %d), recompiling\n", version, MODEL_VERSION);
				needs_compile = true;
			}
		}
	}
	file.reset();

	if (!needs_compile && timestamp_of_cmdl <= timestamp_of_def) {
		sys_print("*** .def newer than .cmdl, recompiling\n");
		needs_compile = true;
	}

	
	ModelDefData def_data;
	try {
		def_data = ModelCompileHelper::parse_definition_file(name);
	}
	catch (std::runtime_error er) {
		sys_print("!!! error parsing compile file %s: %s\n", name, er.what());
		return false;
	}


	// check dependencies
	auto check_timestamp_file = [](uint64_t cmdl_time, const std::string& full_path) -> bool {

		auto input_file = FileSys::open_read_os(full_path.c_str());
		if (!input_file)
			return false;	// no file = doesnt need update

		uint64_t time = input_file->get_timestamp();
		return time >= cmdl_time;
	};
	auto check_timestamp_folder = [&](uint64_t cmdl_time, const std::string& folder) -> bool {

		auto tree = FileSys::find_files(folder.c_str());

		for (const auto& file : tree) {
			if (get_extension(file) != ".glb")
				continue;
			bool res = check_timestamp_file(cmdl_time, file);
			if (res)
				return true;
		}
		return false;
	};

	bool needs_compile_before_src = needs_compile;

	if (!needs_compile)
		needs_compile |= check_timestamp_file(timestamp_of_cmdl, modelpath_to_fullpath(def_data.model_source));
	for (int i = 0; i < def_data.imports.size() && !needs_compile; i++) {
		if(def_data.imports[i].type == AnimImportType_Load::File)
			needs_compile |= check_timestamp_file(timestamp_of_cmdl, modelpath_to_fullpath(def_data.imports[i].name));
		else if (def_data.imports[i].type == AnimImportType_Load::Folder)
			needs_compile |= check_timestamp_folder(timestamp_of_cmdl, modelpath_to_fullpath(def_data.imports[i].name));
	}
	if (!needs_compile_before_src && needs_compile)
		sys_print("*** source files out of data, recompiling\n");

	if (!needs_compile) {
		sys_print("*** skipping compile\n");
		return true;	// no need to compile
	}

	return ModelCompileHelper::compile_model(name, def_data);
}