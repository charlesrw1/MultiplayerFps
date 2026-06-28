#ifdef EDITOR_BUILD
#define IMGUI_DEFINE_MATH_OPERATORS
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

// ── MiEditorState ────────────────────────────────────────────────────────────

struct MiEditorState {
    std::string parent_path;

    std::vector<MaterialParameterDefinition> param_defs;
    std::vector<MaterialParameterValue>      param_values;
    std::vector<std::string>                 texture_paths; // game-path per Texture2D param

    // Picker state for the master material slot
    std::string master_picker_filter;
    bool        master_picker_needs_focus = false;

    // PropertyGrid for the VAR parameters (rebuilt on init / master change)
    std::unique_ptr<PropertyGrid> params_pg;

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

void AssetInspectorPane::load_for(const AssetOnDisk& selected) {
    last_selected = selected;
    settings_dirty = false;
    cache_.reset();
    mi_state_.reset();
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

// Draws a single-row slot that only accepts .mm drag-drops and shows a .mm-filtered picker.
// Returns true and sets out_path when the selection changes.
// .mm-only asset slot: colored slot + drag-drop + browse button + picker popup.
// Returns true and writes out_path when the selection changes.
static bool draw_master_mat_slot(const std::string& current_path, const AssetMetadata* mat_meta,
                                  std::string& picker_filter, bool& picker_needs_focus,
                                  std::string& out_path) {
    auto* dl   = ImGui::GetWindowDrawList();
    auto& style = ImGui::GetStyle();
    const float fh = ImGui::GetFrameHeight();
    const float bw = fh;
    const float tw = ImGui::GetContentRegionAvail().x - bw - style.ItemSpacing.x;
    bool changed = false;

    // Slot background
    ImVec2 s_min = ImGui::GetCursorScreenPos();
    ImVec2 s_max = { s_min.x + tw, s_min.y + fh };
    if (mat_meta) {
        Color32 bg = mat_meta->get_browser_color();
        bg.r = (uint8_t)(bg.r * 0.35f); bg.g = (uint8_t)(bg.g * 0.35f); bg.b = (uint8_t)(bg.b * 0.35f);
        dl->AddRectFilled(s_min, s_max, bg.to_uint(), 3.f);
    }
    ImGui::PushClipRect(s_min, s_max, true);
    ImVec2 tc = ImGui::GetCursorPos();
    ImGui::SetCursorPosY(tc.y + style.FramePadding.y * 0.5f);
    if (current_path.empty()) ImGui::TextDisabled("(none — drag a .mm here)");
    else ImGui::TextUnformatted(current_path.c_str());
    ImGui::PopClipRect();
    ImGui::SetCursorPos(tc);
    ImGui::InvisibleButton("##mm_slot", { tw, fh });
    bool hov = ImGui::IsItemHovered();
    dl->AddRect(s_min, s_max,
        (hov && current_path.empty()) ? IM_COL32(200,200,200,180) : IM_COL32(180,180,180,50), 3.f);
    if (hov) ImGui::SetTooltip(current_path.empty() ? "Drag a .mm master material here" : current_path.c_str());
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && current_path.empty()) {
        ImGui::OpenPopup("##mm_picker"); picker_filter.clear(); picker_needs_focus = true;
    }
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* peek = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop", ImGuiDragDropFlags_AcceptPeekOnly);
        if (peek) {
            AssetOnDisk* res = *(AssetOnDisk**)peek->Data;
            if (res->type == mat_meta && StringUtils::get_extension_no_dot(res->filename) == "mm")
                if (ImGui::AcceptDragDropPayload("AssetBrowserDragDrop")) { out_path = res->filename; changed = true; }
        }
        ImGui::EndDragDropTarget();
    }

