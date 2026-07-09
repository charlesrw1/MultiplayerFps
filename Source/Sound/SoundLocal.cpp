#include "SoundLocal.h"
#include "SoundComponent.h"
#include "Assets/AssetRegistry.h"
#include "AssetCompile/SoundAsset.h"
#include "AssetCompile/Someutils.h"

#ifdef EDITOR_BUILD
class SoundAssetMetadata : public AssetMetadata
{
public:
	SoundAssetMetadata() { extensions.push_back("csnd"); }
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const override { return Color32(134, 217, 181); }

	virtual std::string get_type_name() const override { return "Sound"; }
	virtual const ClassTypeInfo* get_asset_class_type() const { return &SoundFile::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(SoundAssetMetadata);

extern ConfigVar developer_mode;
#endif

static SoundSystemLocal soundsys_local;
SoundSystemPublic* isound = &soundsys_local;

ConfigVar snd_max_voices("snd.max_voices", "16", CVAR_INTEGER | CVAR_DEV, 0, 64);
#include "Assets/AssetDatabase.h"
SoundFile* SoundFile::load(std::string s) {
	return g_assets.find<SoundFile>(s).get();
}

// Fixed .csnd header layout: magic(4) + version(4) + codec(1) + load_mode(1)
// + loop_start(4) + loop_end(4) + container_size(4) = 22 bytes, followed by the
// raw WAV/OGG bytes ffmpeg produced (see SoundCompilier.cpp).
static constexpr uint64_t CSND_HEADER_SIZE = 22;

bool SoundFile::load_asset() {
	const std::string& path = get_name(); // .csnd

#ifdef EDITOR_BUILD
	if (developer_mode.get_bool()) {
		std::string ais_path = strip_extension(path) + ".ais";
		if (!compile_sound_asset(ais_path))
			sys_print(Error, "sound compile failed for %s\n", ais_path.c_str());
	}
#endif

	auto file = FileSys::open_read_game(path);
	if (!file)
		return false;
	if (file->size() < CSND_HEADER_SIZE) {
		sys_print(Error, "bad .csnd header (truncated) %s\n", path.c_str());
		return false;
	}

	uint32_t magic = 0, version = 0;
	uint8_t codec = 0, load_mode = 0;
	float loop_start = 0.f, loop_end = 0.f;
	uint32_t container_size = 0;
	file->read(&magic, 4);
	file->read(&version, 4);
	file->read(&codec, 1);
	file->read(&load_mode, 1);
	file->read(&loop_start, 4);
	file->read(&loop_end, 4);
	file->read(&container_size, 4);
	file->close();

	if (magic != CSND_MAGIC || version != SOUND_VERSION) {
		sys_print(Error, "bad .csnd header (magic/version mismatch) %s\n", path.c_str());
		return false;
	}

	std::string pathfull = FileSys::get_full_path_from_game_path(path);
	SDL_IOStream* io = SDL_IOFromFile(pathfull.c_str(), "rb");
	if (!io) {
		sys_print(Error, "couldn't reopen .csnd for playback: %s\n", path.c_str());
		return false;
	}
	SDL_SeekIO(io, (Sint64)CSND_HEADER_SIZE, SDL_IO_SEEK_SET);

	// SDL3_mixer's MIX_LoadAudio_IO only exposes a single predecode bool -- true decodes
	// eagerly into raw PCM now, false keeps the compressed data and decodes lazily on each
	// play. There's no third "true chunked streaming from disk" mode available through this
	// API, so Streaming (reserved) behaves the same as DecodeOnPlay here, both non-predecode.
	bool predecode = ((AudioLoadMode)load_mode == AudioLoadMode::Predecode);
	MIX_Audio* data = MIX_LoadAudio_IO(soundsys_local.mixer, io, predecode, /*closeio=*/true);
	if (!data) {
		if ((AudioCodec)codec == AudioCodec::Vorbis)
			sys_print(Warning, "Vorbis playback failed -- requires sdl3-mixer built with libvorbis: %s\n", path.c_str());
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

float SoundPlayer::get_playback_position_seconds() const {
	auto* self = (const SoundPlayerInternal*)this;
	if (self->voice_index == -1)
		return 0.f;
	MIX_Track* track = soundsys_local.tracks[self->voice_index];
	Sint64 frames = MIX_GetTrackPlaybackPosition(track);
	Sint64 ms = MIX_TrackFramesToMS(track, frames);
	return (ms > 0) ? float(ms) * 0.001f : 0.f;
}

void SoundPlayer::seek_seconds(float seconds) {
	SoundPlayerInternal* self = (SoundPlayerInternal*)this;
	if (self->voice_index == -1) {
		// No active voice slot yet -- request playback so one gets acquired on the next
		// tick(); the seek itself only takes effect once a track is actually assigned.
		self->should_play = true;
		return;
	}
	MIX_Track* track = soundsys_local.tracks[self->voice_index];
	Sint64 frames = MIX_TrackMSToFrames(track, (Sint64)(seconds * 1000.f));
	MIX_SetTrackPlaybackPosition(track, frames);
}

bool SoundPlayer::is_playing() const {
	auto* self = (const SoundPlayerInternal*)this;
	if (self->voice_index == -1)
		return false;
	return MIX_TrackPlaying(soundsys_local.tracks[self->voice_index]);
}

void SoundFile::uninstall() {
	if (internal_data) {
		MIX_DestroyAudio(internal_data);
		internal_data = nullptr;
	}
}
