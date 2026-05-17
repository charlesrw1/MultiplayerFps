// Source/IntegrationTests/GpuTimer.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "GpuTimer.h"
#include "Render/IGraphicsDevice.h"
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
	if (!f)
		return;
	fprintf(f, "[\n");
	for (size_t i = 0; i < results_.size(); ++i) {
		fprintf(f, "  {\"name\": \"%s\", \"ms\": %.4f}%s\n", results_[i].name.c_str(), results_[i].ms,
				i + 1 < results_.size() ? "," : "");
	}
	fprintf(f, "]\n");
	fclose(f);
}

ScopedGpuTimer::ScopedGpuTimer(const char* name) : name_(name) {
	start_ = gfx().create_timer_query();
	stop_  = gfx().create_timer_query();
	start_->record_timestamp();
}

ScopedGpuTimer::~ScopedGpuTimer() {
	if (stop_ && !result_read_) {
		// End-of-scope: record stop so a later ms() call works. If the caller
		// already called ms() (result_read_), the stop timestamp is already
		// recorded and read.
		stop_->record_timestamp();
	}
	if (result_read_) {
		GpuTimingLog::get().record(name_, cached_ms_);
	}
	if (start_) start_->release();
	if (stop_)  stop_->release();
}

ScopedGpuTimer::ScopedGpuTimer(ScopedGpuTimer&& o) noexcept
	: name_(std::move(o.name_)), start_(o.start_), stop_(o.stop_),
	  cached_ms_(o.cached_ms_), result_read_(o.result_read_) {
	o.start_ = nullptr;
	o.stop_  = nullptr;
}

double ScopedGpuTimer::ms() const {
	if (result_read_)
		return cached_ms_;
	// Record stop now if it hasn't been recorded yet.
	stop_->record_timestamp();
	const uint64_t a = start_->read_timestamp_ns();
	const uint64_t b = stop_->read_timestamp_ns();
	cached_ms_ = static_cast<double>(b - a) / 1e6;
	result_read_ = true;
	return cached_ms_;
}
