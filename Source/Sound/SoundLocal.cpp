#include "SoundLocal.h"
#include "SoundComponent.h"
#include "Assets/AssetRegistry.h"

CLASS_IMPL(SoundFile);

#ifdef EDITOR_BUILD
class SoundAssetMetadata : public AssetMetadata
{
public:
	SoundAssetMetadata() {
		extensions.push_back("wav");
	}
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 134, 217, 181 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Sound";
	}
	virtual const ClassTypeInfo* get_asset_class_type() const { return &SoundFile::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(SoundAssetMetadata);
#endif

static SoundSystemLocal soundsys_local;
SoundSystemPublic* isound = &soundsys_local;

ConfigVar snd_max_voices("snd.max_voices", "16", CVAR_INTEGER | CVAR_DEV, 0, 64);