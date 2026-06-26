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
#include "Render/Texture.h"
#include "Render/IGraphicsDevice.h"
#include "Assets/AssetDatabase.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <algorithm>

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

static const char* texture_format_to_string(GraphicsTextureFormat fmt) {
    using gtf = GraphicsTextureFormat;
    switch (fmt) {
    case gtf::bc1:              return "BC1_UNORM";
    case gtf::bc1_srgb:         return "BC1_UNORM_SRGB";
    case gtf::bc3:              return "BC3_UNORM";
    case gtf::bc4:              return "BC4_UNORM";
    case gtf::bc5:              return "BC5_UNORM";
    case gtf::bc6:              return "BC6H";
    case gtf::r8:               return "R8_UNORM";
    case gtf::rg8:              return "RG8_UNORM";
    case gtf::rgb8:             return "RGB8_UNORM";
    case gtf::rgba8:            return "RGBA8_UNORM";
    case gtf::r16f:             return "R16F";
    case gtf::rg16f:            return "RG16F";
    case gtf::rgb16f:           return "RGB16F";
    case gtf::rgba16f:          return "RGBA16F";
    case gtf::r32f:             return "R32F";
    case gtf::rg32f:            return "RG32F";
    case gtf::r11f_g11f_b10f:   return "R11G11B10F";
    case gtf::rgba16_snorm:     return "RGBA16_SNORM";
    default:                    return "Unknown";
    }
}

AssetInspectorPane::AssetInspectorPane() = default;
AssetInspectorPane::~AssetInspectorPane() = default;

void AssetInspectorPane::load_for(const AssetOnDisk& selected) {
    last_selected = selected;
    settings_dirty = false;
    cache_.reset();
    active_tis_path_.clear();

    auto ext = StringUtils::get_extension_no_dot(selected.filename);

    // .dds → resolve to the .tis sidecar
    std::string load_path = selected.filename;
    if (ext == "dds") {
        load_path = strip_extension(selected.filename) + ".tis";
        ext = "tis";
    }

    raw_file_contents = read_game_text(load_path);

    if (ext != "tis" && ext != "mis") return;

    active_tis_path_ = (ext == "tis") ? load_path : "";

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
    size_t len = strlen(raw_file_contents.c_str());
    auto f = FileSys::open_write_game(last_selected.filename);
    if (f) {
        f->write(raw_file_contents.data(), len);
        f->close();
    }
    AssetCompiler::compile_asset(last_selected.filename);
    settings_dirty = false;
}

void AssetInspectorPane::draw_tis_settings(const std::string& gamepath) {
    if (!cache_ || !cache_->pg) { ImGui::TextDisabled("(parse error)"); return; }
    auto* tis = cache_->obj ? cache_->obj->cast_to<TextureImportSettings>() : nullptr;
    if (!tis) { ImGui::TextDisabled("(not TIS)"); return; }

    // --- Texture preview ---
    std::string dds_path = strip_extension(gamepath) + ".dds";
    Texture* tex = g_assets.find<Texture>(dds_path).get();

    if (tex && tex->gpu_ptr) {
        auto sz      = tex->gpu_ptr->get_size();
        int  mips    = tex->gpu_ptr->get_num_mips();
        auto fmt_str = texture_format_to_string(tex->gpu_ptr->get_texture_format());

        // Thumbnail — cap at 256px on the longest axis
        const float MAX_THUMB = 256.f;
        float scale = (sz.x > 0 && sz.y > 0)
            ? std::min(MAX_THUMB / sz.x, MAX_THUMB / sz.y)
            : 1.f;
        ImVec2 thumb_size(sz.x * scale, sz.y * scale);

        ImGui::Image(ImTextureID(uint64_t(tex->get_internal_render_handle())), thumb_size);

        // Metadata column to the right
        ImGui::SameLine();
        ImGui::BeginGroup();

        ImGui::TextUnformatted(fmt_str);
        ImGui::Text("%d x %d", sz.x, sz.y);
        ImGui::Text("%d mip%s", mips, mips != 1 ? "s" : "");

        // Simplified color swatch (read-only)
        Color32 sc = tis->simplifiedColor;
        ImVec4 col(sc.r / 255.f, sc.g / 255.f, sc.b / 255.f, sc.a / 255.f);
        ImGui::ColorButton("##simpcol", col,
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoBorder,
            ImVec2(16, 16));
        ImGui::SameLine();
        ImGui::TextDisabled("Simplified color");

        ImGui::EndGroup();

        // Mip chain table
        if (mips > 1) {
            ImGui::Spacing();
            if (ImGui::BeginTable("##mips", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Mip", ImGuiTableColumnFlags_WidthFixed, 40.f);
                ImGui::TableSetupColumn("Size");
                ImGui::TableHeadersRow();
                int w = sz.x, h = sz.y;
                for (int i = 0; i < mips; ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%d", i);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%d x %d", w, h);
                    w = std::max(1, w / 2);
                    h = std::max(1, h / 2);
                }
                ImGui::EndTable();
            }
        }
    } else {
        ImGui::TextDisabled("(no compiled .dds — click Apply to compile)");
    }

    ImGui::Separator();

    // --- Settings PropertyGrid ---
    cache_->pg->update();
    if (cache_->pg->rows_had_changes) {
        cache_->pg->rows_had_changes = false;
        settings_dirty = true;
    }

    if (settings_dirty) {
        if (ImGui::Button("Apply")) {
            write_texture_import_settings(tis, gamepath);
            AssetCompiler::compile_asset(gamepath);
            settings_dirty = false;
            load_for(last_selected); // refresh cache: compile updates simplifiedColor
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert")) {
            load_for(last_selected);
        }
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
    static constexpr size_t kEditBufExtra = 4096;
    std::string& buf = raw_file_contents;

    size_t text_len = strlen(buf.c_str());
    size_t needed = text_len + kEditBufExtra;
    if (buf.size() < needed)
        buf.resize(needed, '\0');

    if (ImGui::InputTextMultiline("##mattext", buf.data(), buf.size(),
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
    else if (ext == "dds") draw_tis_settings(active_tis_path_.empty() ? strip_extension(selected.filename) + ".tis" : active_tis_path_);
    else if (ext == "mis") draw_mis_settings(selected.filename);
    else if (ext == "mm" || ext == "mi") draw_material_text(selected.filename);
    else    ImGui::TextDisabled("Extension: .%s", ext.c_str());

    ImGui::End();
}

#endif
