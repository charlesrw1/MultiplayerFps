# IntegrationTests Module

C++20 coroutine integration test framework. Built as a static lib and `/WHOLEARCHIVE`-linked into `App.exe` so static-init `TestRegistrar` ctors keep tests alive. Running tests / CLI / vars.txt sectioning lives in [[docs/testing.md]] — this file covers what's relevant when editing code *inside* the module.

## Architecture

- **Coroutine task model** — Tests are `TestTask` coroutines (custom `promise_type`). `co_await` on an awaitable suspends the test; `TestRunner` resumes it once per frame from inside the engine loop, so tests run in the real runtime with real GPU/level/tick timing — no threads.
- **Static-init registration** — `GAME_TEST` / `EDITOR_TEST` macros expand to a file-scope `TestRegistrar` whose ctor pushes a `TestEntry` into a global registry. Hence the `/WHOLEARCHIVE` requirement on the App link.
- **Mode-agnostic runner** — One `TestRunner` implementation. The mode (`Game` / `Editor`) is just a filter over the registry plus a different `vars.txt` section selected by `EngineMain` before engine init.
- **Lua phase** — After the C++ phase finishes, the same runner resumes `_lua_run_all_tests` (from `Data/scripts/integration_test_framework.lua`) as a coroutine, ticked per-frame, results merged into the same XML. Glob filters apply to both phases.
- **Awaitables** — `tick(n)`, `seconds(t)`, `wait_for(delegate)`, `load_level(path)`, `screenshot(name)`. Add new ones as small awaitable types in `TestContext.h`; they only need `await_ready/suspend/resume` plus whatever per-frame state the runner pokes at.
- **Failure model** — `check()` records and continues; `require()` throws `TestAbortException` which the promise catches and marks the test failed. Per-test `timeout_seconds` enforced by the runner.

## Screenshot / golden machinery

- `Screenshot.cpp`: `glReadPixels` → stb PNG → pixel-diff vs golden.
- Goldens live in `TestFiles/goldens/` (committed). Output lives in `TestFiles/screenshots/` (gitignored).
- `--promote` writes/overwrites the golden instead of comparing — that's the only way new baselines get created.
- `STB_IMAGE_IMPLEMENTATION` / `STB_IMAGE_WRITE_IMPLEMENTATION` are already defined in `Source/External/External.cpp`. Do NOT redefine them here — just include the headers.

## EditorTestContext

Helpers for editor-mode tests (`entity_count`, `save_level`, `undo`, etc.). Two things that aren't obvious from the header:

- Getting the doc:
  ```cpp
  auto* doc = static_cast<EditorDoc*>(eng_local.editorState->get_tool());
  ```
  pulls `GameEngineLocal.h` and `LevelEditor/EditorDocLocal.h`.
- `save_level(path)` is `set_document_path(path)` + `save_document_internal()`. The path is **game-relative** (e.g. `"TestFiles/tmp.tmap"`), not absolute.

## Conventions / gotchas

- `AdditionalIncludeDirectories` is `$(SolutionDir)Source\`, so include as `"IntegrationTests/TestContext.h"`, never bare `"TestContext.h"`.
- Directory creation uses `_mkdir("TestFiles")` from `<direct.h>`, **not** `std::filesystem` (matches the rest of the engine).
- Test-runner status lines must be prefixed `[TEST]` — that's the sentinel `integration_test.ps1` greps for and the only thing that survives when `-ShowEngineLog` is off. Don't route them through the logger.
- Level API for queries: `eng->get_level()->get_all_objects()` → `const hash_map<BaseUpdater*>&`.
- One XML per mode: `TestFiles/integration_<mode>_results.xml`. Both C++ and Lua phases append into the same file.
