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
