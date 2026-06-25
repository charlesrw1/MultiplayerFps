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

    PostProcessParams to_params() const {
        return {exposure, contrast, saturation, bloom_intensity, bloom_enabled, tonemap_type,
                vignette_intensity, vignette_falloff, chromatic_ab,
                grain_intensity, grain_size, sharpness, color_temp};
    }

    void save_to_disk();
};