    // Browse button
    ImGui::SameLine(0, style.ItemSpacing.x);
    ImVec2 bp = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##mm_browse", { bw, fh });
    bool bhov = ImGui::IsItemHovered();
    dl->AddRectFilled(bp, { bp.x+bw, bp.y+fh }, bhov ? IM_COL32(75,75,75,200) : IM_COL32(50,50,50,160), 3.f);
    dl->AddRect(bp, { bp.x+bw, bp.y+fh }, IM_COL32(100,100,100,120), 3.f);
    auto btex = g_assets.find<Texture>("eng/icons/doc_search.png");
    if (btex) {
        float ico = fh - style.FramePadding.y * 2.f, ix = bp.x + (bw-ico)*0.5f, iy = bp.y + style.FramePadding.y;
        dl->AddImage(ImTextureID(uint64_t(btex->get_internal_render_handle())), {ix,iy}, {ix+ico,iy+ico});
    }
    if (bhov && !current_path.empty()) ImGui::SetTooltip("Find in browser");
    if (ImGui::IsItemClicked() && !current_path.empty() && AssetBrowser::inst)
        { AssetBrowser::inst->set_selected(current_path); AssetBrowser::inst->force_focus = true; }

    // Picker popup (.mm only)
    ImGui::SetNextWindowSize({ 320, 360 }, ImGuiCond_Always);
    if (ImGui::BeginPopup("##mm_picker")) {
        if (picker_needs_focus) { ImGui::SetKeyboardFocusHere(); picker_needs_focus = false; }
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##pf", (char*)picker_filter.c_str(), picker_filter.size() + 1,
            ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize, &picker_filter);
        picker_filter = picker_filter.c_str();
        auto fl = StringUtils::to_lower(picker_filter);
        ImGui::BeginChild("##pl", { 0, 0 });
        for (auto* node : AssetRegistrySystem::get().get_linear_list()) {
            if (node->is_folder() || node->asset.type != mat_meta) continue;
            if (StringUtils::get_extension_no_dot(node->asset.filename) != "mm") continue;
            if (!fl.empty()) {
                auto nl = StringUtils::to_lower(node->asset.filename);
                if (nl.find(fl) == std::string::npos) continue;
            }
            if (ImGui::Selectable(node->asset.filename.c_str())) { out_path = node->asset.filename; changed = true; ImGui::CloseCurrentPopup(); }
        }
        ImGui::EndChild(); ImGui::EndPopup();
    }
    return changed;
}

void AssetInspectorPane::draw_material_instance_editor(const std::string& gamepath) {
    if (!mi_state_) { ImGui::TextDisabled("(failed to load .mi)"); return; }
    auto& s = *mi_state_;

    const auto* mat_meta = AssetRegistrySystem::get().find_type("Material");

    // ── Master material ──────────────────────────────────────────────────────
    ImGui::SeparatorText("Master Material");
    std::string new_master;
    if (draw_master_mat_slot(s.parent_path, mat_meta, s.master_picker_filter,
                              s.master_picker_needs_focus, new_master)) {
        auto new_mat = g_assets.find_sync_sptr<MaterialInstance>(new_master);
        if (new_mat && new_mat->impl && new_mat->impl->masterImpl) {
            s.parent_path = new_master;
            s.param_defs  = new_mat->impl->masterImpl->param_defs;
            s.param_values.resize(s.param_defs.size());
            for (int i = 0; i < (int)s.param_defs.size(); i++)
                s.param_values[i] = s.param_defs[i].default_value;
            s.texture_paths.assign(s.param_defs.size(), "");
            s.build_params_pg();
            settings_dirty = true;
        }
    }

    // ── VAR parameters via PropertyGrid ──────────────────────────────────────
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
        load_for(last_selected);
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
    else if (ext == "mis")  draw_mis_settings(selected.filename);
    else if (ext == "cmdl") draw_mis_settings(strip_extension(selected.filename) + ".mis");
    else if (ext == "mm") draw_material_text(selected.filename);
    else if (ext == "mi") draw_material_instance_editor(selected.filename);
    else    ImGui::TextDisabled("Extension: .%s", ext.c_str());

    ImGui::End();
}

#endif
