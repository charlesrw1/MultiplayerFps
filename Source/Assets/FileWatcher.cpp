// Source/Assets/FileWatcher.cpp
#ifdef EDITOR_BUILD
#include "Assets/FileWatcher.h"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct FileWatcher::Impl {
    HANDLE dir_handle  = INVALID_HANDLE_VALUE;
    HANDLE stop_event  = INVALID_HANDLE_VALUE;
    std::thread worker_thread;
    std::mutex mutex;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> pending;

    void worker();
};

FileWatcher::FileWatcher() : impl_(std::make_unique<Impl>()) {}

FileWatcher::~FileWatcher() {
    if (impl_->stop_event != INVALID_HANDLE_VALUE) {
        SetEvent(impl_->stop_event);
        if (impl_->worker_thread.joinable())
            impl_->worker_thread.join();
        CloseHandle(impl_->stop_event);
        impl_->stop_event = INVALID_HANDLE_VALUE;
    }
    if (impl_->dir_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(impl_->dir_handle);
        impl_->dir_handle = INVALID_HANDLE_VALUE;
    }
}

bool FileWatcher::init(const std::string& dir) {
    impl_->dir_handle = CreateFileA(
        dir.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (impl_->dir_handle == INVALID_HANDLE_VALUE)
        return false;

    impl_->stop_event = CreateEventA(
        nullptr, /*manualReset=*/TRUE, /*initial=*/FALSE, nullptr);
    if (impl_->stop_event == INVALID_HANDLE_VALUE) {
        CloseHandle(impl_->dir_handle);
        impl_->dir_handle = INVALID_HANDLE_VALUE;
        return false;
    }

    impl_->worker_thread = std::thread(&Impl::worker, impl_.get());
    return true;
}

void FileWatcher::Impl::worker() {
    // 64 KB is large enough to avoid overflow under normal editor usage.
    // Must be DWORD-aligned; alignas guarantees that.
    alignas(DWORD) char buf[65536];
    OVERLAPPED ov = {};
    ov.hEvent = CreateEventA(
        nullptr, /*manualReset=*/TRUE, /*initial=*/FALSE, nullptr);
    if (ov.hEvent == INVALID_HANDLE_VALUE)
        return;

    auto issue_read = [&]() -> bool {
        ResetEvent(ov.hEvent);
        DWORD dummy = 0;
        BOOL ok = ReadDirectoryChangesW(
            dir_handle,
            buf, sizeof(buf),
            /*watchSubtree=*/TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            &dummy,
            &ov,
            /*completionRoutine=*/nullptr);
        // ERROR_IO_PENDING is the expected return path for overlapped I/O.
        return ok != FALSE || GetLastError() == ERROR_IO_PENDING;
    };

    if (!issue_read()) {
        CloseHandle(ov.hEvent);
        return;
    }

    HANDLE handles[2] = {ov.hEvent, stop_event};
    while (true) {
        DWORD wait = WaitForMultipleObjects(2, handles, /*waitAll=*/FALSE, INFINITE);
        if (wait != WAIT_OBJECT_0)
            break; // stop_event fired or error

        DWORD bytes = 0;
        if (!GetOverlappedResult(dir_handle, &ov, &bytes, /*wait=*/FALSE) || bytes == 0) {
            // Buffer overflow (bytes == 0) or I/O error: reissue and continue.
            // We may miss which specific files changed during overflow, but the
            // next write to those files will generate a new notification.
            issue_read();
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        const char* p = buf;
        while (true) {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(p);

            // Convert the UTF-16LE filename to UTF-8
            int wlen = static_cast<int>(info->FileNameLength / sizeof(WCHAR));
            int utf8_len = WideCharToMultiByte(
                CP_UTF8, 0, info->FileName, wlen, nullptr, 0, nullptr, nullptr);
            std::string rel_path(static_cast<size_t>(utf8_len), '\0');
            WideCharToMultiByte(
                CP_UTF8, 0, info->FileName, wlen, &rel_path[0], utf8_len, nullptr, nullptr);

            // Normalise to forward slashes (game-path convention)
            std::replace(rel_path.begin(), rel_path.end(), '\\', '/');

            {
                std::lock_guard<std::mutex> lock(mutex);
                pending[rel_path] = now;
            }

            if (info->NextEntryOffset == 0)
                break;
            p += info->NextEntryOffset;
        }

        // Re-arm the read before going back to wait so no events are missed
        // between GetOverlappedResult returning and the next WaitForMultipleObjects.
        issue_read();
    }

    CloseHandle(ov.hEvent);
}

std::vector<std::string> FileWatcher::poll(int debounce_ms) {
    auto cutoff = std::chrono::steady_clock::now()
                  - std::chrono::milliseconds(debounce_ms);
    std::vector<std::string> ready;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (auto it = impl_->pending.begin(); it != impl_->pending.end();) {
        if (it->second <= cutoff) {
            ready.push_back(std::move(it->first));
            it = impl_->pending.erase(it);
        } else {
            ++it;
        }
    }
    return ready;
}
#endif // EDITOR_BUILD
