# Integration Test System Design
**Date:** 2026-03-26

## Goals

- Behavioral assertions: engine state, entity counts, component properties
- Screenshot regression: render a frame, diff against a promoted golden image
- GPU timing: measure pass costs, optionally assert budgets
- Two modes: headless/automated (CI) and interactive (debug failing tests)
- Cover two domains: game/renderer and level editor
- Replace the existing `IntegrationTester` / `EngineTestcase` / `EngineTesterApp` skeleton

---

## Test Framework

### Coroutine-based (C++20)

Tests are C++20 coroutines. No background thread, no mutex, no condition variable. The `TestRunner` resumes the coroutine once per tick from the main thread. A failing test has a normal single-threaded call stack.

```cpp
TestTask test_shadowmaps(TestContext& t) {
    co_await t.load_level("shadow_test.tmap");
    co_await t.wait_ticks(2);
    auto timer = t.gpu_timer("shadowmap_pass");
    co_await t.capture_screenshot("shadowmaps_basic");
    t.check(timer.ms() < 5.f, "shadowmap pass exceeds 5ms budget");
}
```

### Registration

```cpp
// game mode tests
GAME_TEST("renderer/shadowmaps", 10.f, test_shadowmaps);
GAME_TEST("renderer/materials", 10.f, test_materials);

// editor mode tests
EDITOR_TEST("editor/serialize_round_trip", 5.f, test_serialize_round_trip);
EDITOR_TEST("editor/undo_redo", 5.f, test_undo_redo);
```

Macros register into a global list filtered by mode at startup.

---

## TestContext API

```cpp
struct TestContext {
    // flow control (co_await these)
    Awaitable wait_ticks(int n);
    Awaitable wait_seconds(float t);
    Awaitable wait_for(MulticastDelegate<>& d);

    // assertions
    void check(bool b, const char* msg);    // records failure, continues test
    void require(bool b, const char* msg);  // aborts current test on failure

    // level (co_await — yields until level is fully loaded)
    Awaitable load_level(const char* path);

    // screenshot (co_await — yields one tick so frame renders, then reads pixels)
    Awaitable capture_screenshot(const char* name);

    // gpu timing — result available after co_await wait_ticks(1)
    ScopedGpuTimer gpu_timer(const char* name);

    // editor mode only — asserts/crashes if called from GameTestRunner
    EditorTestContext& editor();
};
```

### EditorTestContext

```cpp
struct EditorTestContext {
    EditorDoc& doc();
    int entity_count();
    void undo();
    void redo();
    void select(EntityId id);
    void set_property(EntityId id, const char* prop, PropertyValue val);
    void save_level(const char* path);
};
```

---

## TestRunner (polymorphic)

The engine calls `runner->tick(dt)` in one place regardless of mode. Two concrete implementations:

```cpp
class TestRunner {
public:
    virtual void tick(float dt) = 0;
};

class GameTestRunner : public TestRunner {
    // resumes coroutine, provides TestContext backed by game systems
};

class EditorTestRunner : public TestRunner {
    // resumes coroutine, provides TestContext backed by EditorDoc
    // editor() accessor is valid here
};
```

At startup, `--mode=game` constructs `GameTestRunner`, `--mode=editor` constructs `EditorTestRunner`. The engine loop is unchanged.

`GameTestRunner` boots with a minimal `TestGameApp : Application` — no game logic, just camera setup, injected via the existing `set_tester()` hook.

`EditorTestRunner` boots with no `Application`. Editor subsystems initialize as normal.

---

## Screenshot System

**Note on headless mode:** The existing `headless_mode` flag skips `scene_draw()` entirely. This is incompatible with screenshot tests. The integration test runner must run with rendering active but with `SDL_GL_SwapWindow` skipped (no visible window in CI). The existing flag will need to be split or replaced: `skip_swap` (CI-safe, renders normally, no window presentation) vs a future true headless path.

`co_await capture_screenshot(name)`:
1. Yields one tick so the frame renders fully
2. `glReadPixels` into a CPU buffer
3. Saves to `TestFiles/screenshots/<name>_actual.png`
4. Looks for golden at `TestFiles/goldens/<name>.png`
   - **No golden found:** fail with message "no golden — run with --promote"
   - **Golden found:** per-pixel diff, fail if max channel delta > 8 or > 0.1% of pixels differ
