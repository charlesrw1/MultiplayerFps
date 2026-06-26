#pragma once
#include <vector>
#include "glm/vec3.hpp"

struct PostProcessParams {
    float exposure          = 1.f;
    float contrast          = 1.f;
    float saturation        = 1.f;
    float bloom_intensity   = 0.05f;
    bool  bloom_enabled     = true;
    int   tonemap_type      = 0;   // 0=linear,1=reinhard,2=aces,3=uncharted2
    float vignette_intensity = 0.f;
    float vignette_falloff   = 1.5f;
    float chromatic_ab       = 0.f; // UV fraction; 0=off
    float grain_intensity    = 0.f;
    float grain_size         = 1.f; // higher = finer grain
    float sharpness          = 0.f;
    float color_temp         = 0.f; // -1=cool, +1=warm
    glm::vec3 lift     = {0.f, 0.f, 0.f}; // shadow offset (CDL)
    glm::vec3 gamma_rgb = {1.f, 1.f, 1.f}; // midtone power (CDL)
    glm::vec3 gain     = {1.f, 1.f, 1.f}; // highlight scale (CDL)
    // Auto-exposure
    bool  auto_exposure = false;
    int   ae_method     = 0;    // 0=downsample(bloom), 1=histogram
    float ae_min_ev     = -3.f; // min log2 exposure clamp
    float ae_max_ev     =  3.f; // max log2 exposure clamp
    float ae_speed_up   =  3.f; // dark→bright adaptation speed (1/seconds)
    float ae_speed_down =  1.f; // bright→dark adaptation speed (1/seconds)
    float ae_key        = 0.18f;// target middle grey
    float ae_low_pct    =  0.4f;// fraction of darkest pixels to exclude (0=none)
    float ae_high_pct   =  0.1f;// fraction of brightest pixels to exclude (0=none)
};

class PPManager {
public:
    static PPManager* inst;

    PPManager();
    ~PPManager();

    struct Handle {
        int id = -1;
        bool is_valid() const { return id >= 0; }
    };

    Handle register_settings(int priority = 0);
    void   update_settings(Handle h, const PostProcessParams& p);
    void   remove_settings(Handle& h);

    // push_override = register at high priority; pop_override to remove
    Handle push_override(const PostProcessParams& p, int priority = 100);
    void   pop_override(Handle& h);

    PostProcessParams get_active() const;

private:
    struct Entry {
        PostProcessParams params;
        int  priority = 0;
        bool valid    = false;
    };
    std::vector<Entry> entries_;
    PostProcessParams  defaults_;
};
