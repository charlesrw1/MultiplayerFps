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

    REF float exposure        = 1.f;
    REF float contrast        = 1.f;
    REF float saturation      = 1.f;
    REF float bloom_intensity = 0.05f;
    REF bool  bloom_enabled   = true;
    REF int   tonemap_type    = 0; // 0=linear,1=reinhard,2=aces,3=uncharted2

    PostProcessParams to_params() const {
        return {exposure, contrast, saturation, bloom_intensity, bloom_enabled, tonemap_type};
    }

    void save_to_disk();
};
