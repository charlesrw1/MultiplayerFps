#pragma once

#include "IAsset.h"
#include "Material.h"
#include "DrawTypedefs.h"

#include <string>
#include "Framework/StringName.h"

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

// A global "UBO" of material params
// Cant hold texture types
CLASS_H(MaterialParameterBuffer,IAsset)
public:
	void set_float_parameter(StringName name, float f);
	void set_bool_parameter(StringName name, bool b);
	void set_vec_parameter(StringName name, Color32 c);
	void set_fvec_parameter(StringName name, glm::vec4 v4);
private:
	std::vector<MaterialParameterDefinition> param_defs;
	std::vector<MaterialParameterValue> values;

	bufferhandle ubo_buffer = 0;
	uint32_t buffer_size = 0;
};

class MasterMaterial;
CLASS_H(MaterialInstance, IAsset)
public:
	MaterialInstance(bool is_dynamic_mat = false) : is_dynamic_material(is_dynamic_mat) {}

	const std::vector<const Texture*>& get_textures() const;

	// Only valid for dynamic materials
	void set_float_parameter(StringName name, float f);
	void set_bool_parameter(StringName name, bool b);
	void set_vec_parameter(StringName name, Color32 c);
	void set_fvec_parameter(StringName name, glm::vec4 v4);
	void set_tex_parameter(StringName name, const Texture* t);
private:
	bool is_dynamic_material = false;

	const MasterMaterial* master = nullptr;
	std::vector<const Texture*> texture_bindings;
	std::vector<MaterialParameterValue> params;

	uint32_t gpu_handle = 0;	// offset in buffer if uploaded

	void upload_parameters_to_buffer();

	void init();

	friend class MaterialManagerLocal;
};

enum class LightingMode : uint8_t
{
	Lit,
	Unlit
};
enum class MaterialUsage : uint8_t
{
	Default,
	Postprocess
};

// compilied material, material instances can be based off it to allow for variation but minimize draw call changes
CLASS_H(MasterMaterial,IAsset)
public:
	MasterMaterial() : default_inst(false) {}

	// generated glsl fragment and vertex shader

	const MaterialInstance* get_material_inst() const { return &default_inst; }
protected:

	// All parameters that can be set by instances
	std::vector<MaterialParameterDefinition> param_defs;
	
	struct UboBinding {
		MaterialParameterBuffer* buffer = nullptr;
		uint32_t binding_loc = 0;
	};
	std::vector<UboBinding> constant_buffers;

	MaterialInstance default_inst;

	// Material state parameters
	bool alpha_tested = false;
	blend_state blend = blend_state::OPAQUE;
	LightingMode light_mode = LightingMode::Lit;
	bool backface = false;

	// uses the shared depth material
	// this is true when nothing writes to worldPositionOffset and the material mode is not masked
	bool is_using_default_depth = false;

	uint32_t material_id = 0;
};

class MaterialManagerPublic
{
public:
	// Find a material instance with the given name (or a MasterMaterial and return the default instance)
	virtual const MaterialInstance* find_material_instance(const char* mat_inst_name) = 0;
	// Create a dynamic material from a material instance
	virtual MaterialInstance* create_dynmaic_material(const MaterialInstance* material) = 0;
	// Delete a created dynamic material
	virtual void free_dynamic_material(MaterialInstance*& mat) = 0;
	virtual MaterialParameterBuffer* find_parameter_buffer(const char* name) = 0;


	//virtual void mark_unreferenced() = 0;
	//virtual void free_unused() = 0;
	virtual void pre_render_update() = 0;
};

extern MaterialManagerPublic* imaterials;
