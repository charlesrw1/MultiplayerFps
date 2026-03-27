// Source/IntegrationTests/GpuTimer.h
#pragma once
#include <string>
#include <vector>
#include "External/glad/glad.h"

struct GpuTimingResult {
    std::string name;
    double ms = 0.0;
};

// Accumulates results per run, written to JSON at end
class GpuTimingLog {
public:
    static GpuTimingLog& get();
    void record(const std::string& name, double ms);
    void write_json(const char* path);
private:
    std::vector<GpuTimingResult> results_;
};

// RAII GL timer query. Create before the work, read ms() after co_await wait_ticks(1).
class ScopedGpuTimer {
public:
    explicit ScopedGpuTimer(const char* name);
    ~ScopedGpuTimer();

    ScopedGpuTimer(const ScopedGpuTimer&) = delete;
    ScopedGpuTimer(ScopedGpuTimer&&) noexcept;

    // Blocks until result available. Call after co_await wait_ticks(1).
    double ms() const;

    const std::string& name() const { return name_; }

private:
    std::string name_;
    GLuint query_id_ = 0;
    mutable double cached_ms_ = -1.0;
    mutable bool result_read_ = false;
};
