#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>

class AssetPackager {
public:
    struct PackageManifest {
        std::vector<std::string> asset_paths;
    };

    PackageManifest gather_manifest(const std::string& root_dir) const;
    bool package_to_bundle(const PackageManifest&, const std::string& output_path) const;
};

#endif
