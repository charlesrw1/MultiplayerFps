#pragma once
#include "Assets/IAsset.h"
#include "Framework/Reflection2.h"
#include "Framework/CurveEditorImgui.h"
#include "PPManager.h"
#include "Render/Texture.h"

// Flat curve at y=1 across the whole domain: matches the pre-curve behavior
// (every bloom mip contributes with an unweighted upsample).
inline EditingCurve make_flat_bloom_curve() {
    EditingCurve c;
    c.name = "Bloom Mip Curve";
    CurvePoint p0; p0.time = 0.f; p0.value = 1.f; p0.type = CurvePointType::Linear;
    CurvePoint p1; p1.time = 1.f; p1.value = 1.f; p1.type = CurvePointType::Linear;
    c.points = {p0, p1};
    return c;
}

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
    REF float bloom_filter_radius = 0.005f; // upsample tent-filter radius, in UV space
    REF AssetPtr<Texture> bloom_lens_dirt;  // optional dirt/smudge mask
    REF float bloom_lens_dirt_intensity = 0.f;
    // Per-mip upsample weight curve. X = normalized mip index (0=largest/first
    // upsample .. 1=smallest/last mip), Y = weight (1=default/unweighted).
    // Not REF: edited via the dedicated curve popup in PostProcessComponentEditorUi,
    // like MinMaxCurve fields elsewhere.
    EditingCurve bloom_mip_curve = make_flat_bloom_curve();
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
    REF float ae_speed_up   =  3.f;
    REF float ae_speed_down =  1.f;
    REF float ae_key        = 0.18f;
    REF float ae_low_pct    =  0.4f; // fraction of darkest pixels to exclude
    REF float ae_high_pct   =  0.1f; // fraction of brightest pixels to exclude

    PostProcessParams to_params() const;

    void save_to_disk();
};
