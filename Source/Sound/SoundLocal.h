#pragma once

#include "SoundPublic.h"
#include <SDL2/SDL_mixer.h>
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

// reworked https://gist.github.com/hydren/f60d107f144fcb41dd6f898b126e17b2

static inline Uint16 formatSampleSize(Uint16 format) { return (format & 0xFF) / 8; }

const int audioChannelCount = 2;
const double audioFrequency = MIX_DEFAULT_FREQUENCY;
const int audioFormat = MIX_DEFAULT_FORMAT;


// Get chunk time length (in ms) given its size and current audio format
static int computeChunkLengthMillisec(int chunkSize)
{
    const Uint32 points = chunkSize / formatSampleSize(audioFormat);  // bytes / samplesize == sample points
    const Uint32 frames = (points / audioChannelCount);  // sample points / channels == sample frames
    return ((frames * 1000) / audioFrequency);  // (sample frames * 1000) / frequency == play length, in ms
}

template<typename AudioFormatType>
struct PlaybackSpeedEffectHandler
{
    const AudioFormatType* chunkData = nullptr;  // pointer to the chunk sample data (as array)
    float speedFactor = 1.f;  // the playback speed factor
    float position = 0.f;  // current position of the sound, in ms
    int duration=0;  // the duration of the sound, in ms
    int chunkSize=0;  // the size of the sound, as a number of indexes (or sample points). thinks of this as a array size when using the proper array type (instead of just Uint8*).
    bool loop=false;  // flags whether playback should stay looping
    bool altered=false;  // true if this playback has been pitched by this handler

    void init(const Mix_Chunk& chunk, bool loop) {
        chunkData =reinterpret_cast<AudioFormatType*>(chunk.abuf);
        position = 0.f;
        speedFactor = 1.f;
        chunkSize = chunk.alen / formatSampleSize(audioFormat);
        duration = computeChunkLengthMillisec(chunk.alen);
        this->loop = loop;
        this->altered = false;
    }

    // processing function to be able to change chunk speed/pitch.
    void modifyStreamPlaybackSpeed(int mixChannel, void* stream, int length)
    {
        AudioFormatType* buffer = static_cast<AudioFormatType*>(stream);
        const int bufferSize = length / sizeof(AudioFormatType);  // buffer size (as array)

        // if there is still sound to be played
        if (position < duration || loop)
        {
            const float delta = 1000.0 / audioFrequency,  // normal duration of each sample
                vdelta = delta * speedFactor;  // virtual stretched duration, scaled by 'speedFactor'

        // if playback is unaltered and pitch is required (for the first time)
            if (!altered && speedFactor != 1.0f)
                altered = true;  // flags playback modification and proceed to the pitch routine.
            ASSERT(speedFactor >= 0.0);
            if (speedFactor < 0.0)
                speedFactor = 1.0;

            if (altered)  // if unaltered, this pitch routine is skipped
            {
                for (int i = 0; i < bufferSize; i += audioChannelCount)
                {
                    const int j = i / audioChannelCount;  // j goes from 0 to size/channelCount, incremented 1 by 1
                    const float x = position + j * vdelta;  // get "virtual" index. its corresponding value will be interpolated.
                    const int k = floor(x / delta);  // get left index to interpolate from original chunk data (right index will be this plus 1)
                    const float prop = (x / delta) - k;  // get the proportion of the right value (left will be 1.0 minus this)

                    // usually just 2 channels: 0 (left) and 1 (right), but who knows...
                    for (int c = 0; c < audioChannelCount; c++)
                    {
                        // check if k will be within bounds
                        if (k * audioChannelCount + audioChannelCount - 1 < chunkSize || loop)
                        {
                            AudioFormatType v0 = chunkData[(k * audioChannelCount + c) % chunkSize],
                                // v_ = chunkData[((k-1) * channelCount + c) % chunkSize],
                                // v2 = chunkData[((k+2) * channelCount + c) % chunkSize],
                                v1 = chunkData[((k + 1) * audioChannelCount + c) % chunkSize];

                            // put interpolated value on 'data'
                            // buffer[i + c] = (1 - prop) * v0 + prop * v1;  // linear interpolation
                            buffer[i + c] = v0 + prop * (v1 - v0);  // linear interpolation (single multiplication)
                            // buffer[i + c] = v0 + 0.5f * prop * ((prop - 3) * v0 - (prop - 2) * 2 * v1 + (prop - 1) * v2);  // quadratic interpolation
                            // buffer[i + c] = v0 + (prop / 6) * ((3 * prop - prop2 - 2) * v_ + (prop2 - 2 * prop - 1) * 3 * v0 + (prop - prop2 + 2) * 3 * v1 + (prop2 - 1) * v2);  // cubic interpolation
                            // buffer[i + c] = v0 + 0.5f * prop * ((2 * prop2 - 3 * prop - 1) * (v0 - v1) + (prop2 - 2 * prop + 1) * (v0 - v_) + (prop2 - prop) * (v2 - v2));  // cubic spline interpolation
                        }
                        else  // if k will be out of bounds (chunk bounds), it means we already finished; thus, we'll pass silence
                        {
                            buffer[i + c] = 0;
                        }
                    }
                }
            }

            // update position
            position += (bufferSize / audioChannelCount) * vdelta;

            // reset position if looping
            if (loop) while (position > duration)
                position -= duration;
        }
        else  // if we already played the whole sound but finished earlier than expected by SDL_mixer (due to faster playback speed)
        {
            // set silence on the buffer since Mix_HaltChannel() poops out some of it for a few ms.
            for (int i = 0; i < bufferSize; i++)
                buffer[i] = 0;
        }
    }

