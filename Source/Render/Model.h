#ifndef MODEL_H
#define MODEL_H
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>
#include "glm/gtc/quaternion.hpp"
#include "Framework/Util.h"
#include "DrawTypedefs.h"
#include "Framework/StringName.h"
#include "Framework/InlineVec.h"
#include "Assets/IAsset.h"
#include "Framework/MathLib.h"
#include "Framework/MulticastDelegate.h"
#include "Framework/Reflection2.h"
class MaterialInstance;
using std::string;
using std::vector;
using std::unique_ptr;
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

class RawMeshData
{
public:
	int get_num_indicies(int index_size) const {
		return (int)indicies.size();
	}
	int get_num_verticies(int vertex_size) const {
		return (int)verts.size();
	}
	int get_num_vertex_bytes() const {
		return (int)verts.size()*sizeof(ModelVertex);
	}
	int get_num_index_bytes() const {
		return (int)indicies.size()*sizeof(uint16_t);
	}
	const uint8_t* get_index_data(size_t* size) const {
		*size = indicies.size() * sizeof(uint16_t);
		return (uint8_t*)indicies.data();
	}
	const uint8_t* get_vertex_data(size_t* size) const {
		*size = verts.size() * sizeof(ModelVertex);
		return (uint8_t*)verts.data();
	}
	const ModelVertex& get_vertex_at_index(int index) const {
		return verts[index];
	}
	uint16_t get_index_at_index(int index) const {
		return indicies[index];
	}

private:
	// index offset always = 0
	std::vector<ModelVertex> verts;
	std::vector<uint16_t> indicies;
	friend class Model;
	friend class ModelMan;
};

class Submesh {
public:
	int base_vertex = 0;	// index
	int element_offset = 0;	// in bytes
	int element_count = 0;	// in element size
	int material_idx = 0;
	int vertex_count = 0;	// in element size
};

struct ModelTag {
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
class PhysicsBodyDefinition;
class IAssetLoadingInterface;
class Model : public IAsset {
public:
	CLASS_BODY(Model);
	Model();
	~Model() override;

	static Model* load(const std::string& path);

	void uninstall() override;
	void sweep_references(IAssetLoadingInterface* loading) const override;
	void post_load() override;
	bool load_asset(IAssetLoadingInterface* loading) override;
	void move_construct(IAsset* src) override;
	bool check_import_files_for_out_of_data() const override;
	int get_uid() const { return uid; }
	int bone_for_name(StringName name) const;
	const glm::vec4& get_bounding_sphere() const { return bounding_sphere; }
	Bounds get_aabb_from_sphere() const { 
		return Bounds(glm::vec3(bounding_sphere) - glm::vec3(bounding_sphere.w),
			glm::vec3(bounding_sphere) + glm::vec3(bounding_sphere.w));
	}
	MSkeleton* get_skel() const { return skel.get(); }
	const Submesh& get_part(int index) const { return parts[index]; }
	int get_num_lods() const { return lods.size(); }
	const MeshLod& get_lod(int index) const { return lods[index]; }
	const MaterialInstance* get_material(int index) const { return materials[index]; }
	int get_num_materials() const { return materials.size(); }
	glm::ivec2 get_lightmap_size() const { return glm::ivec2(lightmapX,lightmapY); }

	bool has_lightmap_coords() const;
	bool has_colors() const;
	bool has_bones() const;
	bool has_tangents() const;
	uint32_t get_merged_index_ptr() const { return merged_index_pointer; }
	uint32_t get_merged_vertex_ofs() const { return merged_vert_offset; }
	const glm::mat4& get_root_transform() const { return skeleton_root_transform; }
	const Bounds& get_bounds() const { return aabb; }
	const PhysicsBodyDefinition* get_physics_body() const { return collision.get(); }
	const RawMeshData* get_raw_mesh_data() const { return &data; }
	
	static MulticastDelegate<Model*> on_model_loaded;
private:
	bool load_internal(IAssetLoadingInterface* loading);

	int uid = 0;
	InlineVec<MeshLod, 2> lods;
	vector<Submesh> parts;
	Bounds aabb;
	glm::vec4 bounding_sphere=glm::vec4(0.f);
	uint32_t merged_index_pointer = 0;	// in bytes
	uint32_t merged_vert_offset = 0;
	RawMeshData data;
	// skeleton + animation data
	unique_ptr<MSkeleton> skel;
	// collision geometry, if null, then the aabb of the model will be used if the object is used as collision
	unique_ptr<PhysicsBodyDefinition> collision;
	vector<ModelTag> tags;
	vector<const MaterialInstance*> materials;
	glm::mat4 skeleton_root_transform = glm::mat4(1.f);
	bool isLightmapped = false;
	int16_t lightmapX = 0;
	int16_t lightmapY = 0;

	friend class ModelMan;
	friend class ModelCompileHelper;
	friend class ModelEditorTool;
	friend class ModelLoadJob;
};

#endif // !MODEL_H
