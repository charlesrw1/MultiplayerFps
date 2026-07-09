#pragma once

#include "SoundPublic.h"
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3/SDL.h>
#include "Framework/Util.h"
#include "Assets/IAsset.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <unordered_set>
#include "Framework/Config.h"
#include "Framework/Hashset.h"
#include "Framework/Files.h"

// Low-pass IIR filter applied to float32 samples via MIX_SetTrackCookedCallback.
// SDL3_mixer cooks all tracks down to float32 before this fires, so we don't
// need an int16 template specialisation any more.
struct LowPassFilter
{
	float alpha = 0.f;
	float last_sample[2] = {0.f, 0.f};
	bool seen_first_sample = false;
	void init(float alpha) {
		seen_first_sample = false;
		this->alpha = alpha;
	}

	void do_effect(float* buffer, int samples, int channels) {
		if (samples < 2 || channels < 1)
			return;
		if (alpha <= 0.000001f)
			return;
		const int ch = (channels > 2) ? 2 : channels;
		if (!seen_first_sample) {
			for (int c = 0; c < ch; c++)
				last_sample[c] = buffer[c];
			seen_first_sample = true;
		}
		for (int i = 0; i < samples; i += channels) {
			for (int c = 0; c < ch; c++) {
				buffer[i + c] = buffer[i + c] * (1.f - alpha) + last_sample[c] * alpha;
				last_sample[c] = buffer[i + c];
			}
		}
	}

	static void SDLCALL mixCallback(void* userData, MIX_Track* /*track*/, const SDL_AudioSpec* spec, float* pcm, int samples) {
		((LowPassFilter*)userData)->do_effect(pcm, samples, spec ? spec->channels : 2);
	}
};

struct SoundPlayerInternal : public SoundPlayer
{
	bool is_oneshot = false;	// else owned by someone
	int voice_index = -1;		// slot in SoundSystemLocal::active_voices, -1 if not playing
	float time_elapsed = 0.0;
	float computedAttenuation = 0.0;
	bool should_play = false;

	bool does_have_active_slot() const { return voice_index != -1; }
	bool wants_to_start_playing() const { return voice_index == -1 && should_play; }
};

inline float get_attenuation_factor(SndAtn sa, float minRadius, float maxRadius, float dist) {
	if (dist <= minRadius)
		return 1.0;
	float window = maxRadius - minRadius;
	if (glm::abs(window) < 0.00001)
		return 0.0;
	float x = 1.0 - ((dist - minRadius) / window);
	switch (sa) {
	case SndAtn::Linear:
		return x;
	case SndAtn::Cubic:
		return x * x * x;
	case SndAtn::InvCubic:
		return 1.0 - (1 - x) * (1 - x) * (1 - x);
	}
	return 1.0;
}

extern ConfigVar snd_max_voices;

class SoundSystemLocal : public SoundSystemPublic
{
public:
	SoundSystemLocal() : all_sound_players(5) {}

	void init() override {
		if (!MIX_Init()) {
			Fatalf("MIX_Init failed: %s\n", SDL_GetError());
		}
		mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
		if (!mixer) {
			Fatalf("MIX_CreateMixerDevice failed: %s\n", SDL_GetError());
		}
		const int num_voices = snd_max_voices.get_integer();
		active_voices.resize(num_voices, nullptr);
		tracks.resize(num_voices, nullptr);
		low_pass_mods.resize(num_voices);
		for (int i = 0; i < num_voices; i++) {
			tracks[i] = MIX_CreateTrack(mixer);
			if (!tracks[i]) {
				Fatalf("MIX_CreateTrack failed: %s\n", SDL_GetError());
			}
			MIX_SetTrackCookedCallback(tracks[i], LowPassFilter::mixCallback, &low_pass_mods[i]);
		}
	}
	void cleanup() override {
		for (auto* t : tracks)
			if (t) MIX_DestroyTrack(t);
		tracks.clear();
		if (mixer) {
			MIX_DestroyMixer(mixer);
			mixer = nullptr;
		}
		MIX_Quit();
	}

