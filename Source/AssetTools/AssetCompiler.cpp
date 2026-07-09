#ifdef EDITOR_BUILD
#include "AssetTools/AssetCompiler.h"
#include "AssetTools/AssetDiagnostics.h"
#include "AssetTools/AssetTemplates.h"
#include "AssetCompile/Compiliers.h"
#include "AssetCompile/SoundAsset.h"
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

namespace AssetCompiler {

static void diag_ok(const std::string& gamepath) {
    AssetDiagnostics::get().clear(gamepath);
}
static void diag_err(const std::string& gamepath, const std::string& msg) {
    AssetDiagnostics::get().set(gamepath, {{AssetSeverity::Error, msg}});
}
static void diag_warn(const std::string& gamepath, const std::string& msg) {
    AssetDiagnostics::get().set(gamepath, {{AssetSeverity::Warning, msg}});
}
static void diag_info(const std::string& gamepath, const std::string& msg) {
    AssetDiagnostics::get().set(gamepath, {{AssetSeverity::Info, msg}});
}

static bool game_file_exists(const std::string& rel) {
    return FileSys::does_file_exist(rel.c_str(), FileSys::GAME_DIR);
}

AssetCompileResult compile_model(const std::string& mis_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(mis_gamepath) == "mis");
    AssetCompileResult r;

    if (!game_file_exists(mis_gamepath)) {
        r.success = false;
        r.error_message = "no .mis import settings: " + mis_gamepath;
        diag_err(mis_gamepath, r.error_message);
        return r;
    }

    auto ret = ModelCompilier::compile(mis_gamepath.c_str());
    switch (ret) {
    case ModelCompilier::CompileGood:
        r.success = true;
        {
            auto s = mis_gamepath.substr(0, mis_gamepath.size() - 3);
            r.output_files.push_back(s + "cmdl");
            diag_ok(mis_gamepath);
            diag_ok(s + "cmdl");
        }
        break;
    case ModelCompilier::Skipped:
        r.success = true;
        r.error_message = "skipped (up-to-date)";
        diag_ok(mis_gamepath);
        break;
    case ModelCompilier::CompileErr:
        r.success = false;
        r.error_message = "model compile failed: " + mis_gamepath;
        diag_err(mis_gamepath, r.error_message);
        diag_err(mis_gamepath.substr(0, mis_gamepath.size() - 3) + "cmdl", r.error_message);
        break;
    }
    return r;
}

AssetCompileResult compile_texture(const std::string& tis_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(tis_gamepath) == "tis");
    AssetCompileResult r;

    if (!game_file_exists(tis_gamepath)) {
        r.success = false;
        r.error_message = "no .tis import settings: " + tis_gamepath;
        diag_warn(tis_gamepath, r.error_message);
        return r;
    }

    Color32 dummy{};
    r.success = compile_texture_asset(tis_gamepath, dummy);
    if (r.success) {
        auto s = tis_gamepath.substr(0, tis_gamepath.size() - 3);
        r.output_files.push_back(s + "dds");
        diag_ok(tis_gamepath);
        diag_ok(s + "dds");
    } else {
        r.error_message = "texture compile failed: " + tis_gamepath;
        diag_err(tis_gamepath, r.error_message);
        diag_err(tis_gamepath.substr(0, tis_gamepath.size() - 3) + "dds", r.error_message);
    }
    return r;
}

AssetCompileResult compile_sound(const std::string& ais_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(ais_gamepath) == "ais");
    AssetCompileResult r;

    if (!game_file_exists(ais_gamepath)) {
        r.success = false;
        r.error_message = "no .ais import settings: " + ais_gamepath;
        diag_err(ais_gamepath, r.error_message);
        return r;
    }

    r.success = compile_sound_asset(ais_gamepath);
    if (r.success) {
        auto s = ais_gamepath.substr(0, ais_gamepath.size() - 3);
        r.output_files.push_back(s + "csnd");
        diag_ok(ais_gamepath);
        diag_ok(s + "csnd");
    } else {
        r.error_message = "sound compile failed: " + ais_gamepath;
        diag_err(ais_gamepath, r.error_message);
        diag_err(ais_gamepath.substr(0, ais_gamepath.size() - 3) + "csnd", r.error_message);
    }
    return r;
}

AssetCompileResult compile_material(const std::string& mm_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(mm_gamepath) == "mm");
    // Material compilation happens via the asset reload path (GLSL→SPIRV→HLSL).
    // A headless standalone path isn't wired yet.
    AssetCompileResult r;
    r.success = true;
    r.error_message = "material compile: use asset reload path";
    return r;
}

