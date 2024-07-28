#pragma once


#include "Render/MaterialPublic.h"

#include <vector>
#include <unordered_set>

#include "Framework/Handle.h"
#include "Shader.h"

const int MAX_INSTANCE_PARAMETERS = 8;	// 8 scalars/color32s
const uint32_t MATERIAL_SIZE = 64;	// 64 bytes
const uint32_t MAX_MAXTERIALS_BUFFER_SIZE = MATERIAL_SIZE * 1024;

enum class LightingMode : uint8_t
{
	Lit,
	Unlit
};
enum class MaterialUsage : uint8_t
{
	Default,
	Postprocess,
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

class MaterialInstanceLocal : public MaterialInstance
{
public:
	using MaterialInstance::MaterialInstance;

	const std::vector<const Texture*>& get_textures() const { return texture_bindings; }
	std::vector<const Texture*> texture_bindings;
	std::vector<MaterialParameterValue> params;
	uint32_t gpu_handle = 0;	// offset in buffer if uploaded
	void upload_parameters_to_buffer();
	void init();
	friend class MaterialManagerLocal;

	void set_float_parameter(StringName name, float f) override {}
	void set_bool_parameter(StringName name, bool b)override {}
	void set_vec_parameter(StringName name, Color32 c) override {}
	void set_fvec_parameter(StringName name, glm::vec4 v4) override {}
	void set_tex_parameter(StringName name, const Texture* t) override {}
};

// compilied material, material instances can be based off it to allow for variation but minimize draw call changes
CLASS_H(MasterMaterial, IAsset)
public:
	MasterMaterial() : default_inst(false) {}

	// generated glsl fragment and vertex shader
	const MaterialInstance* get_default_material_inst() const { return &default_inst; }

	// All parameters that can be set by instances
	std::vector<MaterialParameterDefinition> param_defs;

	struct UboBinding {
		MaterialParameterBuffer* buffer = nullptr;
		uint32_t binding_loc = 0;
	};
	std::vector<UboBinding> constant_buffers;

	MaterialInstanceLocal default_inst;

	// Material state parameters
	bool alpha_tested = false;
	blend_state blend = blend_state::OPAQUE;
	LightingMode light_mode = LightingMode::Lit;
	bool backface = false;

	// uses the shared depth material
	// this is true when nothing writes to worldPositionOffset and the material mode is not masked
	bool is_using_default_depth = false;

	uint32_t material_id = 0;

	bool load_from_file(const std::string& filename);
	void create_material_instance();
	std::string create_glsl_shader(
		std::string& vs_code,
		std::string& fs_code,
		const std::vector<InstanceData>& instdat
	);
};


class MaterialManagerLocal : public MaterialManagerPublic
{
public:
	void init() override {
		MasterMaterial mm;
		mm.load_from_file("terrain/terrainMaster.txt");

		Shader s;
		Shader::compile_vert_frag_single_file(&s, "./Data/Materials/terrain/terrainMaster_shader.glsl");
	}

	// public interface
	void pre_render_update() override {}
	const MaterialInstance* find_material_instance(const char* mat_inst_name) override { return nullptr; }
	MaterialInstance* create_dynmaic_material(const MaterialInstance* material) override { return nullptr; }
	void free_dynamic_material(MaterialInstance*& mat) override {}

	MaterialParameterBuffer* find_parameter_buffer(const char* name) override { return nullptr; }

	MasterMaterial* find_master_material(const std::string& mastername);
private:
	
	bufferhandle gpuMaterialBuffer = 0;
	uint32_t materialBufferSize = 0;

	// bitmap allocator for materials
	std::vector<uint64_t> materialBitmapAllocator;

	std::unordered_map<std::string, MasterMaterial*> master_materials;
	std::unordered_map<std::string, MaterialInstanceLocal*> static_materials;
	std::unordered_set<MasterMaterial*> dynamic_materials;
	std::unordered_map<std::string, MaterialParameterBuffer*> parameter_buffers;
};