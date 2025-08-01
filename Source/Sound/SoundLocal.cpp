#include "SoundLocal.h"
#include "SoundComponent.h"
#include "Assets/AssetRegistry.h"

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
		return Color32( 134, 217, 181 );
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
#include "Assets/AssetDatabase.h"
SoundFile* SoundFile::load(std::string s) {
	return g_assets.find_sync<SoundFile>(s).get();
}
bool SoundFile::load_asset(IAssetLoadingInterface*)
{
	std::string pathfull = FileSys::get_full_path_from_game_path(get_name());
	Mix_Chunk* data = Mix_LoadWAV(pathfull.c_str());
	if (!data) {
		return false;
	}

	this->internal_data = data;
	this->duration = data->alen / 44100.0;

	return true;
}

void SoundFile::uninstall()
{
	Mix_FreeChunk(internal_data);
	internal_data = nullptr;
}