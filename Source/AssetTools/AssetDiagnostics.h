#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

// Ordering matters: higher value = higher priority in get_severity()
enum class AssetSeverity { Info = 0, TransitiveWarning = 1, Warning = 2, Error = 3 };

struct AssetDiagnostic {
    AssetSeverity severity;
    std::string message;
};

class AssetDiagnostics {
public:
    static AssetDiagnostics& get();

    void set(const std::string& gamepath, std::vector<AssetDiagnostic> diags);
    void clear(const std::string& gamepath);
    const std::vector<AssetDiagnostic>* get_diags(const std::string& gamepath) const;
    std::optional<AssetSeverity> get_severity(const std::string& gamepath) const;

    const auto& get_all() const { return cache_; }

    void save() const;
    void load();

    // Fast single-file existence check (no content reading)
    void scan_dependencies(const std::string& gamepath);
    // Fast startup scan: file-existence checks only, no content reading
    void scan_all();
    // Slow transitive pass: reads text-asset content to propagate severity.
    // Only call during explicit build_all(), never at startup.
    void scan_transitive();

private:
    std::unordered_map<std::string, std::vector<AssetDiagnostic>> cache_;
};

#endif
