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

	int bone_for_name(const char* name) const;
};

// not sure why i seperated this but :/
class Prefab_Model
{
public:
	struct Node {
		string name;
		glm::mat4 transform;
		int mesh_idx = -1;
	};
	string name;
	vector<Mesh> meshes;
	vector<Node> nodes;
	vector<Material*> mats;
	unique_ptr<Physics_Mesh> physics;
};

class cgltf_data;
class cgltf_node;
class Game_Mod_Manager
{
public:

	typedef void(*prefab_callback)(void* user, cgltf_data* data, cgltf_node* node, glm::mat4 globaltransform);

	void init();
	Model* find_or_load(const char* filename);
	Prefab_Model* find_or_load_prefab(const char* filename, bool dont_append_path,
		prefab_callback callback, void* userdata);
	void free_prefab(Prefab_Model* prefab);
	void free_model(Model* m);
	void compact_memory();
	void print_usage();



	struct Gpu_Buffer {
		bufferhandle handle = 0;
		uint32_t allocated = 0;
		uint32_t target = 0;
		uint32_t used = 0;
	};

	struct Shared_Vertex_Buffer {
		Gpu_Buffer attributes[MAX_MESH_ATTRIBUTES];
		uint32_t main_vao = 0;
	};

	std::unordered_map<string, Prefab_Model*> prefabs;
	std::unordered_map<string, Model*> models;

	vertexarrayhandle depth_animated_vao = 0;
	vertexarrayhandle depth_static_vao = 0;
	Gpu_Buffer global_index_buffer;
	Shared_Vertex_Buffer global_vertex_buffers[(int)mesh_format::COUNT];

private:
	uint32_t cur_mesh_id = 0;

	bool upload_mesh(Mesh* mesh);
	bool append_to_buffer(Gpu_Buffer& buf, char* input_data, uint32_t input_length);
};
extern Game_Mod_Manager mods;

void FreeLoadedModels();
Model* FindOrLoadModel(const char* filename);
void ReloadModel(Model* m);

#endif // !MODEL_H
