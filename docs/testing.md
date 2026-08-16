# Testing

Unit tests use GTest and live under `Source/UnitTests/` (`UnitTests.vcxproj`).

Integration tests use C++20 coroutines and Lua, and live under `Source/IntegrationTests/` (`IntegrationTests.vcxproj`). This project builds as a static lib. `App.exe` links it via `/WHOLEARCHIVE`.

---

## Running tests

Run unit tests with [[Scripts/build_and_test.ps1]]:
```powershell
powershell Scripts/build_and_test.ps1 -Configuration Debug -Platform x64
```

Run integration tests only through [[Scripts/integration_test.ps1]]. It builds the project, runs both test passes, and writes the results XML. Do not call `App.exe --tests` directly, unless you are debugging the test runner itself.

Use integration tests two ways: as the full test suite, and as a way to test one feature in isolation. For the second use, write a test that checks the output directly, or dumps data to disk for you to inspect later.

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

Switches:
- `-Config Release` — default is Debug.
- `-Promote` — rewrite screenshot goldens.
- `-Interactive` — keep the window visible.
- `-TimingAssert` — fail the test on slow GPU timings.
- `-ShowEngineLog` — forward the full engine output from `App.exe` to the console. Off by default. Normally only `[TEST] ...` sentinel lines pass through.
- `-Debugger` — attach a running VS 2026 instance to `App.exe` via DTE. Falls back to `vsjitdebugger.exe` if no VS instance is in the ROT. This mode bypasses output filtering.

An empty `-Pattern` runs all tests for the selected mode or modes. Run `Get-Help Scripts/integration_test.ps1 -Examples` for the same list inline.

The runner always prints a parsed XML summary at the end: a per-test PASS/FAIL list. It also writes the full, uncoloured engine log to `Logs/test_<mode>_output.log`, regardless of `-ShowEngineLog`. Log lines carry a prefix — `[Error]`, `[Warning]`, `[Debug]`, or `[Info]` — so you can grep them cleanly. Test-runner status lines (`[TEST] ==> [N/M] ...`, `[TEST]   PASS/FAIL ...`, `[TEST] === <mode> Tests: ... ===`) go to stdout/stderr only. They do not pass through the logger, so they do not appear in the `.log` file.

> `build_and_test.ps1` calls `check_deps`, which runs `dumpbin /SYMBOLS`. Do not combine this with the integration runner. Call `integration_test.ps1` directly instead.

---

## How the test runner is wired

- `Source/App/App.vcxproj` is the only test and runtime executable. It links `Core.lib` and `IntegrationTests.lib` with `/WHOLEARCHIVE`, so the static-init `TestRegistrar` constructors are kept. Tests register themselves at startup.
- `game_engine_main` (in `Source/EngineMain.cpp`) parses `--tests <mode> [pattern...]` before engine init. It configures the vars-file section and the log file, then constructs a `TestRunner`. The engine loop pumps this runner once per frame.
- `EngineVars.ini` has three sections: `[app]` for a normal launch, `[game_test]` for `--tests game`, and `[editor_test]` for `--tests editor`. Sections are independent. There is no fall-through between them, so duplicate any var that must apply in more than one mode. The engine selects exactly one section, through `Cmd_Manager::execute_file_section`. `--tests` runs never load a per-project `.ini` (unlike `[app]`). Instead, `game_test` and `editor_test` set vars such as `g_project_base` directly in their own section.

---

## Integration test framework (C++)

The framework uses C++20 coroutines. It has two passes, game and editor, selected by `--tests <mode>`, in a single binary.

### Key files

- `ITestRunner.h` — the polymorphic interface: `tick(float dt)`, `exit_code()`.
- `TestTask.h` — the coroutine task. `TestAbortException` signals a `require()` failure.
- `TestContext.h/.cpp` — the test API: `check()`, `require()`, and awaitables for ticks, seconds, delegates, levels, and screenshots.
- `TestRegistry.h` — `GAME_TEST(name, timeout, fn)` and `EDITOR_TEST(...)`, registered at static init. `get_filtered(mode, patterns)` accepts a glob list.
- `TestRunner.h/.cpp` — the single, mode-agnostic runner. It drives the C++ tests, then the Lua phase, then writes one XML file.
- `EditorTestContext.h/.cpp` — editor helpers: `entity_count()`, `save_level(path)`, `undo()`.
- `Screenshot.h/.cpp` — `screenshot_capture_and_compare()`. It reads pixels with `glReadPixels`, encodes them with stb as a PNG, then diffs the pixels against a golden image.
- `GpuTimer.h/.cpp` — `ScopedGpuTimer` wraps `GL_TIME_ELAPSED`. `GpuTimingLog` writes the result as JSON.
- [[Scripts/integration_test.ps1]] — builds the project and runs both passes.

### Existing tests

- `Tests/Renderer/test_basic.cpp` — `game/boot`, `game/level_load`, `renderer/screenshot_smoke`.
- `Tests/Editor/test_serialize.cpp` — `editor/serialize_round_trip`, `editor/undo_noop`.

### Gotchas

