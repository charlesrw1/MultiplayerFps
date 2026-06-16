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
    case AssetSeverity::Error:             return "error";
    case AssetSeverity::Warning:           return "warning";
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

// Returns the gamepath stem: "textures/foo.tis" -> "textures/foo."
static std::string stem(const std::string& gp, const std::string& ext) {
    return gp.substr(0, gp.size() - ext.size()); // includes trailing '.'
}

// scan_all: fast file-existence checks only — no content reading.
// Sets Warning diagnostics for missing sidecars and obvious source-file mismatches.
// Does NOT overwrite existing Error-level diagnostics (set by the compiler).
// Called at editor startup (after load()) and at the end of build_all().
void AssetDiagnostics::scan_all() {
    for (const auto& full : FileSys::find_game_files()) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        auto ext = StringUtils::get_extension_no_dot(gp);

        // Don't downgrade compile errors with passive warnings.
        auto existing = get_severity(gp);
        if (existing && *existing == AssetSeverity::Error) continue;

        auto s = stem(gp, ext);
        std::vector<AssetDiagnostic> diags;

        if (ext == "cmdl") {
            // Compiled model: usable but can't reimport without .mis
            if (!game_file_exists(s + "mis"))
                diags.push_back({AssetSeverity::Warning, "no .mis import settings"});
        } else if (ext == "dds") {
            // Compiled texture: usable but can't reimport without .tis
            if (!game_file_exists(s + "tis"))
                diags.push_back({AssetSeverity::Warning, "no .tis import settings"});
        } else if (ext == "mis") {
            // Import settings: source .glb should be nearby (same-stem convention)
            if (!game_file_exists(s + "glb"))
                diags.push_back({AssetSeverity::Warning, "source .glb not found alongside .mis"});
        } else if (ext == "tis") {
            // Import settings: source .png should be nearby (same-stem convention)
            if (!game_file_exists(s + "png"))
                diags.push_back({AssetSeverity::Warning, "source .png not found alongside .tis"});
        }

        // Only update if we found something new and it's not already covered.
        if (!diags.empty() && !existing)
            set(gp, std::move(diags));
    }

    save();
}

// scan_transitive: content-reading pass for propagating errors across references.
// Runs only during build_all(), never at startup.
void AssetDiagnostics::scan_transitive() {
    static constexpr const char* kTextExts[] = { "mi", "mm", "mis", "tis", "tmap", nullptr };

    static constexpr std::string_view kRefExts[] = {
        "dds","png","cmdl","mm","mi","glb","tis","mis","lua","tmap"
    };

    auto extract_refs = [&](const std::string& text) -> std::vector<std::string> {
        std::vector<std::string> refs;
        size_t pos = 0;
        while ((pos = text.find('"', pos)) != std::string::npos) {
            ++pos;
            size_t end = text.find('"', pos);
            if (end == std::string::npos) break;
            std::string_view sv(text.data() + pos, end - pos);
            pos = end + 1;
            if (sv.empty() || sv[0] == '/' || sv.find('/') == std::string_view::npos) continue;
            auto dot = sv.rfind('.');
            if (dot == std::string_view::npos) continue;
            auto e = sv.substr(dot + 1);
            for (auto& ke : kRefExts)
                if (e == ke) { refs.emplace_back(sv); break; }
        }
        return refs;
    };

    for (const auto& full : FileSys::find_game_files()) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        auto ext = StringUtils::get_extension_no_dot(gp);

        bool is_text = false;
        for (int i = 0; kTextExts[i]; ++i)
            if (ext == kTextExts[i]) { is_text = true; break; }
        if (!is_text) continue;

        auto existing = get_severity(gp);
        if (existing && *existing == AssetSeverity::Error) continue;

        auto f = FileSys::open_read_game(gp);
        if (!f) continue;
        std::vector<char> buf(f->size() + 1, 0);
        f->read(buf.data(), f->size());
        f->close();
        std::string text(buf.data());

        auto refs = extract_refs(text);
        if (refs.empty()) continue;

        AssetSeverity worst_dep = AssetSeverity::TransitiveWarning;
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

    // Propagate sidecar diagnostics to compiled outputs:
    //   .mis issues (bad material refs, etc.) -> .cmdl
    //   .tis issues (bad texture refs, etc.)  -> .dds
    // Snapshot keys first to avoid modifying the map while iterating.
    std::vector<std::pair<std::string, std::string>> pairs;
    for (auto& [path, _] : cache_) {
        auto ext = StringUtils::get_extension_no_dot(path);
        if (ext == "mis")
            pairs.emplace_back(path, path.substr(0, path.size() - 3) + "cmdl");
        else if (ext == "tis")
            pairs.emplace_back(path, path.substr(0, path.size() - 3) + "dds");
    }
    for (auto& [src, dst] : pairs) {
        auto src_sev = get_severity(src);
        if (!src_sev) continue;
        auto dst_sev = get_severity(dst);
        if (dst_sev && *dst_sev >= *src_sev) continue;
        auto* d = get_diags(dst);
        std::vector<AssetDiagnostic> merged;
        if (d) merged = *d;
        merged.push_back({*src_sev, "from " + src});
        set(dst, std::move(merged));
    }

    save();
}

// scan_dependencies: kept for compatibility — delegates to both passes.
void AssetDiagnostics::scan_dependencies(const std::string& gamepath) {
    // Individual file fast-check: just re-run scan_all logic for one file.
    auto ext = StringUtils::get_extension_no_dot(gamepath);
    auto existing = get_severity(gamepath);
    if (existing && *existing == AssetSeverity::Error) return;

    auto s = stem(gamepath, ext);
    std::vector<AssetDiagnostic> diags;

    if      (ext == "cmdl" && !game_file_exists(s + "mis"))
        diags.push_back({AssetSeverity::Warning, "no .mis import settings"});
    else if (ext == "dds"  && !game_file_exists(s + "tis"))
        diags.push_back({AssetSeverity::Warning, "no .tis import settings"});
    else if (ext == "mis"  && !game_file_exists(s + "glb"))
        diags.push_back({AssetSeverity::Warning, "source .glb not found alongside .mis"});
    else if (ext == "tis"  && !game_file_exists(s + "png"))
        diags.push_back({AssetSeverity::Warning, "source .png not found alongside .tis"});

    if (!diags.empty() && !existing)
        set(gamepath, std::move(diags));
}

#endif
