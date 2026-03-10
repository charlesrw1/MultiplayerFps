#pragma once
#include "Render/MaterialPublic.h"
#include <vector>
#include <unordered_set>
#include "Framework/Handle.h"
#include "Shader.h"
#include <unordered_map>
#include "Framework/Files.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/InlineVec.h"
#include "Framework/MapUtil.h"
#include "Animation/Editor/Optional.h"

const int MAX_INSTANCE_PARAMETERS = 8;	// 8 scalars/color32s
const int MATERIAL_SIZE = 64;	// 64 bytes
const int MAX_MATERIALS = 1024;
const int MAX_MAXTERIALS_BUFFER_SIZE = MATERIAL_SIZE * MAX_MATERIALS;

enum class LightingMode : int8_t
{
	Lit,
	Unlit
};
enum class MaterialUsage : int8_t
{
	Default,
	Postprocess,
	Terrain,
	Decal,
	UI,
	Particle,
};

#undef OPAQUE	// windows header leaking
enum class BlendState : int8_t
{
	OPAQUE,
	BLEND,
	ADD,
	MULT,

	// just for fun testing
	SCREEN,
	PREMULT_BLEND
};

// Parameter types
enum class MatParamType : int8_t
{
	Empty,
	FloatVec,	// float[4]
	Float,		// float
	Vector,		// uint8[4]
	Bool,		// uint8
	Texture2D,
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
	MaterialParameterValue(std::shared_ptr<Texture> t) {
		this->tex = std::move(t);
		type = MatParamType::Texture2D;
	}
	MaterialParameterValue() = default;
	MatParamType type = MatParamType::Empty;
	union {
		glm::vec4 vector;
		bool boolean;
		unsigned int color32;
		float scalar = 0.0;
	};
	std::shared_ptr<Texture> tex;
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
	int index = 0;
};
#include "Animation/Editor/Optional.h"
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
	bool is_translucent() const { return blend != BlendState::OPAQUE; }
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
	BlendState blend = BlendState::OPAQUE;
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

	bool is_valid() const {
		return bool(masterImpl) != bool(masterMaterial);
	}
	MaterialParameterValue* find_param_type(StringName name, MatParamType type) {
		auto param = find_parameter(name);
		if (!param || param->type != type)
			return nullptr;
		return param;
	}
	bool is_this_currently_uploaded() const { return gpu_buffer_offset != INVALID_MAPPING; }
	const std::vector<Texture*>& get_textures() const { return texture_bindings; }
	void init_from(const std::shared_ptr<MaterialInstance>& ptr);
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
		assert(masterMaterial || masterImpl);
		return masterImpl ? masterImpl.get() : masterMaterial.get()->impl->masterImpl.get();

		//return masterMaterial->impl ? masterMaterial->impl->masterImpl.get() : nullptr;
	}
	int get_material_index_from_buffer_ofs() const {
		return (gpu_buffer_offset * 4) / MATERIAL_SIZE;
	}

	bool is_transparent_material() const {
		return get_master_impl()->is_translucent();
	}

	MaterialInstance* self = nullptr;
	bool is_dynamic_material = false;

	int get_texture_id_hash();

	opt<int> texture_id_hash;
	std::shared_ptr<MaterialInstance> masterMaterial;
	std::unique_ptr<MasterMaterialImpl> masterImpl;	// if this material instance is a default instance of a master material, this is filled
	std::vector<Texture*> texture_bindings;
	std::vector<MaterialParameterValue> params;
	int gpu_buffer_offset = INVALID_MAPPING;	// offset in buffer if uploaded (the buffer is uint's so byte = buffer_offset*4)
	bool has_called_post_load_already = false;
	bool used_in_fastpath_cache = false;

	friend class MaterialManagerLocal;
	friend class MaterialLodJob;
};


namespace gpu {
	struct Material_Data;
}


// master shader flags are #ifdef defines in the shader. compilied seperately
enum master_shader_flags {
	MSF_ANIMATED = 1,
	MSF_EDITOR_ID = 2,
	MSF_DEPTH_ONLY = 4,
	MSF_DITHER = 8,
	MSF_DEBUG = 16,
	MSF_LIGHTMAPPED = 32,
	MSF_IS_FORCED_FORWARD = 64,
	MSF_NO_TAA = 128,
	MSF_MATERIAL_IN_INSTANCE = 256,
};

struct shader_key
{
	shader_key() {
		material_id = 0;
		msf_flags = 0;
	}
	bool has_flag(master_shader_flags f) {
		return (int(msf_flags) & f);
	}

