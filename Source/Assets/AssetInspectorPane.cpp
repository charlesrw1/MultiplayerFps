#ifdef EDITOR_BUILD
#define IMGUI_DEFINE_MATH_OPERATORS
#include "Assets/AssetInspectorPane.h"
#include "Animation/AnimSeqEditor.h"
#include "Animation/SkeletonEditor.h"
#include "Animation/AnimationSeqAsset.h"
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
#include "Render/MaterialPublic.h"
#include "Render/MaterialLocal.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetBrowser.h"
#include "Assets/AssetRegistryLocal.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include <sstream>

extern int imgui_std_string_resize(ImGuiInputTextCallbackData* data);

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
    std::unique_ptr<PropertyGrid> mat_pg; // property grid for myMaterials array
};

// ── Custom IPropertyEditor subclasses for .mi VAR parameters ────────────────

// Helper: make a minimal synthetic PropertyInfo with a name (no type-specific fields needed).
static PropertyInfo make_mi_prop(const char* name) {
    PropertyInfo p;
    p.name    = name;
    p.tooltip = "";
    p.type    = core_type_id::Float; // arbitrary; only name is used for display
    return p;
}

class MiFloatEditor : public IPropertyEditor {
public:
    MiFloatEditor(std::string nm, float* v, float def)
        : value(v), default_val(def) {
        name_storage = std::move(nm);
        synth = make_mi_prop(name_storage.c_str());
        prop  = &synth;
    }
    bool internal_update() override {
        ImGui::SetNextItemWidth(-1.f);
        return ImGui::DragFloat("##v", value, 0.01f);
    }
    bool can_reset() override  { return *value != default_val; }
    void reset_value() override { *value = default_val; }
private:
    std::string name_storage;
    PropertyInfo synth;
    float* value;
    float  default_val;
};

class MiFloat4Editor : public IPropertyEditor {
public:
    MiFloat4Editor(std::string nm, glm::vec4* v, glm::vec4 def)
        : value(v), default_val(def) {
        name_storage = std::move(nm);
        synth = make_mi_prop(name_storage.c_str());
        prop  = &synth;
    }
    bool internal_update() override {
        ImGui::SetNextItemWidth(-1.f);
        return ImGui::DragFloat4("##v", &value->x, 0.01f);
    }
    bool can_reset() override  { return *value != default_val; }
    void reset_value() override { *value = default_val; }
private:
    std::string  name_storage;
    PropertyInfo synth;
    glm::vec4*   value;
    glm::vec4    default_val;
};

class MiColorEditor : public IPropertyEditor {
public:
    MiColorEditor(std::string nm, unsigned int* v, unsigned int def)
        : value(v), default_val(def) {
        name_storage = std::move(nm);
        synth = make_mi_prop(name_storage.c_str());
        prop  = &synth;
    }
    bool internal_update() override {
        Color32 c(*value);
        float col[4] = { c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f };
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::ColorEdit4("##v", col, ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar)) {
            c.r = (uint8_t)(col[0] * 255.f + 0.5f);
            c.g = (uint8_t)(col[1] * 255.f + 0.5f);
            c.b = (uint8_t)(col[2] * 255.f + 0.5f);
            c.a = (uint8_t)(col[3] * 255.f + 0.5f);
            *value = c.to_uint();
            return true;
        }
        return false;
    }
    bool can_reset() override  { return *value != default_val; }
    void reset_value() override { *value = default_val; }
private:
    std::string  name_storage;
    PropertyInfo synth;
    unsigned int* value;
    unsigned int  default_val;
};

class MiBoolEditor : public IPropertyEditor {
public:
    MiBoolEditor(std::string nm, bool* v, bool def)
        : value(v), default_val(def) {
        name_storage = std::move(nm);
        synth = make_mi_prop(name_storage.c_str());
        prop  = &synth;
    }
    bool internal_update() override { return ImGui::Checkbox("##v", value); }
    bool can_reset() override  { return *value != default_val; }
    void reset_value() override { *value = default_val; }
private:
    std::string  name_storage;
    PropertyInfo synth;
    bool* value;
    bool  default_val;
};

