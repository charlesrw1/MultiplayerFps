#pragma once
// Fast CPU + GPU marker profiler. Replaces the old Tracy integration and the
// single-threaded Profiler in Util.h (see Profiler.cpp).
//
// Zone identity is a per-call-site static slot index (see CPU_SCOPE/GPU_SCOPE
// macros below) -- no string hashing or heap allocation on the hot path.
//
// CPU zones are captured per-thread (each thread gets its own nested stack +
// event buffer). GPU zones are captured on a single timeline (there's one GPU
// command stream) and resolved asynchronously a few frames later via a
// pooled set of timer queries, never blocking on the CPU.
#include <cstdint>
#include <atomic>
#include <string>
#include <vector>

namespace prof {

constexpr int kMaxRecordedFrames = 240; // recording ring capacity (~4s at 60fps)
constexpr int kGpuPoolFrames      = 3;   // GPU query double/triple buffering depth

// One executed zone within a single frame's capture.
struct ZoneEvent
{
	uint32_t slot     = 0;         // index into g_zone_locations
	uint16_t depth    = 0;
	uint64_t start_ns = 0;
	uint64_t end_ns   = 0;         // GPU: UINT64_MAX until the query result resolves
};

struct FrameEvents
{
	uint64_t frame_index = UINT64_MAX; // UINT64_MAX == slot never written
	std::vector<ZoneEvent> events;

	void reset(uint64_t new_frame_index) {
		frame_index = new_frame_index;
		events.clear();
	}
};

// Static info about a call site, registered once the first time it executes.
struct ZoneLocation
{
	std::string name;
	const char* file = "";
	int line          = 0;
	bool is_gpu       = false;
};

// Registration + shared zone-location table. Thread-safe (mutex only touched
// on first-ever hit of a call site); lookups after that are lock-free array
// indexing done by the caller via the cached static slot.
class ProfilerRegistry
{
public:
	static uint32_t register_zone(const char* name, const char* file, int line, bool is_gpu);
	static const ZoneLocation& location(uint32_t slot);
	static uint32_t zone_count();
};

// ---- CPU capture ------------------------------------------------------

// Per-thread capture state. Registered lazily (once per thread) into a
// global list so the UI can enumerate lanes.
class ThreadCapture
{
public:
	explicit ThreadCapture(std::string name);

	void push(uint32_t slot);
	void pop();

	// Called once per frame by the owning thread's next push/frame boundary
	// housekeeping; actual rotation is driven centrally by Profiler::end_frame().
	FrameEvents current;
	std::vector<uint16_t> open_stack; // indices into current.events

	const std::string& name() const { return name_; }
	void set_name(std::string n) { name_ = std::move(n); }

	// Recording ring; index [frame_index % capacity]. Capacity is small while
	// live (just enough retention for GPU results to resolve) and
	// kMaxRecordedFrames while recording.
	std::vector<FrameEvents> ring;

private:
	std::string name_;
};

// RAII CPU scope. Cheap no-op when the profiler is globally disabled.
class ProfilerCpuScope
{
public:
	explicit ProfilerCpuScope(uint32_t slot);
	~ProfilerCpuScope();

private:
	bool active_;
};

// ---- GPU capture -------------------------------------------------------

// RAII GPU scope. Records a timestamp pair via a pooled query pair keyed by
// (zone slot, frame_index % kGpuPoolFrames) -- never allocates a GL object
// on the hot path after warmup, never blocks on readback.
class ProfilerGpuScope
{
public:
	explicit ProfilerGpuScope(uint32_t slot);
	~ProfilerGpuScope();

private:
	bool active_;
	uint32_t slot_;
};

// ---- Global control / frame boundary -----------------------------------

enum class RecordingState : uint8_t { Live, Recording, Paused };

class Profiler
{
public:
	static void init();

	// Called once per frame from the main loop, after the frame's work has
	// been submitted (see EngineMain_Loop.cpp). Rotates CPU per-thread
	// buffers into their rings, resolves any GPU queries that are ready, and
	// rotates the GPU ring.
	static void end_frame();

	static bool enabled() { return g_enabled.load(std::memory_order_relaxed); }
	static void set_enabled(bool e) { g_enabled.store(e, std::memory_order_relaxed); }

	static RecordingState recording_state() { return g_state; }
	static void set_recording_state(RecordingState s);
	static void clear_history();

	static uint64_t current_frame_index() { return g_frame_index.load(std::memory_order_relaxed); }

	// Last frame index actually written into the rings. Equal to
	// current_frame_index()-1 while Live/Recording, but frozen while Paused
	// -- the UI must use this (not current_frame_index()) as the upper bound
	// when enumerating scrubbable history, or the display window keeps
	// sliding forward past the frozen data every frame.
	static uint64_t last_committed_frame() { return g_last_committed_frame.load(std::memory_order_relaxed); }

	// Thread/GPU enumeration for the UI (Source/Framework/ProfilerUI.cpp).
	static std::vector<ThreadCapture*> all_threads();
	static ThreadCapture& gpu_capture(); // single pseudo-thread lane for GPU zones

	static std::atomic<bool> g_enabled;

private:
	static std::atomic<uint64_t> g_frame_index;
	static std::atomic<uint64_t> g_last_committed_frame;
	static RecordingState g_state;
};

ThreadCapture& this_thread_capture();

// Rename the calling thread's lane (e.g. job system workers naming
// themselves "JobWorker N" instead of the default "Thread N").
void set_current_thread_profiler_name(const char* name);

} // namespace prof

// ---- Public macros ------------------------------------------------------
//
// Each call site registers exactly once (function-local static), then every
// later hit is a direct array index -- safe and cheap to call multiple times
// per frame.

#define PROF_CONCAT_(a, b) a##b
#define PROF_CONCAT(a, b) PROF_CONCAT_(a, b)

#define CPU_SCOPE(name)                                                                                               \
	static uint32_t PROF_CONCAT(_prof_cpu_slot_, __LINE__) =                                                          \
		prof::ProfilerRegistry::register_zone(name, __FILE__, __LINE__, false);                                       \
	prof::ProfilerCpuScope PROF_CONCAT(_prof_cpu_scope_, __LINE__)(PROF_CONCAT(_prof_cpu_slot_, __LINE__))

#define GPU_SCOPE(name)                                                                                               \
	static uint32_t PROF_CONCAT(_prof_gpu_slot_, __LINE__) =                                                          \
		prof::ProfilerRegistry::register_zone(name, __FILE__, __LINE__, true);                                        \
	prof::ProfilerGpuScope PROF_CONCAT(_prof_gpu_scope_, __LINE__)(PROF_CONCAT(_prof_gpu_slot_, __LINE__))

// Times a render pass on both clocks at once. The two zones are independent
// records under the hood -- one shows up in the CPU tab, one in the GPU tab
// -- because CPU submission time and GPU execution time are different clocks
// with different latencies; see docs/rendering (profiler section) if added.
#define RENDER_SCOPE(name)                                                                                            \
	CPU_SCOPE(name);                                                                                                   \
	GPU_SCOPE(name)

#define CPU_FUNCTION() CPU_SCOPE(__FUNCTION__)
#define GPU_FUNCTION() GPU_SCOPE(__FUNCTION__)
