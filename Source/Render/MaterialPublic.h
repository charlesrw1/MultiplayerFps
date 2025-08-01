#pragma once



#include "Assets/IAsset.h"
#include "DrawTypedefs.h"
#include <memory>
#include <string>
#include "Framework/StringName.h"
#include "glm/glm.hpp"
#include "DynamicMaterialPtr.h"
#include "Framework/ConsoleCmdGroup.h"
#include "Framework/MulticastDelegate.h"
#include "Framework/SharedPtr.h"

class MasterMaterialImpl;
class Texture;
class IAssetLoadingInterface;
class MaterialImpl;
class PhysicsMaterialWrapper;


class MaterialInstance : public IAsset {
public:
	CLASS_BODY(MaterialInstance);

	MaterialInstance();
	virtual ~MaterialInstance() override;
	MaterialInstance& operator=(MaterialInstance&& other) = default;

	REF static MaterialInstance* load(const std::string& path);

	// aight this is unsafe as crap, but whatever. for lua
	// dynamic materials, can set parameters through code
	REF static MaterialInstance* alloc_dynamic_mat(MaterialInstance* from);
	REF static void free_dynamic_mat(MaterialInstance* mat);

	// ONLY valid for dynamic materials! (is_this_a_dynamic_material())
	REF void set_float_parameter(StringName name, float f);
	REF void set_tex_parameter(StringName name, const Texture* t);

	REF bool is_this_a_dynamic_material() const;
	const MasterMaterialImpl* get_master_material() const;
	bool is_this_a_master_material() const;

	// IAsset interface
	void uninstall();
	void post_load();
	bool load_asset(IAssetLoadingInterface* loading);
	void sweep_references(IAssetLoadingInterface* loading) const;
	void move_construct(IAsset* other);

	REF void set_physics_material(PhysicsMaterialWrapper* material) {
		this->physics_mat = material;
	}
	PhysicsMaterialWrapper* get_physics_material() const {
		return physics_mat;
	}

	std::unique_ptr<MaterialImpl> impl;
	static MulticastDelegate<MaterialInstance*> on_material_loaded;
protected:
	PhysicsMaterialWrapper* physics_mat = nullptr;

	friend class MaterialManagerLocal;
};


class MaterialManagerPublic 
{
public:
	virtual void init() = 0;
	// Create a dynamic material from a material instance
	virtual DynamicMatUniquePtr create_dynmaic_material(const MaterialInstance* material) = 0;
	virtual void pre_render_update() = 0;
	// same shader, may change this in future (depth shaders get ifdef'd anyways)
	MaterialInstance* get_shared_depth() { return fallback.get(); }
	MaterialInstance* get_fallback() { return fallback.get(); }
	MaterialInstance* get_default_billboard() { return defaultBillboard.get(); }
	sptr<MaterialInstance> get_fallback_sptr() { return fallback; }
	sptr<MaterialInstance> get_default_billboard_sptr() { return defaultBillboard; }
protected:
	sptr<MaterialInstance> defaultBillboard = nullptr;
	sptr<MaterialInstance> fallback = nullptr;
};

extern MaterialManagerPublic* imaterials;
