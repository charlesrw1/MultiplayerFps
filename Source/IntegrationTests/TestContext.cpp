// Source/IntegrationTests/TestContext.cpp
#include "TestContext.h"
#include "EditorTestContext.h"
#include "GpuTimer.h"
#include <cstdio>
#include <cassert>

void TestContext::check(bool b, const char* msg) {
	if (b) {
		++checks_passed;
	} else {
		++checks_failed;
		failures.push_back(std::string("FAIL: ") + msg);
		fprintf(stderr, "  CHECK FAILED: %s\n", msg);
	}
}

void TestContext::require(bool b, const char* msg) {
	if (!b) {
		++checks_failed;
		failures.push_back(std::string("REQUIRE FAILED: ") + msg);
		fprintf(stderr, "  REQUIRE FAILED: %s\n", msg);
		throw TestAbortException{};
	}
	++checks_passed;
}

void TestContext::DelegateAwaitable::await_suspend(std::coroutine_handle<>) noexcept {
	wait.waiting_delegate = true;
	wait.delegate_fired = false;
	TestWaitState* w = &wait;
	MulticastDelegate<>* d = &delegate;
	delegate.add(w, [w, d]() {
		w->delegate_fired = true;
		d->remove(w);
	});
}

ScopedGpuTimer TestContext::gpu_timer(const char* name) {
	return ScopedGpuTimer(name);
}

EditorTestContext& TestContext::editor() {
	assert(is_editor_mode && "editor() called from a game mode test");
	static EditorTestContext s_editor_ctx;
	return s_editor_ctx;
}
