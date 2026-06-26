#pragma once
#include "Assets/IAsset.h"
#include "Framework/Reflection2.h"
#include "PPManager.h"

// Asset file extension: .ppset
// Holds post-process parameters that can be shared across multiple PostProcessComponents.
class PostProcessSettings : public IAsset {
public:
    CLASS_BODY(PostProcessSettings);
    bool load_asset() final;
    void post_load() final {}
    void uninstall() final {}

    REF float exposure           = 1.f;
    REF float contrast           = 1.f;
    REF float saturation         = 1.f;
    REF float bloom_intensity    = 0.05f;
    REF bool  bloom_enabled      = true;
    REF int   tonemap_type       = 0; // 0=linear,1=reinhard,2=aces,3=uncharted2
    REF float vignette_intensity = 0.f;
    REF float vignette_falloff   = 1.5f;
    REF float chromatic_ab       = 0.f;
    REF float grain_intensity    = 0.f;
    REF float grain_size         = 1.f;
    REF float sharpness          = 0.f;
    REF float color_temp         = 0.f; // -1=cool, +1=warm
    // CDL primary grading — not REF (handled by color wheel custom UI)
    glm::vec3 lift      = {0.f, 0.f, 0.f};
    glm::vec3 gamma_rgb = {1.f, 1.f, 1.f};
    glm::vec3 gain      = {1.f, 1.f, 1.f};

    REF bool  auto_exposure = false;
    REF int   ae_method     = 0;
    REF float ae_min_ev     = -3.f;
    REF float ae_max_ev     =  3.f;
    REF float ae_speed      =  1.f;
    REF float ae_key        = 0.18f;

    PostProcessParams to_params() const {
        return {exposure, contrast, saturation, bloom_intensity, bloom_enabled, tonemap_type,
                vignette_intensity, vignette_falloff, chromatic_ab,
                grain_intensity, grain_size, sharpness, color_temp,
                lift, gamma_rgb, gain,
                auto_exposure, ae_method, ae_min_ev, ae_max_ev, ae_speed, ae_key};
    }

    void save_to_disk();
};
