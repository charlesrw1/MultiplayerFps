#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

// Ordering matters: higher value = higher priority in get_severity()
enum class AssetSeverity { TransitiveWarning = 0, Warning = 1, Error = 2 };

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

    void save() const;
    void load();

    // Reads asset file, checks direct source file dependencies exist
    void scan_dependencies(const std::string& gamepath);
    // Two-pass full scan: direct errors then transitive propagation
    void scan_all();

private:
    std::unordered_map<std::string, std::vector<AssetDiagnostic>> cache_;
};

#endif