	void begin_sound_object_play(SoundPlayerInternal* spi) {
		assert(spi->voice_index == -1);
		assert(spi->asset);
		assert(spi->asset->internal_data);
		// find first free
		int index = 0;
		for (; index < (int)active_voices.size(); index++)
			if (!active_voices[index])
				break;
		if (index == (int)active_voices.size())
			return;
		spi->voice_index = index;
		active_voices[index] = spi;
		spi->time_elapsed = 0.f;

		MIX_Track* track = tracks[index];
		MIX_SetTrackAudio(track, spi->asset->internal_data);
		MIX_SetTrackLoops(track, spi->looping ? -1 : 0);
		MIX_SetTrackFrequencyRatio(track, glm::max(spi->pitch_multiply, 0.001f));
		low_pass_mods[index].init(spi->lowpass_filter);
		MIX_PlayTrack(track, 0);
		sys_print(Debug, "playchannel");
	}

	void end_sound_object_play(SoundPlayerInternal* spi) {
		if (spi->voice_index != -1) {
			MIX_Track* track = tracks[spi->voice_index];
			// Apply immediate mute for one-shot sounds to prevent clicks on early stop
			if (spi->is_oneshot && spi->asset) {
				float duration = spi->asset->get_duration();
				float fade_out_start = glm::max(0.f, duration - spi->fade_out_time);
				if (spi->time_elapsed < fade_out_start) {
					MIX_SetTrackGain(track, 0.f);
				}
			}
			MIX_StopTrack(track, 0);
			MIX_SetTrackAudio(track, nullptr);
			active_voices.at(spi->voice_index) = nullptr;
			spi->voice_index = -1;
		}
		if (spi->is_oneshot) {
			assert(!spi->looping);
			all_sound_players.erase(spi);
			delete spi;
		} else {
			if (!spi->looping)
				spi->should_play = false;
		}
	}

	void tick(float dt) override {

		// tick current sounds and end any bad ones
		for (int i = 0; i < (int)active_voices.size(); i++) {
			auto spi = active_voices[i];
			if (!spi)
				continue;
			if (!spi->should_play) {
				end_sound_object_play(spi);
				continue;
			}
			auto asset = spi->asset;
			if (!asset) {
				end_sound_object_play(spi);
				continue;
			}

			// A non-looping clip that finished playing on its own (not via an explicit
			// set_play(false)/attenuation stop) leaves its SDL track idle but was never
			// releasing the voice slot or clearing should_play -- so pressing Play again
			// after natural completion was a no-op (voice_index stayed valid, should_play
			// stayed true). Detect that and release the voice like any other stop.
			if (!spi->looping && !MIX_TrackPlaying(tracks[i])) {
				end_sound_object_play(spi);
				continue;
			}

			spi->time_elapsed += dt;
			if (spi->attenuate) {
				float dist_sq = glm::dot(spi->spatial_pos - listener_position, spi->spatial_pos - listener_position);
				if (dist_sq >= spi->maxRadius * spi->maxRadius) {
					end_sound_object_play(spi);
					continue;
				}
			}
		}

		std::vector<SoundPlayerInternal*> sorted_list;

		{
			static std::vector<SoundPlayerInternal*> objs;
			objs.clear();
			for (auto o : all_sound_players)
				objs.push_back(o);

			for (auto o : objs) {
				auto& spi = *o;

				ASSERT(spi.voice_index == -1 || spi.should_play);
				if (!spi.asset) {
					end_sound_object_play(&spi);
					continue;
				}

				float dist = glm::length(spi.spatial_pos - listener_position);
				spi.computedAttenuation =
					spi.attenuate ? get_attenuation_factor(spi.attenuation, spi.minRadius, spi.maxRadius, dist) : 1.0;

				if (spi.computedAttenuation < 0) {
					end_sound_object_play(&spi);
					continue;
				}

				sorted_list.push_back(&spi);
			}
		}
		std::sort(sorted_list.begin(), sorted_list.end(), [&](SoundPlayerInternal* a, SoundPlayerInternal* b) -> bool {
			return a->computedAttenuation < b->computedAttenuation;
		});

		// end any playing clips outside of MAX_VOICES
		for (int i = (int)active_voices.size(); i < (int)sorted_list.size(); i++) {
			auto spi = sorted_list[i];
			if (spi->voice_index != -1) {
				end_sound_object_play(spi);
			}
		}

		// start any sounds that want to play but arent playing
		for (int i = 0; i < (int)sorted_list.size() && i < (int)active_voices.size(); i++) {
			auto& spi = *sorted_list[i];

			ASSERT(spi.asset);

			if (!spi.should_play)
				continue;

			if (spi.voice_index == -1)
				begin_sound_object_play(&spi);

			MIX_Track* track = tracks[spi.voice_index];

			if (spi.pitch_multiply < 0.0) {
				sys_print(Warning, "negative pitch values not allowed\n");
				spi.pitch_multiply = 1.0;
			}
			MIX_LockMixer(mixer);
			MIX_SetTrackFrequencyRatio(track, glm::max(spi.pitch_multiply, 0.001f));
			low_pass_mods[spi.voice_index].alpha = spi.lowpass_filter;
			MIX_UnlockMixer(mixer);

			// Envelope multiplier for one-shot sounds (fade in/out)
			float envelope_mult = 1.0f;
			if (spi.is_oneshot && spi.asset) {
				float duration = spi.asset->get_duration();
				float elapsed = spi.time_elapsed;

				if (elapsed < spi.fade_in_time && spi.fade_in_time > 0.0f) {
					envelope_mult *= elapsed / spi.fade_in_time;
				}

				float fade_out_start = glm::max(0.f, duration - spi.fade_out_time);
				if (elapsed > fade_out_start && spi.fade_out_time > 0.0f) {
					float fade_out_progress = (elapsed - fade_out_start) / spi.fade_out_time;
					envelope_mult *= (1.0f - glm::clamp(fade_out_progress, 0.f, 1.f));
				}
			}

			const float gain = glm::clamp(spi.volume_multiply * spi.computedAttenuation * envelope_mult, 0.f, 1.f);
			MIX_SetTrackGain(track, gain);

			if (spi.spatialize) {
				glm::vec3 v = glm::normalize(spi.spatial_pos - listener_position);
				float l = glm::dot(v, listener_right) * 0.5 + 0.5 + 0.2;
				float r = glm::dot(v, -listener_right) * 0.5 + 0.5 + 0.2;
				r *= 1.0 / 1.2;
				l *= 1.0 / 1.2;

				MIX_StereoGains g{ float(spi.computedAttenuation * r), float(spi.computedAttenuation * l) };
				MIX_SetTrackStereo(track, &g);
			} else {
				MIX_StereoGains g{ float(spi.computedAttenuation), float(spi.computedAttenuation) };
				MIX_SetTrackStereo(track, &g);
			}
		}
	}

