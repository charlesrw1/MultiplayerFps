#include "Framework/Profiler.h"
#include "Framework/Config.h"
#include "Framework/Util.h"
#include "Render/IGraphicsDevice.h"
#include <chrono>
#include <mutex>
#include <thread>
#include <array>
#include <algorithm>
#include <cassert>

using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

static uint64_t now_ns() {
	return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

namespace prof {

std::atomic<bool> Profiler::g_enabled{ true };
std::atomic<uint64_t> Profiler::g_frame_index{ 0 };
std::atomic<uint64_t> Profiler::g_last_committed_frame{ 0 };
RecordingState Profiler::g_state = RecordingState::Live;

// ---- ProfilerRegistry ---------------------------------------------------

static constexpr uint32_t kMaxZones = 8192;
static std::mutex g_registry_mutex;
static std::vector<ZoneLocation> g_zone_locations; // reserved to kMaxZones, never reallocates

uint32_t ProfilerRegistry::register_zone(const char* name, const char* file, int line, bool is_gpu) {
	std::lock_guard<std::mutex> lock(g_registry_mutex);
	if (g_zone_locations.capacity() == 0)
		g_zone_locations.reserve(kMaxZones);
	ASSERT(g_zone_locations.size() < kMaxZones);
	g_zone_locations.push_back(ZoneLocation{ name, file, line, is_gpu });
	return (uint32_t)g_zone_locations.size() - 1;
}
const ZoneLocation& ProfilerRegistry::location(uint32_t slot) {
	return g_zone_locations[slot];
}
uint32_t ProfilerRegistry::zone_count() {
	return (uint32_t)g_zone_locations.size();
}

// ---- Recording capacity --------------------------------------------------

// Live mode still keeps a short rolling window -- GPU zone results resolve a
// few frames after submission (see kGpuPoolFrames), so "just show current
// timers" needs a little retention to have anything to show at all.
static constexpr size_t kLiveCapacity = kGpuPoolFrames * 2 + 2;

// Paused keeps the same capacity Recording had -- the buffer stays frozen
// (and scrubbable) rather than shrinking back down to the live window.
static size_t current_capacity() {
	return Profiler::recording_state() != RecordingState::Live ? kMaxRecordedFrames : kLiveCapacity;
}

static void rotate_into_ring(ThreadCapture& tc, uint64_t finishing_frame) {
	const size_t cap = current_capacity();
	if (tc.ring.size() != cap)
		tc.ring.assign(cap, FrameEvents{});

	FrameEvents finished = std::move(tc.current);
	tc.current.reset(finishing_frame + 1);

	// While paused, capture keeps running (cheap) but the recorded buffer is
	// frozen -- drop this frame's data instead of overwriting the ring.
	if (Profiler::recording_state() == RecordingState::Paused)
		return;

	if (cap > 0)
		tc.ring[finishing_frame % cap] = std::move(finished);
}

// ---- Thread registry ------------------------------------------------------

static std::mutex g_threads_mutex;
static std::vector<ThreadCapture*> g_threads;
static std::thread::id g_main_thread_id;
static std::atomic<int> g_anon_thread_counter{ 0 };

ThreadCapture::ThreadCapture(std::string name) : name_(std::move(name)) {
	current.reset(Profiler::current_frame_index());
}

void ThreadCapture::push(uint32_t slot) {
	ZoneEvent e;
	e.slot     = slot;
	e.depth    = (uint16_t)open_stack.size();
	e.start_ns = now_ns();
	e.end_ns   = UINT64_MAX;
	current.events.push_back(e);
	open_stack.push_back((uint16_t)(current.events.size() - 1));
}
void ThreadCapture::pop() {
	ASSERT(!open_stack.empty());
	uint16_t idx = open_stack.back();
	open_stack.pop_back();
	current.events[idx].end_ns = now_ns();
}

ThreadCapture& this_thread_capture() {
	thread_local ThreadCapture* tc = nullptr;
	if (!tc) {
		std::string name;
		if (std::this_thread::get_id() == g_main_thread_id)
			name = "Main";
		else
			name = "Thread " + std::to_string(g_anon_thread_counter.fetch_add(1) + 1);

		std::lock_guard<std::mutex> lock(g_threads_mutex);
		g_threads.push_back(new ThreadCapture(std::move(name)));
		tc = g_threads.back();
	}
	return *tc;
}

void set_current_thread_profiler_name(const char* name) {
	this_thread_capture().set_name(name);
}

// ---- GPU capture ----------------------------------------------------------

struct GpuPoolEntry
{
	IGraphicsTimerQuery* start = nullptr;
	IGraphicsTimerQuery* end   = nullptr;
	bool pending               = false;
	uint64_t submission_frame  = 0;
	uint32_t event_index       = 0;
};

static std::mutex g_gpu_pool_mutex;
static std::vector<std::array<GpuPoolEntry, kGpuPoolFrames>> g_gpu_pool;

static GpuPoolEntry& gpu_pool_entry(uint32_t slot, int pool_index) {
	std::lock_guard<std::mutex> lock(g_gpu_pool_mutex);
	if (slot >= g_gpu_pool.size())
		g_gpu_pool.resize(slot + 1);
	auto& arr = g_gpu_pool[slot];
	for (auto& e : arr) {
		if (!e.start) {
			e.start = gfx().create_timer_query();
			e.end   = gfx().create_timer_query();
		}
	}
	return arr[pool_index];
}

ThreadCapture& Profiler::gpu_capture() {
	static ThreadCapture gc("GPU");
	return gc;
}

static void resolve_gpu_queries() {
	ThreadCapture& gc = Profiler::gpu_capture();
	const uint32_t count = ProfilerRegistry::zone_count();
	for (uint32_t slot = 0; slot < count && slot < g_gpu_pool.size(); slot++) {
		if (!ProfilerRegistry::location(slot).is_gpu)
			continue;
		for (auto& entry : g_gpu_pool[slot]) {
			if (!entry.pending)
				continue;
			if (!entry.start->is_available() || !entry.end->is_available())
				continue;

			const uint64_t start_ns = entry.start->read_timestamp_ns();
			const uint64_t end_ns   = entry.end->read_timestamp_ns();
			entry.pending = false;

			FrameEvents* target = nullptr;
			if (gc.current.frame_index == entry.submission_frame) {
				target = &gc.current;
			} else if (!gc.ring.empty()) {
				const size_t idx = entry.submission_frame % gc.ring.size();
				if (gc.ring[idx].frame_index == entry.submission_frame)
					target = &gc.ring[idx];
			}
			if (target && entry.event_index < target->events.size()) {
				target->events[entry.event_index].start_ns = start_ns;
				target->events[entry.event_index].end_ns   = end_ns;
			}
		}
	}
}

ProfilerGpuScope::ProfilerGpuScope(uint32_t slot) : slot_(slot) {
	active_ = Profiler::enabled();
	if (!active_)
		return;

	const uint64_t frame = Profiler::current_frame_index();
	const int pool_index = (int)(frame % kGpuPoolFrames);
	GpuPoolEntry& entry   = gpu_pool_entry(slot_, pool_index);

	if (entry.pending) {
		// GPU has fallen behind more than kGpuPoolFrames -- drop this
		// sample rather than stalling waiting on the old one.
		active_ = false;
		return;
	}

	ThreadCapture& gc = Profiler::gpu_capture();
	ZoneEvent e;
	e.slot     = slot_;
	e.depth    = (uint16_t)gc.open_stack.size();
	e.start_ns = UINT64_MAX;
	e.end_ns   = UINT64_MAX;
	gc.current.events.push_back(e);
	gc.open_stack.push_back((uint16_t)(gc.current.events.size() - 1));

	entry.pending          = true;
	entry.submission_frame = frame;
	entry.event_index      = (uint32_t)(gc.current.events.size() - 1);
	entry.start->record_timestamp();

	gfx().push_debug_group(ProfilerRegistry::location(slot_).name.c_str());
}
ProfilerGpuScope::~ProfilerGpuScope() {
	if (!active_)
		return;
	ThreadCapture& gc = Profiler::gpu_capture();

	const uint64_t frame  = Profiler::current_frame_index();
	const int pool_index  = (int)(frame % kGpuPoolFrames);
	GpuPoolEntry& entry    = gpu_pool_entry(slot_, pool_index);
	entry.end->record_timestamp();

	gfx().pop_debug_group();

	ASSERT(!gc.open_stack.empty());
	gc.open_stack.pop_back();
}

// ---- CPU scope --------------------------------------------------------

ProfilerCpuScope::ProfilerCpuScope(uint32_t slot) {
	active_ = Profiler::enabled();
	if (active_)
		this_thread_capture().push(slot);
}
ProfilerCpuScope::~ProfilerCpuScope() {
	if (active_)
		this_thread_capture().pop();
}

// ---- Profiler control ---------------------------------------------------

void Profiler::init() {
	g_main_thread_id = std::this_thread::get_id();
	this_thread_capture(); // registers "Main"
}

void Profiler::set_recording_state(RecordingState s) {
	// Only starting a fresh recording (from Live) clears the buffer --
	// resuming from Paused keeps rolling the same ~4s window.
	if (s == RecordingState::Recording && g_state == RecordingState::Live)
		clear_history();
	g_state = s;
}
void Profiler::clear_history() {
	std::lock_guard<std::mutex> lock(g_threads_mutex);
	for (auto* t : g_threads)
		t->ring.clear();
	gpu_capture().ring.clear();
}

std::vector<ThreadCapture*> Profiler::all_threads() {
	std::lock_guard<std::mutex> lock(g_threads_mutex);
	return g_threads;
}

void Profiler::end_frame() {
	const uint64_t finishing_frame = g_frame_index.fetch_add(1, std::memory_order_relaxed);

	resolve_gpu_queries();
	rotate_into_ring(gpu_capture(), finishing_frame);

	{
		std::lock_guard<std::mutex> lock(g_threads_mutex);
		for (auto* t : g_threads)
			rotate_into_ring(*t, finishing_frame);
	}

	if (g_state != RecordingState::Paused)
		g_last_committed_frame.store(finishing_frame, std::memory_order_relaxed);
}

} // namespace prof
