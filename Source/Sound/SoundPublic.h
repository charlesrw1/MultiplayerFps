#pragma once
#include "Assets/IAsset.h"
#include <glm/glm.hpp>

// rudimentary sound system
// one shot and retained sounds
// sound stealing based on distance to listener

enum class SndAttenuation : uint8_t
{
    Linear,
    ReverseLog,
    Inv,
    InvSquared,
};

struct Mix_Chunk;
CLASS_H(SoundFile, IAsset)
public:
private:
    Mix_Chunk* internal_data = nullptr;
    float duration = 0.0;
    glm::vec2 pitchRange;   // [min,max] to set pitch, (min=max to disable randomness)
    glm::vec2 volRange;     // [min,max] to set volume, (min=max to disable randomness)
    float minRadius = 1.f;  // [0,minRadius] sound will play without attenuation
    float maxRadius = 4.f;  // [minRadius,maxRadius] sound will attenuate
    float nonSpatializeRadius = 0.f;    // [0,nonSpatializeRadius] sound will be non spatialized (treated like 2d)
    SndAttenuation attenuation = SndAttenuation::Linear;    // how to attenuate the radius
    bool attenuate = true;  // dont attenuate
    bool spatialize = true; // dont spatialize

    struct SoundClip {
        Mix_Chunk* raw_data = nullptr;
        float summedWeight = 1.f;   // use for randomly picking sounds
    };
    //std::vector<SoundClip> sounds;

    void post_load(ClassBase*) {}
    bool load_asset(ClassBase*&);
    
    void uninstall() {
        delete internal_data;
        internal_data = nullptr;
    }
    void move_construct(IAsset* o) {
        *this = std::move(*(SoundFile*)o);
    }
    void sweep_references() const {}

    friend class SoundSystemLocal;
};

struct SoundPlayer
{
    const SoundFile* asset = nullptr;
    glm::vec3 spatial_pos{};
    float volume_multiply = 1.f;
    float pitch_multiply = 1.f;
    bool is_spatial = true; // true if sound is attenutated
    bool looping = false;   // if true, then sound will loop indefinitely
};

class SoundSystemPublic
{
public:
    virtual void init() = 0;
    virtual void tick(float dt) = 0;
    virtual void stop_all_sounds() = 0;

    // set the listener position of all spatial sounds
    virtual void set_listener_position(glm::vec3 p) = 0;

    // one shot functions
    virtual void play_sound(const SoundFile* asset, float volume_mult = 1.f, float pitch_mult = 1.f) = 0;
    virtual void play_sound_3d(const SoundFile* asset, glm::vec3 position, float volume_mult = 1.f, float pitch_mult = 1.f) = 0;

    // retained functions
    virtual handle<SoundPlayer> register_sound_player(const SoundPlayer& player) = 0;
    virtual void update_sound_player(handle<SoundPlayer> handle, const SoundPlayer& player) = 0;
    // play/stop the sound on the player
    virtual void play_sound_player(handle<SoundPlayer> player) = 0;
    virtual void stop_sound_player(handle<SoundPlayer> player) = 0;
    virtual void get_sound_player_status(handle<SoundPlayer> handle, float& duration, bool& finished) = 0;
    virtual void remove_sound_player(handle<SoundPlayer>& handle) = 0;
};
extern SoundSystemPublic* isound;