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

using std::string;
using std::vector;

#define MAX_BONES 256

enum class VertexFormat
{
	Static,
	Skinned
};

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
	short layout = 0;
};
struct Texture;
struct MeshMaterial
{
	Texture* t1 = nullptr;
	Texture* t2 = nullptr;
};

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

	VertexFormat format=VertexFormat::Static;
	vector<MeshPart> parts;
	vector<GpuBuffer> buffers;
	vector<MeshMaterial> materials;

	int BoneForName(const char* name);
};

void FreeLoadedModels();
Model* FindOrLoadModel(const char* filename);
void ReloadModel(Model* m);

// So the level loader can have access
namespace tinygltf {
	class Model;
	class Mesh;
}
void AppendGltfMeshToModel(Model* model, tinygltf::Model& inputMod, const tinygltf::Mesh& mesh, std::map<int, int>& buffer_view_to_buffer);
#endif // !MODEL_H
