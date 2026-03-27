// Source/IntegrationTests/GpuTimer.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "GpuTimer.h"
#include <cstdio>

GpuTimingLog& GpuTimingLog::get() {
    static GpuTimingLog s;
    return s;
}

void GpuTimingLog::record(const std::string& name, double ms) {
    results_.push_back({name, ms});
}

void GpuTimingLog::write_json(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "[\n");
    for (size_t i = 0; i < results_.size(); ++i) {
        fprintf(f, "  {\"name\": \"%s\", \"ms\": %.4f}%s\n",
            results_[i].name.c_str(), results_[i].ms,
            i + 1 < results_.size() ? "," : "");
    }
    fprintf(f, "]\n");
    fclose(f);
}

ScopedGpuTimer::ScopedGpuTimer(const char* name) : name_(name) {
    glGenQueries(1, &query_id_);
    glBeginQuery(GL_TIME_ELAPSED, query_id_);
}

ScopedGpuTimer::~ScopedGpuTimer() {
    if (query_id_) {
        glEndQuery(GL_TIME_ELAPSED);
        if (result_read_) {
            // Caller already read the result via ms() — record it
            GpuTimingLog::get().record(name_, cached_ms_);
        }
        // If not result_read_, caller forgot to read before destruction.
        // Don't block on GL_QUERY_RESULT here — just clean up.
        glDeleteQueries(1, &query_id_);
    }
}

ScopedGpuTimer::ScopedGpuTimer(ScopedGpuTimer&& o) noexcept
    : name_(std::move(o.name_)), query_id_(o.query_id_),
      cached_ms_(o.cached_ms_), result_read_(o.result_read_) {
    o.query_id_ = 0;
}

double ScopedGpuTimer::ms() const {
    if (result_read_) return cached_ms_;
    GLuint64 ns = 0;
    glGetQueryObjectui64v(query_id_, GL_QUERY_RESULT, &ns);
    cached_ms_ = static_cast<double>(ns) / 1e6;
    result_read_ = true;
    return cached_ms_;
}
