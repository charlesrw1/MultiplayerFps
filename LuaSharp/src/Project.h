#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace luasharp {
// luasharp.json marks the root of a script project:
//   { "declarations": [".luasharp"] }
// Each entry is a .ls file or a folder of them, relative to luasharp.json. They declare the host types (the
// CsRemake engine writes .luasharp/engine.ls). A .ls file belongs to the nearest luasharp.json above it.
inline constexpr const char* ProjectFileName = "luasharp.json";

struct Project {
    std::filesystem::path file;                          // luasharp.json
    std::filesystem::path root;                          // its folder
    std::vector<std::filesystem::path> declarationFiles; // sorted
    std::vector<std::string> missing;                    // declaration entries that do not exist (yet)
};

// Throws std::runtime_error when the file cannot be read or is malformed.
Project LoadProject(const std::filesystem::path& projectFile);
// The luasharp.json in the folder of sourceFile or above, if any.
std::optional<std::filesystem::path> FindProjectFile(const std::filesystem::path& sourceFile);
// Absolute, normal and lower case: equal paths give equal keys on Windows.
std::string PathKey(const std::filesystem::path& path);
bool IsInside(const std::filesystem::path& path, const std::filesystem::path& folder);
}
