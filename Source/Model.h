#ifndef MODEL_H
#define MODEL_H
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <map>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Util.h"
#include "Animation.h"
#include "BVH.h"

using std::string;
using std::vector;

#define MAX_BONES 256

struct Bone
{
	int parent;
	int name_table_ofs;
	glm::mat4x3 posematrix;	// bone space -> mesh space
	glm::mat4x3 invposematrix; // mesh space -> bone space
	glm::quat rot;
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

	std::unique_ptr<Physics_Mesh> collision;

	int BoneForName(const char* name) const;
};

class Model_Manager
{
public:

	enum Attribute_Types {
		POS,
		UV0,
		NORMAL,
		INDICIES,
		WEIGHTS,
		UV1,
		COLOR,
		TANGENT,
		NUM_ATTRIBUTES
	};
	enum Vertex_Format_Types {
		POS_UV0_NORMAL,
		POS_UV0_NORMAL_BONES,
		POS_UV0_NORMAL_COLOR,
		POS_UV0_NORMAL_UV1,
		POS_UV0_NORMAL_UV1_COLOR,
		NUM_VERT_FORMATS,
	};
	enum Index_Formats {
		INT16,
		INT32,
		NUM_INDEX_FORMATS
	};
	struct Buffer {
		uint32_t size = 0;
		uint32_t used = 0;
		uint32_t id = 0;
		bool is_index_target = false;
		bool dirty = false;
		int stride = 0;

		void append_data(void* data, int input_stride, uint32_t input_len, int output_stride);
	};

	struct Per_Format {
		Buffer buffers[NUM_ATTRIBUTES];	// per attribute
		uint32_t vao_16;
		uint32_t vao_32;
	};
	Per_Format formats[NUM_VERT_FORMATS];
	Buffer index[NUM_INDEX_FORMATS];

	void check_vaos();

	std::vector<char> staging_buffer;
};

void FreeLoadedModels();
Model* FindOrLoadModel(const char* filename);
void ReloadModel(Model* m);

// So the level loader can have access
namespace tinygltf {
	class Model;
	class Mesh;
}
#endif // !MODEL_H
