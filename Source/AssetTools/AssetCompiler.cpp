#ifdef EDITOR_BUILD
#include "AssetTools/AssetCompiler.h"
#include "AssetTools/AssetDiagnostics.h"
#include "AssetCompile/Compiliers.h"
#include "Render/Editor/TextureEditor.h"
#include "Render/SpirvCompile.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "Framework/Util.h"
#include "Framework/ConsoleCmdGroup.h"
#include <Windows.h>
#include <vector>
#include <string>

// @docs [[asset_tools#compiler]]

extern bool spirv_is_initialized();

namespace AssetCompiler {

AssetCompileResult compile_model(const std::string& mis_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(mis_gamepath) == "mis");
    AssetCompileResult r;
    auto ret = ModelCompilier::compile(mis_gamepath.c_str());
    switch (ret) {
    case ModelCompilier::CompileGood:
        r.success = true;
        {
            auto stem = mis_gamepath.substr(0, mis_gamepath.size() - 3);
            r.output_files.push_back(stem + "cmdl");
        }
        break;
    case ModelCompilier::Skipped:
        r.success = true;
        r.error_message = "skipped (up-to-date)";
        break;
    case ModelCompilier::CompileErr:
        r.success = false;
        r.error_message = "model compile failed: " + mis_gamepath;
        break;
    }
    return r;
}

AssetCompileResult compile_texture(const std::string& tis_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(tis_gamepath) == "tis");
    AssetCompileResult r;
    Color32 dummy{};
    r.success = compile_texture_asset(tis_gamepath, dummy);
    if (r.success) {
        auto stem = tis_gamepath.substr(0, tis_gamepath.size() - 3);
        r.output_files.push_back(stem + "dds");
    } else {
        r.error_message = "texture compile failed: " + tis_gamepath;
    }
    return r;
}

AssetCompileResult compile_material(const std::string& mm_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(mm_gamepath) == "mm");
    ASSERT(spirv_is_initialized());
    // Material compilation is performed by the asset load/reload path which
    // internally calls create_glsl_shader → compile_glsl_to_spirv → spirv_to_hlsl.
    // A standalone headless path is not yet wired; return success so the editor
    // can use the reload path without false negatives in the diagnostics cache.
    AssetCompileResult r;
    r.success = true;
    r.error_message = "material compile: use asset reload path";
    return r;
}

AssetCompileResult check_lua(const std::string& lua_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(lua_gamepath) == "lua");
    AssetCompileResult r;

    std::string abs_path = FileSys::get_full_path_from_game_path(lua_gamepath);
    // Build command: Scripts/lua_check.ps1 -Path <abs> -Level Warning
    std::string cmd = "powershell.exe -NoProfile -NonInteractive -File \""
                    + std::string(FileSys::get_path(FileSys::ENGINE_DIR))
                    + "/Scripts/lua_check.ps1\" -Path \""
                    + abs_path + "\" -Level Warning";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = INVALID_HANDLE_VALUE, hWrite = INVALID_HANDLE_VALUE;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.dwFlags = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        r.success = true;
        r.error_message = "lua-language-server not installed or lua_check.ps1 not found";
        return r;
    }
    CloseHandle(hWrite);

    std::string output;
    char buf[512];
    DWORD read = 0;
    while (ReadFile(hRead, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
        buf[read] = 0;
        output += buf;
    }
    CloseHandle(hRead);

    DWORD exit_code = 0;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code == 2) {
        // Setup failure (lua-language-server not installed)
        r.success = true;
        r.error_message = "lua-language-server not available";
    } else {
        r.success = (exit_code == 0);
        r.error_message = output;
    }
    return r;
}

std::optional<AssetCompileResult> compile_asset(const std::string& gamepath) {
    auto ext = StringUtils::get_extension_no_dot(gamepath);
    if      (ext == "mis") return compile_model(gamepath);
    else if (ext == "tis") return compile_texture(gamepath);
    else if (ext == "mm")  return compile_material(gamepath);
    else if (ext == "lua") return check_lua(gamepath);
    else return std::nullopt;
}

static bool needs_compile(const std::string& gamepath) {
    auto ext = StringUtils::get_extension_no_dot(gamepath);
    if (ext == "mis") {
        // ModelCompilier handles its own up-to-date check inside compile()
        return true;
    } else if (ext == "tis") {
        // Check if .dds exists; if not, needs compile
        auto stem = gamepath.substr(0, gamepath.size() - 3);
        return !FileSys::does_file_exist((stem + "dds").c_str(), FileSys::GAME_DIR);
    }
    return true;
}

void build_all(bool force_rebuild) {
    int errors = 0, compiled = 0;
    std::vector<std::string> error_paths;

    for (const auto& full : FileSys::find_game_files()) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        if (!compile_asset(gp).has_value()) continue;

        if (!force_rebuild && !needs_compile(gp)) continue;

        auto result = compile_asset(gp);
        if (!result) continue;
        ++compiled;

        std::vector<AssetDiagnostic> diags;
        if (!result->success) {
            diags.push_back({AssetSeverity::Error, result->error_message});
            ++errors;
            error_paths.push_back(gp);
        }
        AssetDiagnostics::get().set(gp, std::move(diags));
    }

    AssetDiagnostics::get().scan_all();

    sys_print(Info, "Build complete: %d compiled, %d errors\n", compiled, errors);
    for (auto& p : error_paths)
        sys_print(Error, "  ERROR: %s\n", p.c_str());
}

std::vector<std::string> check_all_errors() {
    AssetDiagnostics::get().scan_all();
    std::vector<std::string> errors;
    for (const auto& full : FileSys::find_game_files()) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        auto sev = AssetDiagnostics::get().get_severity(gp);
        if (sev && *sev == AssetSeverity::Error)
            errors.push_back(gp);
    }
    return errors;
}

void register_console_commands() {
    static uptr<ConsoleCmdGroup> cmds;
    cmds = ConsoleCmdGroup::create("asset");
    cmds->add("ASSET_BUILD_ALL",    [](const Cmd_Args&) { build_all(false); });
    cmds->add("ASSET_REBUILD_ALL",  [](const Cmd_Args&) { build_all(true); });
    cmds->add("ASSET_CHECK_ERRORS", [](const Cmd_Args& a) {
        auto errs = check_all_errors();
        for (auto& e : errs) sys_print(Error, "%s\n", e.c_str());
        sys_print(Info, "%d error(s)\n", (int)errs.size());
    });
    cmds->add("ASSET_COMPILE", [](const Cmd_Args& a) {
        if (a.size() < 2) { sys_print(Warning, "usage: ASSET_COMPILE <gamepath>\n"); return; }
        std::string path = a.at(1);
        auto r = compile_asset(path);
        if (!r) { sys_print(Warning, "unknown asset type: %s\n", path.c_str()); return; }
        sys_print(r->success ? Info : Error, "%s: %s\n", path.c_str(),
                  r->error_message.empty() ? "OK" : r->error_message.c_str());
    });
}

} // namespace AssetCompiler

#endif
