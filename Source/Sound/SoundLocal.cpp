#include "SoundLocal.h"
#include "SoundComponent.h"
#include "Assets/AssetRegistry.h"

#ifdef EDITOR_BUILD
class SoundAssetMetadata : public AssetMetadata
{
public:
	SoundAssetMetadata() { extensions.push_back("wav"); }
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const override { return Color32(134, 217, 181); }

	virtual std::string get_type_name() const override { return "Sound"; }
	virtual const ClassTypeInfo* get_asset_class_type() const { return &SoundFile::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(SoundAssetMetadata);
#endif

static SoundSystemLocal soundsys_local;
SoundSystemPublic* isound = &soundsys_local;

ConfigVar snd_max_voices("snd.max_voices", "16", CVAR_INTEGER | CVAR_DEV, 0, 64);
#include "Assets/AssetDatabase.h"
SoundFile* SoundFile::load(std::string s) {
	return g_assets.find<SoundFile>(s).get();
}
bool SoundFile::load_asset() {
	std::string pathfull = FileSys::get_full_path_from_game_path(get_name());
	MIX_Audio* data = MIX_LoadAudio(soundsys_local.mixer, pathfull.c_str(), /*predecode=*/true);
	if (!data) {
		return false;
	}

	this->internal_data = data;
	// Duration in seconds via SDL_mixer's frame-count helper.
	Sint64 frames = MIX_GetAudioDuration(data);
	if (frames <= 0) {
		this->duration = 0.f;
	} else {
		Sint64 ms = MIX_AudioFramesToMS(data, frames);
		this->duration = (ms > 0) ? float(ms) * 0.001f : 0.f;
	}

	return true;
}

void SoundPlayer::update() {}
void SoundPlayer::set_play(bool b) {
	SoundPlayerInternal* self = (SoundPlayerInternal*)this;
	self->should_play = b;
}

void SoundFile::uninstall() {
	if (internal_data) {
		MIX_DestroyAudio(internal_data);
		internal_data = nullptr;
	}
}
