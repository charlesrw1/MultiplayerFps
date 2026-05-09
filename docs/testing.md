# Testing

Unit tests (GTest) under `Source/UnitTests/` (`UnitTests.vcxproj`). Integration tests (C++20 coroutines + Lua) under `Source/IntegrationTests/` (`IntegrationTests.vcxproj`, now built as a static lib and linked into `App.exe` via `/WHOLEARCHIVE`).

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

Or invoke `App.exe` directly:
```
x64\Debug\App.exe --tests game [pattern...]
x64\Debug\App.exe --tests editor [pattern...]
```

`<pattern>` is a glob (`*` matches any sequence). Empty = all tests for the mode. Patterns starting with `@` read newline-separated globs from a file (`#` for comments). Mix and repeat freely:
```
App.exe --tests game racing_line/* renderer/*
App.exe --tests game @smoke.txt
App.exe --tests game racing_line/smoke @extra.txt
```

Other CLI flags: `--promote` (write current screenshots as new goldens), `--interactive` (keep window visible), `--timing-assert` (fail on slow GPU timings).

> `build_and_test.ps1` calls `check_deps` (runs `dumpbin /SYMBOLS`). Don't combine with the integration runner — call `integration_test.ps1` directly.

---

## How the test runner is wired

- `Source/App/App.vcxproj` is the only test/runtime executable. It links `Core.lib` and `IntegrationTests.lib` with `/WHOLEARCHIVE` so static-init `TestRegistrar` constructors are kept and tests register themselves at startup.
- `game_engine_main` (in `Source/EngineMain.cpp`) parses `--tests <mode> [pattern...]` before engine init, configures the vars file section + log file, then constructs a `TestRunner` that the engine loop pumps once per frame.
- `vars.txt` is sectioned: `[app]` for normal launch, `[game_test]` for `--tests game`, `[editor_test]` for `--tests editor`. Sections are independent — there is no fall-through, so duplicate every var that needs to apply in multiple modes. The engine selects exactly one section via `Cmd_Manager::execute_file_section`.

---

## Integration Test Framework (C++)

C++20 coroutines, two passes (game / editor) selected by `--tests <mode>`, single binary.

### Key files

- `ITestRunner.h` — polymorphic interface: `tick(float dt)`, `exit_code()`.
- `TestTask.h` — coroutine task; `TestAbortException` for `require()` failures.
- `TestContext.h/.cpp` — test API: `check()`, `require()`, awaitables for ticks/seconds/delegates/levels/screenshots.
- `TestRegistry.h` — `GAME_TEST(name, timeout, fn)` / `EDITOR_TEST(...)`; static-init registration. `get_filtered(mode, patterns)` accepts a glob list.
- `TestRunner.h/.cpp` — single mode-agnostic runner. Drives C++ tests, then the Lua phase, then writes one XML.
- `EditorTestContext.h/.cpp` — editor helpers: `entity_count()`, `save_level(path)`, `undo()`.
- `Screenshot.h/.cpp` — `screenshot_capture_and_compare()`: glReadPixels → stb PNG → pixel diff vs golden.
- `GpuTimer.h/.cpp` — `ScopedGpuTimer` wrapping GL_TIME_ELAPSED; `GpuTimingLog` writes JSON.
- [[Scripts/integration_test.ps1]] — builds, runs both passes.

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

Tests live under `Data/scripts/tests/**/*.lua` (gitignored along with the rest of `Data/`). The engine only loads them when `--tests` is set; in normal runs the directory is skipped so `add_test()` calls don't fire.

Each file calls `add_test(name, fn)` at file scope. The function is a coroutine — `coroutine.yield(seconds)` waits, raising an error fails the test:

```lua
add_test("smoke/level_loads", function()
    GameplayStatic.change_level("demo_level_0.tmap")
    coroutine.yield(1.5)
    assert(GameplayStatic.find_by_name("player") ~= nil, "player should exist after load")
end)
```

The framework lives in `Data/scripts/integration_test_framework.lua`. The C++ `TestRunner` runs C++ tests first, then resumes `_lua_run_all_tests` (defined by the framework) inside a coroutine, ticking it each frame and respecting yielded wait seconds. Per-test results flow back via `LuaTestRunner.report(name, passed, msg)`; `LuaTestRunner.set_done()` ends the phase. Both phases append into the same `TestFiles/integration_<mode>_results.xml`.

`--tests` patterns filter Lua tests too (same glob syntax, matched against the test name).

---

## Known Pre-Existing Failures

- `ScriptManagerTest.SyntaxErrorLeavesCleanStack` — flaky, suspected isolation issue.
- `ClassBaseRegistryTest` suite setup — fails, blocks some unit test runs.