	uint32_t material_id : 23;
	uint32_t msf_flags : 9;

	uint32_t as_uint32() const {
		return *((uint32_t*)this);
	}
};
static_assert(sizeof(shader_key) == 4, "shader key needs 4 bytes");



class MaterialShaderTable
{
public:
	MaterialShaderTable();
	program_handle lookup(shader_key key);
	void insert(shader_key key, program_handle handle);
	void recompile_for_material(MasterMaterialImpl* mat);

	std::unordered_map<uint32_t, program_handle> shader_key_to_program_handle;
};

class TextureBindingHasher {
public:
	int get_texture_hash_id_for_material(MaterialImpl* mat);
private:
	static const int NO_TEXTURE_ID = 0;
	struct HashItem {
		InlineVec<Texture*, 6> textures;
		int id = 0;
	};
	using HashItemVec = InlineVec<HashItem, 1>;
	static bool are_arrays_equal(const InlineVec<Texture*, 6>& v1, const std::vector<Texture*>& v2);
	int insert_new(const std::vector<Texture*> bindings);
	opt<int> find_existing(const std::vector<Texture*> bindings);

	std::unordered_map<Texture*, HashItemVec> table;
	int current_texture_hash_id = NO_TEXTURE_ID+1;
};

class BitmapAllocator {
public:
	BitmapAllocator(int size);
	int allocate();
	void free(int id);
private:
	int max_ids = 0;
	std::vector<uint64_t> materialBitmapAllocator;
};
class AllMaterialTable {
public:
	AllMaterialTable(int max_materials);
	void register_material(MaterialInstance* mat);
	void unregister_material(MaterialInstance* mat);
	const std::vector<MaterialInstance*>& get_all_mat_array() const;
private:
	std::vector<MaterialInstance*> all_mats;
	BitmapAllocator allocator;
};
#include "Framework/Hashset.h"

class DynamicMaterialAllocator {
public:
	MaterialInstance* allocate_dynamic();
	void free_dynamic(MaterialInstance* mat);

	int get_num_dynamic_mats() const {
		return outstanding_dynamic_mats;
	}
private:
	int outstanding_dynamic_mats = 0;	// to check for leaks
	hash_set<MaterialInstance> free_dynamic_ptrs;
};

class IGraphicsBuffer;
class Model;
class MaterialManagerLocal : public MaterialManagerPublic
{
public:
	void init() override;
	// public interface
	void pre_render_update() override;	// material buffer updates
	DynamicMatUniquePtr create_dynmaic_material(const MaterialInstance* material) final {
		return DynamicMatUniquePtr(create_dynmaic_material_unsafier(material));
	}
	MaterialInstance* create_dynmaic_material_unsafier(const MaterialInstance* master_material);
	void free_dynamic_material(MaterialInstance* mat);
	program_handle get_mat_shader(const Model* mod, const MaterialInstance* gs, int flags /* MSF_X*/);
	IGraphicsBuffer* get_gpu_material_buffer() { return gpuMatBufferPtr; }
	void add_to_dirty_list(MaterialInstance* mat);
	void remove_from_dirty_list_if_it_is(MaterialInstance* mat);
	void free_material_instance(MaterialInstance* m);
	int get_next_master_id() {return ++current_master_id;}
	MaterialInstance* get_default_editor_sel_PP() {return pp_editor_select_mat.get();}
	AllMaterialTable* get_material_table() { return mat_offset_table.get(); }
	int compute_tex_hash_for(MaterialImpl* m) {
		return binding_hasher.get_texture_hash_id_for_material(m);
	}
private:
	void on_reload_shader_invoke();
	program_handle compile_mat_shader(const MaterialInstance* mat, shader_key key);

	std::shared_ptr<MaterialInstance> pp_editor_select_mat = nullptr;
	MaterialInstance* fallback_master = nullptr;
	MaterialInstance* shared_depth_master = nullptr;

	int materialBufferSize = 0;
	IGraphicsBuffer* gpuMatBufferPtr = nullptr;

	// materials to allocate or update
	hash_set<MaterialInstance> dirty_list;
	int current_master_id = 0;

	MaterialShaderTable mat_shader_table;
	std::unique_ptr<AllMaterialTable> mat_offset_table;
	TextureBindingHasher binding_hasher;
	DynamicMaterialAllocator dynamic_mat_allocator;
};

extern MaterialManagerLocal matman;