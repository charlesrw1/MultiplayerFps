#pragma once
#include <vector>

struct PostProcessParams {
    float exposure        = 1.f;
    float contrast        = 1.f;
    float saturation      = 1.f;
    float bloom_intensity = 0.05f;
    bool  bloom_enabled   = true;
    int   tonemap_type    = 0; // 0=linear,1=reinhard,2=aces,3=uncharted2
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
