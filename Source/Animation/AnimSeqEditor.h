#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <memory>
#include <vector>

class Model;
class CurveEditorImgui;
struct AnimEvent;

// Animation sequence editor panel.
// Embedded inside AssetInspectorPane when a .cmdl is selected.
// Edits per-animation events and the is_additive flag, stored in the .amd sidecar.
class AnimSeqEditor {
public:
    AnimSeqEditor();
    ~AnimSeqEditor();

    // Call when the selected .cmdl changes. Loads the model and sidecar state.
    void set_model(const std::string& cmdl_game_path);

    const std::string& get_model_path() const { return model_path_; }

    // Draw ImGui content (no Begin/End wrapper — caller owns the window).
    void imgui_draw();

private:
    void load_clips_from_model();
    void sync_editor_to_clip(); // populate CurveEditorImgui from current clip
    void sync_clip_from_editor(); // write editor events back to AnimationSeq
    void save_sidecar();

    void draw_toolbar();
    void draw_event_properties();

    static void on_right_click_canvas(CurveEditorImgui* ed);

    std::string model_path_;
    Model* model_ = nullptr; // non-owning, managed by AssetManager

    std::vector<std::string> clip_names_;
    int selected_clip_idx_ = 0;

    std::unique_ptr<CurveEditorImgui> curve_ed_;
    bool dirty_ = false;
};

#endif // EDITOR_BUILD
