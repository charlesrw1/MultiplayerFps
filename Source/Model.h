#ifndef MODEL_H
#define MODEL_H
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <map>
#include <unordered_map>
#include "Framework/MathLib.h"
#include "glm/gtc/quaternion.hpp"
#include "Framework/Util.h"
#include "Animation/Runtime/Animation.h"
#include "Framework/BVH.h"
#include "DrawTypedefs.h"

// Hardcoded attribute locations for shaders
const int POSITION_LOC = 0;
const int UV_LOC = 1;
const int NORMAL_LOC = 2;
const int JOINT_LOC = 3;
const int WEIGHT_LOC = 4;
const int COLOR_LOC = 5;
const int UV2_LOC = 6;
const int TANGENT_LOC = 7;


enum Vertex_Attributes
{
	ATTRIBUTE_POS = 0,
	ATTRIBUTE_UV = 1,
	ATTRIBUTE_NORMAL = 2,
	ATTRIBUTE_TANGENT = 3,
	ATTRIBUTE_JOINT_OR_COLOR = 4,
	ATTRIBUTE_WEIGHT_OR_COLOR2 = 5,
	MAX_ATTRIBUTES,
};

struct ModelVertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	int16_t normal[3];
	int16_t tangent[3];
	uint8_t color[4];	// or bone index
	uint8_t color2[4];	// or bone weight
};
static_assert(sizeof(ModelVertex) == 40, "vertex size wrong");


class Material;



using std::string;
using std::vector;
using std::unique_ptr;

#define MAX_BONES 256

struct Bone
{
	string name;
	int parent;
	glm::mat4x3 posematrix;	// bone space -> mesh space
	glm::mat4x3 invposematrix; // mesh space -> bone space
	glm::mat4x3 localtransform;
	glm::quat rot;
};

class Skeleton2
{
public:

private:
	std::vector<Bone> bones;
	std::vector<uint8_t> mirror_map;
};



struct Physics_Triangle
{
	int indicies[3];
	glm::vec3 face_normal;
	float plane_offset = 0.f;
	int surf_type = 0;
};

struct Physics_Mesh
{
	std::vector<glm::vec3> verticies;
	std::vector<Physics_Triangle> tris;
	BVH bvh;

	void build();
};

struct Texture;


struct Raw_Mesh_Data
{
	Raw_Mesh_Data() {
		memset(attribute_offsets, 0, sizeof(attribute_offsets));
	}

	vector<char> buffers[MAX_ATTRIBUTES];
	vector<char> indicies;

	int attribute_offsets[MAX_ATTRIBUTES];
	int index_offset = 0;
	vector<uint8_t> data;
};

class Submesh
{
public:
	int base_vertex = 0;
	int element_offset = 0;
	int element_count = 0;
	int material_idx = 0;
};



enum class mesh_format
{
	SKINNED,
	STATIC,
	STATIC_PLUS,
	COUNT
};

class Mesh
{
public:
	uint32_t id = 0;
	vector<Submesh> parts;
	Raw_Mesh_Data data;
	Bounds aabb;
	int attributes = 0;	// bitmask of vertex attributes
	uint32_t merged_index_pointer = 0;	// in bytes
	uint32_t merged_vert_offset = 0;
	bool is_merged = false;
	mesh_format format = mesh_format::STATIC;
	vertexarrayhandle vao = 0;

	bool has_lightmap_coords() const;
	bool has_colors() const;
	bool has_bones() const;
	bool has_tangents() const;
	int format_as_int() {
		return (int)format;
	}
};

struct Model_Tag
{
	string name;
	int bone_index = -1;
	glm::mat4 transform;
};

struct Collision_Box
{
	string name;
	int bone_idx = -1;
	Bounds aabb;
};

class Mesh_Lod
{
	Mesh* mesh;
	float end_dist;
};
class Model
{
public:
	string name;
	Mesh mesh;
	// skeleton heirarchy
	vector<Bone> bones;
	glm::mat4 skeleton_root_transform = glm::mat4(1.f);
	int root_bone_index;
	// embedded ainmations
	unique_ptr<Animation_Set> animations;
	// tags for attachments etc.
	vector<Model_Tag> tags;
	vector<Material*> mats;

	unique_ptr<Physics_Mesh> collision;
	vector<Collision_Box> boxes;
	bool loaded_in_memory = false;

	int bone_for_name(const char* name) const;
};

#include <array>
class MainVbIbAllocator
{
public:
	
	void init(uint32_t num_indicies, uint32_t num_verts);

	struct buffer {
		bufferhandle handle = 0;
		uint32_t allocated = 0;
		uint32_t used = 0;
	};

	buffer vbuffer;
	buffer ibuffer;
};

class ModelMan
{
public:
	void init();
	Model* find_or_load(const char* filename);
	void free_model(Model* m);

	void compact_memory();
	void print_usage();

	vertexarrayhandle get_vao(bool animated) {
		if (animated)
			return animated_vao;
		else
			return static_vao;
	}

	Model* error_model = nullptr;

private:

	bool parse_model_into_memory(Model* m, std::string path);
	bool upload_model(Mesh* m);

	vertexarrayhandle static_vao;
	vertexarrayhandle animated_vao;
	MainVbIbAllocator allocator;

	uint32_t cur_mesh_id = 0;
	std::unordered_map<string, Model*> models;
};
extern ModelMan mods;

void FreeLoadedModels();
Model* FindOrLoadModel(const char* filename);
void ReloadModel(Model* m);

#endif // !MODEL_H
