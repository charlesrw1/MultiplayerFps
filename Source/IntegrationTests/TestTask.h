// Source/IntegrationTests/TestTask.h
#pragma once
#include <coroutine>

// The return type for all test coroutines.
// Usage: TestTask my_test(TestContext& t) { co_await t.wait_ticks(1); }
struct TestTask {
    struct promise_type {
        TestTask get_return_object() noexcept {
            return TestTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        // Start suspended so the runner controls first resume
        std::suspend_always initial_suspend() noexcept { return {}; }
        // Stay alive after completion so done() is readable
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    explicit TestTask(std::coroutine_handle<promise_type> h) : handle(h) {}
    TestTask(const TestTask&) = delete;
    TestTask& operator=(const TestTask&) = delete;
    TestTask(TestTask&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
    ~TestTask() { if (handle) handle.destroy(); }

    bool done() const { return !handle || handle.done(); }
    void resume() { if (!done()) handle.resume(); }
};
