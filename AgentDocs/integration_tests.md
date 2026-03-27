# Integration Test System

## Overview

C++20 coroutine-based integration test framework. Two-pass: game mode and editor mode.
Located in `Source/IntegrationTests/`.

## Key Files

- `ITestRunner.h` — polymorphic interface with `tick(float dt)` and `exit_code()`
- `TestTask.h` — coroutine task type; `TestAbortException` for `require()` failures
- `TestContext.h/.cpp` — test API: `check()`, `require()`, awaitables for ticks/seconds/delegates/levels/screenshots
- `TestRegistry.h` — `GAME_TEST(name, timeout, fn)` / `EDITOR_TEST(name, timeout, fn)` macros; static-init registration
- `GameTestRunner.h/.cpp` — drives GAME_TEST coroutines, writes `TestFiles/integration_game_results.xml`
- `EditorTestRunner.h/.cpp` — drives EDITOR_TEST coroutines, writes `TestFiles/integration_editor_results.xml`
- `EditorTestContext.h/.cpp` — editor helpers: `entity_count()`, `save_level(path)`, `undo()`
- `Screenshot.h/.cpp` — `screenshot_capture_and_compare()`: glReadPixels → stb PNG → pixel diff vs golden
- `GpuTimer.h/.cpp` — `ScopedGpuTimer` wrapping GL_TIME_ELAPSED; `GpuTimingLog` writes JSON
- `main.cpp` — parses `--mode=game|editor`, creates runner, injects via `g_pending_test_runner`
- `Scripts/integration_test.ps1` — builds, runs both passes, merges JUnit XML

## Test Files

- `Source/IntegrationTests/Tests/Renderer/test_basic.cpp` — game/boot, game/level_load, renderer/screenshot_smoke
- `Source/IntegrationTests/Tests/Editor/test_serialize.cpp` — editor/serialize_round_trip, editor/undo_noop

## Important Details

**STB:** `STB_IMAGE_IMPLEMENTATION` and `STB_IMAGE_WRITE_IMPLEMENTATION` are already defined in `Source/External/External.cpp`. Do NOT redefine them in Screenshot.cpp — just include the headers.

**Include paths:** IntegrationTests' `AdditionalIncludeDirectories` is `$(SolutionDir)Source\`. Use `"IntegrationTests/TestContext.h"` not `"TestContext.h"`.

**EditorDoc access:** Follow the existing engine pattern:
```cpp
auto* doc = static_cast<EditorDoc*>(eng_local.editorState->get_tool());
```
Includes needed: `GameEngineLocal.h`, `LevelEditor/EditorDocLocal.h`.

**save_level:** Call `doc->set_document_path(path)` then `doc->save_document_internal()`. Path is game-relative (e.g. `"TestFiles/tmp.tmap"`).

**Level API:** `eng->get_level()->get_all_objects()` returns `const hash_map<BaseUpdater*>&`.

**Directory creation:** Use `_mkdir("TestFiles")` (Windows; `<direct.h>`), not `std::filesystem`.

**Screenshots:** Goldens live in `TestFiles/goldens/` (committed). `TestFiles/screenshots/` is gitignored. Use `--promote` flag to create/update goldens.

**No build_and_test.ps1:** That script calls `check_deps` which runs `dumpbin /SYMBOLS`. Don't invoke it. Use `integration_test.ps1` instead.

**VS project:** `Source/IntegrationTests/IntegrationTests.vcxproj`. Both game and editor mode build from the same binary; mode selected at runtime via `--mode=`.
