#pragma once


#include "Render/MaterialPublic.h"

#include <vector>
#include <unordered_set>

#include "Framework/Handle.h"
#include "Shader.h"
#include <unordered_map>

#include "Framework/Files.h"

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

class MasterMaterial;
class MaterialInstanceLocal : public MaterialInstance
{
public:
	static const uint32_t INVALID_MAPPING = uint32_t(-1);
	MaterialInstanceLocal(bool is_dynamic_mat=false) : MaterialInstance(is_dynamic_mat) {}

	bool is_this_currently_uploaded() const { return gpu_buffer_offset != INVALID_MAPPING; }

	bool is_a_default_inst = false;

	uint32_t unique_id = 0;	

	const std::vector<const Texture*>& get_textures() const { return texture_bindings; }
	std::vector<const Texture*> texture_bindings;
	std::vector<MaterialParameterValue> params;
	uint32_t gpu_buffer_offset = INVALID_MAPPING;	// offset in buffer if uploaded (the buffer is uint's so byte = buffer_offset*4)

	int dirty_buffer_index = -1;	// if not -1, then its sitting in a queue already

	void init_from(MasterMaterial* parent);
	void init_from(MaterialInstanceLocal* parent);

	friend class MaterialManagerLocal;

	void set_float_parameter(StringName name, float f) override {}
	void set_bool_parameter(StringName name, bool b)override {}
	void set_vec_parameter(StringName name, Color32 c) override {}
	void set_fvec_parameter(StringName name, glm::vec4 v4) override {}
	void set_tex_parameter(StringName name, const Texture* t) override {}

	bool load_from_file(const std::string& fullpath, IFile* file);
};

// compilied material, material instances can be based off it to allow for variation but minimize draw call changes
CLASS_H(MasterMaterial, IAsset)
public:
	MasterMaterial() : default_inst(false) {}

	// generated glsl fragment and vertex shader
	const MaterialInstance* get_default_material_inst() const { return &default_inst; }

	const MaterialParameterDefinition* find_definition(const std::string& str, int& index) const {
		for (int i = 0; i < param_defs.size(); i++)
			if (param_defs[i].name == str) {
				index = i;
				return &param_defs[i];
			}
		return nullptr;
	}

	// All parameters that can be set by instances
	std::vector<MaterialParameterDefinition> param_defs;
	uint32_t num_texture_bindings = 0;
	struct UboBinding {
		MaterialParameterBuffer* buffer = nullptr;
		uint32_t binding_loc = 0;
	};
	std::vector<UboBinding> constant_buffers;

	MaterialInstanceLocal default_inst;

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
	void create_material_instance() {
		default_inst.is_a_default_inst = true;
		default_inst.init_from(this);
	}
	std::string create_glsl_shader(
		std::string& vs_code,
		std::string& fs_code,
		const std::vector<InstanceData>& instdat
	);

	friend class MaterialManagerLocal;
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



// opaque materials get one path
// transparent materials get another path
// post process materials get another
// (different output targets, different inputs)

// materials also vary depending on context:
// is it animated?
// is it outputting editor id?

// so step 1: determine WHAT material we have (ie what shader)
//		if this is a depth pass, then get a special shader that is more easily batched
// now we have the shader, the options function like "#ifdefs for the shader" so check if the shader already exists first

// Essentially we want a hashmap of uint64(shader id, shader parameters) maped to a shader object
// to make sorting them easier, each shader object is also assigned a program id the first time its created (thus we can store shader id under, say, 16 bits)
// hashmap<uint64_t, {glShader, uint16}> shaderid_to_shaderobj
// and vector<uint64_t> (maps from uint16 back to shader key)
// if the shader doesnt modify verticies then it can get the uber depth shader, if it does, then it gets its own depth shader but fragment part is simplified
// if its alpha tested, then the depth material gets its own shader
// thus basic opaques can be merged, but anything else cant be merged in the depth pass


class Material_Shader_Table
{
public:
	Material_Shader_Table();

	program_handle lookup(shader_key key);
	void insert(shader_key key, program_handle handle);

	std::unordered_map<uint32_t, program_handle> shader_key_to_program_handle;
};


class Model;
class MaterialManagerLocal : public MaterialManagerPublic
{
public:
	void init() override;

	// public interface
	void pre_render_update() override;	// material buffer updates
	const MaterialInstance* find_material_instance(const char* mat_inst_name) override;

	MaterialInstance* create_dynmaic_material(const MaterialInstance* material) override { return nullptr; }
	void free_dynamic_material(MaterialInstance*& mat) override {}
	MaterialParameterBuffer* find_parameter_buffer(const char* name) override { return nullptr; }

	void reload_all();

	program_handle get_mat_shader(
		bool is_animated, 
		const Model* mod, 
		const MaterialInstanceLocal* gs, 
		bool depth_pass, 
		bool dither, 
		bool is_editor_mode,
		bool debug_mode
	);

	bufferhandle get_gpu_material_buffer() { return gpuMaterialBuffer; }
	void add_to_dirty_list(MaterialInstanceLocal* mat) {
		if (mat->dirty_buffer_index == -1) {
			dirty_list.push_back(mat);
			mat->dirty_buffer_index = dirty_list.size() - 1;
		}
	}
	void remove_from_dirty_list_if_it_is(MaterialInstanceLocal* mat) {
		if (mat->dirty_buffer_index != -1) {
			ASSERT(mat->dirty_buffer_index >= 0 && mat->dirty_buffer_index < dirty_list.size());
			dirty_list[mat->dirty_buffer_index] = nullptr;
		}
	}
private:
	MasterMaterial* fallback_master = nullptr;
	MasterMaterial* shared_depth_master = nullptr;

	void on_reload_shader_invoke();

	program_handle compile_mat_shader(const MasterMaterial* mat, shader_key key);
	Material_Shader_Table mat_table;

	// global material buffer, all parameters get stuff in here and accessed by shaders
	bufferhandle gpuMaterialBuffer = 0;
	uint32_t materialBufferSize = 0;

	// bitmap allocator for materials
	std::vector<uint64_t> materialBitmapAllocator;

	struct MaterialItem {
		union {
			MasterMaterial* mm = nullptr;
			MaterialInstanceLocal* mi;
		};
		bool is_master_material = false;
	};

	std::unordered_map<std::string, MaterialItem> all_materials;
	std::unordered_set<MaterialInstanceLocal*> dynamic_materials;

	std::unordered_map<std::string, MaterialParameterBuffer*> parameter_buffers;

	// materials to allocate or update
	std::vector<MaterialInstanceLocal*> dirty_list;

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