// Source/IntegrationTests/TestContext.h
#pragma once
#include <string>
#include <vector>
#include <coroutine>
#include "TestTask.h"
#include "Framework/MulticastDelegate.h"

class EditorTestContext;
class ScopedGpuTimer;

// State the runner checks each tick to decide whether to resume the coroutine.
// Awaitables write into this struct during await_suspend.
struct TestWaitState
{
	int wait_ticks = 0;
	float wait_seconds = 0.f;
	bool waiting_delegate = false;
	bool delegate_fired = false;
	bool screenshot_pending = false;
	std::string screenshot_name;
	// Per-shot warn-band overrides. <0 means "leave the runner's cfg alone"
	// (strict mode). Set via TestContext::capture_screenshot's optional args.
	int screenshot_warn_channel_delta = -1;
	float screenshot_warn_diff_fraction = -1.f;
};

struct TestContext
{
	TestWaitState wait;
	int checks_passed = 0;
	int checks_failed = 0;
	std::vector<std::string> failures;

	// -- Assertions --
	void check(bool b, const char* msg);   // records, continues
	void require(bool b, const char* msg); // throws TestAbortException if false

	// -- Awaitables --
	struct TickAwaitable
	{
		TestWaitState& wait;
		int n;
		bool await_ready() const noexcept { return n <= 0; }
		void await_suspend(std::coroutine_handle<>) noexcept { wait.wait_ticks = n; }
		void await_resume() noexcept {}
	};
	struct SecondAwaitable
	{
		TestWaitState& wait;
		float t;
		bool await_ready() const noexcept { return t <= 0.f; }
		void await_suspend(std::coroutine_handle<>) noexcept { wait.wait_seconds = t; }
		void await_resume() noexcept {}
	};
	struct DelegateAwaitable
	{
		TestWaitState& wait;
		MulticastDelegate<>& delegate;
		bool await_ready() const noexcept { return false; }
		void await_suspend(std::coroutine_handle<>) noexcept;
		void await_resume() noexcept {}
	};
	struct ScreenshotAwaitable
	{
		TestWaitState& wait;
		const char* name;
		int warn_channel_delta;   // <0 = strict (no warn band)
		float warn_diff_fraction; // <0 = strict (no warn band)
		bool await_ready() const noexcept { return false; }
		void await_suspend(std::coroutine_handle<>) noexcept {
			wait.screenshot_pending = true;
			wait.screenshot_name = name;
			wait.screenshot_warn_channel_delta = warn_channel_delta;
			wait.screenshot_warn_diff_fraction = warn_diff_fraction;
			wait.wait_ticks = 1; // yield one tick for frame to render
		}
		void await_resume() noexcept {}
	};

	TickAwaitable wait_ticks(int n) { return {wait, n}; }
	SecondAwaitable wait_seconds(float t) { return {wait, t}; }
	DelegateAwaitable wait_for(MulticastDelegate<>& d) { return {wait, d}; }
	// Capture a screenshot and diff it against the golden.
	// Pass warn_channel_delta / warn_diff_fraction to opt this shot into the
	// soft-fail band: deltas above the strict thresholds but inside the warn
	// band print "SCREENSHOT WARN" and the test still passes. Negative = strict.
	ScreenshotAwaitable capture_screenshot(const char* name,
										   int warn_channel_delta = -1,
										   float warn_diff_fraction = -1.f) {
		return {wait, name, warn_channel_delta, warn_diff_fraction};
	}

	ScopedGpuTimer gpu_timer(const char* name);

	// Editor mode only — asserts/aborts if called from GameTestRunner
	EditorTestContext& editor();

	// Set by runner before each test
	bool is_editor_mode = false;
};
