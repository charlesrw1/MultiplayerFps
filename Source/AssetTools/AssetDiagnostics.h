#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

enum class AssetSeverity { Warning, Error };

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

    // Reads asset file as text/JSON, extracts referenced paths, checks they exist on disk
    void scan_dependencies(const std::string& gamepath);
    // Scan all game files — run at editor startup + after any compile
    void scan_all();

private:
    std::unordered_map<std::string, std::vector<AssetDiagnostic>> cache_;
};

#endif
