#pragma once
#include <string>
#include "Framework/ClassBase.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"
#include "Framework/EnumDefReflection.h"

// Compiled sound container magic/version — bump SOUND_VERSION on any .csnd layout change.
constexpr uint32_t CSND_MAGIC = 'CSND';
constexpr uint32_t SOUND_VERSION = 1;

NEWENUM(AudioCodec, uint8_t){
	PCM,
	ADPCM,
	Vorbis,
};

// Streaming is reserved for a future pass -- compiler/loader currently treat it like DecodeOnPlay.
NEWENUM(AudioLoadMode, uint8_t){
	Predecode,
	DecodeOnPlay,
	Streaming,
};

class AudioImportSettings : public ClassBase
{
public:
	CLASS_BODY(AudioImportSettings);

	REF std::string src_file; // relative filepath, must be in same directory

	REF AudioCodec codec = AudioCodec::PCM;
	REF AudioLoadMode load_mode = AudioLoadMode::Predecode;

	REF bool force_mono = false;
	REF int sample_rate_override = 0; // 0 = keep source sample rate

	// Reserved for future loop-aware playback; not consumed by the runtime yet.
	REF float loop_start_seconds = -1.f;
	REF float loop_end_seconds = -1.f;

	REF int vorbis_quality_percent = 70; // 0-100, 100 = best; mapped to ffmpeg's -q:a 0-10 range
};

extern void write_audio_import_settings(AudioImportSettings* ais, const std::string& path);

// Compiles the .ais at ais_gamepath into its sibling .csnd if out of date.
// Implementation lives in SoundCompilier.cpp and is EDITOR_BUILD-only -- callers must
// guard call sites with #ifdef EDITOR_BUILD themselves (mirrors compile_texture_asset).
extern bool compile_sound_asset(const std::string& ais_gamepath);
