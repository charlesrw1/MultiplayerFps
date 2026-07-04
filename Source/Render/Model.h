#ifndef MODEL_H
#define MODEL_H
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>
#include "DynamicModelPtr.h"
#include "glm/gtc/quaternion.hpp"
#include "Framework/Util.h"
#include "DrawTypedefs.h"
#include "Framework/StringName.h"
#include "Framework/InlineVec.h"
#include "Assets/IAsset.h"
#include "Framework/MathLib.h"
#include "Framework/MulticastDelegate.h"
#include "Framework/Reflection2.h"
#include "GpuAllocator.h"
class MaterialInstance;
using std::string;
using std::unique_ptr;
using std::vector;
// Hardcoded attribute locations for shaders
const int POSITION_LOC = 0;
const int UV_LOC = 1;
const int NORMAL_LOC = 2;
const int TANGENT_LOC = 3;
const int JOINT_LOC = 4;
const int WEIGHT_OR_COLOR_LOC = 5;
const int LIGHTMAPCOORD_LOC = 4;

// const int COLOR_LOC = 5;
// const int UV2_LOC = 6;

struct ModelVertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	int16_t normal[3];
	int16_t tangent[3];
	uint8_t color[4];  // or bone index (or lightmap coord, normalized uint16[2])
	uint8_t color2[4]; // or bone weight
};
static_assert(sizeof(ModelVertex) == 40, "vertex size wrong");

class RawMeshData
{
public:
	int get_num_indicies(int index_size) const { return (int)indicies.size(); }
	int get_num_verticies(int vertex_size) const { return (int)verts.size(); }
	int get_num_vertex_bytes() const { return (int)verts.size() * sizeof(ModelVertex); }
	int get_num_index_bytes() const { return (int)indicies.size() * sizeof(uint16_t); }
	const uint8_t* get_index_data(size_t* size) const {
		*size = indicies.size() * sizeof(uint16_t);
		return (uint8_t*)indicies.data();
	}
	const uint8_t* get_vertex_data(size_t* size) const {
		*size = verts.size() * sizeof(ModelVertex);
		return (uint8_t*)verts.data();
	}
	const ModelVertex& get_vertex_at_index(int index) const { return verts[index]; }
	uint16_t get_index_at_index(int index) const { return indicies[index]; }

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
	int element_offset = 0; // in bytes
	int element_count = 0;	// in element size
	int material_idx = 0;
	int vertex_count = 0; // in element size

	bool is_material_transparent() const { return material_idx & (1 << 30); }
	int get_material_idx_to_use() const { return material_idx & ~(1 << 30); }
	void set_material_transparent() { material_idx |= (1 << 30); }
};

/// Accumulates vertices and indexed triangles for use with
/// ModelMan::create_dynamic_model() / refresh_dynamic_model().
/// Vertex indices are 16-bit, so at most 65535 unique vertices per builder.
class ModelBuilder {
public:
    /// Adds a vertex with position, UV, and surface normal.
    /// The normal is normalized and packed as int16 (same encoding used by
    /// loaded .cmdl models).  Returns the 0-based vertex index; pass this
    /// value to add_triangle() or add_quad().
    uint16_t add_vertex(glm::vec3 pos,
                        glm::vec2 uv     = {0.f, 0.f},
                        glm::vec3 normal = {0.f, 1.f, 0.f});

    /// Records three previously-added vertex indices as a CCW-wound triangle.
    void add_triangle(uint16_t a, uint16_t b, uint16_t c);

    /// Convenience wrapper: splits a quad into two CCW triangles
    /// (a,b,c) and (a,c,d).
    void add_quad(uint16_t a, uint16_t b, uint16_t c, uint16_t d);

    /// Begin a new submesh using mat (nullptr = engine fallback material).
    /// All subsequent add_triangle/add_quad calls belong to this submesh.
    /// If never called, all geometry forms a single submesh with the fallback.
    void begin_submesh(std::shared_ptr<MaterialInstance> mat = nullptr);

    int get_vertex_count()  const { return (int)vertices.size(); }
    int get_index_count()   const { return (int)indices.size();  }
    int get_submesh_count() const { return submesh_entries.empty() ? 1 : (int)submesh_entries.size(); }

    /// Per-submesh descriptor recorded by begin_submesh().
    struct SubMeshEntry {
        std::shared_ptr<MaterialInstance> material; // null → engine fallback
        int index_start = 0; // first index in builder.indices for this submesh
    };

private:
    std::vector<ModelVertex>  vertices;
    std::vector<uint16_t>     indices;
    std::vector<SubMeshEntry> submesh_entries;
    friend class ModelMan;
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
extern glm::vec4 bounds_to_sphere(Bounds b);

class PhysicsMaterialWrapper;
class MSkeleton;
class PhysicsBodyDefinition;
class Model : public IAsset
{
public:
	CLASS_BODY(Model);
	Model();
	~Model() override;

