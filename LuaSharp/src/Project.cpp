#include "Project.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace luasharp {
namespace fs = std::filesystem;

std::string PathKey(const fs::path& path) {
    std::error_code ec;
    fs::path absolute = fs::weakly_canonical(fs::absolute(path, ec), ec);
    if (ec) absolute = fs::absolute(path).lexically_normal();
    std::string key = absolute.generic_string();
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    while (key.size() > 1 && key.back() == '/') key.pop_back();
    return key;
}

bool IsInside(const fs::path& path, const fs::path& folder) {
    const std::string inner = PathKey(path), outer = PathKey(folder);
    return inner.size() > outer.size() && inner.compare(0, outer.size(), outer) == 0 && inner[outer.size()] == '/';
}

std::optional<fs::path> FindProjectFile(const fs::path& sourceFile) {
    std::error_code ec;
    for (fs::path folder = fs::absolute(sourceFile, ec).parent_path(); !folder.empty(); folder = folder.parent_path()) {
        const fs::path candidate = folder / ProjectFileName;
        if (fs::is_regular_file(candidate, ec)) return candidate;
        if (folder == folder.root_path()) break;
    }
    return std::nullopt;
}

Project LoadProject(const fs::path& projectFile) {
    std::ifstream file(projectFile, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read " + projectFile.string());
    const auto json = nlohmann::json::parse(file, nullptr, false);
    if (json.is_discarded() || !json.is_object()) throw std::runtime_error(projectFile.string() + ": not a JSON object");
    const auto entries = json.value("declarations", nlohmann::json::array());
    if (!entries.is_array()) throw std::runtime_error(projectFile.string() + ": \"declarations\" must be an array of paths");

    Project project{projectFile, projectFile.parent_path(), {}, {}};
    std::error_code ec;
    for (const auto& entry : entries) {
        if (!entry.is_string()) throw std::runtime_error(projectFile.string() + ": \"declarations\" must be an array of paths");
        const fs::path path = project.root / fs::path(entry.get<std::string>());
        if (fs::is_regular_file(path, ec)) { project.declarationFiles.push_back(path); continue; }
        if (!fs::is_directory(path, ec)) { project.missing.push_back(entry.get<std::string>()); continue; }
        for (const auto& item : fs::recursive_directory_iterator(path, ec))
            if (item.is_regular_file() && item.path().extension() == ".ls") project.declarationFiles.push_back(item.path());
    }
    std::sort(project.declarationFiles.begin(), project.declarationFiles.end());
    return project;
}
}