    // Mix_EffectFunc_t callback that redirects to handler method (handler passed via userData)
    static void mixEffectFuncCallback(int channel, void* stream, int length, void* userData)
    {
        static_cast<PlaybackSpeedEffectHandler*>(userData)->modifyStreamPlaybackSpeed(channel, stream, length);
    }
    static void mixEffectDoneCallback(int, void* userData)
    {
    }

};

template<typename AudioFormatType>
struct LowPassFilter
{
    float alpha = 0.f;
    float last_sample[2];
    bool seen_first_sample = false;
    void init(float alpha) {
        seen_first_sample = false;
        this->alpha = alpha;
    }

    void do_effect(int channel, void* stream, int length) {
        AudioFormatType* buffer = static_cast<AudioFormatType*>(stream);
        const int bufferSize = length / sizeof(AudioFormatType);  // buffer size (as array)
        if (bufferSize < 2)
            return;
        this->alpha = 0.0;
        if (this->alpha <= 0.000001)
            return;

        const int audioChannelCount = 2;
        if (!seen_first_sample) {
            last_sample[0] = buffer[0];
            last_sample[1] = buffer[1];
            seen_first_sample = true;
        }
        for (int i = 0; i < bufferSize; i += audioChannelCount)
        {
            for (int c = 0; c < audioChannelCount; c++)
            {
                {
                    buffer[i + c] = buffer[i + c]*(1-alpha) + last_sample[c]*(alpha);
                    last_sample[c] = buffer[i + c];
                }
            }
        }
    }

    static void mixEffectFuncCallback(int channel, void* stream, int length, void* userData)
    {
        ((LowPassFilter*)userData)->do_effect(channel, stream, length);
    }

};

struct SoundPlayerInternal : public SoundPlayer
{
    bool is_oneshot = false;    // else owned by someone  
    int voice_index = -1;
    float time_elapsed = 0.0;
    float computedAttenuation = 0.0;
    bool should_play = false;

    bool does_have_active_slot() const {
        return voice_index != -1;
    }
    bool wants_to_start_playing() const {
       return  voice_index == -1 && should_play;
    }
};

void SoundPlayer::update()
{
    SoundPlayerInternal* self = (SoundPlayerInternal*)this;
}
void SoundPlayer::set_play(bool b)
{
    SoundPlayerInternal* self = (SoundPlayerInternal*)this;
    self->should_play = b;
}