	void stop_all_sounds() override {}
	void set_listener_position(glm::vec3 p, glm::vec3 listener_side) override {
		listener_position = p;
		listener_right = listener_side;
	}

	virtual void play_sound(const SoundFile* asset, float vol, float pitch, float min_rad, float max_rad, SndAtn attn,
							bool attenuate, bool spaitialize, glm::vec3 spatial_pos) {

		auto sp = register_sound_player();
		SoundPlayerInternal* spi = (SoundPlayerInternal*)sp;
		sp->asset = asset;
		sp->volume_multiply = vol;
		sp->pitch_multiply = pitch;
		sp->minRadius = min_rad;
		sp->maxRadius = max_rad;
		sp->attenuation = attn;
		sp->attenuate = attenuate;
		sp->spatialize = spaitialize;
		sp->spatial_pos = spatial_pos;
		sp->looping = false;
		spi->should_play = true;
		spi->is_oneshot = true;
	}
	virtual SoundPlayer* register_sound_player() final {
		SoundPlayerInternal* spi = new SoundPlayerInternal;
		all_sound_players.insert(spi);
		return (SoundPlayer*)spi;
	}
	virtual void remove_sound_player(SoundPlayer*& sp) final {
		SoundPlayerInternal* spi = (SoundPlayerInternal*)sp;
		ASSERT(!spi->is_oneshot);
		end_sound_object_play(spi);
		all_sound_players.erase(spi);
		delete spi;
		sp = nullptr;
	}
	virtual void play_sound_player(handle<SoundPlayer> player) {}
	virtual void stop_sound_player(handle<SoundPlayer> player) {}

	MIX_Mixer* mixer = nullptr;
	glm::vec3 listener_position{};
	glm::vec3 listener_right{};
	std::vector<SoundPlayerInternal*> active_voices;
	std::vector<MIX_Track*> tracks;
	std::unordered_set<SoundPlayerInternal*> all_sound_players;
	std::vector<LowPassFilter> low_pass_mods;
};
