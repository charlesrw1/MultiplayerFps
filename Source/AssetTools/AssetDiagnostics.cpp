#ifdef EDITOR_BUILD
#include "AssetTools/AssetDiagnostics.h"
#include "AssetTools/AssetTemplates.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "Framework/Util.h"
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
    AssetSeverity worst = AssetSeverity::Info;
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
    case AssetSeverity::Info:              return "info";
    }
    return "info";
}

static AssetSeverity str_to_sev(const std::string& s) {
    if (s == "error")   return AssetSeverity::Error;
    if (s == "warning") return AssetSeverity::Warning;
    if (s == "transitive") return AssetSeverity::TransitiveWarning;
    return AssetSeverity::Info;
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
    int imported = AssetTemplates::auto_import_all_png();
    if (imported > 0)
        sys_print(Info, "Auto-imported %d .tis sidecar(s) for orphan .png files\n", imported);

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
            if (!game_file_exists(s + "tis"))
                diags.push_back({AssetSeverity::Info, "no .tis import settings"});
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

// Parse PARENT and VAR texture refs from a .mi (material instance) file.
// Format: "PARENT foo.mm" and "VAR <name> <value>" where value is a texture
// gamepath or built-in (built-ins start with '_', numerics and multi-token are skipped).
static std::vector<std::string> parse_mi_refs(const std::string& text) {
    static constexpr std::string_view kTexExts[] = { "dds", "hdr", "png", "tga" };
    std::vector<std::string> refs;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() > 7 && line.substr(0, 7) == "PARENT ") {
            std::string p = line.substr(7);
            while (!p.empty() && std::isspace((unsigned char)p.back())) p.pop_back();
            if (!p.empty()) refs.push_back(p);
        } else if (line.size() > 4 && line.substr(0, 4) == "VAR ") {
            size_t name_end = line.find(' ', 4);
            if (name_end == std::string::npos) continue;
            std::string val = line.substr(name_end + 1);
            while (!val.empty() && std::isspace((unsigned char)val.front())) val.erase(val.begin());
            while (!val.empty() && std::isspace((unsigned char)val.back())) val.pop_back();
            // skip multi-token (colors/vectors), built-ins, numbers
            if (val.find(' ') != std::string::npos) continue;
            if (val.empty() || val[0] == '_') continue;
            if (std::isdigit((unsigned char)val[0])) continue;
            auto dot = val.rfind('.');
            if (dot == std::string::npos) continue;
            auto e = val.substr(dot + 1);
            for (auto& ke : kTexExts)
                if (e == ke) { refs.push_back(val); break; }
        }
    }
    return refs;
}

// scan_transitive: content-reading pass for propagating errors across references.
// Runs only during build_all(), never at startup.
void AssetDiagnostics::scan_transitive() {
    // .mi uses a custom PARENT/VAR format — handled by parse_mi_refs, not extract_refs
    static constexpr const char* kTextExts[] = { "mm", "mis", "tis", "tmap", nullptr };

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
        // Note: .mi files use parse_mi_refs — handled in the separate loop below.

        AssetSeverity worst_dep = AssetSeverity::TransitiveWarning;
        bool has_issue = false;
        for (auto& ref : refs) {
            if (!game_file_exists(ref)) {
                worst_dep = AssetSeverity::Error;
                has_issue = true;
            } else {
                auto dep_sev = get_severity(ref);
                if (dep_sev && *dep_sev > AssetSeverity::Info) {
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

    // .mi (material instance): PARENT/VAR format — separate pass using parse_mi_refs.
    for (const auto& full : FileSys::find_game_files()) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        if (StringUtils::get_extension_no_dot(gp) != "mi") continue;

        auto existing = get_severity(gp);
        if (existing && *existing == AssetSeverity::Error) continue;

        auto f = FileSys::open_read_game(gp);
        if (!f) continue;
        std::vector<char> buf(f->size() + 1, 0);
        f->read(buf.data(), f->size());
        f->close();

        auto refs = parse_mi_refs(std::string(buf.data()));
        if (refs.empty()) continue;

        AssetSeverity worst_dep = AssetSeverity::TransitiveWarning;
        bool has_issue = false;
        for (auto& ref : refs) {
            if (!game_file_exists(ref)) {
                worst_dep = AssetSeverity::Error;
                has_issue = true;
            } else {
                auto dep_sev = get_severity(ref);
                if (dep_sev && *dep_sev > AssetSeverity::Info) {
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
            merged.push_back({new_sev, "dependency issue"});
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
        if (!src_sev || *src_sev == AssetSeverity::Info) continue;
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

void AssetDiagnostics::scan_dependencies(const std::string& gamepath) {
    auto ext = StringUtils::get_extension_no_dot(gamepath);
    auto existing = get_severity(gamepath);
    if (existing && *existing == AssetSeverity::Error) return;

    // .mi: read content to check PARENT and VAR texture refs
    if (ext == "mi") {
        auto f = FileSys::open_read_game(gamepath);
        if (!f) return;
        std::vector<char> buf(f->size() + 1, 0);
        f->read(buf.data(), f->size());
        f->close();
        auto refs = parse_mi_refs(std::string(buf.data()));
        AssetSeverity worst = AssetSeverity::TransitiveWarning;
        bool has_issue = false;
        for (auto& ref : refs) {
            if (!game_file_exists(ref)) { worst = AssetSeverity::Error; has_issue = true; }
            else { auto s = get_severity(ref); if (s && *s > AssetSeverity::Info) { has_issue = true; if (*s > worst) worst = *s; } }
        }
        if (!has_issue) { clear(gamepath); return; }
        AssetSeverity new_sev = (worst == AssetSeverity::Error) ? AssetSeverity::Warning : AssetSeverity::TransitiveWarning;
        if (!existing || new_sev > *existing) {
            auto* d = get_diags(gamepath);
            std::vector<AssetDiagnostic> merged;
            if (d) merged = *d;
            merged.push_back({new_sev, "dependency issue"});
            set(gamepath, std::move(merged));
        }
        return;
    }

    // Fast existence-check for other types (no content reading)
    auto s = stem(gamepath, ext);
    std::vector<AssetDiagnostic> diags;
    if      (ext == "cmdl" && !game_file_exists(s + "mis"))
        diags.push_back({AssetSeverity::Warning, "no .mis import settings"});
    else if (ext == "dds"  && !game_file_exists(s + "tis"))
        diags.push_back({AssetSeverity::Info, "no .tis import settings"});
    else if (ext == "mis"  && !game_file_exists(s + "glb"))
        diags.push_back({AssetSeverity::Warning, "source .glb not found alongside .mis"});
    else if (ext == "tis"  && !game_file_exists(s + "png"))
        diags.push_back({AssetSeverity::Warning, "source .png not found alongside .tis"});

    if (!diags.empty() && !existing)
        set(gamepath, std::move(diags));
}

#endif
