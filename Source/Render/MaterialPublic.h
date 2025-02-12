#pragma once

#include "Assets/IAsset.h"
#include "DrawTypedefs.h"
#include <memory>
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

class MasterMaterialImpl;
class Texture;
class MaterialImpl;
CLASS_H(MaterialInstance, IAsset)
public:
	MaterialInstance();
	virtual ~MaterialInstance() override;
	MaterialInstance& operator=(MaterialInstance&& other) = default;

	// ONLY valid for dynamic materials! (is_this_a_dynamic_material())
	void set_float_parameter(StringName name, float f);
	void set_bool_parameter(StringName name, bool b);
	void set_vec_parameter(StringName name, Color32 c);
	void set_fvec_parameter(StringName name, glm::vec4 v4);
	void set_tex_parameter(StringName name, const Texture* t);

	const MasterMaterialImpl* get_master_material() const;
	bool is_this_a_dynamic_material() const;

	// IAsset interface
	void uninstall();
	void post_load(ClassBase*);
	bool load_asset(ClassBase*&);
	void sweep_references() const;
	void move_construct(IAsset* other);


	std::unique_ptr<MaterialImpl> impl;
protected:
	friend class MaterialManagerLocal;
};

class MaterialManagerPublic 
{
public:
	virtual void init() = 0;

	// Create a dynamic material from a material instance
	// resource deletion handled by RAII
	virtual std::unique_ptr<MaterialInstance> create_dynmaic_material(const MaterialInstance* material) = 0;

	// find a parameter buffer with the given name
	// ParameterBuffers function like singletons
	virtual MaterialParameterBuffer* find_parameter_buffer(const char* name) = 0;

	virtual void pre_render_update() = 0;

	// same shader, may change this in future (depth shaders get ifdef'd anyways)
	const MaterialInstance* get_shared_depth() const { return fallback; }
	const MaterialInstance* get_fallback() const { return fallback; }
	const MaterialInstance* get_default_billboard() const { return defaultBillboard; }
protected:

	const MaterialInstance* defaultBillboard = nullptr;
	const MaterialInstance* fallback = nullptr;
};

extern MaterialManagerPublic* imaterials;
