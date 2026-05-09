# Testing

Unit tests (GTest) under `Source/UnitTests/` (`UnitTests.vcxproj`). Integration tests (C++20 coroutines + Lua) under `Source/IntegrationTests/` (`IntegrationTests.vcxproj`).

---

## Running

Unit tests — [[Scripts/build_and_test.ps1]]:
```powershell
powershell Scripts/build_and_test.ps1 -Configuration Debug -Platform x64
```

Integration tests (game pass + editor pass) — [[Scripts/integration_test.ps1]]:
```powershell
powershell Scripts/integration_test.ps1 -Config Debug
```

> `build_and_test.ps1` calls `check_deps` (runs `dumpbin /SYMBOLS`). Don't combine with the integration runner — call `integration_test.ps1` directly.

---

## Integration Test Framework (C++)

C++20 coroutines, two-pass: `--mode=game|editor` selects mode at runtime; same binary.

### Key files

- `ITestRunner.h` — polymorphic interface: `tick(float dt)`, `exit_code()`.
- `TestTask.h` — coroutine task; `TestAbortException` for `require()` failures.
- `TestContext.h/.cpp` — test API: `check()`, `require()`, awaitables for ticks/seconds/delegates/levels/screenshots.
- `TestRegistry.h` — `GAME_TEST(name, timeout, fn)` / `EDITOR_TEST(...)`; static-init registration.
- `GameTestRunner.h/.cpp` — drives `GAME_TEST`s; writes `TestFiles/integration_game_results.xml`.
- `EditorTestRunner.h/.cpp` — drives `EDITOR_TEST`s; writes `TestFiles/integration_editor_results.xml`.
- `EditorTestContext.h/.cpp` — editor helpers: `entity_count()`, `save_level(path)`, `undo()`.
- `Screenshot.h/.cpp` — `screenshot_capture_and_compare()`: glReadPixels → stb PNG → pixel diff vs golden.
- `GpuTimer.h/.cpp` — `ScopedGpuTimer` wrapping GL_TIME_ELAPSED; `GpuTimingLog` writes JSON.
- `main.cpp` — parses `--mode`, creates runner, injects via `g_pending_test_runner`.
- [[Scripts/integration_test.ps1]] — builds, runs both passes, merges JUnit XML.

### Existing tests

- `Tests/Renderer/test_basic.cpp` — `game/boot`, `game/level_load`, `renderer/screenshot_smoke`.
- `Tests/Editor/test_serialize.cpp` — `editor/serialize_round_trip`, `editor/undo_noop`.

### Gotchas

- **STB:** `STB_IMAGE_IMPLEMENTATION` and `STB_IMAGE_WRITE_IMPLEMENTATION` already defined in `Source/External/External.cpp`. Do NOT redefine in `Screenshot.cpp` — just include the headers.
- **Includes:** `AdditionalIncludeDirectories` is `$(SolutionDir)Source\`. Use `"IntegrationTests/TestContext.h"`, not `"TestContext.h"`.
- **EditorDoc access:**
  ```cpp
  auto* doc = static_cast<EditorDoc*>(eng_local.editorState->get_tool());
  ```
  Includes: `GameEngineLocal.h`, `LevelEditor/EditorDocLocal.h`.
- **save_level:** `doc->set_document_path(path)` then `doc->save_document_internal()`. Path is game-relative, e.g. `"TestFiles/tmp.tmap"`.
- **Level API:** `eng->get_level()->get_all_objects()` → `const hash_map<BaseUpdater*>&`.
- **Directory creation:** `_mkdir("TestFiles")` (`<direct.h>`), NOT `std::filesystem`.
- **Screenshots:** goldens in `TestFiles/goldens/` (committed); `TestFiles/screenshots/` is gitignored. `--promote` creates/updates goldens.

---

## Lua Integration Tests

Lua-native coroutine runner. Register with `add_test(name, fn)`; runner ticks in `update()` yielding seconds:

```lua
add_test("my/test", function()
    GameplayStatic.change_level("demo_level_0.tmap")
    coroutine.yield(1.5)  -- wait 1.5s
    assert(condition, "message")
end)
```

Driven by `Application:run_integration_tests()` (called when `g_run_tests=1`). `LuaTestRunner.finish(pass, fail, failures_string)` writes `TestFiles/integration_lua_results.xml` and calls `Quit()`.

To run directly:
```
test_game_vars.txt:
  g_run_tests 1
  g_application_class "FpsGameApplication"
  g_project_base "Data"
```
Then [[Scripts/integration_test.ps1]] or:
```
x64\Debug\App.exe --vars test_game_vars.txt
```

---

## Known Pre-Existing Failures

- `ScriptManagerTest.SyntaxErrorLeavesCleanStack` — flaky, suspected isolation issue.
- `ClassBaseRegistryTest` suite setup — fails, blocks some unit test runs.