class MiTextureEditor : public SharedAssetPropertyEditor {
public:
    MiTextureEditor(std::string nm, std::string* path_ptr)
        : path(path_ptr) {
        name_storage        = std::move(nm);
        synth               = make_mi_prop(name_storage.c_str());
        prop                = &synth;
        class_type_override = &Texture::StaticType;
    }
    std::string get_str() override            { return *path; }
    void set_asset(const std::string& s) override { *path = s; }
private:
    std::string  name_storage;
    PropertyInfo synth;
    std::string* path;
};

class MiMasterMatEditor : public SharedAssetPropertyEditor {
public:
    MiMasterMatEditor(std::string* path_ptr, std::function<void(const std::string&)> on_change)
        : path(path_ptr), on_change_(std::move(on_change)) {
        synth               = make_mi_prop("Master Material");
        prop                = &synth;
        class_type_override = &MaterialInstance::StaticType;
    }
    std::string get_str() override { return *path; }
    void set_asset(const std::string& s) override { *path = s; on_change_(s); }
    // Only accept .mm master materials, not .mi instances
    bool accept_asset(const AssetOnDisk& asset) override {
        return StringUtils::get_extension_no_dot(asset.filename) == "mm";
    }
private:
    PropertyInfo synth;
    std::string* path;
    std::function<void(const std::string&)> on_change_;
};

// ── MiEditorState ────────────────────────────────────────────────────────────

struct MiEditorState {
    std::string parent_path;

    std::vector<MaterialParameterDefinition> param_defs;
    std::vector<MaterialParameterValue>      param_values;
    std::vector<std::string>                 texture_paths; // game-path per Texture2D param

    // Separate PropertyGrids so the master slot and params have independent rows/separators
    std::unique_ptr<PropertyGrid> master_pg;  // single-row: master material slot
    std::unique_ptr<PropertyGrid> params_pg;  // one row per VAR param (rebuilt on master change)

    void rebuild_params_on_master_change(const std::string& new_path) {
        auto new_mat = g_assets.find_sync_sptr<MaterialInstance>(new_path);
        if (new_mat && new_mat->impl && new_mat->impl->masterImpl) {
            param_defs = new_mat->impl->masterImpl->param_defs;
            param_values.resize(param_defs.size());
            for (int i = 0; i < (int)param_defs.size(); i++)
                param_values[i] = param_defs[i].default_value;
            texture_paths.assign(param_defs.size(), "");
            build_params_pg();
        }
    }

    void build_master_pg() {
        master_pg = std::make_unique<PropertyGrid>(get_basic_factory());
        master_pg->add_iproped_manual(new MiMasterMatEditor(
            &parent_path,
            [this](const std::string& new_path) { rebuild_params_on_master_change(new_path); }
        ));
    }

    void build_params_pg() {
        params_pg = std::make_unique<PropertyGrid>(get_basic_factory());
        for (int i = 0; i < (int)param_defs.size(); i++) {
            const auto& def = param_defs[i];
            auto& val        = param_values[i];
            switch (def.default_value.type) {
            case MatParamType::Float:
                params_pg->add_iproped_manual(
                    new MiFloatEditor(def.name, &val.scalar, def.default_value.scalar));
                break;
            case MatParamType::FloatVec:
                params_pg->add_iproped_manual(
                    new MiFloat4Editor(def.name, &val.vector, def.default_value.vector));
                break;
            case MatParamType::Vector:
                params_pg->add_iproped_manual(
                    new MiColorEditor(def.name, &val.color32, def.default_value.color32));
                break;
            case MatParamType::Bool:
                params_pg->add_iproped_manual(
                    new MiBoolEditor(def.name, &val.boolean, def.default_value.boolean));
                break;
            case MatParamType::Texture2D:
                params_pg->add_iproped_manual(
                    new MiTextureEditor(def.name, &texture_paths[i]));
                break;
            default:
                break;
            }
        }
    }

