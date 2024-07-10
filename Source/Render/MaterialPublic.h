#pragma once

#include "IAsset.h"
#include "Material.h"
#include "DrawTypedefs.h"

#include <string>
#include "Framework/StringName.h"


enum class MatParamType : uint8_t
{
	Empty,
	Float,
	Bool,
	Vector,
	FloatVec,
	Texture2D,
};

struct MaterialParameterValue
{
private:
	MatParamType type = MatParamType::Empty;
	uint8_t index = 0;
	union {
		glm::vec4 vector;
		bool boolean;
		unsigned int color32;
		float scalar = 0.0;
		const Texture* tex_ptr;
	};
};

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
class MaterialParameterBuffer
{
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

enum class MatInstType
{
	Static,	// created with tools
	Dynamic	// created at runtime
};

class MaterialType;
class MaterialInstance
{
public:
	MaterialInstance(MatInstType type = MatInstType::Static) : type(type) {}

	const MatInstType type = MatInstType::Static;

	const std::vector<const Texture*>& get_textures() const;
	// @called by renderer to get a list of textures to bind for rendering
	// @if going bindless, then these textures will be stored in material buffer
	// @this can be lazily evaluated: when renderer calls for textures, check if they are ready

private:
	const MaterialType* master = nullptr;
	const MaterialInstance* parent = nullptr;
	std::vector<const Texture*> texture_bindings;

	// params usage depends on type
	// Static: index = index
	// Dynamic: index is fetched inside value; params remains compact
	std::vector<MaterialParameterValue> params;

	void upload_parameters_to_buffer();
	// @called by material manager
	// @code is responsible for uploading all parameters according to the definition in "master" when parameters change
	// @for static, these can be precached
	// @for dynamic, these should be only the delta's and the rest of the parameters are found by walking to parent

	void init();
	// @called by material manager
	// @for static, has to walk MatInst tree to get all the parameters
	// @for dynamic, nothing

	bool is_loaded = false;
	bool is_referenced = false;

	friend class MaterialManagerLocal;
};

// compilied material, material instances can be based off it to allow for variation but minimize draw call changes
class MaterialType : public IAsset
{
public:
	// generated glsl fragment and vertex shader

	const MaterialInstance* get_material_inst() const { return &default_inst; }
private:

	// All parameters that can be set by instances
	std::vector<MaterialParameterDefinition> param_defs;

	// Textures that arent parameters, but need to be bound still
	struct ConstantTextureRef {
		uint32_t index = 0;	// binding index
		const Texture* tex = nullptr;
	};
	std::vector<ConstantTextureRef> constant_textures;

	// Self material instance, uses default values
	MaterialInstance default_inst;

	// Material state parameters
	bool alpha_tested = false;
	blend_state blend = blend_state::OPAQUE;
	bool backface = false;

	// uses the shared depth material
	// this is true when nothing writes to worldPositionOffset and the material mode is not masked
	bool is_using_default_depth = false;

	uint32_t material_id = 0;
};

class DynamicMatView
{
public:
	DynamicMatView();

	void set_float_parameter(StringName name, float f);
	void set_bool_parameter(StringName name, bool b);
	void set_vec_parameter(StringName name, Color32 c);
	void set_fvec_parameter(StringName name, glm::vec4 v4);
	void set_tex_parameter(StringName name, const Texture* t);

	MaterialInstance data;
};

class MaterialManagerPublic
{
public:
	// Find a material instance with the given name (or a MasterMaterial and return the default instance)
	virtual const MaterialInstance* find_material_instance(const char* mat_inst_name) = 0;
	// Create a dynamic material from a material instance
	virtual DynamicMatView* create_dynmaic_material(const MaterialInstance* material) = 0;
	// Delete a created dynamic material
	virtual void free_dynamic_material(DynamicMatView*& mat) = 0;

	virtual MaterialParameterBuffer* find_parameter_buffer(const char* name) = 0;


	//virtual void mark_unreferenced() = 0;
	//virtual void free_unused() = 0;
	virtual void pre_render_update() = 0;
private:
	virtual const MaterialType* find_master_material(const char* master_name) = 0;
};

extern MaterialManagerPublic* imaterials;
