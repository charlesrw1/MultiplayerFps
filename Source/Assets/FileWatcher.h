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
