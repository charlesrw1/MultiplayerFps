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

// Cache that persists across frames so PropertyGrid rows keep their void* pointers alive.
// Destruction order (reverse of declaration): pg → reader → objmaker — this is correct:
//   pg must die before reader (pg rows hold pointers into reader-owned objects).
//   reader must die before objmaker (reader holds objmaker reference, though only needed during read).
struct InspectorCache {
    MakeObjectFromPathGeneric objmaker;
    std::unique_ptr<ReadSerializerBackendJson> reader;
    std::unique_ptr<PropertyGrid> pg;
    ClassBase* obj = nullptr; // non-owning; reader owns the lifetime
};

static std::string read_game_text(const std::string& gamepath) {
    auto f = FileSys::open_read_game(gamepath);
    if (!f) return {};
    std::vector<char> buf(f->size() + 1, 0);
    f->read(buf.data(), f->size());
    f->close();
    return buf.data();
}

// Strip the "!json\n" prefix written by write_texture_import_settings / write_model_import_settings
static std::string strip_json_prefix(const std::string& text) {
    if (text.find("!json") != 0) return {};
    size_t nl = text.find('\n');
    return (nl != std::string::npos) ? text.substr(nl + 1) : text.substr(5);
}

AssetInspectorPane::AssetInspectorPane() = default;
AssetInspectorPane::~AssetInspectorPane() = default;

void AssetInspectorPane::load_for(const AssetOnDisk& selected) {
    last_selected = selected;
    raw_file_contents = read_game_text(selected.filename);
    settings_dirty = false;
    cache_.reset();

    auto ext = StringUtils::get_extension_no_dot(selected.filename);
    if (ext != "tis" && ext != "mis") return;

    std::string json = strip_json_prefix(raw_file_contents);
    if (json.empty()) return;

    auto c = std::make_unique<InspectorCache>();
    c->reader = std::make_unique<ReadSerializerBackendJson>("inspector", json, c->objmaker);
    c->obj = c->reader->get_root_obj();
    if (c->obj) {
        c->pg = std::make_unique<PropertyGrid>(get_basic_factory());
        c->pg->add_class_to_grid(c->obj);
        cache_ = std::move(c);
    }
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
    if (!cache_ || !cache_->pg) { ImGui::TextDisabled("(parse error)"); return; }
    auto* tis = cache_->obj ? cache_->obj->cast_to<TextureImportSettings>() : nullptr;
    if (!tis) { ImGui::TextDisabled("(not TIS)"); return; }

    cache_->pg->update();
    if (cache_->pg->rows_had_changes) {
        cache_->pg->rows_had_changes = false;
        write_texture_import_settings(tis, gamepath);
        AssetCompiler::compile_asset(gamepath);
    }
}

void AssetInspectorPane::draw_mis_settings(const std::string& gamepath) {
    if (!cache_ || !cache_->pg) { ImGui::TextDisabled("(parse error)"); return; }
    auto* mis = cache_->obj ? cache_->obj->cast_to<ModelImportSettings>() : nullptr;
    if (!mis) { ImGui::TextDisabled("(not MIS)"); return; }

    cache_->pg->update();
    if (cache_->pg->rows_had_changes) {
        cache_->pg->rows_had_changes = false;
        write_model_import_settings(mis, gamepath);
        AssetCompiler::compile_asset(gamepath);
    }
}

void AssetInspectorPane::draw_material_text(const std::string& gamepath) {
    static constexpr int kEditBufExtra = 4096;
    std::string& buf = raw_file_contents;

    if (buf.capacity() < buf.size() + kEditBufExtra)
        buf.reserve(buf.size() + kEditBufExtra);
    buf.resize(buf.capacity(), '\0');

    if (ImGui::InputTextMultiline("##mattext", buf.data(), buf.capacity(),
                                  ImVec2(-1, -30))) {
        buf.resize(strlen(buf.data()));
        settings_dirty = true;
    }

    if (settings_dirty) {
        if (ImGui::Button("Apply"))  apply_changes();
        ImGui::SameLine();
        if (ImGui::Button("Revert")) {
            raw_file_contents = read_game_text(gamepath);
            settings_dirty = false;
        }
    }
}

void AssetInspectorPane::imgui_draw(const AssetOnDisk& selected) {
    if (!ImGui::Begin("Asset Inspector")) {
        ImGui::End();
        return;
    }

    if (selected.filename.empty()) {
        ImGui::TextDisabled("(no asset selected)");
        ImGui::End();
        return;
    }

    if (selected.filename != last_selected.filename)
        load_for(selected);

    ImGui::TextUnformatted(selected.filename.c_str());
    ImGui::Separator();

    auto ext = StringUtils::get_extension_no_dot(selected.filename);

    if      (ext == "tis") draw_tis_settings(selected.filename);
    else if (ext == "mis") draw_mis_settings(selected.filename);
    else if (ext == "mm" || ext == "mi") draw_material_text(selected.filename);
    else    ImGui::TextDisabled("Extension: .%s", ext.c_str());

    ImGui::End();
}

#endif
