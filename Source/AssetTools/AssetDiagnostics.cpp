#ifdef EDITOR_BUILD
#include "AssetTools/AssetDiagnostics.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "json.hpp"
#include <fstream>
#include <sstream>
#include <vector>

// @docs [[asset_tools#diagnostics]]

AssetDiagnostics& AssetDiagnostics::get() {
    static AssetDiagnostics inst;
    return inst;
}

void AssetDiagnostics::set(const std::string& gamepath, std::vector<AssetDiagnostic> diags) {
    if (diags.empty())
        cache_.erase(gamepath);
    else
        cache_[gamepath] = std::move(diags);
}

void AssetDiagnostics::clear(const std::string& gamepath) {
    cache_.erase(gamepath);
}

const std::vector<AssetDiagnostic>* AssetDiagnostics::get_diags(const std::string& gamepath) const {
    auto it = cache_.find(gamepath);
    if (it == cache_.end())
        return nullptr;
    return &it->second;
}

std::optional<AssetSeverity> AssetDiagnostics::get_severity(const std::string& gamepath) const {
    auto* diags = get_diags(gamepath);
    if (!diags || diags->empty())
        return std::nullopt;
    for (auto& d : *diags) {
        if (d.severity == AssetSeverity::Error)
            return AssetSeverity::Error;
    }
    return AssetSeverity::Warning;
}

static std::string cache_full_path() {
    return std::string(FileSys::get_game_path()) + "/.asset_diag_cache.json";
}

void AssetDiagnostics::save() const {
    nlohmann::json j;
    for (auto& [path, diags] : cache_) {
        nlohmann::json arr = nlohmann::json::array();
        for (auto& d : diags) {
            arr.push_back({
                {"severity", d.severity == AssetSeverity::Error ? "error" : "warning"},
                {"message", d.message}
            });
        }
        j[path] = arr;
    }
    std::ofstream f(cache_full_path());
    if (f.is_open())
        f << j.dump(2);
}

void AssetDiagnostics::load() {
    cache_.clear();
    std::ifstream f(cache_full_path());
    if (!f.is_open())
        return;
    try {
        nlohmann::json j;
        f >> j;
        for (auto& [path, arr] : j.items()) {
            std::vector<AssetDiagnostic> diags;
            for (auto& entry : arr) {
                AssetDiagnostic d;
                std::string sev = entry.value("severity", "error");
                d.severity = (sev == "error") ? AssetSeverity::Error : AssetSeverity::Warning;
                d.message = entry.value("message", "");
                diags.push_back(std::move(d));
            }
            if (!diags.empty())
                cache_[path] = std::move(diags);
        }
    } catch (...) {
        cache_.clear();
    }
}

static bool game_file_exists(const std::string& rel) {
    return FileSys::does_file_exist(rel.c_str(), FileSys::GAME_DIR);
}

static std::string read_game_file_text(const std::string& gamepath) {
    auto f = FileSys::open_read_game(gamepath);
    if (!f) return {};
    std::vector<char> buf(f->size() + 1, 0);
    f->read(buf.data(), f->size());
    f->close();
    return buf.data();
}

void AssetDiagnostics::scan_dependencies(const std::string& gamepath) {
    auto ext = StringUtils::get_extension_no_dot(gamepath);
    std::vector<AssetDiagnostic> diags;

    if (ext == "tis" || ext == "mis") {
        std::string text = read_game_file_text(gamepath);
        if (text.empty()) return;
        try {
            auto j = nlohmann::json::parse(text);
            if (j.contains("src_file")) {
                std::string src = j["src_file"].get<std::string>();
                if (!src.empty()) {
                    // src_file is relative to the asset's directory
                    auto slash = gamepath.rfind('/');
                    std::string dir = (slash != std::string::npos) ? gamepath.substr(0, slash + 1) : "";
                    std::string src_path = dir + src;
                    if (!game_file_exists(src_path))
                        diags.push_back({AssetSeverity::Error, "missing source file: " + src});
                }
            }
        } catch (...) {}
    } else if (ext == "mi") {
        std::string text = read_game_file_text(gamepath);
        if (text.empty()) return;
        std::istringstream ss(text);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.rfind("PARENT", 0) == 0) {
                std::string parent = line.substr(6);
                // trim whitespace
                auto trim = [](std::string& s) {
                    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
                    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
                };
                trim(parent);
                if (!parent.empty()) {
                    if (!game_file_exists(parent)) {
                        diags.push_back({AssetSeverity::Error, "missing dependency: " + parent});
                    } else {
                        auto sev = get_severity(parent);
                        if (sev && *sev == AssetSeverity::Error)
                            diags.push_back({AssetSeverity::Warning, "dependency has errors: " + parent});
                    }
                }
                break;
            }
        }
    }

    if (!diags.empty())
        set(gamepath, std::move(diags));
    else
        clear(gamepath);
}

void AssetDiagnostics::scan_all() {
    for (const auto& full : FileSys::find_game_files()) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        auto ext = StringUtils::get_extension_no_dot(gp);
        if (ext == "tis" || ext == "mis" || ext == "mi")
            scan_dependencies(gp);
    }
    save();
}

#endif
