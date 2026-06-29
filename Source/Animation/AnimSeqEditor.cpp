#ifdef EDITOR_BUILD
#include "AnimSeqEditor.h"
#include "Animation/AnimEvent.h"
#include "Animation/AnimSidecarFile.h"
#include "Animation/AnimationSeqAsset.h"
#include "Animation/SkeletonData.h"
#include "Framework/CurveEditorImgui.h"
#include "Framework/Util.h"
#include "Render/Model.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Entity.h"
#include "Level.h"
#include "GameEnginePublic.h"
#include "LevelEditor/EditorDocLocal.h"
#include "LevelEditor/SelectionState.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
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
    for (auto& def : AnimEventRegistry::get().get_defs()) {
        if (std::find(names.begin(), names.end(), def.name) == names.end())
            names.push_back(def.name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

// ---------------------------------------------------------------------------
// Right-click canvas callback — add event menu
// ---------------------------------------------------------------------------

struct AddEventCtx {
    AnimSeqEditor* editor = nullptr;
    Model*         model  = nullptr;
    char custom_name[128] = {};
};

void AnimSeqEditor::on_right_click_canvas(CurveEditorImgui* ed) {
    auto* ctx = static_cast<AddEventCtx*>(ed->user_ptr);
    auto names = collect_model_event_names(ctx->model);

    ImGui::TextDisabled("Add Event");
    ImGui::Separator();

    for (auto& name : names) {
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
    if (ImGui::Button("Add Instant") && ctx->custom_name[0] != '\0') {
        auto item = std::make_unique<AnimEventEditorItem>();
        item->data.name = ctx->custom_name;
        item->data.is_duration = false;
        item->instant_item = true;
        item->color = COLOR_GREEN;
        ed->add_item_from_menu(std::move(item));
        ctx->custom_name[0] = '\0';
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Duration") && ctx->custom_name[0] != '\0') {
        auto item = std::make_unique<AnimEventEditorItem>();
        item->data.name = ctx->custom_name;
        item->data.is_duration = true;
        item->instant_item = false;
        item->color = COLOR_CYAN;
        ed->add_item_from_menu(std::move(item));
        ctx->custom_name[0] = '\0';
        ImGui::CloseCurrentPopup();
    }
}

// ---------------------------------------------------------------------------
// AnimSeqEditor
// ---------------------------------------------------------------------------

AnimSeqEditor::AnimSeqEditor() {
    curve_ed_ = std::make_unique<CurveEditorImgui>();
    curve_ed_->show_add_curve_button = false;
    curve_ed_->window_name = "Anim Sequence";
    curve_ed_->callback = &AnimSeqEditor::on_right_click_canvas;
}

AnimSeqEditor::~AnimSeqEditor() {
    cleanup_preview();
    Model::on_model_loaded.remove(this);
}

void AnimSeqEditor::set_asset(const std::string& asset_path) {
    cleanup_preview();
    Model::on_model_loaded.remove(this);

    asset_path_ = asset_path;
    asset_ = g_assets.find<AnimationSeqAsset>(asset_path).get();
    model_ = nullptr;
    clip_name_.clear();
    dirty_ = false;
    curve_ed_->clear_all();

    if (!asset_ || !asset_->srcModel || !asset_->seq) return;

    model_ = asset_->srcModel.get();

    // Extract clip name: asset path is "model/path/ClipName"
    auto pos = asset_path.rfind('/');
    clip_name_ = (pos != std::string::npos) ? asset_path.substr(pos + 1) : asset_path;

    // Listen for model reloads so we can re-sync without rebuilding the editor.
    Model::on_model_loaded.add(this, [this](Model* reloaded) {
        if (reloaded != model_) return;
        // Asset srcModel pointer may have changed — refresh.
        if (asset_) {
            clip_name_; // still valid; re-find seq
        }
        sync_editor_to_clip();
        activate_preview();
    });

    static AddEventCtx ctx;
    ctx.editor = this;
    ctx.model = model_;
    curve_ed_->user_ptr = &ctx;

    sync_editor_to_clip();
    activate_preview();
}

// ---------------------------------------------------------------------------
// Sync editor <-> AnimationSeq
// ---------------------------------------------------------------------------

void AnimSeqEditor::sync_editor_to_clip() {
    curve_ed_->clear_all();
    if (!model_ || !model_->get_skel()) return;

    auto* seq = model_->get_skel()->find_clip(clip_name_);
    if (!seq) return;

    curve_ed_->max_x_value = seq->get_duration() > 0.f ? seq->get_duration() : 5.f;

    for (int i = 0; i < (int)seq->anim_events.size(); ++i) {
        const AnimEvent& ev = seq->anim_events[i];
        auto item = std::make_unique<AnimEventEditorItem>();
        item->data       = ev;
        item->instant_item = !ev.is_duration;
        item->time_start = ev.time_start;
        item->time_end   = ev.is_duration ? ev.time_end : ev.time_start;
        item->y_coord    = i * 0.5f;
        item->color      = ev.is_duration ? COLOR_CYAN : COLOR_GREEN;
        curve_ed_->add_event_direct(std::move(item));
    }
    curve_ed_->request_fit();
    dirty_ = false;
}

void AnimSeqEditor::sync_clip_from_editor() {
    if (!model_ || !model_->get_skel()) return;
    auto* seq = model_->get_skel()->find_clip(clip_name_);
    if (!seq) return;

    seq->anim_events.clear();
    for (auto& item_ptr : curve_ed_->get_event_array()) {
        auto* item = static_cast<AnimEventEditorItem*>(item_ptr.get());
        AnimEvent ev = item->data;
        ev.is_duration = !item->instant_item;
        ev.time_start  = item->time_start;
        ev.time_end    = ev.is_duration ? item->time_end : item->time_start;
        seq->anim_events.push_back(std::move(ev));
    }
}

void AnimSeqEditor::apply_sidecar() {
    sync_clip_from_editor();
    if (AnimSidecarFile::save_from_model(model_)) {
        // reload is synchronous; on_model_loaded fires inside, which calls sync_editor_to_clip
        g_assets.reload(model_);
    }
}

void AnimSeqEditor::revert_editor() {
    dirty_ = false;
    sync_editor_to_clip(); // discards unsaved editor changes
}

// ---------------------------------------------------------------------------
// Preview via AnimPreviewComponent
// ---------------------------------------------------------------------------

void AnimSeqEditor::activate_preview() {
    cleanup_preview();
    if (!model_ || !asset_) return;

    // Only animate the entity the user has selected; never guess by scanning the scene.
    auto* editor_doc = static_cast<EditorDoc*>(eng->get_tool());
    if (!editor_doc || !editor_doc->selection_state) return;
    if (!editor_doc->selection_state->has_only_one_selected()) return;

    Entity* sel = editor_doc->selection_state->get_only_one_selected().get();
    if (!sel) return;

    auto* mesh = sel->get_component<MeshComponent>();
    if (!mesh || mesh->get_model() != model_) return;

    auto* apc = sel->get_component<AnimPreviewComponent>();
    if (!apc) {
        apc = sel->create_component<AnimPreviewComponent>();
        we_added_preview_comp_ = true;
    } else {
        we_added_preview_comp_ = false;
    }
    apc->model             = model_;
    apc->asset             = asset_;
    apc->wants_force_frame = !auto_play_;
    apc->force_frame       = 0;
    apc->update_mesh_component();
    preview_entity_ = EntityPtr(sel);
}

void AnimSeqEditor::cleanup_preview() {
    Entity* ent = preview_entity_.get();
    if (ent && we_added_preview_comp_) {
        auto* apc = ent->get_component<AnimPreviewComponent>();
        if (apc) apc->destroy();
    }
    preview_entity_ = EntityPtr{};
    we_added_preview_comp_ = false;
}

void AnimSeqEditor::update_preview_scrubber() {
    Entity* ent = preview_entity_.get();
    if (!ent) return;
    auto* apc = ent->get_component<AnimPreviewComponent>();
    if (!apc || !apc->asset || !apc->asset->seq) return;

    bool wants_force = !auto_play_;
    if (wants_force != apc->wants_force_frame) {
        // Mode changed — rebuild animator for the new mode (looping vs. frame-locked).
        apc->wants_force_frame = wants_force;
        apc->update_mesh_component();
    }

    if (wants_force && apc->eval) {
        float fps = apc->asset->seq->fps > 0.f ? apc->asset->seq->fps : 30.f;
        apc->force_frame = (int)(curve_ed_->current_time * fps);
        apc->eval->frame = apc->force_frame;
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void AnimSeqEditor::draw_toolbar() {
    ImGui::TextUnformatted(clip_name_.c_str());

    ImGui::SameLine();
    if (ImGui::Button(auto_play_ ? "[Auto]" : "[Manual]")) {
        auto_play_ = !auto_play_;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Toggle between manual scrubber and automatic looping playback");

    ImGui::SameLine();
    ImGui::BeginDisabled(!dirty_);
    if (ImGui::Button("Apply")) apply_sidecar();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!dirty_);
    if (ImGui::Button("Revert")) revert_editor();
    ImGui::EndDisabled();
}

void AnimSeqEditor::draw_event_properties() {
    SequencerEditorItem* sel = curve_ed_->get_selected_event();
    if (!sel) {
        ImGui::TextDisabled("(select an event to edit)");
        return;
    }
    auto* item = static_cast<AnimEventEditorItem*>(sel);
    auto known = collect_model_event_names(model_);

    ImGui::Text("Event");
    ImGui::Separator();

    // Name — combo with autocomplete
    ImGui::SetNextItemWidth(200.f);
    if (ImGui::BeginCombo("Name##evname", item->data.name.c_str())) {
        for (auto& n : known) {
            if (ImGui::Selectable(n.c_str(), n == item->data.name)) {
                item->data.name = n;
                const AnimEventDef* def = AnimEventRegistry::get().find(n);
                if (def) {
                    item->data.is_duration = def->is_duration;
                    item->instant_item     = !def->is_duration;
                }
                dirty_ = true;
            }
        }
        ImGui::EndCombo();
    }

    // Free-text override
    char buf[128];
    strncpy_s(buf, item->data.name.c_str(), sizeof(buf) - 1);
    ImGui::SetNextItemWidth(200.f);
    if (ImGui::InputText("Custom##evname2", buf, sizeof(buf))) {
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
        item->instant_item     = !is_dur;
        item->data.is_duration = is_dur;
        dirty_ = true;
    }
}

void AnimSeqEditor::imgui_draw() {
    if (!asset_ || !model_ || !model_->get_skel()) {
        ImGui::TextDisabled("Animation data unavailable.");
        return;
    }

    // Track editor selection — re-activate (or cleanup) whenever it changes.
    {
        auto* editor_doc = static_cast<EditorDoc*>(eng->get_tool());
        Entity* editor_sel = nullptr;
        if (editor_doc && editor_doc->selection_state &&
            editor_doc->selection_state->has_only_one_selected())
            editor_sel = editor_doc->selection_state->get_only_one_selected().get();

        if (editor_sel != preview_entity_.get())
            activate_preview();
    }

    update_preview_scrubber();
    draw_toolbar();
    ImGui::Separator();
    if (curve_ed_->draw_content())
        dirty_ = true;
    ImGui::Separator();
    draw_event_properties();
}

#endif // EDITOR_BUILD
