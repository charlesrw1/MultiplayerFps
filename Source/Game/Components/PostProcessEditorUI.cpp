#ifdef EDITOR_BUILD
#include "PostProcessEditorUI.h"
#include "PostProcessComponent.h"
#include "Render/PostProcessSettings.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetBrowser.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "glm/vec3.hpp"
#include <json.hpp>
#include "imgui.h"

static bool create_ppset_file(const std::string& path) {
    if (FileSys::does_file_exist(path.c_str(), FileSys::GAME_DIR))
        return false;
    nlohmann::json j;
    j["exposure"]           = 1.f;
    j["contrast"]           = 1.f;
    j["saturation"]         = 1.f;
    j["bloom_intensity"]    = 0.05f;
    j["bloom_enabled"]      = true;
    j["tonemap_type"]       = 0;
    j["vignette_intensity"] = 0.f;
    j["vignette_falloff"]   = 1.5f;
    j["chromatic_ab"]       = 0.f;
    j["grain_intensity"]    = 0.f;
    j["grain_size"]         = 1.f;
    j["sharpness"]          = 0.f;
    j["color_temp"]         = 0.f;
    j["lift"]      = {0.f, 0.f, 0.f};
    j["gamma_rgb"] = {1.f, 1.f, 1.f};
    j["gain"]      = {1.f, 1.f, 1.f};
    std::string text = j.dump(2);
    auto f = FileSys::open_write_game(path);
    if (!f) return false;
    f->write(text.data(), text.size());
    f->close();
    return true;
}

