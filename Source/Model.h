#ifndef MODEL_H
#define MODEL_H
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <map>
#include <unordered_map>
#include "MathLib.h"
#include "glm/gtc/quaternion.hpp"
#include "Util.h"
#include "Animation.h"
#include "BVH.h"
using std::string;
using std::vector;
using std::unique_ptr;

#define MAX_BONES 256

struct Bone
{
	int parent;
	int name_table_ofs;
	glm::mat4x3 posematrix;	// bone space -> mesh space
	glm::mat4x3 invposematrix; // mesh space -> bone space
	glm::quat rot;
	string name;
};

struct MeshPart
{
	// TODO: batch multiple buffers together, for this project its fine though, makes loading easier
	uint32_t vao = 0;
	int base_vertex = 0;
	int element_offset = 0;	// in bytes
	int element_count = 0;
	int element_type = 0;	// unsigned_short, unsigned_int
	short material_idx = 0;
	short attributes = 0;

	bool has_lightmap_coords() const;
	bool has_colors() const;
	bool has_bones() const;
	bool has_tangents() const;
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
class Game_Shader;
struct ModelAttachment
{
	int str_table_start = 0;
	int bone_parent = 0;
	glm::mat4x3 transform;
};
struct ModelHitbox
{
	int str_table_start = 0;
	int bone_parent = 0;
	glm::vec3 size;
};


#define MAX_MESH_ATTRIBUTES 8

struct Raw_Mesh_Data
{
	vector<char> buffers[MAX_MESH_ATTRIBUTES];
	vector<char> indicies;
};

class Submesh
{
public:
	int base_vertex = 0;
	int element_offset = 0;
	int element_count = 0;
	int material_idx = 0;
};

class Mesh
{
public:
	vector<Submesh> parts;
	Raw_Mesh_Data data;
	Bounds aabb;
	int attributes = 0;	// bitmask of vertex attributes
	uint32_t merged_index_pointer = 0;	// in bytes
	uint32_t merged_vert_offset = 0;
	bool is_merged = false;
	int format = 0;
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

class Model2
{
public:
	string name;
	Mesh mesh;
	// skeleton heirarchy
	vector<Bone> bones;
	// embedded ainmations
	unique_ptr<Animation_Set> animations;
	// tags for attachments etc.
	vector<Model_Tag> tags;
	vector<Game_Shader*> mats;

	unique_ptr<Physics_Mesh> collision;
	vector<Collision_Box> boxes;

	int bone_for_name(const char* name) const;
};

class Model
{
public:
	struct GpuBuffer
	{
		uint32_t target = 0;	// element vs vertex
		uint32_t handle = 0;
		uint32_t size = 0;
		void Bind();
	};

	string name;
	vector<Bone> bones;
	vector<char> bone_string_table;
	std::unique_ptr<Animation_Set> animations;
	vector<ModelHitbox> hitboxes;
	vector<ModelAttachment> attachments;
	vector<MeshPart> parts;
	vector<GpuBuffer> buffers;
	vector<Game_Shader*> materials;
	Raw_Mesh_Data data;
	Bounds aabb;
	int attributes = 0;

	bool is_merged_into_gpu = false;
	uint32_t merged_vertex_offset = 0;
	uint32_t merged_index_pointer = 0;	// in bytes

	std::unique_ptr<Physics_Mesh> collision;

	int BoneForName(const char* name) const;
};

class Prefab_Model
{
public:
	struct Node {
		string name;
		int mesh_idx = -1;
		int transform_idx = -1;
		int parent_idx = -1;
	};
	vector<Mesh> meshes;
	vector<glm::mat4> transforms;
	vector<Node> nodes;
	vector<Game_Shader*> mats;
};

class cgltf_data;
class cgltf_node;
class Game_Mod_Manager
{
public:

	typedef void(*prefab_callback)(cgltf_data* data, cgltf_node* node, glm::mat4 globaltransform);

	void init();
	Model2* find_or_load(const char* filename);
	Prefab_Model* find_or_load_prefab(const char* filename, 
		prefab_callback callback);
	void free_prefab(Prefab_Model* prefab);
	void free_model(Model2* m);
	void compact_geometry();

	struct Gpu_Buffer {
		uint32_t handle = 0;
		uint32_t allocated = 0;
		uint32_t target = 0;
		uint32_t used = 0;
	};

	struct Shared_Vertex_Buffer {
		Gpu_Buffer attributes[MAX_MESH_ATTRIBUTES];
		uint32_t main_vao = 0;
	};

	enum Formats {
		FMT_SKINNED,	
		FMT_STATIC,
		FMT_STATIC_PLUS,
		NUM_FMT
	};

	std::unordered_map<string, Prefab_Model*> prefabs;
	std::unordered_map<string, Model2*> models;

	uint32_t depth_animated_vao = 0;
	uint32_t depth_static_vao = 0;
	Gpu_Buffer global_index_buffer;
	Shared_Vertex_Buffer global_vertex_buffers[NUM_FMT];

private:
	bool upload_mesh(Mesh* mesh);
	bool append_to_buffer(Gpu_Buffer& buf, char* input_data, uint32_t input_length);
};

void FreeLoadedModels();
Model* FindOrLoadModel(const char* filename);
void ReloadModel(Model* m);

#endif // !MODEL_H
