#pragma once

#include "SoundPublic.h"
#include <SDL2/SDL_mixer.h>
#include "Framework/Util.h"
#include "Framework/FreeList.h"
#include "Assets/IAsset.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <algorithm>

#include "Framework/Config.h"

static const char* const SOUND_DIRECTORY = "./Data/Sounds/";

class PostProcessSoundEffect
{
public:
    virtual void apply() = 0;
};


// sound isnt playing
// sound is playing and on a channel

struct SoundPlayerInternal
{
    SoundPlayer sp;
    bool is_oneshot = false;    // else owned by someone  
    int voice_index = -1;  
    float time_elapsed = 0.0;
    float computedAttenuation = 0.0;
    bool should_play = false;
};

inline float get_attenuation_factor(SndAttenuation sa, float minRadius, float maxRadius, float dist)
{
    if (dist <= minRadius)
        return 1.0;
    float window = maxRadius - minRadius;
    if (glm::abs(window) < 0.00001) return 0.0;
    switch (sa) {
    case SndAttenuation::Linear:
        return 1.0 - ((dist - minRadius) / window);
    }
    return 1.0;
}

extern ConfigVar snd_max_voices;

class SoundSystemLocal : public SoundSystemPublic
{
public:
	void init() override {
        //Initialize SDL_mixer
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            Fatalf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
        }
        Mix_AllocateChannels(snd_max_voices.get_integer());
        active_voices.resize(snd_max_voices.get_integer());
	}

    void begin_sound_object_play(SoundPlayerInternal& spi, handle<SoundPlayer> handle) {
        auto& sp = spi.sp;
        assert(spi.voice_index == -1);
        // find first free
        int index = 0;
        for (; index < active_voices.size(); index++)
            if (!active_voices[index].is_valid())
                break;
        if (index == active_voices.size())
            return;
        spi.voice_index = index;
        active_voices[index] = handle;
        spi.time_elapsed = 0.f;
        Mix_PlayChannel(index, sp.asset->internal_data, (sp.looping) ? -1 : 0);
    }

    void end_sound_object_play(SoundPlayerInternal& spi, handle<SoundPlayer> handle) {
        if (spi.voice_index != -1) {
            Mix_HaltChannel(spi.voice_index);
            active_voices.at(spi.voice_index) = { -1 };
            spi.voice_index = -1;
        }
        if (spi.is_oneshot) {
            assert(!spi.sp.looping);
            all_sound_players.free(handle.id);
        }
        else {
            if (!spi.sp.looping)
                spi.should_play = false;
        }
    }

    void tick(float dt) override {
        CPUFUNCTIONSTART;

        auto& sound_objs = all_sound_players.objects;

        // tick current sounds and end any bad ones
        for (int i = 0; i < active_voices.size(); i++) {
            auto& voice = active_voices[i];
            if (!voice.is_valid())
                continue;
            auto& obj = all_sound_players.get(voice.id);

            auto asset = obj.sp.asset;
            if (!asset) {
                end_sound_object_play(obj, voice);
                continue;
            }
            obj.time_elapsed += dt;
            if (obj.time_elapsed > asset->duration && !obj.sp.looping) {
                end_sound_object_play(obj, voice);
                continue;
            }
            if (obj.sp.is_spatial) {
                float dist_sq = glm::dot(obj.sp.spatial_pos - listener_position, obj.sp.spatial_pos - listener_position);
                if (dist_sq >= asset->maxRadius * asset->maxRadius) {
                    end_sound_object_play(obj, voice);
                    continue;
                }
            }
        }

        // get sound objects that want to play
        // sort them by distance
        // take n closest sounds and halt anything else

        std::vector<handle<SoundPlayer>> sorted_list;
        for (int i = 0; i < sound_objs.size(); i++) {
            auto& spi = sound_objs[i].type_;
            auto& sp = spi.sp;
            if (spi.voice_index!=-1&&!spi.should_play)
                continue;
            float dist = glm::length(sp.spatial_pos - listener_position);
            spi.computedAttenuation = sp.is_spatial ? get_attenuation_factor(sp.asset->attenuation, sp.asset->minRadius,sp.asset->maxRadius, dist) : 1.0;
            
            // add the handle to the sort list
            sorted_list.push_back({ sound_objs[i].handle });
        }
        // sort them
        std::sort(sorted_list.begin(), sorted_list.end(), [&](handle<SoundPlayer> a, handle<SoundPlayer> b) -> bool {
            return all_sound_players.get(a.id).computedAttenuation < all_sound_players.get(b.id).computedAttenuation;
            });
        
        // end any playing clips outside of MAX_VOICES
        for (int i = active_voices.size(); i < sorted_list.size(); i++) {
            auto handle = sorted_list[i];
            if (all_sound_players.get(handle.id).voice_index != -1) {
                end_sound_object_play(all_sound_players.get(handle.id), handle);
            }
        }
        // start any sounds that want to play but arent playing
        for (int i = 0; i < sorted_list.size() && i < active_voices.size(); i++) {
            auto handle = sorted_list[i];
            auto& spi = all_sound_players.get(handle.id);
            if (spi.voice_index == -1)
                begin_sound_object_play(spi, handle);
        }
    }

    void stop_all_sounds() override {
    
    }
    void set_listener_position(glm::vec3 p) override {
        listener_position = p;
    }

    void play_sound_one_shot_internal(const SoundFile* f, float vm, float pm, glm::vec3 p, bool spatialized) {
        assert(f);
        if (spatialized) {
            float dist_sq = glm::dot(listener_position - p, listener_position-p);
            if (dist_sq >= f->maxRadius*f->maxRadius)    // sound is beyond the max radius, skip it
                return;
        }
        
        auto handle = all_sound_players.make_new();
        SoundPlayerInternal& spi = all_sound_players.get(handle);
        spi.is_oneshot = true;
        spi.should_play = true;
        spi.sp.asset = f;
        spi.sp.spatial_pos = p;
        spi.sp.is_spatial = spatialized;
        spi.sp.looping = false;
        spi.sp.pitch_multiply = pm;
        spi.sp.volume_multiply = vm;
    }

    void play_sound(const SoundFile* asset, float volume_mult, float pitch_mult) {
        play_sound_one_shot_internal(asset, volume_mult, pitch_mult, {}, false);
    }
    void play_sound_3d(const SoundFile* asset, glm::vec3 position, float volume_mult, float pitch_mult) {
        play_sound_one_shot_internal(asset, volume_mult, pitch_mult, position, true);
    }

    // retained functions
    virtual handle<SoundPlayer> register_sound_player(const SoundPlayer& player) {
        return { -1 };
    }
    virtual void play_sound_player(handle<SoundPlayer> player) {

    }
    virtual void stop_sound_player(handle<SoundPlayer> player) {

    }
    virtual void update_sound_player(handle<SoundPlayer> handle, const SoundPlayer& player) {
        
    }
    virtual void remove_sound_player(handle<SoundPlayer>& handle) {

    }

    const SoundFile* load_sound_file(const std::string& file) {
        if (loaded_sounds.find(file)!=loaded_sounds.end()) {
            return loaded_sounds.find(file)->second;
        }

        std::string pathfull = SOUND_DIRECTORY + file;
        Mix_Chunk* data = Mix_LoadWAV(pathfull.c_str());
        if (!data) {
            sys_print("!!! couldnt load sound file %s\n", file.c_str());
            return nullptr;
        }
        SoundFile* sf = new SoundFile;
        sf->internal_data = data;
        sf->path = file;
        sf->duration = data->alen / 44100.0;

        loaded_sounds.insert({ file,sf });

        return sf;
    }
    void get_sound_player_status(handle<SoundPlayer> handle, float& duration, bool& finished) override {

    }

    std::unordered_map<std::string, SoundFile*> loaded_sounds;
    glm::vec3 listener_position{};
    glm::vec3 listener_front{};
    std::vector<handle<SoundPlayer>> active_voices;
	Free_List<SoundPlayerInternal> all_sound_players;
};