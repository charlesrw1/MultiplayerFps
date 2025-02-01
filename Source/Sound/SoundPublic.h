#pragma once
#include "Assets/IAsset.h"
#include <glm/glm.hpp>

// rudimentary sound system
// one shot and retained sounds
// sound stealing based on distance to listener

enum class SndAttenuation : uint8_t
{
    Linear,  // (1-x)
    Cubic,    // (1-x)^3
    InvCubic, // (1-x)^(1/3)
};

struct Mix_Chunk;
CLASS_H(SoundFile, IAsset)
public:
private:
    Mix_Chunk* internal_data = nullptr;
    float duration = 0.0;

    void post_load(ClassBase*) {}
    bool load_asset(ClassBase*&);
    
    void uninstall();
    void move_construct(IAsset* o) {
        *this = std::move(*(SoundFile*)o);
    }
    void sweep_references() const {}

    friend class SoundSystemLocal;
};

class SoundPlayer
{
public:
    const SoundFile* asset = nullptr;
    glm::vec3 spatial_pos{};
    float volume_multiply = 1.f;
    float pitch_multiply = 1.f;
    bool looping = false;   // if true, then sound will loop indefinitely
    float minRadius = 1.f;  // [0,minRadius] sound will play without attenuation
    float maxRadius = 4.f;  // [minRadius,maxRadius] sound will attenuate
    float nonSpatializeRadius = 0.f;    // [0,nonSpatializeRadius] sound will be non spatialized (treated like 2d)
    SndAttenuation attenuation = SndAttenuation::Linear;    // how to attenuate the radius
    bool attenuate = true;
    bool spatialize = true;

    // after changes, call update
    void update();
    // starts/stops playback
    void set_play(bool b);
    // returns -1 if finished, else current duration
    float get_status();
};

class SoundSystemPublic
{
public:
    virtual void init() = 0;
    virtual void cleanup() = 0;
    virtual void tick(float dt) = 0;
    virtual void stop_all_sounds() = 0;

    // set the listener position of all spatial sounds
    virtual void set_listener_position(glm::vec3 pos, glm::vec3 listener_right_vec) = 0;

    // one shot play
    virtual void play_sound(
        const SoundFile* asset,
        float vol,
        float pitch,
        float min_rad,
        float max_rad,
        SndAttenuation attn,
        bool attenuate,
        bool spaitialize,
        glm::vec3 spatial_pos
    ) = 0;

    // retained functions
    virtual SoundPlayer* register_sound_player() = 0;
    virtual void remove_sound_player(SoundPlayer*& sp) = 0;
};
extern SoundSystemPublic* isound;