#pragma once
#include "Game/EntityComponent.h"
#include "Sound/SoundPublic.h"
#include "Game/SerializePtrHelpers.h"
class MeshBuilderComponent;
CLASS_H(SoundComponent, EntityComponent)
public:
    SoundComponent();
    void start() override;
    void end() override;
    void update() override;
    void editor_on_change_property() override;
    void on_changed_transform() override;


    // can use the component like an "Asset" for playing sounds with settings
    void play_one_shot_at_pos(const glm::vec3& v) ;
    void play_one_shot() {  // const (get_ws_position isnt const, fixme)
        play_one_shot_at_pos(get_ws_position());
    }

    static const PropertyInfoList* get_props();

    void set_pitch(float f) {
        if (player)
            player->pitch_multiply = f;
    }
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
    float minRadius = 1.f;  // [0,minRadius] sound will play without attenuation
    float maxRadius = 5.f;  // [minRadius,maxRadius] sound will attenuate
    float nonSpatializeRadius = 0.f;    // [0,nonSpatializeRadius] sound will be non spatialized (treated like 2d)
    SndAttenuation attenuation = SndAttenuation::Linear;    // how to attenuate the radius
    bool attenuate = true;
    bool spatialize = true;
    bool looping = false;
    AssetPtr<SoundFile> sound;
    bool enable_on_start = false;

    // STATE
    SoundPlayer* player = nullptr;

#ifdef EDITOR_BUILD
    bool editor_test_sound = false;
    MeshBuilderComponent* editor_mesh = nullptr;
    void update_ed_mesh();
#endif
    void update_player();
};