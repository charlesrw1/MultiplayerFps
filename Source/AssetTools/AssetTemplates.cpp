#ifdef EDITOR_BUILD
#include "AssetTools/AssetTemplates.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "Render/Editor/TextureEditor.h"
#include "AssetCompile/ModelAsset2.h"
#include "Framework/Util.h"
#include "Render/MaterialLocal.h"
#include "Render/Model.h"
#include "Assets/AssetDatabase.h"

// @docs [[asset_tools#templates]]

// Defined in Model.cpp (Core) — forward-declared here to avoid a circular header dep
extern void write_model_import_settings(ModelImportSettings* mis, const std::string& savepath);

// Defined in TextureEditor.cpp (moved into AssetTools) — available in same lib
extern void write_texture_import_settings(TextureImportSettings* tis, const std::string& path);

namespace AssetTemplates {

static std::string strip_name(const std::string& name) {
    auto result = StringUtils::strip_extension(name);
    return result.empty() ? name : result;
}

std::optional<std::string> create_tis_for_png(const std::string& png_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(png_gamepath) == "png");

    std::string tis_gamepath = png_gamepath.substr(0, png_gamepath.size() - 3) + "tis";

    if (FileSys::does_file_exist(tis_gamepath.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    auto slash = png_gamepath.rfind('/');
    std::string src_filename = (slash != std::string::npos) ? png_gamepath.substr(slash + 1) : png_gamepath;

    bool is_normal = (src_filename.find("normal") != std::string::npos ||
                      src_filename.find("Normal") != std::string::npos);

    TextureImportSettings tis;
    tis.src_file = src_filename;
    tis.compression = is_normal ? TextureCompressionType::NormalMap_BC5 : TextureCompressionType::Compressed_BC1;
    tis.is_srgb = !is_normal;

    write_texture_import_settings(&tis, tis_gamepath);
    return tis_gamepath;
}

std::optional<std::string> create_mis_for_glb(const std::string& glb_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(glb_gamepath) == "glb");

    std::string mis_gamepath = glb_gamepath.substr(0, glb_gamepath.size() - 3) + "mis";

    if (FileSys::does_file_exist(mis_gamepath.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    auto slash = glb_gamepath.rfind('/');
    std::string src_filename = (slash != std::string::npos) ? glb_gamepath.substr(slash + 1) : glb_gamepath;

    ModelImportSettings mis;
    mis.srcGlbFile = src_filename;

    write_model_import_settings(&mis, mis_gamepath);
    return mis_gamepath;
}

std::optional<std::string> create_mi_from_template(const std::string& dir,
    const std::string& name, const std::string& master_mm_path) {
    auto base = strip_name(name);
    std::string mi_gamepath = dir.empty() ? (base + ".mi") : (dir + "/" + base + ".mi");

    if (FileSys::does_file_exist(mi_gamepath.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    std::string text = "PARENT " + master_mm_path + "\n";

    auto f = FileSys::open_write_game(mi_gamepath);
    if (!f) return std::nullopt;
    f->write(text.data(), text.size());
    f->close();

    return mi_gamepath;
}

std::optional<std::string> create_mm_from_template(const std::string& dir,
    const std::string& name, const std::string& domain) {
    auto base = strip_name(name);
    std::string mm_gamepath = dir.empty() ? (base + ".mm") : (dir + "/" + base + ".mm");

    if (FileSys::does_file_exist(mm_gamepath.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    std::string text;
    text += "TYPE MaterialMaster\n";
    text += "DOMAIN " + domain + "\n";
    text += "\n";
    text += "// OPT BlendMode    Opaque | Blend | Add | Mult | Screen | PreMult\n";
    text += "// OPT LightingMode Lit | Unlit\n";
    text += "// OPT AlphaTested  false | true\n";
    text += "// OPT ShowBackfaces false | true\n";
    text += "// Decal only: OPT WriteAlbedo, WriteNormal, WriteEmissive, WriteRoughMetal\n";
    text += "OPT BlendMode Opaque\n";
    text += "OPT LightingMode Lit\n";
    text += "\n";
    text += "// VAR types: texture2D, float, vec4, bool\n";
    text += "VAR texture2D Albedo \"_white\"\n";
    text += "VAR texture2D Normalmap \"_flat_normal\"\n";
    text += "VAR texture2D Roughness \"_white\"\n";
    text += "VAR float RoughnessMult 1.0\n";
    text += "VAR float MetallicValue 0.0\n";
    text += "\n";
    text += "_FS_BEGIN\n";
    text += "// Inputs:\n";
    text += "//   FS_IN_Texcoord    (vec2)  UV coordinates\n";
    text += "//   FS_IN_FragPos     (vec3)  world-space position\n";
    text += "//   FS_IN_Normal      (vec3)  world-space normal\n";
    text += "//   FS_IN_TBN         (mat3)  tangent-to-world matrix\n";
    text += "//   FS_IN_VertexColor (vec4)  vertex color\n";
    text += "//   g.viewpos_time.w  (float) elapsed time\n";
    text += "//\n";
    text += "// Outputs:\n";
    text += "//   BASE_COLOR (vec3)  albedo, default vec3(0)\n";
    text += "//   NORMALMAP  (vec3)  tangent-space normal, default vec3(0.5,0.5,1)\n";
    text += "//   ROUGHNESS  (float) default 0.5\n";
    text += "//   METALLIC   (float) default 0.0\n";
    text += "//   EMISSIVE   (vec3)  added after lighting, default vec3(0)\n";
    text += "//   OPACITY    (float) alpha, default 1.0\n";
    text += "//   AOMAP      (float) ambient occlusion, default 1.0\n";
    text += "void FSmain()\n";
    text += "{\n";
    text += "    BASE_COLOR = texture(Albedo, FS_IN_Texcoord).xyz;\n";
    text += "    NORMALMAP = texture(Normalmap, FS_IN_Texcoord).xyz;\n";
    text += "    ROUGHNESS = texture(Roughness, FS_IN_Texcoord).r * RoughnessMult;\n";
    text += "    METALLIC = MetallicValue;\n";
    text += "}\n";
    text += "_FS_END\n";

    auto f = FileSys::open_write_game(mm_gamepath);
    if (!f) return std::nullopt;
    f->write(text.data(), text.size());
    f->close();

    return mm_gamepath;
}

std::optional<std::string> create_empty_map(const std::string& dir, const std::string& name) {
    auto base = strip_name(name);
    std::string map_gamepath = dir.empty() ? (base + ".tmap") : (dir + "/" + base + ".tmap");

    if (FileSys::does_file_exist(map_gamepath.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    std::string text = "!json\n{\"objs\":[]}\n";

    auto f = FileSys::open_write_game(map_gamepath);
    if (!f) return std::nullopt;
    f->write(text.data(), text.size());
    f->close();

    return map_gamepath;
}

std::optional<std::string> create_empty_particle(const std::string& dir, const std::string& name) {
    auto base = strip_name(name);
    std::string path = dir.empty() ? (base + ".particle") : (dir + "/" + base + ".particle");

    if (FileSys::does_file_exist(path.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    std::string text =
        "{ \"subsystems\": [\n"
        "    { \"name\": \"Default\", \"editor_visible\": true,\n"
        "      \"main\": { \"duration\": 5.0, \"looping\": true, \"start_lifetime\": { \"mode\": \"Constant\", \"min\": 1.0 }, \"start_speed\": { \"mode\": \"Constant\", \"min\": 5.0 }, \"start_size\": { \"mode\": \"Constant\", \"min\": 1.0 } },\n"
        "      \"emission\": { \"enabled\": true, \"rate_over_time\": { \"mode\": \"Constant\", \"min\": 10.0 } },\n"
        "      \"shape\": { \"enabled\": true, \"shape\": \"Cone\", \"radius\": 1.0, \"angle\": 25.0 },\n"
        "      \"renderer\": { \"enabled\": true, \"material\": \"eng/default_particle.mi\", \"render_mode\": \"Billboard\" }\n"
        "    }\n"
        "] }\n";

    auto f = FileSys::open_write_game(path);
    if (!f) return std::nullopt;
    f->write(text.data(), text.size());
    f->close();

    return path;
}

std::optional<std::string> create_empty_prefab(const std::string& dir, const std::string& name) {
    auto base = strip_name(name);
    std::string path = dir.empty() ? (base + ".tprefab") : (dir + "/" + base + ".tprefab");

    if (FileSys::does_file_exist(path.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    std::string text = "!json\n{\"__version\":1,\"objs\":[]}\n";

    auto f = FileSys::open_write_game(path);
    if (!f) return std::nullopt;
    f->write(text.data(), text.size());
    f->close();

    return path;
}

std::optional<std::string> create_prefab_for_model(const std::string& dir, const std::string& name,
    const std::string& model_gamepath) {
    auto base = strip_name(name);
    std::string path = dir.empty() ? (base + ".tprefab") : (dir + "/" + base + ".tprefab");

    if (FileSys::does_file_exist(path.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    // Offset the mesh within the prefab so the model's bounding box sits x/z-centered with its
    // bottom on the ground plane (y=0) -- the prefab's own local origin, not the model's raw pivot,
    // becomes the "stand on the ground" point. Falls back to no offset (identity) if the model can't
    // be loaded yet (e.g. not imported).
    std::string position_field;
    auto model = g_assets.find_sync_sptr<Model>(model_gamepath);
    if (model) {
        const Bounds b = model->get_bounds();
        const glm::vec3 center = b.get_center();
        const glm::vec3 offset = glm::vec3(-center.x, -b.bmin.y, -center.z);
        position_field = ",\"position\":[" + std::to_string(offset.x) + "," + std::to_string(offset.y) + ","
            + std::to_string(offset.z) + "]";
    }

    std::string text = "!json\n{\"__version\":1,\"objs\":[{\"__typename\":\"MeshComponent\",\"model\":\""
        + model_gamepath + "\"" + position_field + "}]}\n";

    auto f = FileSys::open_write_game(path);
    if (!f) return std::nullopt;
    f->write(text.data(), text.size());
    f->close();

    return path;
}

std::optional<std::string> create_mi_from_master(const std::string& dir,
    const std::string& name, const std::string& master_mm_path) {
    auto base = strip_name(name);
    std::string mi_gamepath = dir.empty() ? (base + ".mi") : (dir + "/" + base + ".mi");

    if (FileSys::does_file_exist(mi_gamepath.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    std::string text;
    text += "TYPE MaterialInstance\n";
    text += "PARENT " + master_mm_path + "\n";

    auto mat = g_assets.find<MaterialInstance>(master_mm_path);
    if (mat.get() && mat->is_valid_to_use() && mat->impl) {
        auto* master = mat->impl->get_master_impl();
        if (master) {
            for (auto& def : master->param_defs) {
                auto& val = def.default_value;
                switch (val.type) {
                case MatParamType::Float:
                    text += "VAR " + def.name + " " + std::to_string(val.scalar) + "\n";
                    break;
                case MatParamType::Vector:
                {
                    Color32 c(val.color32);
                    text += "VAR " + def.name + " " + std::to_string(c.r) + " "
                        + std::to_string(c.g) + " " + std::to_string(c.b) + " "
                        + std::to_string(c.a) + "\n";
                    break;
                }
                case MatParamType::Texture2D:
                    if (val.tex)
                        text += "VAR " + def.name + " " + val.tex->get_name() + "\n";
                    else
                        text += "VAR " + def.name + " _white\n";
                    break;
                case MatParamType::FloatVec:
                    text += "VAR " + def.name + " " + std::to_string(val.vector.x) + " "
                        + std::to_string(val.vector.y) + " " + std::to_string(val.vector.z) + " "
                        + std::to_string(val.vector.w) + "\n";
                    break;
                case MatParamType::Bool:
                    text += "VAR " + def.name + " " + (val.boolean ? "1" : "0") + "\n";
                    break;
                default:
                    break;
                }
            }
        }
    }

    auto f = FileSys::open_write_game(mi_gamepath);
    if (!f) return std::nullopt;
    f->write(text.data(), text.size());
    f->close();

    return mi_gamepath;
}

int auto_import_all_png() {
    int count = 0;
    for (const auto& full : FileSys::find_game_files()) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        if (StringUtils::get_extension_no_dot(gp) != "png")
            continue;
        if (gp.find(".thumbnails/") != std::string::npos)
            continue;
        auto result = create_tis_for_png(gp);
        if (result) {
            sys_print(Info, "Auto-import: created %s for %s\n", result->c_str(), gp.c_str());
            ++count;
        }
    }
    return count;
}

} // namespace AssetTemplates

#endif
