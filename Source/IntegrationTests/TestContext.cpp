// Source/IntegrationTests/TestContext.cpp
#include "TestContext.h"
#include "EditorTestContext.h"
#include "GpuTimer.h"
#include "GameEnginePublic.h"
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
    delegate.add(this, [this]() {
        wait.delegate_fired = true;
        delegate.remove(this);
    });
}

void TestContext::LevelAwaitable::await_suspend(std::coroutine_handle<>) noexcept {
    eng->load_level(path);
    // level load is synchronous — one tick to settle
    wait.wait_ticks = 1;
}

ScopedGpuTimer TestContext::gpu_timer(const char* name) {
    return ScopedGpuTimer(name);
}

EditorTestContext& TestContext::editor() {
    assert(is_editor_mode && "editor() called from a game mode test");
    static EditorTestContext s_editor_ctx;
    return s_editor_ctx;
}
