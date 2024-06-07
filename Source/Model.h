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
#include "Framework/BVH.h"
#include "DrawTypedefs.h"
#include "Framework/StringName.h"
#include "Framework/InlineVec.h"

// Hardcoded attribute locations for shaders
const int POSITION_LOC  = 0;
const int UV_LOC		= 1;
const int NORMAL_LOC    = 2;
const int TANGENT_LOC   = 3;
const int JOINT_LOC		= 4;
const int WEIGHT_LOC	= 5;

//const int COLOR_LOC = 5;
//const int UV2_LOC = 6;

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

class RawMeshData
{
public:
	int get_num_indicies(int index_size) const {
		return indicies.size();
	}
	int get_num_verticies(int vertex_size) const {
		return verts.size();
	}
	int get_num_vertex_bytes() const {
		return verts.size()*sizeof(ModelVertex);
	}
	int get_num_index_bytes() const {
		return indicies.size()*sizeof(uint16_t);
	}
	const uint8_t* get_index_data(size_t* size) const {
		*size = indicies.size() * sizeof(uint16_t);
		return (uint8_t*)indicies.data();
	}
	const uint8_t* get_vertex_data(size_t* size) const {
		*size = verts.size() * sizeof(ModelVertex);
		return (uint8_t*)verts.data();
	}
private:
	// index offset always = 0
	std::vector<ModelVertex> verts;
	std::vector<uint16_t> indicies;


	friend class Model;
	friend class ModelMan;
};

class Submesh
{
public:
	int base_vertex = 0;	// index
	int element_offset = 0;	// in bytes
	int element_count = 0;	// in element size
	int material_idx = 0;
	int vertex_count = 0;	// in element size
};

struct ModelTag
{
	string name;
	int bone_index = -1;
	glm::mat4 transform;
};


struct MeshLod
{
	float end_percentage = -1.0;
	int part_ofs = 0;
	int part_count = 0;
};

class MSkeleton;
class PhysicsBody;
class Model
{
public:
	uint32_t get_uid() const { return uid; }
	const string& get_name() const { return name; }
	int bone_for_name(StringName name) const;
	const glm::vec4& get_bounding_sphere() const { return bounding_sphere; }
	Bounds get_aabb_from_sphere() const { 
		return Bounds(glm::vec3(bounding_sphere) - glm::vec3(bounding_sphere.w),
			glm::vec3(bounding_sphere) + glm::vec3(bounding_sphere.w));
	}

	const MSkeleton* get_skel() const { return skel.get(); }

	const Submesh& get_part(int index) const { return parts[index]; }
	int get_num_lods() const { return lods.size(); }
	const MeshLod& get_lod(int index) const { return lods[index]; }

	Material* get_material(int index) const { return materials[index]; }

	bool has_lightmap_coords() const;
	bool has_colors() const;
	bool has_bones() const;
	bool has_tangents() const;

	uint32_t get_merged_index_ptr() const { return merged_index_pointer; }
	uint32_t get_merged_vertex_ofs() const { return merged_vert_offset; }

	bool is_loaded_in_memory() const { return loaded_in_memory; }

	const glm::mat4& get_root_transform() const { return skeleton_root_transform; }

	const Bounds& get_bounds() const { return aabb; }

	const PhysicsBody* get_physics_body() const {
		return collision.get();
	}
private:
	uint32_t uid = 0;
	string name;
	InlineVec<MeshLod, 2> lods;
	vector<Submesh> parts;

	Bounds aabb;
	glm::vec4 bounding_sphere;

	uint32_t merged_index_pointer = 0;	// in bytes
	uint32_t merged_vert_offset = 0;
	RawMeshData data;

	// skeleton + animation data
	unique_ptr<MSkeleton> skel;

	// collision geometry, if null, then the aabb of the model will be used if the object is used as collision
	unique_ptr<PhysicsBody> collision;

	vector<ModelTag> tags;
	vector<Material*> materials;

	glm::mat4 skeleton_root_transform = glm::mat4(1.f);
	bool loaded_in_memory = false;

	friend class ModelMan;
	friend class ModelCompileHelper;
};

#include <array>
class MainVbIbAllocator
{
public:
	
	void init(uint32_t num_indicies, uint32_t num_verts);
	void print_usage() const;

	void append_to_v_buffer(const uint8_t* data, size_t size);
	void append_to_i_buffer(const uint8_t* data, size_t size);


	struct buffer {
		bufferhandle handle = 0;
		uint32_t allocated = 0;
		uint32_t used = 0;
	};

	buffer vbuffer;
	buffer ibuffer;

private:
	void append_buf_shared(const uint8_t* data, size_t size, const char* name, buffer& buf, uint32_t target);
};

class ModelMan
{
public:
	void init();
	Model* find_or_load(const char* filename);

	void compact_memory();
	void print_usage() const;

	vertexarrayhandle get_vao(bool animated) {
		return animated_vao;
	}

	int get_index_type_size() const;

	Model* get_error_model() { return error_model; }
	Model* get_sprite_model() { return _sprite; }
private:
	Model* error_model = nullptr;
	Model* _sprite = nullptr;

	void create_default_models();

	void set_v_attributes();
	bool read_model_into_memory(Model* m, std::string path);
	bool upload_model(Model* m);

	vertexarrayhandle animated_vao;
	//vertexarrayhandle static_vao;
	MainVbIbAllocator allocator;

	uint32_t cur_mesh_id = 0;
	std::unordered_map<string, Model*> models;
};
extern ModelMan mods;

#endif // !MODEL_H
