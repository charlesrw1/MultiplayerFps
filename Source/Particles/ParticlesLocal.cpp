#include "ParticlesLocal.h"
#include "Render/Model.h"
#include "Render/MaterialLocal.h"

class ParticleFXAssetMetadata : public AssetMetadata
{
	// Inherited via AssetMetadata
	virtual std::string get_type_name() const override
	{
		return "ParticleFX";
	}
	virtual Color32 get_browser_color() const override
	{
		return { 245, 186, 213 };
	}
	virtual void index_assets(std::vector<std::string>& filepaths) const override
	{
	}

	virtual const ClassTypeInfo* get_asset_class_type() const { return &ParticleFXAsset::StaticType; }
};

REGISTER_ASSETMETADATA_MACRO(ParticleFXAssetMetadata);
CLASS_IMPL(ParticleFXAsset);
CLASS_IMPL(ParticleEmitter);

ParticleEmitter::ParticleEmitter() {}
ParticleEmitter::~ParticleEmitter() {}
ParticleFXAsset::ParticleFXAsset() {}
ParticleFXAsset::~ParticleFXAsset() {}
const PropertyInfoList* ParticleEmitter::get_props()
{
	MAKE_VECTORCALLBACK_ATOM(std::unique_ptr<ParticleModule>, module_stack);
	START_PROPS(ParticleEmitter)
		REG_STDVECTOR(module_stack, PROP_SERIALIZE),
		REG_ASSET_PTR(spriteMaterial, PROP_DEFAULT),
		REG_ASSET_PTR(particleModel, PROP_DEFAULT)
	END_PROPS(ParticleEmitter)
}
const PropertyInfoList* ParticleFXAsset::get_props()
{
	MAKE_VECTORCALLBACK_ATOM(std::unique_ptr<ParticleEmitter>, emitters);
	START_PROPS(ParticleFXAsset)
		REG_STDVECTOR(emitters, PROP_SERIALIZE),
	END_PROPS(ParticleFXAsset)
}