#pragma once
#include "Game/EntityComponent.h"
#include "Sound/SoundPublic.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/Reflection2.h"
class MeshBuilderComponent;
NEWCLASS(SoundComponent, EntityComponent)
public:
    SoundComponent();
    void start() override;
    void end() override;
    void update() override;
    void editor_on_change_property() override;
    void on_changed_transform() override;


    // can use the component like an "Asset" for playing sounds with settings
    void play_one_shot_at_pos(const glm::vec3& v) ;
    
    REFLECT();
    void play_one_shot() {  // const (get_ws_position isnt const, fixme)
        play_one_shot_at_pos(get_ws_position());
    }

    REFLECT();
    void set_pitch(float f) {
        if (player)
            player->pitch_multiply = f;
    }
    REFLECT();
    void set_play(bool b) {
        if (player)
            player->set_play(b);
    }
    void set_enable_sound_on_start(bool b) {
        enable_on_start = b;
    }
private:
    // SETTINGS
    glm::vec2 pitchRange;   // [min,max] to set pitch, (min=max to disable randomness)
    glm::vec2 volRange;     // [min,max] to set volume, (min=max to disable randomness)
    REFLECT();
    float minRadius = 1.f;  // [0,minRadius] sound will play without attenuation
    REFLECT();
    float maxRadius = 5.f;  // [minRadius,maxRadius] sound will attenuate
    float nonSpatializeRadius = 0.f;    // [0,nonSpatializeRadius] sound will be non spatialized (treated like 2d)
    REFLECT();
    int attenuation = 0;    // how to attenuate the radius
    REFLECT();
    bool attenuate = true;
    REFLECT();
    bool spatialize = true;
    REFLECT();
    bool looping = false;
    REFLECT();
    AssetPtr<SoundFile> sound;
    REFLECT();
    bool enable_on_start = false;

    // STATE
    SoundPlayer* player = nullptr;

    REFLECT(type="BoolButton", transient,hint="Test Sound");
    bool editor_test_sound = false;
#ifdef EDITOR_BUILD
    MeshBuilderComponent* editor_mesh = nullptr;
    void update_ed_mesh();
#endif
    void update_player();
};