5. On failure in `--interactive` mode: renders diff to window, pauses until keypress

**Golden promotion (`--promote` flag):** copies `_actual` → golden instead of diffing. Developer runs this after visually verifying output, then commits the golden.

Goldens are stored in `TestFiles/goldens/` and committed to the repo. Migrate to git-lfs if history bloat becomes an issue.

---

## GPU Timing

`ScopedGpuTimer` wraps OpenGL timer queries (`GL_TIME_ELAPSED`):

- Constructor: `glBeginQuery`
- Destructor: `glEndQuery`
- Result has 1-frame latency — read after `co_await wait_ticks(1)`
- Accessing `timer.ms()` before result is ready blocks on `glGetQueryObjectui64v`

Results are:
- Printed in test output alongside pass/fail
- Appended to `TestFiles/timing_<YYYY-MM-DD>.json` for trend tracking
- Timing threshold assertions (`check(timer.ms() < N, ...)`) are skipped in CI unless `--timing-assert` is passed, since machine variance makes them fragile

---

## Two-Pass Runner

Same binary, two invocations:

```
IntegrationTests.exe --mode=game   [--test=<glob>] [--promote] [--interactive] [--timing-assert]
IntegrationTests.exe --mode=editor [--test=<glob>] [--promote] [--interactive]
```

The binary is compiled with `EDITOR_BUILD` defined (new VS project configuration: `IntegrationTest`) so both game and editor code are available. Mode is selected at runtime.

### Flags

| Flag | Effect |
|------|--------|
| `--mode=game\|editor` | Required. Selects test runner and subsystem initialization |
| `--test=<glob>` | Filter tests by name pattern, e.g. `renderer/*`, `editor/serialize*` |
| `--promote` | Write actual screenshots as new goldens instead of diffing |
| `--interactive` | Windowed mode, pause on first failure for inspection |
| `--timing-assert` | Enable GPU timing threshold assertions (off by default) |

---

## PS1 Runner (`Scripts/integration_test.ps1`)

1. Builds the `IntegrationTests` VS project (`IntegrationTest` config, x64)
2. Runs `--mode=game`, captures exit code
3. Runs `--mode=editor`, captures exit code
4. Writes combined JUnit XML to `TestFiles/integration_results.xml`
5. Exits non-zero if either pass failed

Extends the existing `build_and_test.ps1` pattern so CI can call a single script.

---

## File Layout

```
Source/
  IntegrationTests/
    IntegrationTests.vcxproj    -- new VS project, EDITOR_BUILD defined
    TestRunner.h/cpp            -- base TestRunner, coroutine machinery
    GameTestRunner.h/cpp
    EditorTestRunner.h/cpp
    TestContext.h/cpp           -- TestContext, EditorTestContext
    Screenshot.h/cpp            -- capture, diff, promote
    GpuTimer.h/cpp              -- ScopedGpuTimer
    Tests/
      Renderer/
        test_shadowmaps.cpp
        test_materials.cpp
        ...
      Editor/
        test_serialize.cpp
        test_undo_redo.cpp
        ...

TestFiles/
  goldens/                      -- committed golden PNGs
  screenshots/                  -- gitignored actual output
  timing_<date>.json            -- gitignored timing reports

Scripts/
  integration_test.ps1          -- two-pass runner + JUnit output
```

---

## What Gets Removed

- `Source/IntegrationTest.h` / `IntegrationTest.cpp`
- `Source/UnitTests/EngineTesterApplication.h` / `.cpp` (the integration test parts)
- `EngineTestcase` base class
- The camera flythrough test loop in `EngineTesterApp::start()`

The `UnitTests` project and its gtest unit tests are unaffected.

---

## Open Questions (deferred)

- Git-lfs for goldens if repo size becomes a problem
- Offscreen FBO vs default framebuffer for headless screenshot capture (PBO async readback is preferable but adds complexity)
- Whether `--mode=editor` needs physics/scripting subsystems active for any planned tests
