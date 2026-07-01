#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <memory>
#include <vector>
#include "Game/EntityPtr.h"

class AnimationSeqAsset;
class Model;
class AnimPreviewComponent;
class CurveEditorImgui;

// Animation sequence editor panel.
// Embedded in AssetInspectorPane when an AnimationSeqAsset is selected.
// Edits per-animation events and is_additive, stored in the .amd sidecar.
class AnimSeqEditor {
public:
    AnimSeqEditor();
    ~AnimSeqEditor();

    // Call when the selected AnimationSeqAsset changes.
    void set_asset(const std::string& asset_path);

    // Draw ImGui content (no Begin/End wrapper — caller owns the window).
    void imgui_draw();

    const std::string& get_asset_path() const { return asset_path_; }

private:
    void sync_editor_to_clip();          // populate CurveEditorImgui from AnimationSeq
    void sync_clip_from_editor();        // write editor events back to AnimationSeq
    void apply_sidecar();                // save .amd + reload model + re-sync editor
    void revert_editor();                // discard editor changes, re-sync from model

    // Additive import settings live in the model's .mis, NOT the .amd sidecar. Writing
    // them forces a (slow) model recompile, so they have their own load/apply path and
    // dirty flag, and we only write the .mis when one of them actually changed.
    void load_additive_settings();       // read this clip's AnimImportSettings from the .mis
    void draw_additive_settings();       // UI section
    void apply_additive_settings();      // write .mis + recompile (only if mis_dirty_)

    void activate_preview();             // find matching entity in scene, add AnimPreviewComponent
    void cleanup_preview();              // remove AnimPreviewComponent if we added it
    void update_preview_scrubber();      // sync current_time → force_frame each frame

    void draw_toolbar();
    void draw_event_properties();

    static void on_right_click_canvas(CurveEditorImgui* ed);

    std::string        asset_path_;
    AnimationSeqAsset* asset_ = nullptr; // non-owning, managed by AssetDatabase
    Model*             model_ = nullptr; // non-owning, stable across reload
    std::string        clip_name_;

    std::unique_ptr<CurveEditorImgui> curve_ed_;
    bool dirty_ = false;

    // Mirror of the current clip's additive import settings (.mis). `orig_*` holds the
    // last-loaded/saved values so we can tell whether a recompile is actually needed.
    std::string mis_path_;
    bool        mis_make_additive_   = false,  orig_make_additive_   = false;
    bool        mis_additive_self_   = false,  orig_additive_self_   = false;
    int         mis_additive_frame_  = 0,      orig_additive_frame_  = 0;
    std::string mis_subtract_clip_,            orig_subtract_clip_;
    bool        mis_dirty_ = false;

    // Preview
    EntityPtr preview_entity_;
    bool      we_added_preview_comp_ = false;
    bool      auto_play_ = false;
};

#endif // EDITOR_BUILD