inline float get_attenuation_factor(SndAtn sa, float minRadius, float maxRadius, float dist)
{
    if (dist <= minRadius)
        return 1.0;
    float window = maxRadius - minRadius;
    if (glm::abs(window) < 0.00001) return 0.0;
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
        //Initialize SDL_mixer
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            Fatalf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
        }
        Mix_AllocateChannels(snd_max_voices.get_integer());
        active_voices.resize(snd_max_voices.get_integer());
        pitch_modifiers.resize(snd_max_voices.get_integer());
        low_pass_mods.resize(snd_max_voices.get_integer());
	}
    void cleanup() override {
        Mix_CloseAudio();
    }

    void begin_sound_object_play(SoundPlayerInternal* spi) {
        assert(spi->voice_index == -1);
        assert(spi->asset);
        assert(spi->asset->internal_data);
        // find first free
        int index = 0;
        for (; index < active_voices.size(); index++)
            if (!active_voices[index])
                break;
        if (index == active_voices.size())
            return;
        spi->voice_index = index;
        active_voices[index] = spi;
        spi->time_elapsed = 0.f;
        using PitchMod = PlaybackSpeedEffectHandler<int16_t>;
        using LowPass = LowPassFilter<int16_t>;

        Mix_PlayChannel(index, spi->asset->internal_data, (spi->looping) ? -1 : 0);
        sys_print(Debug, "playchannel");
        pitch_modifiers.at(index).init(*spi->asset->internal_data, spi->looping);
        pitch_modifiers.at(index).speedFactor = spi->pitch_multiply;
        low_pass_mods.at(index).init(spi->lowpass_filter);
        Mix_RegisterEffect(index, PitchMod::mixEffectFuncCallback, PitchMod::mixEffectDoneCallback,&pitch_modifiers.at(index));
        Mix_RegisterEffect(index, LowPass::mixEffectFuncCallback,nullptr, &low_pass_mods.at(index));
    }

    void end_sound_object_play(SoundPlayerInternal* spi) {
        if (spi->voice_index != -1) {
            using PitchMod = PlaybackSpeedEffectHandler<int16_t>;
            using LowPass = LowPassFilter<int16_t>;

            Mix_UnregisterEffect(spi->voice_index, PitchMod::mixEffectFuncCallback);
            Mix_UnregisterEffect(spi->voice_index, LowPass::mixEffectFuncCallback);
            Mix_HaltChannel(spi->voice_index);
            active_voices.at(spi->voice_index) = nullptr;
            spi->voice_index = -1;
        }
        if (spi->is_oneshot) {
            assert(!spi->looping);
            all_sound_players.erase(spi);
            delete spi;
        }
        else {
            if(!spi->looping)
                spi->should_play = false;
        }
    }

    void tick(float dt) override {
        
        // tick current sounds and end any bad ones
        for (int i = 0; i < active_voices.size(); i++) {
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

            //spi->time_elapsed += dt;
            //if (spi->time_elapsed > asset->duration && !spi->looping) {
            //    end_sound_object_play(spi);
            //    continue;
            //}
            if (spi->attenuate) {
                float dist_sq = glm::dot(spi->spatial_pos - listener_position, spi->spatial_pos - listener_position);
                if (dist_sq >= spi->maxRadius * spi->maxRadius) {
                    end_sound_object_play(spi);
                    continue;
                }
            }
        }

        // get sound objects that want to play
        // sort them by distance
        // take n closest sounds and halt anything else

        std::vector<SoundPlayerInternal*> sorted_list;

        // to get around deleting in the unorderedmap, im lazy
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
                spi.computedAttenuation = spi.attenuate ? get_attenuation_factor(spi.attenuation, spi.minRadius, spi.maxRadius, dist) : 1.0;

                if (spi.computedAttenuation < 0) {
                    end_sound_object_play(&spi);
                    continue;
                }

                // add the handle to the sort list
                sorted_list.push_back(&spi);
            }
        }
        // sort them
        std::sort(sorted_list.begin(), sorted_list.end(), [&](SoundPlayerInternal* a, SoundPlayerInternal* b) -> bool {
            return a->computedAttenuation < b->computedAttenuation;
            });
        
        // end any playing clips outside of MAX_VOICES
        for (int i = active_voices.size(); i < sorted_list.size(); i++) {
            auto spi = sorted_list[i];
            if (spi->voice_index != -1) {
                end_sound_object_play(spi);
            }
        }

        // start any sounds that want to play but arent playing
        for (int i = 0; i < sorted_list.size() && i < active_voices.size(); i++) {
            auto& spi = *sorted_list[i];

            ASSERT(spi.asset);

            if (!spi.should_play)
                continue;

            if (spi.voice_index == -1)
                begin_sound_object_play(&spi);

            {
                if (spi.pitch_multiply < 0.0) {
                    sys_print(Warning, "negative pitch values not allowed\n");
                    spi.pitch_multiply = 1.0;
                }
                SDL_LockAudio();
                pitch_modifiers.at(spi.voice_index).speedFactor = spi.pitch_multiply;
                low_pass_mods.at(spi.voice_index).alpha = spi.lowpass_filter;
                SDL_UnlockAudio();
            }

            if (spi.spatialize) {
                glm::vec3 v = glm::normalize(spi.spatial_pos - listener_position);
                float l = glm::dot(v, listener_right) * 0.5 + 0.5 + 0.2;
                float r = glm::dot(v, -listener_right) * 0.5 + 0.5 + 0.2;
                r *= 1.0 / 1.2;
                l *= 1.0 / 1.2;

                Mix_SetPanning(spi.voice_index, int(spi.computedAttenuation * 255.f*r), int(spi.computedAttenuation * 255.f*l));
            }
            else 
                Mix_SetPanning(spi.voice_index, int(spi.computedAttenuation * 255.f), int(spi.computedAttenuation * 255.f));
        }
    }

    void stop_all_sounds() override {
    
    }
    void set_listener_position(glm::vec3 p, glm::vec3 listener_side) override {
        listener_position = p;
        listener_right = listener_side;
    }


    virtual void play_sound(
        const SoundFile* asset,
        float vol,
        float pitch,
        float min_rad,
        float max_rad,
        SndAtn attn,
        bool attenuate,
        bool spaitialize,
        glm::vec3 spatial_pos
    ) {
 
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
    // retained functions
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
    virtual void play_sound_player(handle<SoundPlayer> player) {

    }
    virtual void stop_sound_player(handle<SoundPlayer> player) {

    }
 

    glm::vec3 listener_position{};
    glm::vec3 listener_right{};
    std::vector<SoundPlayerInternal*> active_voices;
    std::unordered_set<SoundPlayerInternal*> all_sound_players;
    std::vector<PlaybackSpeedEffectHandler<int16_t>> pitch_modifiers;
    std::vector<LowPassFilter<int16_t>> low_pass_mods;

};

