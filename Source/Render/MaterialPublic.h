#pragma once

#include "IAsset.h"
#include "DrawTypedefs.h"

#include <string>
#include "Framework/StringName.h"
#include "glm/glm.hpp"

// A global "UBO" of material params
// Cant hold texture types
CLASS_H(MaterialParameterBuffer,IAsset)
public:
	virtual void set_float_parameter(StringName name, float f) = 0;
	virtual void set_bool_parameter(StringName name, bool b) = 0;
	virtual void set_vec_parameter(StringName name, Color32 c) = 0;
	virtual void set_fvec_parameter(StringName name, glm::vec4 v4) = 0;
};

class MasterMaterial;
class Texture;
CLASS_H(MaterialInstance, IAsset)
public:
	// ONLY valid for dynamic materials! (is_this_a_dynamic_material())
	virtual void set_float_parameter(StringName name, float f) = 0;
	virtual void set_bool_parameter(StringName name, bool b) = 0;
	virtual void set_vec_parameter(StringName name, Color32 c) = 0;
	virtual void set_fvec_parameter(StringName name, glm::vec4 v4) = 0;
	virtual void set_tex_parameter(StringName name, const Texture* t) = 0;

	const MasterMaterial* get_master_material() const { return master; }
	bool is_this_a_dynamic_material() const { return is_dynamic_material; }
protected:
	MaterialInstance(bool is_dynamic_mat = false) : is_dynamic_material(is_dynamic_mat) {}
	bool is_dynamic_material = false;
	const MasterMaterial* master = nullptr;
};

class MaterialManagerPublic : public IAssetLoader
{
public:
	virtual void init() = 0;

	IAsset* load_asset(const std::string& path) override {
		return (IAsset*)find_material_instance(path.c_str());
	}

	// Find a material instance with the given name (or a MasterMaterial and return the default instance)
	virtual const MaterialInstance* find_material_instance(const char* mat_inst_name) = 0;
	// Create a dynamic material from a material instance
	virtual MaterialInstance* create_dynmaic_material(const MaterialInstance* material) = 0;
	// Delete a created dynamic materials (NOT for static/normal materials!)
	virtual void free_dynamic_material(MaterialInstance*& mat) = 0;
	// find a parameter buffer with the given name
	// ParameterBuffers function like singletons
	virtual MaterialParameterBuffer* find_parameter_buffer(const char* name) = 0;

	virtual void pre_render_update() = 0;

	// same shader, may change this in future (depth shaders get ifdef'd anyways)
	const MaterialInstance* get_shared_depth() const { return fallback; }
	const MaterialInstance* get_fallback() const { return fallback; }
protected:
	const MaterialInstance* fallback = nullptr;
};

extern MaterialManagerPublic* imaterials;
