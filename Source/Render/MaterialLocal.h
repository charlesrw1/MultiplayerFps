#pragma once


#include "Render/MaterialPublic.h"

#include <vector>
#include <unordered_set>

#include "Framework/Handle.h"
#include "Shader.h"
#include <unordered_map>

#include "Framework/Files.h"
#include "Game/SerializePtrHelpers.h"

const int MAX_INSTANCE_PARAMETERS = 8;	// 8 scalars/color32s
const uint32_t MATERIAL_SIZE = 64;	// 64 bytes
const uint32_t MAX_MATERIALS = 1024;
const uint32_t MAX_MAXTERIALS_BUFFER_SIZE = MATERIAL_SIZE * MAX_MATERIALS;

enum class LightingMode : uint8_t
{
	Lit,
	Unlit
};
enum class MaterialUsage : uint8_t
{
	Default,
	Postprocess,
	Terrain,
	Decal,
	UI,
	Particle,
};


enum class blend_state : uint8_t
{
	OPAQUE,
	BLEND,
	ADD
};

// Parameter types
enum class MatParamType : uint8_t
{
	Empty,
	FloatVec,	// float[4]
	Float,		// float
	Vector,		// uint8[4]
	Bool,		// uint8
	Texture2D,
	ConstTexture2D,
};

// Variant for material parameters
struct MaterialParameterValue
{
	MatParamType type = MatParamType::Empty;
	union {
		glm::vec4 vector;
		bool boolean;
		unsigned int color32;
		float scalar = 0.0;
		const Texture* tex_ptr;
	};
};

// Defines a modifiable property
struct MaterialParameterDefinition
{
	std::string name;
	StringName hashed_name;
	MaterialParameterValue default_value;
	// For textures: offset = texture index
	// Else: offset = byte offset in parameter SSBO buffer
	uint32_t offset = 0;
};

struct InstanceData
{
	std::string name;
	bool is_vector_type = false;	/* true = is scalar */
	uint32_t index = 0;
};

class MaterialBufferLocal : public MaterialParameterBuffer
{
public:
	std::vector<MaterialParameterDefinition> param_defs;
	std::vector<MaterialParameterValue> values;

	bufferhandle ubo_buffer = 0;
	uint32_t buffer_size = 0;
};


// compilied material, material instances can be based off it to allow for variation but minimize draw call changes
class MasterMaterialImpl
{
public:
	MasterMaterialImpl() {}

	// generated glsl fragment and vertex shader

	const MaterialParameterDefinition* find_definition(const std::string& str, int& index) const {
		for (int i = 0; i < param_defs.size(); i++)
			if (param_defs[i].name == str) {
				index = i;
				return &param_defs[i];
			}
		return nullptr;
	}

	MaterialInstance* self = nullptr;
	// All parameters that can be set by instances
	std::vector<MaterialParameterDefinition> param_defs;
	uint32_t num_texture_bindings = 0;
	struct UboBinding {
		MaterialParameterBuffer* buffer = nullptr;
		uint32_t binding_loc = 0;
	};
	std::vector<UboBinding> constant_buffers;

	bool is_translucent() const {
		return blend == blend_state::ADD || blend == blend_state::BLEND;
	}
	bool is_alphatested() const {
		return alpha_tested;
	}

	// Material state parameters
	bool alpha_tested = false;
	blend_state blend = blend_state::OPAQUE;
	LightingMode light_mode = LightingMode::Lit;
	MaterialUsage usage = MaterialUsage::Default;
	bool backface = false;

	// uses the shared depth material
	// this is true when nothing writes to worldPositionOffset and the material mode is not masked
	bool is_using_default_depth = false;

	uint32_t material_id = 0;

	bool load_from_file(const std::string& fullpath, IFile* file);

	std::string create_glsl_shader(
		std::string& vs_code,
		std::string& fs_code,
		const std::vector<InstanceData>& instdat
	);

	friend class MaterialManagerLocal;
	friend class MaterialLodJob;
};

class MaterialImpl
{
public:
	static const uint32_t INVALID_MAPPING = uint32_t(-1);
	MaterialImpl(
		bool is_dynamic_mat=false
	)  {
	
	}
	MaterialInstance* self = nullptr;

	bool is_this_currently_uploaded() const { return gpu_buffer_offset != INVALID_MAPPING; }

	bool is_dynamic_material = false;

	uint32_t unique_id = 0;	// unique id of this material instance (always valid)

	AssetPtr<MaterialInstance> parentMatInstance;
	const MasterMaterialImpl* masterMaterial = nullptr;	// this points to what master material this is instancing (valid on every material!)
	std::unique_ptr<MasterMaterialImpl> masterImpl;	// if this material instance is a default instance of a master material, this is filled

	const std::vector<const Texture*>& get_textures() const { return texture_bindings; }
	std::vector<const Texture*> texture_bindings;
	std::vector<MaterialParameterValue> params;
	uint32_t gpu_buffer_offset = INVALID_MAPPING;	// offset in buffer if uploaded (the buffer is uint's so byte = buffer_offset*4)

	int dirty_buffer_index = -1;	// if not -1, then its sitting in a queue already

	bool has_called_post_load_already = false;

	void init_from(const MaterialInstance* parent);
	bool load_from_file(MaterialInstance* self);

	bool load_instance(MaterialInstance* self, IFile* file);
	bool load_master(MaterialInstance* self, IFile* file);

	void post_load(MaterialInstance* self);

	friend class MaterialManagerLocal;
	friend class MaterialLodJob;
};


namespace gpu {
	struct Material_Data;
}


