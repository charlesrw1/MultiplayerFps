#pragma once

#include "Assets/IAsset.h"
#include "DrawTypedefs.h"
#include <memory>
#include <string>
#include "Framework/StringName.h"
#include "glm/glm.hpp"
#include "DynamicMaterialPtr.h"
#include "Framework/ConsoleCmdGroup.h"

class MasterMaterialImpl;
class Texture;
class IAssetLoadingInterface;
class MaterialImpl;
class MaterialInstance : public IAsset {
public:
	CLASS_BODY(MaterialInstance);

	MaterialInstance();
	virtual ~MaterialInstance() override;
	MaterialInstance& operator=(MaterialInstance&& other) = default;

	static MaterialInstance* load(const std::string& path);

	// ONLY valid for dynamic materials! (is_this_a_dynamic_material())
	void set_float_parameter(StringName name, float f);
	void set_bool_parameter(StringName name, bool b);
	void set_vec_parameter(StringName name, Color32 c);
	void set_fvec_parameter(StringName name, glm::vec4 v4);
	void set_tex_parameter(StringName name, const Texture* t);

	const MasterMaterialImpl* get_master_material() const;
	bool is_this_a_dynamic_material() const;
	bool is_this_a_master_material() const;

	// IAsset interface
	void uninstall();
	void post_load();
	bool load_asset(IAssetLoadingInterface* loading);
	void sweep_references(IAssetLoadingInterface* loading) const;
	void move_construct(IAsset* other);

	std::unique_ptr<MaterialImpl> impl;
protected:
	friend class MaterialManagerLocal;
};


class IBakedLightingValuesFactory : public ClassBase {
public:
	CLASS_BODY(IBakedLightingValuesFactory);
	virtual bool get_baked_values(const MaterialInstance* instance, Color32& out_albedo, Color32& out_emissive) = 0;
};

class MaterialManagerPublic 
{
public:
	virtual void init() = 0;
	// Create a dynamic material from a material instance
	virtual DynamicMatUniquePtr create_dynmaic_material(const MaterialInstance* material) = 0;
	virtual void pre_render_update() = 0;
	// same shader, may change this in future (depth shaders get ifdef'd anyways)
	const MaterialInstance* get_shared_depth() const { return fallback; }
	const MaterialInstance* get_fallback() const { return fallback; }
	const MaterialInstance* get_default_billboard() const { return defaultBillboard; }
protected:
	uptr<IBakedLightingValuesFactory> bakedFactory;
	const MaterialInstance* defaultBillboard = nullptr;
	const MaterialInstance* fallback = nullptr;
};

extern MaterialManagerPublic* imaterials;
