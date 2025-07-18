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
const int MATERIAL_SIZE = 64;	// 64 bytes
const int MAX_MATERIALS = 1024;
const int MAX_MAXTERIALS_BUFFER_SIZE = MATERIAL_SIZE * MAX_MATERIALS;

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
	ADD,
	MULT,

	// just for fun testing
	SCREEN,
	PREMULT_BLEND,
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
	MaterialParameterValue(bool boolean) {
		this->boolean = boolean;
		type = MatParamType::Bool;
	}
	MaterialParameterValue(glm::vec4 vec) {
		this->vector = vec;
		type = MatParamType::FloatVec;
	}
	MaterialParameterValue(unsigned int color32) {
		this->color32 = color32;
		type = MatParamType::Vector;
	}
	MaterialParameterValue(float scalar) {
		this->scalar = scalar;
		type = MatParamType::Float;
	}
	MaterialParameterValue(const Texture* t) {
		this->tex_ptr = t;
		type = MatParamType::Texture2D;
	}
	MaterialParameterValue() = default;
	bool is_texture() const {
		return type == MatParamType::Texture2D;
	}
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
	int offset = 0;
};

struct InstanceData
{
	std::string name;
	bool is_vector_type = false;	/* true = is scalar */
	uint32_t index = 0;
};

// compilied material, material instances can be based off it to allow for variation but minimize draw call changes
class MasterMaterialImpl
{
public:
	MasterMaterialImpl() {
	
	}
	~MasterMaterialImpl() {
		sys_print(Debug, "~MasterMaterialImpl: %s\n", self->get_name().c_str());
	}
	const MaterialParameterDefinition* find_definition(const std::string& str, int& index) const;
	bool is_translucent() const { return blend != blend_state::OPAQUE; }
	bool render_in_forward_pass() const { return is_translucent(); }
	bool is_alphatested() const { return alpha_tested; }
	void load_from_file(const std::string& fullpath, IFile* file, IAssetLoadingInterface* loading);
#ifdef EDITOR_BUILD
	// generated glsl fragment and vertex shader
	std::string create_glsl_shader(std::string& vs_code,std::string& fs_code,const std::vector<InstanceData>& instdat);
#endif

	MaterialInstance* self = nullptr;
	// All parameters that can be set by instances
	std::vector<MaterialParameterDefinition> param_defs;
	int num_texture_bindings = 0;
	struct UboBinding {
		//MaterialParameterBuffer* buffer = nullptr;
		int binding_loc = 0;
	};
	std::vector<UboBinding> constant_buffers;
	// Material state parameters
	bool alpha_tested = false;
	blend_state blend = blend_state::OPAQUE;
	LightingMode light_mode = LightingMode::Lit;
	MaterialUsage usage = MaterialUsage::Default;
	bool backface = false;
	// uses the shared depth material
	// this is true when nothing writes to worldPositionOffset and the material mode is not masked
	bool is_using_default_depth = false;
	int material_id = 0;
	bool is_compilied_shader_valid = false;

	// decal options
	bool decal_affect_albedo = false;
	bool decal_affect_normal = false;
	bool decal_affect_roughmetal = false;
	bool decal_affect_emissive = false;

	friend class MaterialManagerLocal;
	friend class MaterialLodJob;
};

class MaterialImpl
{
public:
	static const int INVALID_MAPPING = int(-1);
	MaterialImpl(bool is_dynamic_mat=false)  {
	}

	bool is_this_currently_uploaded() const { return gpu_buffer_offset != INVALID_MAPPING; }
	const std::vector<const Texture*>& get_textures() const { return texture_bindings; }
	void init_from(const MaterialInstance* parent);
	bool load_from_file(MaterialInstance* self, IAssetLoadingInterface* loading);
	void load_instance(MaterialInstance* self, IFile* file, IAssetLoadingInterface* loading);
	void load_master(MaterialInstance* self, IFile* file, IAssetLoadingInterface* loading);
	void post_load(MaterialInstance* self);
	MaterialParameterValue* find_parameter(StringName name) {
		auto master = get_master_impl();
		assert(master);
		for (int i = 0; i < master->param_defs.size(); i++) {
			if (master->param_defs[i].hashed_name == name) {
				return &params.at(i);
			}
		}
		return nullptr;
	}

