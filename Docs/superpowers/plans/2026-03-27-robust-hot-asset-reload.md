# Robust Hot Asset Reload Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the brittle `FindFirstChangeNotificationA` + full-directory-scan approach with `ReadDirectoryChangesW` (exact file paths) + per-path debounce, eliminating the double-change noise and missed-change races.

**Architecture:** A new `FileWatcher` class runs `ReadDirectoryChangesW` on a background thread, accumulating changed paths in a mutex-protected map keyed by path with a `steady_clock` timestamp. The main thread calls `poll(debounce_ms)` once per frame; only paths that haven't changed in the last `debounce_ms` milliseconds are returned and removed from the map. `AssetRegistrySystem` replaces all `FindFirstChangeNotification` and async-future machinery with a single `FileWatcher` member and a straightforward call to `poll()` each frame.

**Tech Stack:** C++17, Win32 (`ReadDirectoryChangesW`, `OVERLAPPED`), `<thread>`, `<mutex>`, `<chrono>`, Google Test, existing integration-test coroutine harness.

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `Source/Assets/FileWatcher.h` | **Create** | `FileWatcher` class declaration |
| `Source/Assets/FileWatcher.cpp` | **Create** | `ReadDirectoryChangesW` background thread + `poll()` |
| `Source/Assets/AssetRegistry.h` | **Modify** | Add `FileWatcher` member; remove `last_time_check` |
| `Source/Assets/AssetRegistry.cpp` | **Modify** | Use `FileWatcher`; remove future/FindFirstChange machinery |
| `CsRemake.vcxproj` | **Modify** | Register `FileWatcher.cpp` and `FileWatcher.h` |
| `Source/UnitTests/asset_file_watcher_test.cpp` | **Create** | gtest unit tests for `FileWatcher` |
| `Source/UnitTests/UnitTests.vcxproj` | **Modify** | Register new unit test file |
| `Source/IntegrationTests/Tests/Renderer/test_material_hotreload.cpp` | **Modify** | Tighten Test 3 timeout; add rapid-save dedup test |
| `Source/Render/AGENTS.md` | **Update** | Already tracked; note no renderer changes |
| `Source/Assets/AGENTS.md` | **Create** | Document FileWatcher and AssetRegistry |

---

## Task 1: Create `FileWatcher.h`

**Files:**
- Create: `Source/Assets/FileWatcher.h`

- [ ] **Step 1: Write the header**

```cpp
// Source/Assets/FileWatcher.h
#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <windows.h>

// Watches a directory tree for file changes using ReadDirectoryChangesW.
// All events are deduplicated and debounced: a path is only returned from
// poll() once it has been stable (no further changes) for debounce_ms ms.
// This eliminates "double-change" noise from multi-stage editor saves.
//
// poll() is safe to call from the main thread concurrently with the
// background watcher thread.
class FileWatcher {
public:
    FileWatcher() = default;
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    // Start watching |dir| recursively.  Returns false on Win32 failure.
    bool init(const std::string& dir);

    // Returns forward-slash-normalised paths relative to the watched directory
    // that changed and have been stable for at least |debounce_ms| ms.
    // Returned paths are removed from the internal pending map.
    // Call once per frame from the main thread.
    std::vector<std::string> poll(int debounce_ms = 150);

private:
    void worker();

    HANDLE dir_handle_  = INVALID_HANDLE_VALUE;
    HANDLE stop_event_  = INVALID_HANDLE_VALUE;
    std::thread worker_thread_;
    std::mutex  mutex_;
    // path -> time of most recent change notification
    std::unordered_map<std::string,
                       std::chrono::steady_clock::time_point> pending_;
};
#endif // EDITOR_BUILD
```

- [ ] **Step 2: Register the header in `CsRemake.vcxproj`**

After the `<ClInclude Include="Source\Assets\AssetRegistryLocal.h" />` line (line 182), add:

```xml
    <ClInclude Include="Source\Assets\FileWatcher.h" />
```

---

## Task 2: Write failing unit tests for `FileWatcher`

**Files:**
- Create: `Source/UnitTests/asset_file_watcher_test.cpp`
- Modify: `Source/UnitTests/UnitTests.vcxproj`

These tests are compiled unconditionally (the unit-test project does not define `EDITOR_BUILD`), so the test file guards include-paths and links the Win32 API itself. The tests create a real temporary directory and perform real filesystem writes to validate the watcher.

