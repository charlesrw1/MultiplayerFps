# IntegrationTests Module

C++20 coroutine-based integration testing framework. Tests run inside the full game/editor runtime with real GPU, level loading, and screenshot regression.

## Key Files

- `TestRegistry.h` — Test registration; `TestEntry`, `TestMode`, `TestRunnerConfig`
- `TestContext.h` — Awaitable primitives and assertion helpers
- `TestTask.h` — C++20 coroutine promise type for test functions
- `GameTestRunner.h/cpp` — Main test execution loop
- `TestGameApp.h` — Application entry point for test runs
- `EditorTestContext.h/cpp` — Editor-mode test context
- `GpuTimer.h/cpp` — GPU performance measurement
- `RenderDump.h/cpp` — Capture and compare render state
- `StateDump.h/cpp` — Capture and compare game state
- `Screenshot.h/cpp` — Screenshot capture and diff
- `LuaDebugServer.h/cpp` — Lua debugging over network
- `Tests/Game/test_engine.cpp` — Engine integration tests
- `Tests/Renderer/test_basic.cpp` — Renderer tests
- `Tests/Renderer/test_material_hotreload.cpp` — Material hot reload tests (instance reload, master shader reload, OS file-watcher reload)
- `Tests/Framework/test_filesys.cpp` — File system tests
- `Tests/Framework/test_lua_debug_server.cpp` — Lua debug server tests

## Key Classes

### `TestRegistry` (TestRegistry.h)
Global static registry for all tests.
- `register_test(entry)` — Register a `TestEntry`
- `get_filtered(config)` → list of matching `TestEntry`
- `TestMode` enum: `Game`, `Editor`
- `TestEntry`: `name`, `timeout_seconds`, `mode`, `TestFn` (coroutine factory)
- `TestRunnerConfig`: `filter` (name substring), `promote` (fail-fast), `interactive`, `timing_assert`

### `TestContext` (TestContext.h)
Injected into every test function. Provides assertions and awaitable suspension points.

**Assertions:**
- `check(condition, msg)` — Records pass/fail, continues test execution
- `require(condition, msg)` — Throws on failure, aborts test immediately
- `checks_passed`, `checks_failed`, `failures` — Result tracking

**Awaitables (use with `co_await`):**
- `tick(n)` → `TickAwaitable` — Suspend for N engine ticks
- `seconds(t)` → `SecondAwaitable` — Suspend for N seconds of game time
- `wait_for(delegate)` → `DelegateAwaitable` — Suspend until a delegate fires
- `load_level(path)` → `LevelAwaitable` — Suspend until level is fully loaded
- `screenshot(name)` → `ScreenshotAwaitable` — Capture screenshot and compare to baseline
- `dump_state(name)` → `DumpStateAwaitable` — Capture game state snapshot
- `debug_break()` → `DebugBreakAwaitable` — Break into debugger (interactive mode only)

### `TestTask` (TestTask.h)
C++20 coroutine return type for test functions.
- Custom `promise_type` with `initial_suspend`, `final_suspend`, `yield_value`
- `resume()` — Called by `GameTestRunner` each frame to advance the test coroutine

### `GameTestRunner` (GameTestRunner.h)
Drives test execution each game tick.
- Creates `TestContext`, launches `TestTask` coroutine
- Calls `task.resume()` each frame until complete or timed out
- Reports pass/fail counts; exits process with error code on failure

### `GpuTimer` (GpuTimer.h)
Wraps OpenGL timer queries to measure GPU time for a code block.
- Used in performance-sensitive renderer tests to assert frame time budgets

### `LuaDebugServer` (LuaDebugServer.h)
Runs a debug server that exposes Lua state over a network socket.
- Used by `test_lua_debug_server.cpp` to verify the Lua debugging protocol

## Test Registration Macros

```cpp
// Register a game-mode test
GAME_TEST("test_name", timeout_seconds, [](TestContext& ctx) -> TestTask {
    co_await ctx.load_level("Levels/Test.lvl");
    co_await ctx.tick(10);
    ctx.check(some_condition, "expected X");
    co_await ctx.screenshot("baseline_name");
});

// Register an editor-mode test
EDITOR_TEST("editor_test_name", timeout_seconds, [](TestContext& ctx) -> TestTask {
    // editor-specific operations
});
```

## Key Concepts

- **Coroutine-based tests** — Tests are C++20 coroutines; `co_await` suspends the test and returns control to the game loop, resuming on the next qualifying frame. This allows natural "wait N frames" / "wait for event" patterns without threads.
- **Screenshot regression** — `co_await ctx.screenshot("name")` captures a frame and diffs against a stored baseline image. Baselines stored under `Tests/Baselines/`.
- **Timeout enforcement** — Each `TestEntry` has `timeout_seconds`; `GameTestRunner` aborts and fails the test if it exceeds this.
- **Two modes** — `TestMode::Game` runs in the normal game runtime; `TestMode::Editor` runs with the editor active, enabling editor workflow tests.
- **Run via** — `Scripts/build_and_test.ps1` builds and runs the test binary. Exit code 0 = all pass.