bool PostProcessComponentEditorUi::draw() {
    auto* asset = comp->settings.get();

    if (!asset) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No settings asset assigned");
        if (ImGui::Button("Create New...")) {
            show_create_popup = true;
            memset(create_name, 0, sizeof(create_name));
        }

        if (show_create_popup)
            ImGui::OpenPopup("Create PostProcess Asset");

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Create PostProcess Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            std::string folder;
            if (AssetBrowser::inst)
                folder = AssetBrowser::inst->selected_folder;
            ImGui::TextDisabled("Folder: %s", folder.empty() ? "(root)" : folder.c_str());
            ImGui::Text("Name (no extension):");
            bool enter = ImGui::InputText("##name", create_name, sizeof(create_name), ImGuiInputTextFlags_EnterReturnsTrue);

            bool do_create = enter || ImGui::Button("Create", ImVec2(120, 0));
            ImGui::SameLine();
            bool do_cancel = ImGui::Button("Cancel", ImVec2(120, 0));

            if (do_create && strlen(create_name) > 0) {
                std::string base = StringUtils::strip_extension(create_name);
                if (base.empty()) base = create_name;
                std::string path = folder.empty() ? (base + ".ppset") : (folder + "/" + base + ".ppset");
                if (create_ppset_file(path)) {
                    comp->settings = g_assets.find<PostProcessSettings>(path);
                    sys_print(Info, "Created PostProcessSettings: %s\n", path.c_str());
                } else {
                    sys_print(Warning, "Failed to create PostProcessSettings (already exists?)\n");
                }
                show_create_popup = false;
                ImGui::CloseCurrentPopup();
            }
            if (do_cancel) {
                show_create_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        return false;
    }

    // --- Parameter editing ---
    bool changed = false;
    auto slider = [&](const char* label, float& v, float lo, float hi, const char* fmt = "%.3f") {
        ImGui::SetNextItemWidth(180);
        changed |= ImGui::SliderFloat(label, &v, lo, hi, fmt);
    };

    ImGui::PushID("ppset_tonemap");
    if (ImGui::CollapsingHeader("Tonemap & Exposure", ImGuiTreeNodeFlags_DefaultOpen)) {
        static const char* tonemap_names[] = { "Linear", "Reinhard", "ACES", "Uncharted 2" };
        int tm = asset->tonemap_type;
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("Tonemap", &tm, tonemap_names, 4)) { asset->tonemap_type = tm; changed = true; }
        slider("Exposure", asset->exposure, 0.f, 8.f);
    }
    ImGui::PopID();

    ImGui::PushID("ppset_grade");
    if (ImGui::CollapsingHeader("Color Grading", ImGuiTreeNodeFlags_DefaultOpen)) {
        slider("Contrast",   asset->contrast,   0.f, 3.f);
        slider("Saturation", asset->saturation, 0.f, 3.f);
        slider("Color Temp", asset->color_temp, -1.f, 1.f);
    }
    ImGui::PopID();

    ImGui::PushID("ppset_lgq");
    if (ImGui::CollapsingHeader("Lift / Gamma / Gain")) {
        // Picker 0.5 = neutral for all bands. lift: neutral=0 range=0.2; gamma/gain: neutral=1 range=1
        struct LgqBand { const char* label; glm::vec3* val; float neutral; float range;
                         const char* pid; const char* rid; };
        LgqBand bands[] = {
            { "Shadows",    &asset->lift,      0.f, 0.2f, "##lift_w", "R##lift_r" },
            { "Midtones",   &asset->gamma_rgb, 1.f, 1.0f, "##gam_w",  "R##gam_r"  },
            { "Highlights", &asset->gain,      1.f, 1.0f, "##gain_w", "R##gain_r" },
        };
        constexpr int picker_flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel |
                                     ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs;

        if (ImGui::BeginTable("##lgq_tbl", 3, ImGuiTableFlags_SizingStretchSame)) {
            // Row 1: labels + reset buttons
            ImGui::TableNextRow();
            for (auto& b : bands) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(b.label);
                ImGui::SameLine();
                if (ImGui::SmallButton(b.rid)) { *b.val = glm::vec3(b.neutral); changed = true; }
            }
            // Row 2: color wheels (picker fills column width → automatically small)
            ImGui::TableNextRow();
            for (auto& b : bands) {
                ImGui::TableNextColumn();
                float col[4] = {
                    (b.val->r - b.neutral) / (2.f * b.range) + 0.5f,
                    (b.val->g - b.neutral) / (2.f * b.range) + 0.5f,
                    (b.val->b - b.neutral) / (2.f * b.range) + 0.5f,
                    1.f
                };
                for (int i = 0; i < 3; ++i) col[i] = col[i] < 0.f ? 0.f : (col[i] > 1.f ? 1.f : col[i]);
                if (ImGui::ColorPicker4(b.pid, col, picker_flags)) {
                    b.val->r = (col[0] - 0.5f) * 2.f * b.range + b.neutral;
                    b.val->g = (col[1] - 0.5f) * 2.f * b.range + b.neutral;
                    b.val->b = (col[2] - 0.5f) * 2.f * b.range + b.neutral;
                    changed = true;
                }
            }
            // Row 3: compact RGB readout
            ImGui::TableNextRow();
            for (auto& b : bands) {
                ImGui::TableNextColumn();
                ImGui::Text("%.2f %.2f %.2f", b.val->r, b.val->g, b.val->b);
            }
            ImGui::EndTable();
        }
    }
    ImGui::PopID();

    ImGui::PushID("ppset_bloom");
    if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::Checkbox("Bloom Enabled", &asset->bloom_enabled);
        slider("Bloom Intensity", asset->bloom_intensity, 0.f, 1.f);
    }
    ImGui::PopID();

    ImGui::PushID("ppset_vignette");
    if (ImGui::CollapsingHeader("Vignette")) {
        slider("Intensity", asset->vignette_intensity, 0.f, 1.f);
        slider("Falloff",   asset->vignette_falloff,   0.5f, 4.f);
    }
    ImGui::PopID();

    ImGui::PushID("ppset_ca");
    if (ImGui::CollapsingHeader("Chromatic Aberration")) {
        slider("Amount", asset->chromatic_ab, 0.f, 0.02f, "%.4f");
    }
    ImGui::PopID();

    ImGui::PushID("ppset_sharp");
    if (ImGui::CollapsingHeader("Sharpness")) {
        slider("Amount", asset->sharpness, 0.f, 3.f);
    }
    ImGui::PopID();

    ImGui::PushID("ppset_grain");
    if (ImGui::CollapsingHeader("Film Grain")) {
        slider("Intensity", asset->grain_intensity, 0.f, 0.2f, "%.3f");
        slider("Scale",     asset->grain_size,      0.1f, 3.f);
    }
    ImGui::PopID();

    ImGui::PushID("ppset_ae");
    if (ImGui::CollapsingHeader("Auto-Exposure")) {
        changed |= ImGui::Checkbox("Enabled", &asset->auto_exposure);
        if (asset->auto_exposure) {
            static const char* ae_methods[] = { "Downsample (bloom avg)", "Histogram (256-bin)" };
            int m = asset->ae_method;
            ImGui::SetNextItemWidth(200);
            if (ImGui::Combo("Method", &m, ae_methods, 2)) { asset->ae_method = m; changed = true; }
            slider("Min EV",     asset->ae_min_ev, -8.f,  0.f);
            slider("Max EV",     asset->ae_max_ev,  0.f,  8.f);
            slider("Speed",      asset->ae_speed,   0.1f, 5.f);
            slider("Key (grey)", asset->ae_key,     0.01f, 1.f, "%.3f");
        }
    }
    ImGui::PopID();

    if (changed)
        comp->sync_render_data();

    ImGui::Separator();
    if (ImGui::Button("Save"))
        asset->save_to_disk();
    ImGui::SameLine();
    if (ImGui::Button("Revert")) {
        g_assets.reload(asset);
        comp->sync_render_data();
    }

    return false;
}

#endif
