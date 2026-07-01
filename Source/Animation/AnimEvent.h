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

// How a sampled event was triggered this frame.
enum class AnimEventTrigger : uint8_t {
    Entered, // animation time crossed time_start (all instant events; duration event began)
    Active,  // duration event is still active this frame (didn't cross time_start or time_end)
    Left,    // animation time crossed time_end (duration events only)
};

// One sampled animation event collected during a single AnimatorObject::update().
// Cleared at the start of each update and rebuilt by clip nodes traversing the blend tree.
struct SampledAnimEvent {
    const AnimEvent*  event        = nullptr; // points into AnimationSeq::anim_events (stable until model reload)
    float             sampled_time = 0.f;     // animation clock (seconds) at the crossing
    float             weight       = 1.f;     // blend weight of the source clip in [0,1]
    bool              b_mirrored   = false;   // true if the event came through a mirrored pathway
    AnimEventTrigger  trigger      = AnimEventTrigger::Entered;

    // Convenience helpers (require knowledge of the source clip's duration)
    float time_through_event() const {
        if (!event || !event->is_duration) return 0.f;
        return sampled_time - event->time_start;
    }
    float duration_of_event() const {
        if (!event || !event->is_duration) return 0.f;
        return event->time_end - event->time_start;
    }
    float percent_through_event() const {
        float d = duration_of_event();
        return d > 0.f ? time_through_event() / d : 0.f;
    }
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
