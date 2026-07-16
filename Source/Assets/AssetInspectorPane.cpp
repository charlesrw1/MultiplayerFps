#ifdef EDITOR_BUILD
#define IMGUI_DEFINE_MATH_OPERATORS
#include "Assets/AssetInspectorPane.h"
#include "Animation/AnimSeqEditor.h"
#include "Animation/SkeletonEditor.h"
#include "Animation/AnimationSeqAsset.h"
#include "AssetTools/AssetCompiler.h"
#include "Render/Editor/TextureEditor.h"
#include "AssetCompile/ModelAsset2.h"
#include "AssetCompile/SoundAsset.h"
#include "Sound/SoundPublic.h"
#include <filesystem>
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
#include "Assets/ScriptableObject.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include <sstream>
#include <cstdio>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

extern int imgui_std_string_resize(ImGuiInputTextCallbackData* data);

// Defined in Model.cpp; forward-declare to avoid pulling in Model.h
extern void write_model_import_settings(ModelImportSettings* mis, const std::string& savepath);

// Defined in TextureEditor.cpp; resolves a sidecar's relative src_file against its own directory.
extern std::string turn_gamepath_into_src_path(const std::string& gamepath, const std::string& src_file);

namespace fs = std::filesystem;

static const char* format_size_short(uint64_t bytes, char* buf, size_t buf_size) {
    if (bytes >= 1024ULL * 1024 * 1024)
        snprintf(buf, buf_size, "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ULL * 1024)
        snprintf(buf, buf_size, "%.2f MB", bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024ULL)
        snprintf(buf, buf_size, "%.2f KB", bytes / 1024.0);
    else
        snprintf(buf, buf_size, "%llu B", (unsigned long long)bytes);
    return buf;
}

static FnFactory<IPropertyEditor>& get_basic_factory_asset_inspector() {
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
        master_pg = std::make_unique<PropertyGrid>(get_basic_factory_asset_inspector());
        master_pg->add_iproped_manual(new MiMasterMatEditor(
            &parent_path,
            [this](const std::string& new_path) { rebuild_params_on_master_change(new_path); }
        ));
    }

    void build_params_pg() {
        params_pg = std::make_unique<PropertyGrid>(get_basic_factory_asset_inspector());
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

// ── SobjEditorState ──────────────────────────────────────────────────────────

// Shells out to ffprobe to read an audio file's native sample rate + channel count, for
// display only (the compiler itself never needs this -- ffmpeg re-detects it internally).
static bool probe_audio_info(const std::string& full_path, int& sample_rate, int& channels) {
    std::string cmd = "ffprobe.exe -v error -select_streams a:0 -show_entries stream=sample_rate,channels "
                       "-of csv=p=0 \"" + full_path + "\"";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.dwFlags = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }
    CloseHandle(hWrite);

    std::string output;
    char buf[256];
    DWORD bytes_read = 0;
    while (ReadFile(hRead, buf, sizeof(buf) - 1, &bytes_read, nullptr) && bytes_read > 0) {
        buf[bytes_read] = 0;
        output += buf;
    }
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    int sr = 0, ch = 0;
    if (sscanf(output.c_str(), "%d,%d", &sr, &ch) == 2) {
        sample_rate = sr;
        channels = ch;
        return true;
    }
    return false;
}

struct SoundEditorState {
    MakeObjectFromPathGeneric objmaker;
    std::unique_ptr<ReadSerializerBackendJson> reader;
    AudioImportSettings* ais = nullptr; // non-owning; reader owns the lifetime
    int source_sample_rate = 0;
    int source_channels = 0;
    bool source_probed = false;

    void init_from(const std::string& json) {
        reader = std::make_unique<ReadSerializerBackendJson>("inspector_ais", json, objmaker);
        ais = reader->get_root_obj() ? reader->get_root_obj()->cast_to<AudioImportSettings>() : nullptr;
    }
};

struct SobjEditorState {
    std::shared_ptr<ScriptableObject> obj; // keeps it alive/registered while the pane holds it
    std::unique_ptr<PropertyGrid> pg;

    void init_from(std::shared_ptr<ScriptableObject> asset) {
        obj = std::move(asset);
        pg = std::make_unique<PropertyGrid>(get_basic_factory_asset_inspector());
        pg->add_class_to_grid(obj.get());
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
    case gtf::bc7:              return "BC7_UNORM";
    case gtf::bc7_srgb:         return "BC7_UNORM_SRGB";
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
AssetInspectorPane::~AssetInspectorPane() {
    if (preview_player_)
        isound->remove_sound_player(preview_player_);
}

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
    sobj_state_.reset();
    sound_state_.reset();
    anim_seq_editor_.reset();
    skeleton_editor_.reset();
    active_tis_path_.clear();
    active_ais_path_.clear();

    if (preview_player_) {
        isound->remove_sound_player(preview_player_);
        preview_player_ = nullptr;
    }
    preview_scrub_dragging_ = false;

    auto ext = StringUtils::get_extension_no_dot(selected.filename);

    // .dds / .png → resolve to the .tis sidecar
    // .cmdl        → resolve to the .mis sidecar
    // .csnd / .wav → resolve to the .ais sidecar
    std::string load_path = selected.filename;
    if (ext == "dds" || ext == "png" || ext == "jpg") {
        load_path = strip_extension(selected.filename) + ".tis";
        ext = "tis";
    } else if (ext == "cmdl") {
        load_path = strip_extension(selected.filename) + ".mis";
        ext = "mis";
    } else if (ext == "csnd" || ext == "wav") {
        load_path = strip_extension(selected.filename) + ".ais";
        ext = "ais";
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

    // Build .sobj property editor state from the loaded ScriptableObject.
    if (ext == "sobj") {
        auto so = g_assets.find_sync_sptr<ScriptableObject>(selected.filename);
        if (so) {
            auto state = std::make_unique<SobjEditorState>();
            state->init_from(so);
            sobj_state_ = std::move(state);
        }
        return;
    }

    // Build .ais editor state (hand-rolled widgets, no generic PropertyGrid -- see
    // draw_sound_settings) and acquire a preview player pointed at the compiled .csnd.
    if (ext == "ais") {
        active_ais_path_ = load_path;
        std::string text = read_game_text(load_path);
        std::string json = strip_json_prefix(text);
        if (!json.empty()) {
            auto state = std::make_unique<SoundEditorState>();
            state->init_from(json);
            if (state->ais) {
                std::string src_full = FileSys::get_full_path_from_game_path(
                    turn_gamepath_into_src_path(load_path, state->ais->src_file));
                state->source_probed = probe_audio_info(src_full, state->source_sample_rate, state->source_channels);
            }
            sound_state_ = std::move(state);
        }

        // Never auto-plays -- paused until the user presses Play in draw_sound_settings.
        std::string csnd_path = strip_extension(load_path) + ".csnd";
        SoundFile* sf = g_assets.find<SoundFile>(csnd_path).get();
        if (sf) {
            preview_player_ = isound->register_sound_player();
            preview_player_->asset = sf;
            preview_player_->looping = false;
            preview_player_->spatialize = false;
            preview_player_->attenuate = false;
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
                c->mat_pg = std::make_unique<PropertyGrid>(get_basic_factory_asset_inspector());
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
    migrate_legacy_tis_compression(tis);

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

    {
        using tct = TextureCompressionType;
        static const tct kOptions[] = {
            tct::Compressed_BC1, tct::Uncompressed, tct::NormalMap_BC5,
            tct::GreyscaleMask_BC4, tct::HighQuality_BC7, tct::UseSourceFile,
        };
        static const char* kLabels[] = {
            "Compressed (BC1)", "Uncompressed (RGBA8)", "Normal map (BC5)",
            "Greyscale mask (BC4)", "High quality / alpha (BC7)",
            "Use source file (don't compress, UI texture)",
        };
        const int kCount = (int)(sizeof(kOptions) / sizeof(kOptions[0]));
        int cur = 0;
        for (int i = 0; i < kCount; i++)
            if (kOptions[i] == tis->compression) { cur = i; break; }

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Compression");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.f);
        if (ImGui::Combo("##compression", &cur, kLabels, kCount)) {
            tis->compression = kOptions[cur];
            changed = true;
        }
    }

    // sRGB only affects Compressed_BC1 / HighQuality_BC7; normal maps and masks are non-color data.
    bool srgb_applicable = tis->compression == TextureCompressionType::Compressed_BC1 ||
                            tis->compression == TextureCompressionType::HighQuality_BC7;
    if (!srgb_applicable) ImGui::BeginDisabled();
    changed |= ImGui::Checkbox("sRGB", &tis->is_srgb);
    if (!srgb_applicable) ImGui::EndDisabled();

    changed |= ImGui::Checkbox("Nearest filtering", &tis->nearest_filtering);

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
            if (tis->compression == TextureCompressionType::UseSourceFile) {
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
    if (mis->generate_auto_lods) {
        ImGui::SetNextItemWidth(120.f);
        changed |= ImGui::SliderInt("Prune Disconnected Islands From LOD", &mis->prune_disconnected_islands_min_lod, 0, 4,
            mis->prune_disconnected_islands_min_lod == 0 ? "Disabled" : "%d");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Auto-LOD level (1-4) at which meshopt may drop disconnected mesh islands. 0 disables island pruning entirely.");
    }
    changed |= ImGui::Checkbox("Uses Lightmap UV2", &mis->withLightmap);
    changed |= ImGui::Checkbox("Disable Prune Unused Bones", &mis->disablePruneUnusedBones);
    changed |= ImGui::Checkbox("Export Embedded Textures", &mis->exportEmbeddedTextures);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Write out the glb's embedded \"_ALB\"/\"_NRM\" textures on compile. Off by default.");

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

// Common sample rates offered in the override dropdown; index 0 (0 Hz) means "keep source".
static const int kSampleRateOptions[] = { 0, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 96000 };
static const char* kSampleRateLabels[] = {
    "Keep source", "8000 Hz", "11025 Hz", "16000 Hz", "22050 Hz",
    "24000 Hz", "32000 Hz", "44100 Hz", "48000 Hz", "96000 Hz",
};
static constexpr int kSampleRateCount = (int)(sizeof(kSampleRateOptions) / sizeof(kSampleRateOptions[0]));

void AssetInspectorPane::draw_sound_settings(const std::string& gamepath) {
    if (!sound_state_ || !sound_state_->ais) { ImGui::TextDisabled("(parse error)"); return; }
    auto* ais = sound_state_->ais;

    ImGui::LabelText("Source", "%s", ais->src_file.c_str());

    // --- Source vs. current (effective output) format ---
    if (sound_state_->source_probed) {
        ImGui::Text("Source: %d Hz, %s", sound_state_->source_sample_rate,
            sound_state_->source_channels == 1 ? "mono" : (sound_state_->source_channels == 2 ? "stereo" : "multi-channel"));
    } else {
        ImGui::TextDisabled("Source: (couldn't probe -- is ffprobe.exe on PATH?)");
    }
    int out_rate = (ais->sample_rate_override != 0) ? ais->sample_rate_override : sound_state_->source_sample_rate;
    int out_channels = ais->force_mono ? 1 : sound_state_->source_channels;
    ImGui::Text("Current: %d Hz, %s", out_rate, out_channels == 1 ? "mono" : (out_channels == 2 ? "stereo" : "multi-channel"));

    // --- Precompiled vs compiled size ---
    char buf1[32], buf2[32];
    std::string src_path = turn_gamepath_into_src_path(gamepath, ais->src_file);
    std::string csnd_path = strip_extension(gamepath) + ".csnd";
    std::error_code ec;
    uint64_t src_size = fs::file_size(FileSys::get_full_path_from_game_path(src_path), ec);
    if (ec) src_size = 0;
    uint64_t csnd_size = fs::file_size(FileSys::get_full_path_from_game_path(csnd_path), ec);
    if (ec) csnd_size = 0;
    ImGui::Text("Precompiled: %s", format_size_short(src_size, buf1, sizeof(buf1)));
    ImGui::Text("Compiled:    %s", format_size_short(csnd_size, buf2, sizeof(buf2)));

    ImGui::Separator();

    // --- Preview player ---
    SoundFile* sf = g_assets.find<SoundFile>(csnd_path).get();
    if (preview_player_ && sf) {
        float duration = sf->get_duration();
        bool playing = preview_player_->is_playing();

        if (ImGui::Button(playing ? "Pause" : "Play"))
            preview_player_->set_play(!playing);

        ImGui::SameLine();
        float pos_seconds = preview_player_->get_playback_position_seconds();
        ImGui::Text("%.1f / %.1f s", pos_seconds, duration);

        ImVec2 bar_size(ImGui::GetContentRegionAvail().x, 20.f);
        ImGui::InvisibleButton("##scrub", bar_size);
        ImVec2 bar_min = ImGui::GetItemRectMin();
        ImVec2 bar_max = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (ImGui::IsItemActive() && duration > 0.f) {
            preview_scrub_dragging_ = true;
            float mouse_x = ImGui::GetMousePos().x;
            float frac = (mouse_x - bar_min.x) / std::max(1.f, bar_max.x - bar_min.x);
            frac = std::clamp(frac, 0.f, 1.f);
            pos_seconds = frac * duration;
            preview_player_->seek_seconds(pos_seconds);
        } else {
            preview_scrub_dragging_ = false;
        }

        float frac = (duration > 0.f) ? std::clamp(pos_seconds / duration, 0.f, 1.f) : 0.f;
        dl->AddRectFilled(bar_min, bar_max, IM_COL32(60, 60, 60, 255));
        dl->AddRectFilled(bar_min, ImVec2(bar_min.x + (bar_max.x - bar_min.x) * frac, bar_max.y), IM_COL32(134, 217, 181, 255));
        dl->AddRect(bar_min, bar_max, IM_COL32(200, 200, 200, 255));
    } else {
        ImGui::TextDisabled("(no compiled .csnd to preview)");
    }

    ImGui::Separator();

    // --- Hand-rolled settings widgets ---
    bool changed = false;

    {
        static const AudioCodec kCodecOptions[] = { AudioCodec::PCM, AudioCodec::ADPCM, AudioCodec::Vorbis };
        static const char* kCodecLabels[] = { "PCM (uncompressed)", "ADPCM (compressed, cheap to decode)", "Vorbis (compressed, higher CPU)" };
        int cur = 0;
        for (int i = 0; i < 3; i++) if (kCodecOptions[i] == ais->codec) { cur = i; break; }
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Codec");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(260.f);
        if (ImGui::Combo("##codec", &cur, kCodecLabels, 3)) {
            ais->codec = kCodecOptions[cur];
            changed = true;
        }
    }

    {
        static const AudioLoadMode kModeOptions[] = { AudioLoadMode::Predecode, AudioLoadMode::DecodeOnPlay, AudioLoadMode::Streaming };
        static const char* kModeLabels[] = { "Predecode (decode fully on load)", "Decode on play (decode lazily)", "Streaming (reserved, not yet implemented)" };
        int cur = 0;
        for (int i = 0; i < 3; i++) if (kModeOptions[i] == ais->load_mode) { cur = i; break; }
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Load mode");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(260.f);
        if (ImGui::Combo("##loadmode", &cur, kModeLabels, 3)) {
            ais->load_mode = kModeOptions[cur];
            changed = true;
        }
    }

    changed |= ImGui::Checkbox("Force mono", &ais->force_mono);

    {
        int cur = 0;
        for (int i = 0; i < kSampleRateCount; i++) if (kSampleRateOptions[i] == ais->sample_rate_override) { cur = i; break; }
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Sample rate");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::Combo("##samplerate", &cur, kSampleRateLabels, kSampleRateCount)) {
            ais->sample_rate_override = kSampleRateOptions[cur];
            changed = true;
        }
    }

    if (ais->codec != AudioCodec::Vorbis) ImGui::BeginDisabled();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Vorbis quality");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.f);
    if (ImGui::SliderInt("##vorbisq", &ais->vorbis_quality_percent, 0, 100, "%d%%"))
        changed = true;
    if (ais->codec != AudioCodec::Vorbis) ImGui::EndDisabled();

    if (changed) settings_dirty = true;

    if (settings_dirty) {
        ImGui::Spacing();
        if (ImGui::Button("Apply")) {
            write_audio_import_settings(ais, gamepath);
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

void AssetInspectorPane::draw_scriptable_object(const std::string& gamepath) {
    if (!sobj_state_ || !sobj_state_->obj) { ImGui::TextDisabled("(failed to load)"); return; }
    auto& s = *sobj_state_;

    ImGui::TextDisabled("Type: %s", s.obj->get_type().classname);
    ImGui::Separator();

    // Surface any unknown/bad properties found on last load *before* the grid and Save button,
    // so the user knows a save will bake in whatever the reflection-driven reader could recover
    // (unknown keys are dropped; bad-typed values keep their in-memory default) instead of
    // silently losing/corrupting data on disk.
    const auto& warnings = s.obj->get_load_warnings();
    if (!warnings.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
        ImGui::TextWrapped("Warning: this asset had %zu problem(s) loading from disk:", warnings.size());
        for (auto& w : warnings)
            ImGui::BulletText("%s", w.c_str());
        ImGui::TextWrapped("Saving will write out only the recovered values above.");
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    bool changed = false;
    if (s.pg) {
        s.pg->update();
        changed = s.pg->rows_had_changes;
    }
    if (changed) {
        settings_dirty = true;
        s.obj->on_property_change();
    }
    s.obj->on_editor_gui();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::BeginDisabled(!settings_dirty);
    if (ImGui::Button("Save")) {
        s.obj->save_to_disk();
        settings_dirty = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Revert")) {
        g_assets.reload(s.obj.get());
        settings_dirty = false;
    }
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
    else if (ext == "ais") draw_sound_settings(selected.filename);
    else if (ext == "csnd" || ext == "wav") draw_sound_settings(active_ais_path_.empty() ? strip_extension(selected.filename) + ".ais" : active_ais_path_);
    else if (ext == "mm") draw_material_text(selected.filename);
    else if (ext == "mi") draw_material_instance_editor(selected.filename);
    else if (ext == "sobj") draw_scriptable_object(selected.filename);
    else    ImGui::TextDisabled("Extension: .%s", ext.c_str());

    ImGui::End();
}

#endif