- **STB:** `STB_IMAGE_IMPLEMENTATION` and `STB_IMAGE_WRITE_IMPLEMENTATION` are already defined in `Source/External/External.cpp`. Do not redefine them in `Screenshot.cpp`. Just include the headers.
- **Includes:** `AdditionalIncludeDirectories` is `$(SolutionDir)Source\`. Use `"IntegrationTests/TestContext.h"`, not `"TestContext.h"`.
- **EditorDoc access:**
  ```cpp
  auto* doc = static_cast<EditorDoc*>(eng_local.editorState->get_tool());
  ```
  Include `GameEngineLocal.h` and `LevelEditor/EditorDocLocal.h`.
- **save_level:** call `doc->set_document_path(path)`, then `doc->save_document_internal()`. The path is game-relative, for example `"TestFiles/tmp.tmap"`.
- **Level API:** `eng->get_level()->get_all_objects()` returns `const hash_map<BaseUpdater*>&`.
- **Directory creation:** use `_mkdir("TestFiles")` from `<direct.h>`. Do not use `std::filesystem`.
- **Screenshots:** goldens live in `TestFiles/goldens/` and are committed. `TestFiles/screenshots/` is gitignored. `--promote` creates or updates the goldens.

---

## Lua integration tests

Lua tests live under `Data/scripts/tests/**/*.lua`. Like the rest of `Data/`, this path is gitignored. The engine loads these files only when `--tests` is set. In a normal run, the engine skips the directory, so `add_test()` calls never fire.

Each file calls `add_test(name, fn)` at file scope. The function runs as a coroutine. `coroutine.yield(seconds)` waits for that many seconds. An error raised inside the function fails the test.

```lua
add_test("smoke/level_loads", function()
    GameplayStatic.change_level("demo_level_0.tmap")
    coroutine.yield(1.5)
    assert(GameplayStatic.find_by_name("player") ~= nil, "player should exist after load")
end)
```

The framework lives in `Data/scripts/integration_test_framework.lua`. The C++ `TestRunner` runs the C++ tests first. It then resumes `_lua_run_all_tests`, defined by the framework, inside a coroutine. It ticks this coroutine each frame and honours any yielded wait time. Each Lua test reports its result through `LuaTestRunner.report(name, passed, msg)`. `LuaTestRunner.set_done()` ends the Lua phase. Both phases append to the same file: `TestFiles/integration_<mode>_results.xml`.

`--tests` patterns filter Lua tests too, using the same glob syntax matched against the test name.

---

## Repo health check — `check_codebase.ps1`

[[Scripts/check_codebase.ps1]] is the single-shot quality gate. It runs these sections, in order:

By default, the script skips code coverage. Pass `-Quick` to check coverage only when you ask for it explicitly.

1. **`docs.exe check`** — validates wiki-links and `@docs` references, and checks the freshness of doc-to-source links. Errors fail the run. Freshness issues (`stale`, `unblessed`, `orphan`) are warnings only. See [[tooling/docs-cli#Freshness]].
2. **LOC per file** — warns above 600 lines, errors above 1000. Excludes `Source/External`, `.generated`, and `MEGA.cpp`.
3. **OpenCppCoverage** — builds `App.exe`, runs the integration tests for `game` and `editor`, then writes `TestFiles/coverage/coverage_<mode>.xml` (Cobertura format) and `TestFiles/coverage/coverage_summary.md` (a per-file matrix, sorted ascending by best mode). This section is skipped if OpenCppCoverage is not installed. Install it with `winget install OpenCppCoverage.OpenCppCoverage`.
4. **TODO/FIXME/HACK scan** — uses `rg`. Warning only.

```powershell
# Full run (includes coverage build + both test passes — slow)
powershell Scripts/check_codebase.ps1

# Skip coverage
powershell Scripts/check_codebase.ps1 -Quick

# Granular skips
powershell Scripts/check_codebase.ps1 -SkipDocs -SkipLoc -SkipCoverage -SkipTodos
```

Exit code 0 means no errors. Exit code 1 means at least one section errored. Warnings never cause a failure. The summary block at the end lists each section's status: `ok`, `warn`, `error`, or `skip`.

### Viewing coverage in VS Code

Install the **Coverage Gutters** extension (`ryanluker.vscode-coverage-gutters`). `.vscode/settings.json` already points it at `TestFiles/coverage/` and prefers `coverage_editor.xml`.

- Open the Command Palette and run **Coverage Gutters: Display Coverage** to show gutter marks and the status-bar percentage.
- Run **Coverage Gutters: Watch** to auto-refresh when the XML files change.
- To swap between the editor and game XML, re-order `coverage-gutters.coverageFileNames` so your preferred file matches first. Then run **Remove Coverage**, followed by **Display Coverage**.

For a readable HTML report, run `py [[Scripts/coverage_to_html.py]]`. It writes `TestFiles/coverage/coverage_summary.html`, which you can filter per module.

---

## Known pre-existing failures

- `ScriptManagerTest.SyntaxErrorLeavesCleanStack` — flaky. Suspected test-isolation issue.
- `ClassBaseRegistryTest` suite setup — fails, and blocks some unit test runs.
