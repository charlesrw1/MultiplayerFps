#pragma once
#include <string>
#include <vector>

// Data-driven animation event. Replaces the old ClassBase-derived AnimationEvent/AnimDurationEvent system.
// Events are authored in the .amd sidecar and consumed at runtime by name string.
struct AnimEvent {
    std::string name;
    std::string payload;   // optional extra data, empty if unused
    float time_start = 0.f;
    float time_end   = 0.f; // equals time_start for instant events
    bool is_duration = false;
};

// Known event names for editor autocomplete and the right-click add-event menu.
// Populated at startup; gameplay code may also register names at init time.
struct AnimEventDef {
    std::string name;
    bool is_duration = false;
};

class AnimEventRegistry {
public:
    static AnimEventRegistry& get() {
        static AnimEventRegistry inst;
        return inst;
    }

    void add(AnimEventDef def) { defs.push_back(std::move(def)); }
    const std::vector<AnimEventDef>& get_defs() const { return defs; }

    // Returns nullptr if not found.
    const AnimEventDef* find(const std::string& name) const {
        for (auto& d : defs)
            if (d.name == name) return &d;
        return nullptr;
    }

private:
    std::vector<AnimEventDef> defs;
};