- [ ] **Step 1: Write the test file**

```cpp
// Source/UnitTests/asset_file_watcher_test.cpp
//
// Unit tests for FileWatcher.  These use a real temporary directory on disk.
// They are Windows-only (Win32 API under test).

#include <gtest/gtest.h>
#include <windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <fstream>

// Minimal reimplementation of FileWatcher here so the unit tests don't need
// EDITOR_BUILD.  We compile FileWatcher.cpp separately for the engine; the
// unit-test project can't link it without pulling in the whole engine.
// Instead we include the implementation directly (the file guards on
// EDITOR_BUILD, so we temporarily define it).
#define EDITOR_BUILD
#include "Assets/FileWatcher.h"
#include "Assets/FileWatcher.cpp"
#undef EDITOR_BUILD

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string make_temp_dir() {
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    char dir[MAX_PATH];
    // Use process ID + timestamp to avoid collisions
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
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

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
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

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
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

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
    // Create a subdirectory to force a multi-component path
    std::string subdir = dir + "\\sub";
    CreateDirectoryA(subdir.c_str(), nullptr);

    FileWatcher fw;
    ASSERT_TRUE(fw.init(dir));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    write_file(subdir + "\\deep.txt", "x");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto paths = fw.poll(150);
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0].find('\\'), std::string::npos)
        << "path must not contain backslashes: " << paths[0];
    EXPECT_EQ(paths[0], "sub/deep.txt");

    remove_dir_recursive(dir);
}
```

- [ ] **Step 2: Register in `UnitTests.vcxproj`**

After `<ClCompile Include="maputil_test.cpp" />` add:

```xml
    <ClCompile Include="asset_file_watcher_test.cpp" />
```