	REF static Model* load(const std::string& path);
	// static Model* load(const std::string& path);

	void uninstall() override;
	void post_load() override;
	bool load_asset() override;

	int get_uid() const { return uid; }
	int bone_for_name(StringName name) const;
	const glm::vec4& get_bounding_sphere() const { return bounding_sphere; }
	Bounds get_aabb_from_sphere() const {
		return Bounds(glm::vec3(bounding_sphere) - glm::vec3(bounding_sphere.w),
					  glm::vec3(bounding_sphere) + glm::vec3(bounding_sphere.w));
	}
	MSkeleton* get_skel() const { return skel.get(); }
	const Submesh& get_part(int index) const { return parts[index]; }
	int get_num_parts() const { return parts.size(); }

	int get_num_lods() const { return lods.size(); }
	const MeshLod& get_lod(int index) const { return lods[index]; }
	const MaterialInstance* get_material(int index) const { return materials[index].get(); }
	const MaterialInstance* get_material_for_part(const Submesh& mesh) const {
		return get_material(mesh.get_material_idx_to_use());
	}

	int get_num_materials() const { return materials.size(); }
	glm::ivec2 get_lightmap_size() const { return glm::ivec2(lightmapX, lightmapY); }

	bool has_lightmap_coords() const;
	bool has_bones() const;

	int get_merged_vertex_ofs() const { return vertex_alloc_ptr.aligned_start / sizeof(ModelVertex); }
	int get_merged_index_ptr() const { return index_alloc_ptr.aligned_start; }

	const glm::mat4& get_root_transform() const { return skeleton_root_transform; }
	const Bounds& get_bounds() const { return aabb; }
	const PhysicsBodyDefinition* get_physics_body() const { return collision.get(); }
	const RawMeshData* get_raw_mesh_data() const { return &data; }
	static MulticastDelegate<Model*> on_model_loaded;
	enum class LightmapType
	{
		None = 0,		 // no lightmaps
		Lightmapped = 1, // individual lightmaps
		WorldMerged = 2	 // all WorldMerged get turned into one lightmap. prevents seams on floors/walls stuff
	};
	LightmapType get_lightmap_type() const { return isLightmapped; }
	bool get_has_any_transparent_materials() const { return has_any_transparent_materials; }

	REF void set_physics_material(PhysicsMaterialWrapper* mat) { this->physics_material = mat; }
	REF PhysicsMaterialWrapper* get_physics_material() const { return this->physics_material; }
	// can return null
	// if the model has a physics material, returns that
	// otherwise checks the 1st render material for a physics material
	REF PhysicsMaterialWrapper* get_physics_material_to_use() const;

	/// Returns true if this model was created via ModelMan::create_dynamic_model()
	/// rather than loaded from disk.  Dynamic models must be freed with
	/// ModelMan::free_dynamic_model() or held via DynamicModelUniquePtr.
	bool is_dynamic() const { return is_dynamic_model; }

private:
	bool load_internal();

	int uid = 0;
	InlineVec<MeshLod, 2> lods;
	vector<Submesh> parts;
	Bounds aabb;
	glm::vec4 bounding_sphere = glm::vec4(0.f);

	gpuAllocSpan index_alloc_ptr;
	gpuAllocSpan vertex_alloc_ptr;

	RawMeshData data;
	// skeleton + animation data
	unique_ptr<MSkeleton> skel;
	// collision geometry, if null, then the aabb of the model will be used if the object is used as collision
	unique_ptr<PhysicsBodyDefinition> collision;
	vector<ModelTag> tags;
	vector<std::shared_ptr<MaterialInstance>> materials;
	glm::mat4 skeleton_root_transform = glm::mat4(1.f);
	LightmapType isLightmapped = LightmapType::None;
	int16_t lightmapX = 0;
	int16_t lightmapY = 0;
	bool has_any_transparent_materials = false;

	PhysicsMaterialWrapper* physics_material = nullptr;

	/// True for models created via ModelMan::create_dynamic_model().
	/// Checked by DynamicModelDeleter and free_dynamic_model() to guard
	/// against accidentally freeing asset-system models.
	bool is_dynamic_model = false;

	// Latched true the first time post_load runs successfully.  Gates the
	// scene-walk-refresh in post_load: fires only on reload, not initial load.
	bool first_post_load_done = false;

	friend class ModelMan;
	friend class ModelCompileHelper;
	friend class ModelEditorTool;
	friend class ModelLoadJob;
};

#endif // !MODEL_H
