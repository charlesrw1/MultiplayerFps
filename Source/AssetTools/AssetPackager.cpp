#ifdef EDITOR_BUILD
#include "AssetTools/AssetPackager.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "Framework/Util.h"

// @docs [[asset_tools#packager]]

AssetPackager::PackageManifest AssetPackager::gather_manifest(const std::string& root_dir) const {
    PackageManifest manifest;
    for (const auto& full : FileSys::find_game_files_path(root_dir)) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        manifest.asset_paths.push_back(gp);
    }
    return manifest;
}

bool AssetPackager::package_to_bundle(const PackageManifest&, const std::string& output_path) const {
    sys_print(Info, "AssetPackager::package_to_bundle: not implemented (output: %s)\n", output_path.c_str());
    return false;
}

#endif