struct shader_key
{
	shader_key() {
		material_id = 0;
		depth_only = 0;
		animated = 0;
		editor_id = 0;
		dither = 0;
		debug = 0;
	}

	uint32_t material_id : 27;
	uint32_t animated : 1;
	uint32_t editor_id : 1;
	uint32_t depth_only : 1;
	uint32_t dither : 1;
	uint32_t debug : 1;

	uint32_t as_uint32() const {
		return *((uint32_t*)this);
	}
};
static_assert(sizeof(shader_key) == 4, "shader key needs 4 bytes");


class Material_Shader_Table
{
public:
	Material_Shader_Table();

	program_handle lookup(shader_key key);
	void insert(shader_key key, program_handle handle);
	void recompile_for_material(const MasterMaterialImpl* mat);

	std::unordered_map<uint32_t, program_handle> shader_key_to_program_handle;
};


class Model;
class MaterialManagerLocal : public MaterialManagerPublic
{
public:
	void init() override;

	// public interface
	void pre_render_update() override;	// material buffer updates

	MaterialInstance* create_dynmaic_material(const MaterialInstance* material) override { 
		assert(material);
		MaterialInstance* dynamicMat = new MaterialInstance();

		dynamicMat->impl = std::make_unique<MaterialImpl>();
		dynamicMat->impl->self = dynamicMat;
		dynamicMat->impl->init_from(material);
		dynamicMat->impl->is_dynamic_material = true;
		dynamicMat->impl->post_load(dynamicMat);	// add to dirty list, set material id

		dynamicMat->set_loaded_manually_unsafe("%_DM_%");

		dynamic_materials.insert(dynamicMat);

		return dynamicMat;
	}
	void free_dynamic_material(MaterialInstance*& mat) override {
		if (!mat) return;
		if (dynamic_materials.find((MaterialInstance*)mat) == dynamic_materials.end())
			Fatalf("free_dynamic_material used on a material not in dynamic_material list (double delete?)");
		MaterialInstance* mlocal = (MaterialInstance*)mat;
		dynamic_materials.erase(mlocal);
		if (mlocal->impl->dirty_buffer_index != -1)
			dirty_list.at(mlocal->impl->dirty_buffer_index) = nullptr;
		delete mlocal;
		mat = nullptr;	// set callers ptr to null
	}
	MaterialParameterBuffer* find_parameter_buffer(const char* name) override { return nullptr; }

	program_handle get_mat_shader(
		bool is_animated, 
		const Model* mod, 
		const MaterialInstance* gs, 
		bool depth_pass, 
		bool dither, 
		bool is_editor_mode,
		bool debug_mode
	);

	bufferhandle get_gpu_material_buffer() { return gpuMaterialBuffer; }
	void add_to_dirty_list(MaterialInstance* mat) {
		if (mat->impl->dirty_buffer_index == -1) {
			dirty_list.push_back(mat);
			mat->impl->dirty_buffer_index = dirty_list.size() - 1;
		}
	}
	void remove_from_dirty_list_if_it_is(MaterialInstance* mat) {
		if (mat->impl->dirty_buffer_index != -1) {
			ASSERT(mat->impl->dirty_buffer_index >= 0 && mat->impl->dirty_buffer_index < dirty_list.size());
			dirty_list[mat->impl->dirty_buffer_index] = nullptr;
		}
	}

	void free_material_instance(MaterialInstance* m)
	{
		remove_from_dirty_list_if_it_is(m);
		if (m->impl->gpu_buffer_offset != MaterialImpl::INVALID_MAPPING) {
			int byteIndex = (m->impl->gpu_buffer_offset * 4) / MATERIAL_SIZE;
			int bitmapIndex = byteIndex / 64;
			int bitIndex = byteIndex % 64;
			materialBitmapAllocator[bitmapIndex] &= ~(1ull << bitIndex);
		}

	}

	uint32_t get_next_master_id() {
		return ++current_master_id;
	}
	uint32_t get_next_instance_id() {
		return ++current_instance_id;
	}
	MaterialInstance* get_default_editor_sel_PP() {
		return PPeditorSelectMat;
	}
private:
	MaterialInstance* PPeditorSelectMat = nullptr;
	MaterialInstance* fallback_master = nullptr;
	MaterialInstance* shared_depth_master = nullptr;

	void on_reload_shader_invoke();

	program_handle compile_mat_shader(const MaterialInstance* mat, shader_key key);
	Material_Shader_Table mat_table;

	// global material buffer, all parameters get stuff in here and accessed by shaders
	bufferhandle gpuMaterialBuffer = 0;
	uint32_t materialBufferSize = 0;

	// bitmap allocator for materials
	std::vector<uint64_t> materialBitmapAllocator;

	std::unordered_set<MaterialInstance*> dynamic_materials;

	std::unordered_map<std::string, MaterialParameterBuffer*> parameter_buffers;

	// materials to allocate or update
	std::vector<MaterialInstance*> dirty_list;

	// returns INDEX, not POINTER
	uint32_t allocate_material_instance() {
		for (int i = 0; i < materialBitmapAllocator.size(); i++) {
			if (materialBitmapAllocator[i] == UINT64_MAX)
				continue;
			// find bit
			for (int bit = 0; bit < 64; bit++) {
				if ((materialBitmapAllocator[i] & (1ull << bit)) == 0) {
					materialBitmapAllocator[i] |= (1ull << bit);
					return i * 64 + bit;
				}
			}
			ASSERT(0);	// should have found a bit
		}
		Fatalf("allocate_material_instance: out of memory\n");

		return  0;
	}


	uint32_t current_master_id = 0;
	uint32_t current_instance_id = 0;
};

extern MaterialManagerLocal matman;