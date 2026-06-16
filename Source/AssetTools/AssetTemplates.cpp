#ifdef EDITOR_BUILD
#include "AssetTools/AssetTemplates.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "Render/Editor/TextureEditor.h"
#include "AssetCompile/ModelAsset2.h"
#include "Framework/Util.h"

// @docs [[asset_tools#templates]]

// Defined in Model.cpp (Core) — forward-declared here to avoid a circular header dep
extern void write_model_import_settings(ModelImportSettings* mis, const std::string& savepath);

// Defined in TextureEditor.cpp (moved into AssetTools) — available in same lib
extern void write_texture_import_settings(TextureImportSettings* tis, const std::string& path);

namespace AssetTemplates {

std::optional<std::string> create_tis_for_png(const std::string& png_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(png_gamepath) == "png");

    std::string tis_gamepath = png_gamepath.substr(0, png_gamepath.size() - 3) + "tis";

    if (FileSys::does_file_exist(tis_gamepath.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    auto slash = png_gamepath.rfind('/');
    std::string src_filename = (slash != std::string::npos) ? png_gamepath.substr(slash + 1) : png_gamepath;

    TextureImportSettings tis;
    tis.src_file = src_filename;
    tis.is_normalmap = (src_filename.find("normal") != std::string::npos ||
                        src_filename.find("Normal") != std::string::npos);
    tis.is_srgb = !tis.is_normalmap;

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
    std::string mi_gamepath = dir.empty() ? (name + ".mi") : (dir + "/" + name + ".mi");

    if (FileSys::does_file_exist(mi_gamepath.c_str(), FileSys::GAME_DIR))
        return std::nullopt;

    std::string text = "PARENT " + master_mm_path + "\n";

    auto f = FileSys::open_write_game(mi_gamepath);
    if (!f) return std::nullopt;
    f->write(text.data(), text.size());
    f->close();

    return mi_gamepath;
}

} // namespace AssetTemplates

#endif
