#ifdef EDITOR_BUILD
#include "PostProcessEditorUI.h"
#include "PostProcessComponent.h"
#include "Render/PostProcessSettings.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetBrowser.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
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