	MasterMaterialImpl* get_master_impl() const {
		assert(masterMaterial);
		return masterMaterial->impl ? masterMaterial->impl->masterImpl.get() : nullptr;
	}


	MaterialInstance* self = nullptr;
	bool is_dynamic_material = false;
	int unique_id = 0;	// unique id of this material instance (always valid)
	MaterialInstance* masterMaterial = nullptr;	// this points to what master material this is instancing (valid on every material!)
	std::unique_ptr<MasterMaterialImpl> masterImpl;	// if this material instance is a default instance of a master material, this is filled
	std::vector<const Texture*> texture_bindings;
	std::vector<MaterialParameterValue> params;
	int gpu_buffer_offset = INVALID_MAPPING;	// offset in buffer if uploaded (the buffer is uint's so byte = buffer_offset*4)
	int dirty_buffer_index = -1;	// if not -1, then its sitting in a queue already
	bool has_called_post_load_already = false;

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
		is_lightmapped = 0;
	}

	uint32_t material_id : 26;
	uint32_t animated : 1;
	uint32_t editor_id : 1;
	uint32_t depth_only : 1;
	uint32_t dither : 1;
	uint32_t debug : 1;
	uint32_t is_lightmapped : 1;

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
	void recompile_for_material(MasterMaterialImpl* mat);

	std::unordered_map<uint32_t, program_handle> shader_key_to_program_handle;
};


class Model;
class MaterialManagerLocal : public MaterialManagerPublic
{
public:
	void init() override;

	// public interface
	void pre_render_update() override;	// material buffer updates
	int outstanding_dynamic_mats = 0;	// to check for mem leaks
	DynamicMatUniquePtr create_dynmaic_material(const MaterialInstance* material) final {
		return DynamicMatUniquePtr(create_dynmaic_material_unsafier(material));
	}
	MaterialInstance* create_dynmaic_material_unsafier(const MaterialInstance* material) {
		assert(material);
		MaterialInstance* dynamicMat = new MaterialInstance();
		dynamicMat->impl = std::make_unique<MaterialImpl>();
		dynamicMat->impl->self = dynamicMat;
		dynamicMat->impl->init_from(material);
		dynamicMat->impl->is_dynamic_material = true;
		dynamicMat->impl->post_load(dynamicMat);	// add to dirty list, set material id
		dynamicMat->set_loaded_manually_unsafe("%_DM_%");
		outstanding_dynamic_mats += 1;
		return dynamicMat;
	}
	void free_dynamic_material(MaterialInstance* mat) {
		if (!mat) 
			return;
		queued_dynamic_mats_to_delete.push_back(mat);
		outstanding_dynamic_mats -= 1;
	}


	program_handle get_mat_shader(
		bool is_lightmapped,
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
		if (!m->impl)
			return;
		remove_from_dirty_list_if_it_is(m);
		if (m->impl->gpu_buffer_offset != MaterialImpl::INVALID_MAPPING) {
			int byteIndex = (m->impl->gpu_buffer_offset * 4) / MATERIAL_SIZE;
			int bitmapIndex = byteIndex / 64;
			int bitIndex = byteIndex % 64;
			materialBitmapAllocator[bitmapIndex] &= ~(1ull << bitIndex);
		}

	}

	int get_next_master_id() {
		return ++current_master_id;
	}
	int get_next_instance_id() {
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
	int materialBufferSize = 0;

	// bitmap allocator for materials
	std::vector<uint64_t> materialBitmapAllocator;
	std::vector<MaterialInstance*> queued_dynamic_mats_to_delete;

	// materials to allocate or update
	std::vector<MaterialInstance*> dirty_list;

	// returns INDEX, not POINTER
	int allocate_material_instance() {
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


	int current_master_id = 0;
	int current_instance_id = 0;
};

extern MaterialManagerLocal matman;