#ifdef EDITOR_BUILD
#include "Assets/AssetInspectorPane.h"
#include "AssetTools/AssetCompiler.h"
#include "Render/Editor/TextureEditor.h"
#include "AssetCompile/ModelAsset2.h"
#include "Framework/SerializerJson.h"
#include "Framework/PropertyEd.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "Framework/Util.h"
#include "LevelEditor/PropertyEditors.h"
#include <imgui.h>
#include <vector>
#include <string>

// Defined in Model.cpp; forward-declare to avoid pulling in Model.h
extern void write_model_import_settings(ModelImportSettings* mis, const std::string& savepath);

static FnFactory<IPropertyEditor>& get_basic_factory() {
    static FnFactory<IPropertyEditor> factory;
    static bool registered = false;
    if (!registered) {
        PropertyFactoryUtil::register_basic(factory);
        registered = true;
    }
    return factory;
}

static std::string read_game_text(const std::string& gamepath) {
    auto f = FileSys::open_read_game(gamepath);
    if (!f) return {};
    std::vector<char> buf(f->size() + 1, 0);
    f->read(buf.data(), f->size());
    f->close();
    return buf.data();
}

AssetInspectorPane::AssetInspectorPane() = default;
AssetInspectorPane::~AssetInspectorPane() = default;

void AssetInspectorPane::load_for(const AssetOnDisk& selected) {
    last_selected = selected;
    raw_file_contents = read_game_text(selected.filename);
    settings_dirty = false;
}

void AssetInspectorPane::apply_changes() {
    if (!settings_dirty || last_selected.filename.empty()) return;
    auto f = FileSys::open_write_game(last_selected.filename);
    if (f) {
        f->write(raw_file_contents.data(), raw_file_contents.size());
        f->close();
    }
    AssetCompiler::compile_asset(last_selected.filename);
    settings_dirty = false;
}

void AssetInspectorPane::draw_tis_settings(const std::string& gamepath) {
    std::string text = read_game_text(gamepath);
    if (text.empty()) { ImGui::TextDisabled("(no .tis found)"); return; }
    if (text.find("!json") != 0) { ImGui::TextDisabled("(old format)"); return; }
    text = text.substr(6);

    MakeObjectFromPathGeneric objmaker;
    ReadSerializerBackendJson reader("insp_tis", text, objmaker);
    auto* tis = reader.get_root_obj() ? reader.get_root_obj()->cast_to<TextureImportSettings>() : nullptr;
    if (!tis) { ImGui::TextDisabled("(parse error)"); return; }

    PropertyGrid grid(get_basic_factory());
    grid.add_class_to_grid(tis);
    grid.update();

    if (grid.rows_had_changes) {
        write_texture_import_settings(tis, gamepath);
        AssetCompiler::compile_asset(gamepath);
    }
}

void AssetInspectorPane::draw_mis_settings(const std::string& gamepath) {
    std::string text = read_game_text(gamepath);
    if (text.empty()) { ImGui::TextDisabled("(no .mis found)"); return; }
    if (text.find("!json") != 0) { ImGui::TextDisabled("(old format)"); return; }
    text = text.substr(6);

    MakeObjectFromPathGeneric objmaker;
    ReadSerializerBackendJson reader("insp_mis", text, objmaker);
    auto* mis = reader.get_root_obj() ? reader.get_root_obj()->cast_to<ModelImportSettings>() : nullptr;
    if (!mis) { ImGui::TextDisabled("(parse error)"); return; }

    PropertyGrid grid(get_basic_factory());
    grid.add_class_to_grid(mis);
    grid.update();

    if (grid.rows_had_changes) {
        write_model_import_settings(mis, gamepath);
        AssetCompiler::compile_asset(gamepath);
    }
}

void AssetInspectorPane::draw_material_text(const std::string& gamepath) {
    // raw_file_contents is loaded once per selection in load_for()
    static constexpr int kEditBufExtra = 4096;
    std::string& buf = raw_file_contents;

    // Reserve room for in-place editing
    if (buf.capacity() < buf.size() + kEditBufExtra)
        buf.reserve(buf.size() + kEditBufExtra);
    buf.resize(buf.capacity(), 0);

    if (ImGui::InputTextMultiline("##mattext", buf.data(), buf.capacity(),
                                  ImVec2(-1, 200))) {
        buf.resize(strlen(buf.data()));
        settings_dirty = true;
    }

    if (settings_dirty) {
        if (ImGui::Button("Apply"))
            apply_changes();
        ImGui::SameLine();
        if (ImGui::Button("Revert")) {
            raw_file_contents = read_game_text(gamepath);
            settings_dirty = false;
        }
    }
}

void AssetInspectorPane::imgui_draw(const AssetOnDisk& selected) {
    if (selected.filename.empty()) {
        ImGui::TextDisabled("(no asset selected)");
        return;
    }

    if (selected.filename != last_selected.filename)
        load_for(selected);

    ImGui::TextUnformatted(selected.filename.c_str());
    ImGui::Separator();

    auto ext = StringUtils::get_extension_no_dot(selected.filename);

    if (ext == "tis") {
        draw_tis_settings(selected.filename);
    } else if (ext == "mis") {
        draw_mis_settings(selected.filename);
    } else if (ext == "mm" || ext == "mi") {
        draw_material_text(selected.filename);
    } else {
        ImGui::TextDisabled("Extension: .%s", ext.c_str());
    }
}

#endif
