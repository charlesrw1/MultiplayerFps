#ifdef EDITOR_BUILD
#include "AnimSeqEditor.h"
#include "Animation/AnimEvent.h"
#include "Animation/AnimSidecarFile.h"
#include "Animation/SkeletonData.h"
#include "Framework/CurveEditorImgui.h"
#include "Framework/Util.h"
#include "Render/Model.h"
#include "Assets/AssetDatabase.h"
#include "AssetCompile/Someutils.h"
#include <imgui.h>
#include <algorithm>

// ---------------------------------------------------------------------------
// AnimEventEditorItem — SequencerEditorItem backed by an AnimEvent
// ---------------------------------------------------------------------------

class AnimEventEditorItem : public SequencerEditorItem {
public:
    AnimEvent data;
    std::string get_name() override { return data.name.empty() ? "(unnamed)" : data.name; }
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static AnimationSeq* get_current_seq(Model* model, const std::vector<std::string>& clip_names, int idx) {
    if (!model || idx < 0 || idx >= (int)clip_names.size()) return nullptr;
    return model->get_skel() ? model->get_skel()->find_clip(clip_names[idx]) : nullptr;
}

// Collect all unique event names from every animation on this model (for autocomplete).
static std::vector<std::string> collect_model_event_names(Model* model) {
    std::vector<std::string> names;
    if (!model || !model->get_skel()) return names;
    for (auto& [clip_name, refed] : model->get_skel()->get_clips_hashmap()) {
        if (!refed.ptr) continue;
        for (auto& ev : refed.ptr->anim_events) {
            if (!ev.name.empty() &&
                std::find(names.begin(), names.end(), ev.name) == names.end())
                names.push_back(ev.name);
        }
    }
    // Also add registry names not already present.
    for (auto& def : AnimEventRegistry::get().get_defs()) {
        if (std::find(names.begin(), names.end(), def.name) == names.end())
            names.push_back(def.name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

// ---------------------------------------------------------------------------
// Right-click canvas callback — add new event
// ---------------------------------------------------------------------------

struct AddEventCtx {
    AnimSeqEditor* editor;
    std::vector<std::string> known_names;
    char custom_name[128] = {};
};

// user_ptr on the CurveEditorImgui points to AddEventCtx.
void AnimSeqEditor::on_right_click_canvas(CurveEditorImgui* ed) {
    auto* ctx = static_cast<AddEventCtx*>(ed->user_ptr);

    ImGui::TextDisabled("Add Event");
    ImGui::Separator();

    // Known names from registry + other animations
    for (auto& name : ctx->known_names) {
        const AnimEventDef* def = AnimEventRegistry::get().find(name);
        bool is_dur = def ? def->is_duration : false;
        std::string label = name + (is_dur ? "  [dur]" : "");
        if (ImGui::MenuItem(label.c_str())) {
            auto item = std::make_unique<AnimEventEditorItem>();
            item->data.name = name;
            item->data.is_duration = is_dur;
            item->instant_item = !is_dur;
            item->color = is_dur ? COLOR_CYAN : COLOR_GREEN;
            ed->add_item_from_menu(std::move(item));
        }
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(140.f);
    ImGui::InputText("##custom", ctx->custom_name, sizeof(ctx->custom_name));
    ImGui::SameLine();
    if (ImGui::Button("Add Custom")) {
        if (ctx->custom_name[0] != '\0') {
            auto item = std::make_unique<AnimEventEditorItem>();
            item->data.name = ctx->custom_name;
            item->instant_item = true;
            item->color = COLOR_GREEN;
            ed->add_item_from_menu(std::move(item));
            ctx->custom_name[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
    }
}

// ---------------------------------------------------------------------------
// AnimSeqEditor
// ---------------------------------------------------------------------------

AnimSeqEditor::AnimSeqEditor() {
    curve_ed_ = std::make_unique<CurveEditorImgui>();
    curve_ed_->show_add_curve_button = false; // events only, no curves
    curve_ed_->window_name = "Anim Sequence";
    curve_ed_->callback = &AnimSeqEditor::on_right_click_canvas;
}

AnimSeqEditor::~AnimSeqEditor() = default;

void AnimSeqEditor::set_model(const std::string& cmdl_game_path) {
    model_path_ = cmdl_game_path;
    model_ = Model::load(cmdl_game_path);
    selected_clip_idx_ = 0;
    dirty_ = false;
    load_clips_from_model();
    sync_editor_to_clip();
}

void AnimSeqEditor::load_clips_from_model() {
    clip_names_.clear();
    if (!model_ || !model_->get_skel()) return;
    for (auto& [name, _] : model_->get_skel()->get_clips_hashmap())
        clip_names_.push_back(name);
    std::sort(clip_names_.begin(), clip_names_.end());
}

void AnimSeqEditor::sync_editor_to_clip() {
    curve_ed_->clear_all();
    AnimationSeq* seq = get_current_seq(model_, clip_names_, selected_clip_idx_);
    if (!seq) return;

    curve_ed_->max_x_value = seq->get_duration() > 0.f ? seq->get_duration() : 5.f;

    for (int i = 0; i < (int)seq->anim_events.size(); ++i) {
        const AnimEvent& ev = seq->anim_events[i];
        auto item = std::make_unique<AnimEventEditorItem>();
        item->data = ev;
        item->instant_item = !ev.is_duration;
        item->time_start = ev.time_start;
        item->time_end   = ev.is_duration ? ev.time_end : ev.time_start;
        item->y_coord    = i * 0.5f;
        item->color      = ev.is_duration ? COLOR_CYAN : COLOR_GREEN;
        curve_ed_->add_event_direct(std::move(item));
    }

    // Refresh autocomplete context
    static AddEventCtx ctx;
    ctx.editor = this;
    ctx.known_names = collect_model_event_names(model_);
    curve_ed_->user_ptr = &ctx;
    curve_ed_->request_fit();
}

void AnimSeqEditor::sync_clip_from_editor() {
    AnimationSeq* seq = get_current_seq(model_, clip_names_, selected_clip_idx_);
    if (!seq) return;

    seq->anim_events.clear();
    for (auto& item_ptr : curve_ed_->get_event_array()) {
        auto* item = static_cast<AnimEventEditorItem*>(item_ptr.get());
        AnimEvent ev = item->data;
        ev.time_start  = item->time_start;
        ev.time_end    = item->instant_item ? item->time_start : item->time_end;
        ev.is_duration = !item->instant_item;
        seq->anim_events.push_back(std::move(ev));
    }
}

void AnimSeqEditor::save_sidecar() {
    sync_clip_from_editor();
    if (AnimSidecarFile::save_from_model(model_)) {
        // Reload the model so the runtime picks up the new .amd immediately.
        g_assets.reload(model_);
        dirty_ = false;
    }
}

// ---------------------------------------------------------------------------
// ImGui draw
// ---------------------------------------------------------------------------

void AnimSeqEditor::draw_toolbar() {
    // Animation selector
    ImGui::SetNextItemWidth(200.f);
    const char* preview = clip_names_.empty() ? "(no clips)" :
                          clip_names_[selected_clip_idx_].c_str();
    if (ImGui::BeginCombo("##clip", preview)) {
        for (int i = 0; i < (int)clip_names_.size(); ++i) {
            if (ImGui::Selectable(clip_names_[i].c_str(), i == selected_clip_idx_)) {
                sync_clip_from_editor(); // write pending edits back before switching
                selected_clip_idx_ = i;
                sync_editor_to_clip();
                dirty_ = true;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // Additive toggle
    AnimationSeq* seq = get_current_seq(model_, clip_names_, selected_clip_idx_);
    if (seq) {
        bool additive = seq->is_additive_clip;
        if (ImGui::Checkbox("Additive", &additive)) {
            seq->is_additive_clip = additive;
            dirty_ = true;
        }
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!dirty_);
    if (ImGui::Button("Save")) save_sidecar();
    ImGui::EndDisabled();
}

void AnimSeqEditor::draw_event_properties() {
    SequencerEditorItem* sel = curve_ed_->get_selected_event();
    if (!sel) {
        ImGui::TextDisabled("(select an event to edit)");
        return;
    }
    auto* item = static_cast<AnimEventEditorItem*>(sel);

    // Collect known names for the combo
    static std::vector<std::string> s_known;
    s_known = collect_model_event_names(model_);

    ImGui::Text("Event");
    ImGui::Separator();

    // Name — combo with autocomplete
    ImGui::SetNextItemWidth(200.f);
    if (ImGui::BeginCombo("Name##evname", item->data.name.c_str())) {
        for (auto& n : s_known) {
            bool sel_name = n == item->data.name;
            if (ImGui::Selectable(n.c_str(), sel_name)) {
                item->data.name = n;
                dirty_ = true;
                // Sync duration type from registry if known
                const AnimEventDef* def = AnimEventRegistry::get().find(n);
                if (def) {
                    item->data.is_duration = def->is_duration;
                    item->instant_item = !def->is_duration;
                }
            }
        }
        ImGui::EndCombo();
    }

    // Custom name input
    char buf[128];
    strncpy_s(buf, item->data.name.c_str(), sizeof(buf) - 1);
    ImGui::SetNextItemWidth(200.f);
    if (ImGui::InputText("Custom##evname_custom", buf, sizeof(buf))) {
        item->data.name = buf;
        dirty_ = true;
    }

    // Payload
    char pbuf[256];
    strncpy_s(pbuf, item->data.payload.c_str(), sizeof(pbuf) - 1);
    ImGui::SetNextItemWidth(200.f);
    if (ImGui::InputText("Payload", pbuf, sizeof(pbuf))) {
        item->data.payload = pbuf;
        dirty_ = true;
    }

    // Duration toggle
    bool is_dur = !item->instant_item;
    if (ImGui::Checkbox("Duration", &is_dur)) {
        item->instant_item = !is_dur;
        item->data.is_duration = is_dur;
        dirty_ = true;
    }

    ImGui::Spacing();
    if (ImGui::Button("Delete Event")) {
        // Remove from editor — next sync_clip_from_editor will drop it from the seq
        // CurveEditorImgui doesn't expose a direct delete; force via right-click popup.
        // For now mark dirty so Save will omit it after user deletes via right-click.
        dirty_ = true;
    }
}

void AnimSeqEditor::imgui_draw() {
    if (!model_ || !model_->get_skel()) {
        ImGui::TextDisabled("No skeleton on this model.");
        return;
    }

    draw_toolbar();
    ImGui::Separator();

    // Curve editor (events only)
    // Mark dirty when scrubber or events change
    bool was_dirty = dirty_;
    curve_ed_->draw_content();
    if (!was_dirty) {
        // Check if any event was dragged (heuristic: compare event count/times next frame)
        // For simplicity, mark dirty on any interaction during draw
    }

    ImGui::Separator();
    draw_event_properties();
}

#endif // EDITOR_BUILD
