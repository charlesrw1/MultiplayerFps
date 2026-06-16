#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>
#include <optional>

struct AssetCompileResult {
    bool success = false;
    std::string error_message;
    std::vector<std::string> output_files;
};

namespace AssetCompiler {
    AssetCompileResult compile_model(const std::string& mis_gamepath);
    AssetCompileResult compile_texture(const std::string& tis_gamepath);
    AssetCompileResult compile_material(const std::string& mm_gamepath);
    AssetCompileResult check_lua(const std::string& lua_gamepath);

    // Dispatch by extension — returns nullopt for unrecognised ext
    std::optional<AssetCompileResult> compile_asset(const std::string& gamepath);

    // Compile stale (or all if force_rebuild) assets; then dependency-scan.
    void build_all(bool force_rebuild = false);

    // Dependency scan only — no recompile.
    std::vector<std::string> check_all_errors();

    void register_console_commands();
}

#endif
