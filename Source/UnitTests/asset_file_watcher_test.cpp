// Source/UnitTests/asset_file_watcher_test.cpp
//
// Unit tests for FileWatcher.  These use a real temporary directory on disk.
// Windows-only (Win32 API under test).
//
// We define EDITOR_BUILD locally so we can compile the editor-only
// header/implementation directly into this translation unit without
// pulling in the full engine link.

#define EDITOR_BUILD
#include "Assets/FileWatcher.h"
#include "Assets/FileWatcher.cpp"
#undef EDITOR_BUILD

#include <gtest/gtest.h>
#include <windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string make_temp_dir() {
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    char dir[MAX_PATH];
    snprintf(dir, MAX_PATH, "%sfw_test_%lu_%llu", tmp,
             GetCurrentProcessId(),
             (unsigned long long)std::chrono::steady_clock::now()
                 .time_since_epoch().count());
    CreateDirectoryA(dir, nullptr);
    return dir;
}

static void remove_dir_recursive(const std::string& dir) {
    std::string pattern = dir + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
                continue;
            std::string child = dir + "\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                remove_dir_recursive(child);
            else
                DeleteFileA(child.c_str());
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir.c_str());
}

static void write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << content;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// After writing a file and waiting longer than debounce_ms, poll() must
// return exactly that file's relative path.
TEST(FileWatcher, DetectsWrite) {
    std::string dir = make_temp_dir();

    FileWatcher fw;
    ASSERT_TRUE(fw.init(dir));

    // Give the watcher thread a moment to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    write_file(dir + "\\hello.txt", "v1");

    // Wait longer than the debounce window
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto paths = fw.poll(150);
    EXPECT_EQ(paths.size(), 1u);
    if (!paths.empty())
        EXPECT_EQ(paths[0], "hello.txt");

    remove_dir_recursive(dir);
}

// Writing the same file N times quickly should produce exactly one entry
// from poll() once the debounce window has passed (deduplication).
TEST(FileWatcher, DeduplicatesRapidWrites) {
    std::string dir = make_temp_dir();

    FileWatcher fw;
    ASSERT_TRUE(fw.init(dir));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Write the file 5 times with small gaps (simulating multi-stage editor save)
    for (int i = 0; i < 5; i++) {
        write_file(dir + "\\target.txt", "v" + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Immediately after, nothing should be ready (debounce not elapsed)
    auto immediate = fw.poll(150);
    EXPECT_TRUE(immediate.empty());

    // Wait past the debounce window
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto paths = fw.poll(150);
    EXPECT_EQ(paths.size(), 1u);

    remove_dir_recursive(dir);
}

// poll() must not return the same path twice for a single change.
TEST(FileWatcher, DoesNotReturnPathTwice) {
    std::string dir = make_temp_dir();

    FileWatcher fw;
    ASSERT_TRUE(fw.init(dir));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    write_file(dir + "\\once.txt", "data");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto first  = fw.poll(150);
    auto second = fw.poll(150);

    EXPECT_EQ(first.size(), 1u);
    EXPECT_TRUE(second.empty());

    remove_dir_recursive(dir);
}

// Paths returned must use forward slashes regardless of what the OS reported.
TEST(FileWatcher, NormalisesSlashes) {
    std::string dir = make_temp_dir();
    std::string subdir = dir + "\\sub";
    CreateDirectoryA(subdir.c_str(), nullptr);

    FileWatcher fw;
    ASSERT_TRUE(fw.init(dir));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    write_file(subdir + "\\deep.txt", "x");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto paths = fw.poll(150);
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0].find('\\'), std::string::npos)
        << "path must not contain backslashes: " << paths[0];
    EXPECT_EQ(paths[0], "sub/deep.txt");

    remove_dir_recursive(dir);
}
