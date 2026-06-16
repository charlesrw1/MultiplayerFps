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
    AssetSeverity worst = AssetSeverity::TransitiveWarning;
    for (auto& d : *diags)
        if (d.severity > worst) worst = d.severity;
    return worst;
}

static std::string cache_full_path() {
    return std::string(FileSys::get_game_path()) + "/.asset_diag_cache.json";
}

static const char* sev_to_str(AssetSeverity s) {
    switch (s) {
    case AssetSeverity::Error:            return "error";
    case AssetSeverity::Warning:          return "warning";
    case AssetSeverity::TransitiveWarning: return "transitive";
    }
    return "transitive";
}

static AssetSeverity str_to_sev(const std::string& s) {
    if (s == "error")   return AssetSeverity::Error;
    if (s == "warning") return AssetSeverity::Warning;
    return AssetSeverity::TransitiveWarning;
}

void AssetDiagnostics::save() const {
    nlohmann::json j;
    for (auto& [path, diags] : cache_) {
        nlohmann::json arr = nlohmann::json::array();
        for (auto& d : diags)
            arr.push_back({{"severity", sev_to_str(d.severity)}, {"message", d.message}});
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
                d.severity = str_to_sev(entry.value("severity", "transitive"));
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

// Pass 1: check direct source file dependencies exist on disk
void AssetDiagnostics::scan_dependencies(const std::string& gamepath) {
    auto ext = StringUtils::get_extension_no_dot(gamepath);
    std::vector<AssetDiagnostic> diags;

    if (ext == "tis" || ext == "mis") {
        std::string text = read_game_file_text(gamepath);
        if (text.empty()) return;
        // strip !json prefix
        if (text.find("!json") == 0) {
            size_t nl = text.find('\n');
            text = (nl != std::string::npos) ? text.substr(nl + 1) : text.substr(5);
        }
        try {
            auto j = nlohmann::json::parse(text);
            // tis: src_file; mis: srcGlbFile
            std::string src;
            if (j.contains("src_file"))   src = j["src_file"].get<std::string>();
            if (j.contains("srcGlbFile")) src = j["srcGlbFile"].get<std::string>();
            if (!src.empty()) {
                auto slash = gamepath.rfind('/');
                std::string dir = (slash != std::string::npos) ? gamepath.substr(0, slash + 1) : "";
                std::string src_path = dir + src;
                if (!game_file_exists(src_path))
                    diags.push_back({AssetSeverity::Error, "missing source file: " + src});
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
                while (!parent.empty() && (parent.front() == ' ' || parent.front() == '\t')) parent.erase(parent.begin());
                while (!parent.empty() && (parent.back() <= ' ')) parent.pop_back();
                if (!parent.empty() && !game_file_exists(parent))
                    diags.push_back({AssetSeverity::Error, "missing parent: " + parent});
                break;
            }
        }
    }

    if (!diags.empty())
        set(gamepath, std::move(diags));
    else
        clear(gamepath);
}

// Extract all double-quoted strings from text that look like asset gamepaths
static std::vector<std::string> extract_path_refs(const std::string& text) {
    static constexpr std::string_view kExts[] = {
        "dds","png","cmdl","mm","mi","glb","tis","mis","lua","tmap"
    };
    std::vector<std::string> refs;
    size_t pos = 0;
    while ((pos = text.find('"', pos)) != std::string::npos) {
        ++pos;
        size_t end = text.find('"', pos);
        if (end == std::string::npos) break;
        std::string_view sv(text.data() + pos, end - pos);
        pos = end + 1;
        // must contain a slash (not absolute), not be a URL
        if (sv.empty() || sv[0] == '/' || sv.find('/') == std::string_view::npos) continue;
        auto dot = sv.rfind('.');
        if (dot == std::string_view::npos) continue;
        std::string_view ext = sv.substr(dot + 1);
        for (auto& e : kExts) {
            if (ext == e) { refs.emplace_back(sv); break; }
        }
    }
    return refs;
}

static constexpr const char* kTextAssetExts[] = {
    "mi", "mm", "mis", "tis", "tmap", nullptr
};

static bool is_text_asset(const std::string& ext) {
    for (int i = 0; kTextAssetExts[i]; ++i)
        if (ext == kTextAssetExts[i]) return true;
    return false;
}

void AssetDiagnostics::scan_all() {
    auto files = FileSys::find_game_files();

    // Pass 1: direct source dependency errors
    for (const auto& full : files) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        auto ext = StringUtils::get_extension_no_dot(gp);
        if (ext == "tis" || ext == "mis" || ext == "mi")
            scan_dependencies(gp);
    }

    // Pass 2: transitive reference propagation across all text assets.
    // For each file that currently has no Error: scan quoted path refs, check their severity.
    // dep Error   → Warning on this file
    // dep Warning or TransitiveWarning → TransitiveWarning on this file
    for (const auto& full : files) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        auto ext = StringUtils::get_extension_no_dot(gp);
        if (!is_text_asset(ext)) continue;

        // Don't downgrade an existing Error
        auto existing = get_severity(gp);
        if (existing && *existing == AssetSeverity::Error) continue;

        std::string text = read_game_file_text(gp);
        if (text.empty()) continue;
        auto refs = extract_path_refs(text);
        if (refs.empty()) continue;

        AssetSeverity worst_dep = AssetSeverity::TransitiveWarning; // floor
        bool has_issue = false;
        for (auto& ref : refs) {
            if (!game_file_exists(ref)) {
                worst_dep = AssetSeverity::Error;
                has_issue = true;
            } else {
                auto dep_sev = get_severity(ref);
                if (dep_sev) {
                    has_issue = true;
                    if (*dep_sev > worst_dep) worst_dep = *dep_sev;
                }
            }
        }
        if (!has_issue) continue;

        // Propagation: dep Error → Warning; dep Warning/TW → TransitiveWarning
        AssetSeverity new_sev = (worst_dep == AssetSeverity::Error)
            ? AssetSeverity::Warning
            : AssetSeverity::TransitiveWarning;

        if (!existing || new_sev > *existing) {
            auto* d = get_diags(gp);
            std::vector<AssetDiagnostic> merged;
            if (d) merged = *d;
            merged.push_back({new_sev, "transitive dependency issue"});
            set(gp, std::move(merged));
        }
    }

    save();
}

#endif