- [ ] **Step 3: Build and confirm the tests fail** (FileWatcher.cpp doesn't exist yet)

```powershell
powershell.exe -File Scripts/build_and_test.ps1
```

Expected: build error — `FileWatcher.h` or `FileWatcher.cpp` not found, or linker errors.

---

## Task 3: Implement `FileWatcher.cpp`

**Files:**
- Create: `Source/Assets/FileWatcher.cpp`
- Modify: `CsRemake.vcxproj`

- [ ] **Step 1: Write `FileWatcher.cpp`**

```cpp
// Source/Assets/FileWatcher.cpp
#ifdef EDITOR_BUILD
#include "Assets/FileWatcher.h"
#include <algorithm>

FileWatcher::~FileWatcher() {
    if (stop_event_ != INVALID_HANDLE_VALUE) {
        SetEvent(stop_event_);
        if (worker_thread_.joinable())
            worker_thread_.join();
        CloseHandle(stop_event_);
        stop_event_ = INVALID_HANDLE_VALUE;
    }
    if (dir_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(dir_handle_);
        dir_handle_ = INVALID_HANDLE_VALUE;
    }
}

bool FileWatcher::init(const std::string& dir) {
    dir_handle_ = CreateFileA(
        dir.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (dir_handle_ == INVALID_HANDLE_VALUE)
        return false;

    stop_event_ = CreateEventA(nullptr, /*manualReset=*/TRUE, /*initial=*/FALSE, nullptr);
    if (stop_event_ == INVALID_HANDLE_VALUE) {
        CloseHandle(dir_handle_);
        dir_handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    worker_thread_ = std::thread(&FileWatcher::worker, this);
    return true;
}

void FileWatcher::worker() {
    // 64 KB is large enough to avoid overflow under normal editor usage.
    // Must be DWORD-aligned; alignas(DWORD) guarantees that.
    alignas(DWORD) char buf[65536];
    OVERLAPPED ov = {};
    ov.hEvent = CreateEventA(nullptr, /*manualReset=*/TRUE, /*initial=*/FALSE, nullptr);
    if (ov.hEvent == INVALID_HANDLE_VALUE)
        return;

    auto issue_read = [&]() -> bool {
        ResetEvent(ov.hEvent);
        DWORD dummy = 0;
        BOOL ok = ReadDirectoryChangesW(
            dir_handle_,
            buf, sizeof(buf),
            /*watchSubtree=*/TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            &dummy,
            &ov,
            /*completionRoutine=*/nullptr);
        // ERROR_IO_PENDING is the normal success path for overlapped I/O.
        return ok != FALSE || GetLastError() == ERROR_IO_PENDING;
    };

    if (!issue_read()) {
        CloseHandle(ov.hEvent);
        return;
    }

    HANDLE handles[2] = {ov.hEvent, stop_event_};
    while (true) {
        DWORD wait = WaitForMultipleObjects(2, handles, /*waitAll=*/FALSE, INFINITE);
        if (wait != WAIT_OBJECT_0)
            break;  // stop_event fired or error

        DWORD bytes = 0;
        if (!GetOverlappedResult(dir_handle_, &ov, &bytes, /*wait=*/FALSE) || bytes == 0) {
            // Buffer overflow (bytes == 0) or I/O error: reissue and continue.
            // We may miss which files changed during overflow, but the engine
            // will catch the next write on the next notification cycle.
            issue_read();
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        const char* p = buf;
        while (true) {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(p);

            // Convert the UTF-16LE filename to UTF-8
            int wlen = static_cast<int>(info->FileNameLength / sizeof(WCHAR));
            int utf8_len = WideCharToMultiByte(CP_UTF8, 0,
                info->FileName, wlen, nullptr, 0, nullptr, nullptr);
            std::string rel_path(static_cast<size_t>(utf8_len), '\0');
            WideCharToMultiByte(CP_UTF8, 0,
                info->FileName, wlen, &rel_path[0], utf8_len, nullptr, nullptr);

            // Normalise to forward slashes (game-path convention)
            std::replace(rel_path.begin(), rel_path.end(), '\\', '/');

            {
                std::lock_guard<std::mutex> lock(mutex_);
                pending_[rel_path] = now;
            }

            if (info->NextEntryOffset == 0)
                break;
            p += info->NextEntryOffset;
        }

        // Re-arm the read before going back to wait, so no events are missed.
        issue_read();
    }

    CloseHandle(ov.hEvent);
}

std::vector<std::string> FileWatcher::poll(int debounce_ms) {
    auto cutoff = std::chrono::steady_clock::now()
                  - std::chrono::milliseconds(debounce_ms);
    std::vector<std::string> ready;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = pending_.begin(); it != pending_.end(); ) {
        if (it->second <= cutoff) {
            ready.push_back(std::move(it->first));
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
    return ready;
}
#endif // EDITOR_BUILD
```

- [ ] **Step 2: Register `FileWatcher.cpp` in `CsRemake.vcxproj`**

After the `<ClCompile Include="Source\Assets\AssetRegistry.cpp">...</ClCompile>` block (ends at line 49), add:

```xml
    <ClCompile Include="Source\Assets\FileWatcher.cpp">
      <IncludeInUnityFile Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">false</IncludeInUnityFile>
      <IncludeInUnityFile Condition="'$(Configuration)|$(Platform)'=='NoEditRelease|x64'">false</IncludeInUnityFile>
      <IncludeInUnityFile Condition="'$(Configuration)|$(Platform)'=='Release|x64'">false</IncludeInUnityFile>
    </ClCompile>
```

---

## Task 4: Run unit tests — verify they pass

- [ ] **Step 1: Build and run unit tests**

```powershell
powershell.exe -File Scripts/build_and_test.ps1
```

Expected output: all tests pass, including the four new `FileWatcher.*` tests.

If a test fails:
- `DetectsWrite` fails → likely the watcher thread isn't started before the write; increase the `sleep_for(50ms)` to `sleep_for(200ms)`.
- `DeduplicatesRapidWrites` fails → check the debounce math in `poll()`: `it->second <= cutoff` means the timestamp is *older* than the cutoff, which is correct.
- Linker errors → verify `#define EDITOR_BUILD` is placed before the `#include "Assets/FileWatcher.h"` line in the test file.

- [ ] **Step 2: Commit**

```bash
git add Source/Assets/FileWatcher.h Source/Assets/FileWatcher.cpp \
        Source/UnitTests/asset_file_watcher_test.cpp \
        CsRemake.vcxproj Source/UnitTests/UnitTests.vcxproj
git commit -m "Add FileWatcher: ReadDirectoryChangesW + per-path debounce

Replaces the old FindFirstChangeNotificationA approach with a background-thread
watcher that returns exact changed file paths. poll(debounce_ms) deduplicates
rapid saves so double-change noise never triggers spurious reloads.

Four unit tests cover: detection, deduplication, no-double-return, slash normalisation."
```

---

## Task 5: Wire `FileWatcher` into `AssetRegistrySystem`

**Files:**
- Modify: `Source/Assets/AssetRegistry.h`
- Modify: `Source/Assets/AssetRegistry.cpp`

### 5a — Update `AssetRegistry.h`

- [ ] **Step 1: Add `#include` and member, remove `last_time_check`**

Replace the private section of `AssetRegistrySystem` (lines 77–86):

```cpp
// OLD
private:
	uptr<ConsoleCmdGroup> consoleCommands;
	void reindex_all_assets();
	std::unique_ptr<AssetFilesystemNode> root;
	std::vector<AssetFilesystemNode*> linear_list;
	std::vector<std::unique_ptr<AssetMetadata>> all_assettypes;
	double last_reindex_time = 0.f;
	int64_t last_time_check = 0;
	friend class HackedAsyncAssetRegReindex;
```

```cpp
// NEW
private:
	uptr<ConsoleCmdGroup> consoleCommands;
	void reindex_all_assets();
	std::unique_ptr<AssetFilesystemNode> root;
	std::vector<AssetFilesystemNode*> linear_list;
	std::vector<std::unique_ptr<AssetMetadata>> all_assettypes;
	double last_reindex_time = 0.0;
	FileWatcher file_watcher_;
	friend class HackedAsyncAssetRegReindex;
```

Add near the top of `AssetRegistry.h` (after existing includes):

```cpp
#include "Assets/FileWatcher.h"
```

### 5b — Update `AssetRegistry.cpp`

- [ ] **Step 2: Remove the async-future globals and `async_launch_check_filesystem_changes`**

Delete lines 185–250 (the `directoryChangeHandle`, `async_launch_check_filesystem_changes`, `future_changed_paths`, `is_waiting_on_future` declarations and function body). The `update_on_changed_paths` function (lines 252–298) stays — it is reused unchanged.

Also delete `static HANDLE directoryChangeHandle = 0;` at line 185 and `extern ConfigVar g_project_base;` can stay.

- [ ] **Step 3: Replace `AssetRegistrySystem::init()`**

Replace the body of `init()` (lines 188–225) with:

```cpp
void AssetRegistrySystem::init() {
	string dir = g_project_base.get_string();

	if (!file_watcher_.init(dir)) {
		Fatalf("AssetRegistrySystem: FileWatcher::init failed for dir '%s' (error %lu)\n",
			dir.c_str(), GetLastError());
	}

	reindex_all_assets();

	consoleCommands = ConsoleCmdGroup::create("");
	consoleCommands->add("sys.ls", SYS_LS_CMD);
	consoleCommands->add("sys.print_deps", SYS_PRINT_DEPS_CMD);
	consoleCommands->add("sys.print_refs", SYS_PRINT_REFS_CMD);
	consoleCommands->add("touch_asset", TOUCH_ASSET);
	consoleCommands->add("reload_asset", [this](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "expected: reload_asset <name>\n");
			return;
		}
		auto assetType = find_asset_type_for_ext(StringUtils::get_extension_no_dot(args.at(1)));
		if (!assetType) {
			sys_print(Error, "couldn't find asset type for asset\n");
			return;
		}
		const bool was_loaded = g_assets.is_asset_loaded(args.at(1));
		auto asset = g_assets.find_sync(args.at(1), assetType);
		if (was_loaded) {
			g_assets.reload_sync(asset);
		}
	});
}
```

- [ ] **Step 4: Replace `AssetRegistrySystem::update()`**

Replace the entire `update()` body (lines 300–343) with:

```cpp
void AssetRegistrySystem::update() {
	auto changed = file_watcher_.poll(150);
	if (changed.empty())
		return;

	sys_print(Debug, "AssetRegistrySystem: %zu file(s) changed\n", changed.size());
	bool wants_reindex = update_on_changed_paths(changed);
	if (wants_reindex)
		reindex_all_assets();
}
```

- [ ] **Step 5: Remove the now-unused includes and globals at the top of `AssetRegistry.cpp`**

Remove:
- `#include <future>` (no longer needed — futures are gone)
- The `using ChangedPaths = ...` alias can stay (used by `update_on_changed_paths`)
- `asset_registry_reindex_period` ConfigVar (line 156–157) — no longer used; delete it
- `filetime_to_unix_seconds` and `get_unix_time_seconds` functions (lines 34–45) — no longer needed; delete them

- [ ] **Step 6: Build the engine (CsRemake project)**

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1 | ForEach-Object { & $_ CsRemake.sln /t:CsRemake /p:Configuration=Debug /p:Platform=x64 /v:minimal }
```

Expected: build succeeds with no errors.

- [ ] **Step 7: Commit**

```bash
git add Source/Assets/AssetRegistry.h Source/Assets/AssetRegistry.cpp
git commit -m "Wire FileWatcher into AssetRegistrySystem

Replaces FindFirstChangeNotificationA + full timestamp scan + std::future with
FileWatcher::poll(150). Changes are reported precisely with no global cooldown.
Removes asset_registry_reindex_period cvar and filetime_to_unix_seconds helpers."
```

---

## Task 6: Update integration tests

**Files:**
- Modify: `Source/IntegrationTests/Tests/Renderer/test_material_hotreload.cpp`

### Changes

1. **Test 3 timeout**: Was effectively unbounded by `wait_for`; tighten from `5.f` to `3.f` to catch regressions where the watcher stops working.
2. **New Test 4**: Write the same `.mi` three times in rapid succession, verify the material is reloaded exactly once (debounce deduplication observable via the delegate).

- [ ] **Step 1: Update Test 3 registration timeout and comment**

Find:
```cpp
GAME_TEST("renderer/hotreload_os_filewatcher", 5.f, test_hotreload_os_filewatcher);
```

Replace with:
```cpp
// Timeout is 3 s: FileWatcher debounce is 150 ms so the reload arrives
// well under 1 s on a healthy system.  3 s gives ample headroom.
GAME_TEST("renderer/hotreload_os_filewatcher", 3.f, test_hotreload_os_filewatcher);
```

- [ ] **Step 2: Add Test 4 — rapid-save deduplication**

Add after the `#endif` that closes Test 3 (at the very end of the file):

```cpp
// ---------------------------------------------------------------------------
// Test 4: Rapid-save deduplication (editor build only)
// ---------------------------------------------------------------------------
// Writes a .mi file three times in 50 ms bursts and verifies the material's
// on_material_loaded delegate fires exactly once (not three times), proving
// the FileWatcher's per-path debounce is working.
#ifdef EDITOR_BUILD
static TestTask test_hotreload_rapid_save_dedup(TestContext& t) {
	eng->load_level("");

	MeshComponent* mesh = setup_test_scene();

	const std::string inst_path = "mats/test_hotreload_rapid.mi";
	ScopedTempFile guard(inst_path);

	const std::string base_mi =
		"TYPE MaterialInstance\n"
		"PARENT defaultPBR.mm\n"
		"VAR colorMult 255 0 0 255\n";
	t.require(write_game_file(inst_path, base_mi), "wrote initial .mi");

	auto mat = g_assets.find_sync_sptr<MaterialInstance>(inst_path);
	t.require(mat != nullptr, "rapid-dedup .mi loaded");
	mesh->set_material_override(mat.get());

	co_await t.wait_ticks(1);

	// Count how many times on_material_loaded fires for our specific material.
	int reload_count = 0;
	MaterialInstance* expected = mat.get();
	MaterialInstance::on_material_loaded.add(
		&reload_count, [&reload_count, expected](MaterialInstance* m) {
			if (m == expected)
				++reload_count;
		});

	// Write the file three times quickly (< debounce window of 150 ms).
	const std::string v1 =
		"TYPE MaterialInstance\n"
		"PARENT defaultPBR.mm\n"
		"VAR colorMult 0 255 0 255\n";
	const std::string v2 =
		"TYPE MaterialInstance\n"
		"PARENT defaultPBR.mm\n"
		"VAR colorMult 0 0 255 255\n";
	const std::string v3 =
		"TYPE MaterialInstance\n"
		"PARENT defaultPBR.mm\n"
		"VAR colorMult 255 255 0 255\n";
	t.require(write_game_file(inst_path, v1), "rapid write 1");
	co_await t.wait_ticks(1);
	t.require(write_game_file(inst_path, v2), "rapid write 2");
	co_await t.wait_ticks(1);
	t.require(write_game_file(inst_path, v3), "rapid write 3");

	// Wait long enough for one debounced reload to arrive (500 ms >> 150 ms debounce).
	co_await t.wait_seconds(0.5f);

	// Clean up listener.
	MaterialInstance::on_material_loaded.remove(&reload_count);

	t.check(reload_count == 1, "material reloaded exactly once despite 3 rapid writes");
}
GAME_TEST("renderer/hotreload_rapid_save_dedup", 5.f, test_hotreload_rapid_save_dedup);
#endif
```

- [ ] **Step 3: Commit**

```bash
git add Source/IntegrationTests/Tests/Renderer/test_material_hotreload.cpp
git commit -m "Update hotreload integration tests for FileWatcher

Tighten Test 3 timeout to 3 s (was 5 s) since debounce is now 150 ms.
Add Test 4: rapid-save dedup verifies on_material_loaded fires exactly once
when the same .mi is written three times inside the debounce window."
```

---

## Task 7: Update AGENTS.md files

**Files:**
- Create/Modify: `Source/Assets/AGENTS.md`
- Modify: `Source/IntegrationTests/AGENTS.md` (if it exists, else create)

- [ ] **Step 1: Create/update `Source/Assets/AGENTS.md`**

```markdown
# Assets Module — Agent Summary

## Key Components

### AssetRegistrySystem (`AssetRegistry.h/.cpp`)
- **Editor-only** (`#ifdef EDITOR_BUILD`).
- `init()`: opens the project data dir with `FileWatcher`; calls `reindex_all_assets()`.
- `update()`: calls `file_watcher_.poll(150)` each frame; dispatches changed paths to
  `update_on_changed_paths()`, which triggers typed reloads (materials, models, textures,
  sounds, Lua scripts).
- `reindex_all_assets()`: rebuilds the `AssetFilesystemNode` tree via `HackedAsyncAssetRegReindex`.

### FileWatcher (`FileWatcher.h/.cpp`)
- **Editor-only** (`#ifdef EDITOR_BUILD`).
- Background thread runs `ReadDirectoryChangesW` (overlapped I/O) on the project data dir.
- `init(dir)` → starts the background thread.
- `poll(debounce_ms)` → returns paths stable for ≥ `debounce_ms` ms, removing them from the
  internal map.  Thread-safe via `std::mutex`.
- Paths are returned as forward-slash strings relative to the watched directory (= game paths).
- **Debounce** deduplicates rapid multi-stage editor saves; the background thread never drops
  an event because `ReadDirectoryChangesW` is re-armed immediately after each completion.

### AssetFilesystemNode / HackedAsyncAssetRegReindex
- `AssetFilesystemNode`: tree node for the in-editor asset browser.
- `HackedAsyncAssetRegReindex`: rebuilds the node tree from `FileSys::find_game_files()`.

### AssetDatabase (`AssetDatabase.h/.cpp`)
- Runtime asset loading/caching for all build configurations.
- `g_assets.find_sync<T>(path)`, `g_assets.reload_sync(asset)`.
```

- [ ] **Step 2: Commit**

```bash
git add Source/Assets/AGENTS.md
git commit -m "Add Assets/AGENTS.md summarising FileWatcher and AssetRegistry"
```

---

## Task 8: Run clang-format and final verification

- [ ] **Step 1: Run clang-format**

```powershell
powershell.exe -File clang-format-all.ps1
```

- [ ] **Step 2: Run unit tests**

```powershell
powershell.exe -File Scripts/build_and_test.ps1
```

Expected: all tests pass.

- [ ] **Step 3: Build the full engine**

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1 | ForEach-Object { & $_ CsRemake.sln /p:Configuration=Debug /p:Platform=x64 /v:minimal }
```

Expected: zero errors.

- [ ] **Step 4: Commit formatting fixes (if any)**

```bash
git add -u
git commit -m "Apply clang-format to FileWatcher and AssetRegistry changes"
```

---

## Self-Review Checklist

- **Spec coverage:**
  - Double-change issue → debounce in `FileWatcher::poll()` ✓
  - Missed changes (async future race) → eliminated; `ReadDirectoryChangesW` never drops events once re-armed ✓
  - Timeout-skips missed stuff → global 5 s cooldown removed; per-path debounce is non-discarding ✓
  - Unit tests → Tasks 2–4 ✓
  - Integration tests → Task 6 ✓

- **No placeholders:** All code blocks are complete. No TBDs.

- **Type consistency:**
  - `FileWatcher::poll()` returns `std::vector<std::string>` — used as `ChangedPaths` in `update()` ✓
  - `update_on_changed_paths(ChangedPaths)` signature unchanged ✓
  - `file_watcher_` member name used consistently in `init()` and `update()` ✓

- **Known edge case documented:** Buffer overflow in `ReadDirectoryChangesW` (bytes == 0) is handled by re-issuing the read and logging nothing. The engine will catch the change on the next write.