AssetCompileResult check_lua(const std::string& lua_gamepath) {
    ASSERT(StringUtils::get_extension_no_dot(lua_gamepath) == "lua");
	return {true};

    AssetCompileResult r;

    std::string abs_path = FileSys::get_full_path_from_game_path(lua_gamepath);
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
        r.success = true;
        r.error_message = "lua-language-server not available";
    } else {
        r.success = (exit_code == 0);
        r.error_message = output;
        if (r.success) diag_ok(lua_gamepath);
        else           diag_err(lua_gamepath, output);
    }
    return r;
}

std::optional<AssetCompileResult> compile_asset(const std::string& gamepath) {
    auto ext = StringUtils::get_extension_no_dot(gamepath);

    if      (ext == "mis") return compile_model(gamepath);
    else if (ext == "tis") return compile_texture(gamepath);
    else if (ext == "ais") return compile_sound(gamepath);
    else if (ext == "mm")  return compile_material(gamepath);
    else if (ext == "lua") return check_lua(gamepath);
    else if (ext == "mi") {
        // .mi files aren't compiled; validate PARENT and texture refs
        AssetDiagnostics::get().scan_dependencies(gamepath);
        return AssetCompileResult{true, "material instance validated"};
    }

    // Compiled outputs: find sidecar import settings and rebuild from them.
    // .cmdl with no .mis: trying to build is an Error (can't rebuild + no path to fix).
    // .dds  with no .tis: Warning (texture is usable but unmanaged).
    else if (ext == "cmdl") {
        std::string mis = gamepath.substr(0, gamepath.size() - 4) + "mis";
        if (!game_file_exists(mis)) {
            diag_err(gamepath, "tried to rebuild but no .mis import settings found");
            return AssetCompileResult{false, "no .mis import settings: " + gamepath};
        }
        return compile_model(mis);
    }
    else if (ext == "dds") {
        std::string tis = gamepath.substr(0, gamepath.size() - 3) + "tis";
        if (!game_file_exists(tis)) {
            diag_info(gamepath, "no .tis import settings — texture is unmanaged");
            return AssetCompileResult{true, "no .tis — texture is unmanaged"};
        }
        return compile_texture(tis);
    }
    // .csnd with no .ais: Error, not unmanaged — runtime only loads .csnd, there's no
    // raw-source fallback for audio (unlike .dds's UseSourceFile/unmanaged case above).
    else if (ext == "csnd") {
        std::string ais = gamepath.substr(0, gamepath.size() - 4) + "ais";
        if (!game_file_exists(ais)) {
            diag_err(gamepath, "tried to rebuild but no .ais import settings found");
            return AssetCompileResult{false, "no .ais import settings: " + gamepath};
        }
        return compile_sound(ais);
    }

    return std::nullopt;
}

static bool needs_compile(const std::string& gamepath) {
    auto ext = StringUtils::get_extension_no_dot(gamepath);
    if (ext == "mis") {
        return true; // ModelCompilier handles its own up-to-date check
    } else if (ext == "tis") {
        auto s = gamepath.substr(0, gamepath.size() - 3);
        return !game_file_exists(s + "dds");
    } else if (ext == "ais") {
        return true; // compile_sound_asset handles its own up-to-date check
    }
    return true;
}

void build_all(bool force_rebuild) {
    int errors = 0, compiled = 0;
    std::vector<std::string> error_paths;

    // Create .ais sidecars for any orphan source audio *before* the compile loop below,
    // so a freshly-dropped .wav gets both its .ais and its .csnd within this same call
    // (AssetDiagnostics::scan_all(), further down, also auto-imports but runs after the
    // loop -- too late to compile a brand-new file in the same pass).
    AssetTemplates::auto_import_all_wav();

    for (const auto& full : FileSys::find_game_files()) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        if (gp.find(".thumbnails/") != std::string::npos) continue;
        if (!compile_asset(gp).has_value()) continue;
        if (!force_rebuild && !needs_compile(gp)) continue;

        auto result = compile_asset(gp);
        if (!result) continue;
        ++compiled;

        if (!result->success) {
            ++errors;
            error_paths.push_back(gp);
        }
        // compile_model/texture update AssetDiagnostics as a side effect
    }

    // Fast existence pass, then transitive content pass
    AssetDiagnostics::get().scan_all();
    AssetDiagnostics::get().scan_transitive();

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
    cmds->add("ASSET_AUTO_IMPORT", [](const Cmd_Args&) {
        int n = AssetTemplates::auto_import_all_png();
        sys_print(Info, "Auto-imported %d .tis sidecar(s)\n", n);
        int s = AssetTemplates::auto_import_all_wav();
        sys_print(Info, "Auto-imported %d .ais sidecar(s)\n", s);
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
