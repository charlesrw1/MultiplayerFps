#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>

struct AssetOpResult {
    bool success = false;
    std::string error;
};

namespace AssetOps {
    // Move src to dst_folder, renaming all references in game files
    AssetOpResult mv(const std::string& src_gamepath, const std::string& dst_folder);
    // Delete (trash) an asset and its sidecar files
    AssetOpResult rm(const std::string& gamepath);
    // Copy without updating references
    AssetOpResult cp(const std::string& src_gamepath, const std::string& dst_gamepath);
    // Create directory
    AssetOpResult mkdir(const std::string& dir_gamepath);

    // Find all game files that contain a reference to asset_gamepath
    std::vector<std::string> find_references(const std::string& asset_gamepath);
}

#endif
