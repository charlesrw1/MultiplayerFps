#ifdef EDITOR_BUILD
#include "AssetTools/AssetOps.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "Framework/Util.h"
#include <Windows.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <filesystem>

// @docs [[asset_tools#ops]]

namespace fs = std::filesystem;

// File types scanned for references (mirrors asset_manager.py VALID_REFERENCE_FORMATS)
static const char* const k_ref_exts[] = { "cmdl", "dds", "mm", "mi", "tmap", nullptr };

static bool is_ref_ext(const std::string& ext) {
    for (int i = 0; k_ref_exts[i]; ++i)
        if (ext == k_ref_exts[i])
            return true;
    return false;
}

// Returns sidecar path if one exists for this extension, else empty.
// .tis <-> .dds,  .mis <-> .cmdl,  .ais <-> .csnd
static std::string sidecar(const std::string& gamepath) {
    auto ext = StringUtils::get_extension_no_dot(gamepath);
    auto stem = gamepath.substr(0, gamepath.size() - ext.size() - 1);
    if (ext == "tis")  return stem + ".dds";
    if (ext == "dds")  return stem + ".tis";
    if (ext == "mis")  return stem + ".cmdl";
    if (ext == "cmdl") return stem + ".mis";
    if (ext == "ais")  return stem + ".csnd";
    if (ext == "csnd") return stem + ".ais";
    return {};
}

// Smart replace: only replace when `old_ref` is surrounded by boundary chars.
// Port of asset_manager.py _smart_replace().
static bool smart_replace(std::string& text, const std::string& old_ref, const std::string& new_ref) {
    static const std::string boundaries = "\" :\n\r\t";
    bool changed = false;
    size_t pos = 0;
    while ((pos = text.find(old_ref, pos)) != std::string::npos) {
        bool left_ok = (pos == 0) || (boundaries.find(text[pos - 1]) != std::string::npos);
        size_t end = pos + old_ref.size();
        bool right_ok = (end >= text.size()) || (boundaries.find(text[end]) != std::string::npos);
        if (left_ok && right_ok) {
            text.replace(pos, old_ref.size(), new_ref);
            pos += new_ref.size();
            changed = true;
        } else {
            pos += old_ref.size();
        }
    }
    return changed;
}

static std::string read_text_file(const std::string& fullpath) {
    std::ifstream f(fullpath);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool write_text_file(const std::string& fullpath, const std::string& text) {
    std::ofstream f(fullpath);
    if (!f.is_open()) return false;
    f << text;
    return true;
}

static bool trash_file(const std::string& fullpath) {
    std::wstring wpath(fullpath.begin(), fullpath.end());
    // SHFileOperationW requires double-null terminated
    wpath += L'\0';
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = wpath.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    return SHFileOperationW(&op) == 0;
}

std::vector<std::string> AssetOps::find_references(const std::string& asset_gamepath) {
    std::vector<std::string> refs;
    // Use the filename component as the search key
    auto slash = asset_gamepath.rfind('/');
    std::string filename = (slash != std::string::npos) ? asset_gamepath.substr(slash + 1) : asset_gamepath;

    for (const auto& full : FileSys::find_game_files()) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        if (!is_ref_ext(StringUtils::get_extension_no_dot(gp))) continue;
        std::string text = read_text_file(full);
        if (text.find(asset_gamepath) != std::string::npos || text.find(filename) != std::string::npos)
            refs.push_back(gp);
    }
    return refs;
}

AssetOpResult AssetOps::mv(const std::string& src_gamepath, const std::string& dst_folder) {
    std::string src_full = FileSys::get_full_path_from_game_path(src_gamepath);
    auto slash = src_gamepath.rfind('/');
    std::string filename = (slash != std::string::npos) ? src_gamepath.substr(slash + 1) : src_gamepath;

    // Normalise dst_folder — strip trailing slash
    std::string dst_dir = dst_folder;
    if (!dst_dir.empty() && (dst_dir.back() == '/' || dst_dir.back() == '\\'))
        dst_dir.pop_back();
    std::string dst_gamepath = dst_dir.empty() ? filename : (dst_dir + "/" + filename);
    std::string dst_full = FileSys::get_full_path_from_game_path(dst_gamepath);

    if (!fs::exists(src_full))
        return {false, "source file not found: " + src_gamepath};
    if (src_full == dst_full)
        return {true, {}};

    // Ensure destination directory exists
    fs::create_directories(fs::path(dst_full).parent_path());

    std::error_code ec;
    fs::rename(src_full, dst_full, ec);
    if (ec)
        return {false, "rename failed: " + ec.message()};

    // Move sidecar if it exists
    std::string side_src = sidecar(src_gamepath);
    if (!side_src.empty()) {
        std::string side_full_src = FileSys::get_full_path_from_game_path(side_src);
        if (fs::exists(side_full_src)) {
            auto side_name = side_src.substr(side_src.rfind('/') == std::string::npos ? 0 : side_src.rfind('/') + 1);
            std::string side_full_dst = FileSys::get_full_path_from_game_path(
                dst_dir.empty() ? side_name : (dst_dir + "/" + side_name));
            fs::rename(side_full_src, side_full_dst, ec); // best-effort
        }
    }

    // Update references in scanned file types
    for (const auto& full : FileSys::find_game_files()) {
        auto gp = FileSys::get_game_path_from_full_path(full);
        if (!is_ref_ext(StringUtils::get_extension_no_dot(gp))) continue;
        std::string text = read_text_file(full);
        if (smart_replace(text, src_gamepath, dst_gamepath))
            write_text_file(full, text);
    }

    return {true, {}};
}

AssetOpResult AssetOps::rm(const std::string& gamepath) {
    std::string full = FileSys::get_full_path_from_game_path(gamepath);
    if (!fs::exists(full))
        return {false, "file not found: " + gamepath};

    if (!trash_file(full))
        return {false, "trash failed for: " + gamepath};

    // Trash sidecar if it exists
    std::string side = sidecar(gamepath);
    if (!side.empty()) {
        std::string side_full = FileSys::get_full_path_from_game_path(side);
        if (fs::exists(side_full))
            trash_file(side_full); // best-effort
    }

    return {true, {}};
}

AssetOpResult AssetOps::cp(const std::string& src_gamepath, const std::string& dst_gamepath) {
    std::string src_full = FileSys::get_full_path_from_game_path(src_gamepath);
    std::string dst_full = FileSys::get_full_path_from_game_path(dst_gamepath);

    if (!fs::exists(src_full))
        return {false, "source file not found: " + src_gamepath};

    fs::create_directories(fs::path(dst_full).parent_path());
    std::error_code ec;
    fs::copy_file(src_full, dst_full, fs::copy_options::overwrite_existing, ec);
    if (ec)
        return {false, "copy failed: " + ec.message()};

    return {true, {}};
}

AssetOpResult AssetOps::mkdir(const std::string& dir_gamepath) {
    std::string full = FileSys::get_full_path_from_game_path(dir_gamepath);
    std::error_code ec;
    fs::create_directories(full, ec);
    if (ec)
        return {false, "mkdir failed: " + ec.message()};
    return {true, {}};
}

#endif