    void init_from(const MaterialInstance* mi) {
        ASSERT(mi && mi->impl);
        auto* master_impl = mi->impl->get_master_impl();
        ASSERT(master_impl);

        parent_path = mi->impl->masterMaterial ? mi->impl->masterMaterial->get_name() : "";

        param_defs   = master_impl->param_defs;
        param_values = mi->impl->params;
        ASSERT(param_defs.size() == param_values.size());

        texture_paths.resize(param_defs.size());
        for (int i = 0; i < (int)param_defs.size(); i++) {
            if (param_defs[i].default_value.type == MatParamType::Texture2D)
                texture_paths[i] = param_values[i].tex ? param_values[i].tex->get_name() : "";
        }

        build_master_pg();
        build_params_pg();
    }

    // Serializes back to the .mi text format matching the loader.
    std::string serialize() const {
        std::ostringstream out;
        out << "TYPE MaterialInstance\n";
        out << "PARENT " << parent_path << "\n";
        for (int i = 0; i < (int)param_defs.size(); i++) {
            const auto& def = param_defs[i];
            const auto& val = param_values[i];
            if (def.default_value.type == MatParamType::Texture2D && texture_paths[i].empty())
                continue;
            out << "VAR " << def.name << " ";
            switch (def.default_value.type) {
            case MatParamType::Float:    out << val.scalar; break;
            case MatParamType::FloatVec:
                out << val.vector.x << " " << val.vector.y << " "
                    << val.vector.z << " " << val.vector.w;
                break;
            case MatParamType::Vector: {
                Color32 c(val.color32);
                out << (int)c.r << " " << (int)c.g << " " << (int)c.b << " " << (int)c.a;
                break;
            }
            case MatParamType::Bool:     out << (val.boolean ? "1" : "0"); break;
            case MatParamType::Texture2D: out << texture_paths[i]; break;
            default: break;
            }
            out << "\n";
        }
        return out.str();
    }
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

void AssetInspectorPane::draw_anim_seq_editor(const std::string& asset_path) {
    if (!anim_seq_editor_ || anim_seq_editor_->get_asset_path() != asset_path) {
        anim_seq_editor_ = std::make_unique<AnimSeqEditor>();
        anim_seq_editor_->set_asset(asset_path);
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Animation Sequence Editor");
    ImGui::Separator();
    anim_seq_editor_->imgui_draw();
}

void AssetInspectorPane::draw_skeleton_section(const std::string& cmdl_path) {
    if (!skeleton_editor_)
        skeleton_editor_ = std::make_unique<SkeletonEditor>();
    skeleton_editor_->set_asset(cmdl_path);
    skeleton_editor_->imgui_draw();
}

void AssetInspectorPane::load_for(const AssetOnDisk& selected) {
    last_selected = selected;
    settings_dirty = false;
    cache_.reset();
    mi_state_.reset();
    anim_seq_editor_.reset();
    skeleton_editor_.reset();
    active_tis_path_.clear();

    auto ext = StringUtils::get_extension_no_dot(selected.filename);

    // .dds / .png → resolve to the .tis sidecar
    // .cmdl        → resolve to the .mis sidecar
    std::string load_path = selected.filename;
    if (ext == "dds" || ext == "png" || ext == "jpg") {
        load_path = strip_extension(selected.filename) + ".tis";
        ext = "tis";
    } else if (ext == "cmdl") {
        load_path = strip_extension(selected.filename) + ".mis";
        ext = "mis";
    }

    // Build .mi property editor state from the loaded MaterialInstance.
    if (ext == "mi") {
        auto mat = g_assets.find_sync_sptr<MaterialInstance>(selected.filename);
        if (mat && mat->impl && mat->impl->masterMaterial) {
            auto state = std::make_unique<MiEditorState>();
            state->init_from(mat.get());
            mi_state_ = std::move(state);
        }
        return;
    }

    raw_file_contents = read_game_text(load_path);

    if (ext != "tis" && ext != "mis") return;

    active_tis_path_ = (ext == "tis") ? load_path : "";
    selected_mip_ = 0;

    std::string json = strip_json_prefix(raw_file_contents);
    if (json.empty()) return;

    auto c = std::make_unique<InspectorCache>();
    c->reader = std::make_unique<ReadSerializerBackendJson>("inspector", json, c->objmaker);
    c->obj = c->reader->get_root_obj();
    if (c->obj) {
        if (ext == "mis") {
            auto* mis = c->obj->cast_to<ModelImportSettings>();
            if (mis) {
                c->mat_pg = std::make_unique<PropertyGrid>(get_basic_factory());
                // Point the grid at the reflected myMaterials property
                auto* all_props = ModelImportSettings::StaticType.props;
                if (all_props) {
                    for (int pi = 0; pi < all_props->count; pi++) {
                        if (strcmp(all_props->list[pi].name, "myMaterials") == 0) {
                            PropertyInfoList syn{ &all_props->list[pi], 1, "Material Slots" };
                            c->mat_pg->add_property_list_to_grid(&syn, mis, PG_LIST_PASSTHROUGH);
                            break;
                        }
                    }
                }
            }
        }
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
    if (!cache_ || !cache_->obj) { ImGui::TextDisabled("(parse error)"); return; }
    auto* tis = cache_->obj->cast_to<TextureImportSettings>();
    if (!tis) { ImGui::TextDisabled("(not TIS)"); return; }

    // --- Texture preview ---
    // For game textures: show the compiled .dds. For UI textures (no .dds): show the .png directly.
    std::string dds_path = strip_extension(gamepath) + ".dds";
    std::string png_path = strip_extension(gamepath) + ".png";
    Texture* tex = g_assets.find<Texture>(dds_path).get();
    if (!tex || !tex->gpu_ptr)
        tex = g_assets.find<Texture>(png_path).get();

    if (tex && tex->gpu_ptr) {
        auto sz   = tex->gpu_ptr->get_size();
        int  mips = tex->gpu_ptr->get_num_mips();

        // Clamp selected mip to valid range
        selected_mip_ = std::max(0, std::min(selected_mip_, mips - 1));
        int mip_w = std::max(1, sz.x >> selected_mip_);
        int mip_h = std::max(1, sz.y >> selected_mip_);

        // Always display at MAX_THUMB on the longest axis regardless of selected mip,
        // so the thumbnail never shrinks when browsing mip levels.
        const float MAX_THUMB = 256.f;
        float base_scale = (sz.x >= sz.y)
            ? MAX_THUMB / std::max(1, sz.x)
            : MAX_THUMB / std::max(1, sz.y);
        ImVec2 thumb_size(sz.x * base_scale, sz.y * base_scale);

        auto tex_id = ImTextureID(uint64_t(tex->get_internal_render_handle()));
        ImGui::Image(tex_id, thumb_size);

        // Capture rect BEFORE BeginTooltip — inside the tooltip window GetItemRect*
        // refers to the tooltip's last item, not the image we just drew.
        bool hovered    = ImGui::IsItemHovered();
        ImVec2 item_min = ImGui::GetItemRectMin();
        ImVec2 item_sz  = ImGui::GetItemRectSize();

        if (hovered && ImGui::BeginTooltip()) {
            ImVec2 mouse = ImGui::GetMousePos();
            float mx = (item_sz.x > 0.f) ? (mouse.x - item_min.x) / item_sz.x : 0.5f;
            float my = (item_sz.y > 0.f) ? (mouse.y - item_min.y) / item_sz.y : 0.5f;
            const float region = 0.2f; // fraction of the texture shown in tooltip
            float u0 = mx - region * 0.5f, u1 = mx + region * 0.5f;
            float v0 = my - region * 0.5f, v1 = my + region * 0.5f;
            // Shift window to stay in [0,1] without changing its size
            if (u0 < 0.f) { u1 -= u0; u0 = 0.f; }
            if (u1 > 1.f) { u0 -= (u1 - 1.f); u1 = 1.f; u0 = std::max(0.f, u0); }
            if (v0 < 0.f) { v1 -= v0; v0 = 0.f; }
            if (v1 > 1.f) { v0 -= (v1 - 1.f); v1 = 1.f; v0 = std::max(0.f, v0); }
            ImGui::Image(tex_id, ImVec2(256, 256), ImVec2(u0, v0), ImVec2(u1, v1));
            ImGui::EndTooltip();
        }

        // Metadata + mip selector to the right of thumbnail
        ImGui::SameLine();
        ImGui::BeginGroup();

        ImGui::TextUnformatted(texture_format_to_string(tex->gpu_ptr->get_texture_format()));
        ImGui::Text("%d x %d", sz.x, sz.y);

        if (mips > 1) {
            ImGui::Text("Mip");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.f);
            ImGui::SliderInt("##mip", &selected_mip_, 0, mips - 1);
            ImGui::SameLine();
            ImGui::TextDisabled("%dx%d", mip_w, mip_h);
        } else {
            ImGui::TextDisabled("1 mip");
        }

        Color32 sc = tis->simplifiedColor;
        ImGui::ColorButton("##simpcol",
            ImVec4(sc.r / 255.f, sc.g / 255.f, sc.b / 255.f, sc.a / 255.f),
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoBorder,
            ImVec2(16, 16));
        ImGui::SameLine();
        ImGui::TextDisabled("Simplified color");

        ImGui::EndGroup();
    } else {
        ImGui::TextDisabled("(no compiled .dds)");
    }

    ImGui::Separator();

    // --- Manual settings editor ---
    bool changed = false;

    ImGui::LabelText("Source", "%s", tis->src_file.c_str());

    changed |= ImGui::Checkbox("sRGB",              &tis->is_srgb);
    changed |= ImGui::Checkbox("Normal map (BC5)",  &tis->is_normalmap);
    changed |= ImGui::Checkbox("Uncompressed (R8)", &tis->make_uncompressed);
    changed |= ImGui::Checkbox("Nearest filtering", &tis->nearest_filtering);
    changed |= ImGui::Checkbox("Load source file (UI texture, skips compile)", &tis->load_source_file);

    // resize_width with power-of-2 step buttons
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Resize width");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(72.f);
    if (ImGui::InputInt("##rw", &tis->resize_width, 0)) {
        tis->resize_width = std::max(0, tis->resize_width);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("x2")) {
        tis->resize_width = (tis->resize_width < 1) ? 64 : tis->resize_width * 2;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("/2")) {
        tis->resize_width = std::max(0, tis->resize_width / 2);
        changed = true;
    }
    if (tis->resize_width == 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("(no resize)");
    }

    if (changed) settings_dirty = true;

    if (settings_dirty) {
        ImGui::Spacing();
        if (ImGui::Button("Apply")) {
            write_texture_import_settings(tis, gamepath);
            if (tis->load_source_file) {
                // Remove any compiled .dds so the file watcher switches the browser entry to .png
                FileSys::delete_game_file(strip_extension(gamepath) + ".dds");
            } else {
                AssetCompiler::compile_asset(gamepath);
            }
            settings_dirty = false;
            load_for(last_selected);
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
            load_for(last_selected);
    }
}

void AssetInspectorPane::draw_mis_settings(const std::string& gamepath) {
    if (!cache_ || !cache_->obj) { ImGui::TextDisabled("(parse error)"); return; }
    auto* mis = cache_->obj->cast_to<ModelImportSettings>();
    if (!mis) { ImGui::TextDisabled("(not MIS)"); return; }

    bool changed = false;

    ImGui::LabelText("Source", "%s", mis->srcGlbFile.c_str());
    ImGui::Separator();

    // --- LOD distances ---
    ImGui::Text("LOD Screen-Space Thresholds");
    ImGui::SameLine();
    ImGui::TextDisabled("(fraction of screen height)");
    ImGui::Indent();
    auto& lods = mis->lodScreenSpaceSizes;
    for (int i = 0; i < (int)lods.size(); i++) {
        ImGui::PushID(i);
        ImGui::Text("LOD %d:", i + 1);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.f);
        if (ImGui::DragFloat("##lv", &lods[i], 0.001f, 0.0f, 1.0f, "%.4f"))
            changed = true;
        ImGui::SameLine();
        char overlay[16];
        snprintf(overlay, sizeof(overlay), "%.2f%%", lods[i] * 100.f);
        ImGui::ProgressBar(std::min(lods[i], 1.0f), ImVec2(80.f, 0.f), overlay);
        ImGui::SameLine();
        if (ImGui::SmallButton("-##rm")) {
            lods.erase(lods.begin() + i);
            changed = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    ImGui::Unindent();
    if (ImGui::Button("+ LOD")) {
        float v = lods.empty() ? 0.1f : lods.back() * 0.5f;
        lods.push_back(std::max(0.0001f, v));
        changed = true;
    }

    ImGui::Separator();

    // --- Material slots (via PropertyGrid — full SharedAssetPropertyEditor widget per slot) ---
    if (cache_->mat_pg) {
        cache_->mat_pg->update();
        if (cache_->mat_pg->rows_had_changes) changed = true;
    }

    ImGui::Separator();

    // --- Collision & generation flags ---
    changed |= ImGui::Checkbox("Mesh as Convex", &mis->meshAsConvex);
    changed |= ImGui::Checkbox("Mesh as Collision", &mis->meshAsCollision);
    changed |= ImGui::Checkbox("Generate Auto LODs", &mis->generate_auto_lods);
    changed |= ImGui::Checkbox("Disable Prune Unused Bones", &mis->disablePruneUnusedBones);

    if (changed) settings_dirty = true;

    if (settings_dirty) {
        ImGui::Spacing();
        if (ImGui::Button("Apply")) {
            write_model_import_settings(mis, gamepath);
            AssetCompiler::compile_asset(gamepath);
            settings_dirty = false;
            load_for(last_selected);
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
            load_for(last_selected);
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

void AssetInspectorPane::draw_material_instance_editor(const std::string& gamepath) {
    if (!mi_state_) { ImGui::TextDisabled("(failed to load .mi)"); return; }
    auto& s = *mi_state_;

    // ── Master material (its own PropertyGrid) ───────────────────────────────
    ImGui::SeparatorText("Master Material");
    if (s.master_pg) {
        s.master_pg->update();
        if (s.master_pg->rows_had_changes) settings_dirty = true;
    }

    // ── VAR parameters (separate PropertyGrid) ───────────────────────────────
    ImGui::Separator();
    if (s.params_pg) {
        s.params_pg->update();
        if (s.params_pg->rows_had_changes) settings_dirty = true;
    } else {
        ImGui::TextDisabled("(no parameters)");
    }

    // ── Apply / Revert ───────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::BeginDisabled(!settings_dirty || s.parent_path.empty());
    if (ImGui::Button("Apply")) {
        std::string text = s.serialize();
        auto f = FileSys::open_write_game(gamepath);
        if (f) { f->write(text.data(), text.size()); f->close(); }
        AssetCompiler::compile_asset(gamepath);
        settings_dirty = false;
        // Do NOT call load_for — mi_state_ already has the saved values;
        // compile_asset reloads the scene async.
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Revert")) load_for(last_selected);
}

void AssetInspectorPane::imgui_draw(const AssetOnDisk& selected) {
    if (!ImGui::Begin("Asset Inspector")) {
        ImGui::End();
        return;
    }

    if (selected.filename.empty()) {
        anim_seq_editor_.reset();
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
    else if (ext == "dds" || ext == "png" || ext == "jpg") draw_tis_settings(active_tis_path_.empty() ? strip_extension(selected.filename) + ".tis" : active_tis_path_);
    else if (ext == "mis")  { draw_mis_settings(selected.filename); draw_skeleton_section(strip_extension(selected.filename) + ".cmdl"); }
    else if (ext == "cmdl") { draw_mis_settings(strip_extension(selected.filename) + ".mis"); draw_skeleton_section(selected.filename); }
    else if (selected.type &&
             selected.type->get_asset_class_type() == &AnimationSeqAsset::StaticType)
        draw_anim_seq_editor(selected.filename);
    else if (ext == "mm") draw_material_text(selected.filename);
    else if (ext == "mi") draw_material_instance_editor(selected.filename);
    else    ImGui::TextDisabled("Extension: .%s", ext.c_str());

    ImGui::End();
}

#endif
