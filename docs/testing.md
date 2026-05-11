# Testing

Unit tests (GTest) under `Source/UnitTests/` (`UnitTests.vcxproj`). Integration tests (C++20 coroutines + Lua) under `Source/IntegrationTests/` (`IntegrationTests.vcxproj`, now built as a static lib and linked into `App.exe` via `/WHOLEARCHIVE`).

---

## Running

Unit tests — [[Scripts/build_and_test.ps1]]:
```powershell
powershell Scripts/build_and_test.ps1 -Configuration Debug -Platform x64
```

Integration tests — always go through [[Scripts/integration_test.ps1]] (it builds + runs both passes and writes the XML). Do not invoke `App.exe --tests` directly unless debugging the runner itself.

Use integration tests as both a full "test suite" and as a way to interactively test out a feature autonomously by writing the test and running it in isolation and ensuring correct output or dumping stuff to disk to evaluate later.

```powershell
# Both passes, all tests
powershell Scripts/integration_test.ps1

# One mode
powershell Scripts/integration_test.ps1 -Mode game
powershell Scripts/integration_test.ps1 -Mode editor

# Filter by glob (matches C++ and Lua names; `*` = any sequence)
powershell Scripts/integration_test.ps1 -Mode game -Pattern "game/boot"
powershell Scripts/integration_test.ps1 -Pattern "renderer/*","racing_line/*"

# Glob list from file (one per line, `#` comments)
powershell Scripts/integration_test.ps1 -PatternFile smoke.txt
```

Switches: `-Config Release` (default Debug), `-Promote` (rewrite screenshot goldens), `-Interactive` (keep window visible), `-TimingAssert` (fail on slow GPU timings), `-ShowEngineLog` (forward App.exe's full engine output to the console — off by default; only `[TEST] ...` sentinel lines pass through), `-Debugger` (attach a running VS 2026 instance to App.exe via DTE; falls back to `vsjitdebugger.exe` if no VS is in the ROT — output filtering is bypassed in this mode). Empty `-Pattern` = all tests for the selected mode(s). `Get-Help Scripts/integration_test.ps1 -Examples` for the same list inline.

The runner always prints a parsed XML summary at the end (per-test PASS/FAIL list) and writes the full uncoloured engine log to `test_<mode>_output.log` next to App.exe's working directory regardless of `-ShowEngineLog`. Log lines are prefixed `[Error]`, `[Warning]`, `[Debug]`, `[Info]` so they grep cleanly. Test-runner status lines (`[TEST] ==> [N/M] ...`, `[TEST]   PASS/FAIL ...`, `[TEST] === <mode> Tests: ... ===`) are emitted on stdout/stderr only — they don't go through the logger and don't appear in the .log file.

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

## Repo health check — `check_codebase.ps1`

[[Scripts/check_codebase.ps1]] is the single-shot quality gate. Sections, in order:

By default: skip code coverage! with `-Quick` only check code coverage on request.

1. **`docs.exe check`** — wiki-link + `@docs` ref validation + freshness check on doc→source links. Errors fail the run; freshness issues (`stale` / `unblessed` / `orphan`) are warnings only. See [[tooling/docs-cli#Freshness]].
2. **LOC per file** — warn >600, error >1000. Excludes `Source/External`, `.generated`, `MEGA.cpp`.
3. **OpenCppCoverage** — builds App.exe, runs integration tests for `game` + `editor`, writes `TestFiles/coverage/coverage_<mode>.xml` (Cobertura) + `TestFiles/coverage/coverage_summary.md` (per-file matrix sorted ascending by best mode). Skipped if OpenCppCoverage isn't installed (`winget install OpenCppCoverage.OpenCppCoverage`).
4. **TODO/FIXME/HACK scan** — `rg`-based, warning only.

```powershell
# Full run (includes coverage build + both test passes — slow)
powershell Scripts/check_codebase.ps1

# Skip coverage
powershell Scripts/check_codebase.ps1 -Quick

# Granular skips
powershell Scripts/check_codebase.ps1 -SkipDocs -SkipLoc -SkipCoverage -SkipTodos
```

Exit 0 = no errors. Exit 1 = at least one section errored. Warnings never fail. The summary block at the end lists each section's status (`ok` / `warn` / `error` / `skip`).

### Viewing coverage in VSCode

Install the **Coverage Gutters** extension (`ryanluker.vscode-coverage-gutters`). `.vscode/settings.json` already points it at `TestFiles/coverage/` and picks up `coverage_editor.xml` first.

- Command Palette → **Coverage Gutters: Display Coverage** — shows gutter marks + status-bar %.
- **Coverage Gutters: Watch** — auto-refreshes when XMLs change.
- To swap editor↔game XML: re-order `coverage-gutters.coverageFileNames` (first match wins), then **Remove Coverage** + **Display Coverage**.

Pretty HTML report: `py Scripts/coverage_to_html.py` writes `TestFiles/coverage/coverage_summary.html` (filterable, per-module).

---

## Known Pre-Existing Failures

- `ScriptManagerTest.SyntaxErrorLeavesCleanStack` — flaky, suspected isolation issue.
- `ClassBaseRegistryTest` suite setup — fails, blocks some unit test runs.
