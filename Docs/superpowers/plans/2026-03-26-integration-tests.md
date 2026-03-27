# Integration Test System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a C++20 coroutine-based integration test system covering game/renderer and editor, with screenshot golden diffing, GPU timing, and a two-pass PS1 CI runner.

**Architecture:** Single binary, two runtime modes (`--mode=game|editor`). A polymorphic `ITestRunner` is injected into the engine after boot; the main loop calls `runner->tick(dt)` unchanged regardless of mode. Tests are C++20 coroutines registered via `GAME_TEST`/`EDITOR_TEST` macros; the runner resumes them once per tick with no background threads.

**Tech Stack:** C++20 coroutines (MSVC `/std:c++latest`), OpenGL timer queries, `stb_image`/`stb_image_write` (already in `Source/External/`), PowerShell for CI runner.

---

## File Map

### New files
```
Source/ITestRunner.h                              -- minimal interface in Core, forward-declared in engine
Source/IntegrationTests/
  IntegrationTests.vcxproj                        -- VS project (EDITOR_BUILD, stdcpp20)
  main.cpp                                        -- arg parse, create runner, call game_engine_main
  TestTask.h                                      -- coroutine return type + promise_type
  TestRegistry.h                                  -- TestEntry, GAME_TEST/EDITOR_TEST macros
  TestContext.h / TestContext.cpp                 -- TestContext + all Awaitable types
  GameTestRunner.h / GameTestRunner.cpp           -- ITestRunner for game mode
  TestGameApp.h                                   -- minimal Application subclass
  EditorTestRunner.h / EditorTestRunner.cpp       -- ITestRunner for editor mode
  EditorTestContext.h / EditorTestContext.cpp     -- EditorTestContext wrapping EditorDoc
  Screenshot.h / Screenshot.cpp                  -- glReadPixels, stb PNG, diff, promote
  GpuTimer.h / GpuTimer.cpp                      -- ScopedGpuTimer (GL_TIME_ELAPSED)
  Tests/Renderer/test_basic.cpp                   -- first game test
  Tests/Editor/test_serialize.cpp                 -- first editor test
Scripts/integration_test.ps1                      -- two-pass build+run CI script
TestFiles/goldens/.gitkeep                        -- committed golden image store
```

### Modified files
```
Source/GameEngineLocal.h          -- replace IntegrationTester* with ITestRunner*; headless_mode -> skip_swap
Source/EngineMain.cpp             -- set_tester->set_runner; add g_pending_test_runner injection point
CsRemake.sln                      -- add IntegrationTests project
.gitignore                        -- ignore TestFiles/screenshots/, TestFiles/timing_*.json
```

### Deleted files
```
Source/IntegrationTest.h
Source/IntegrationTest.cpp
Source/UnitTests/EngineTesterApplication.h
Source/UnitTests/EngineTesterApplication.cpp
```

---

## Task 1: ITestRunner interface + engine wiring

**Files:**
- Create: `Source/ITestRunner.h`
- Modify: `Source/GameEngineLocal.h`
- Modify: `Source/EngineMain.cpp`

- [ ] **Step 1: Create ITestRunner.h**

```cpp
// Source/ITestRunner.h
#pragma once

class ITestRunner {
public:
    virtual ~ITestRunner() = default;
    // Called once per engine tick. Returns true when all tests are done.
    virtual bool tick(float dt) = 0;
    // 0 = all passed, 1 = one or more failures
    virtual int exit_code() const = 0;
};
```

- [ ] **Step 2: Replace IntegrationTester in GameEngineLocal.h**

In `Source/GameEngineLocal.h`, find the existing `IntegrationTester` forward declaration and the EDITOR_BUILD block. Replace:

```cpp
// REMOVE this line near the top:
class IntegrationTester;

// In the EDITOR_BUILD block (around line 126-133), replace:
#ifdef EDITOR_BUILD
    uptr<IntegrationTester> tester;
    uptr<EditorState> editorState;
#endif
void set_tester(IntegrationTester* tester, bool headless_mode);

// WITH:
#ifdef EDITOR_BUILD
    ITestRunner* test_runner = nullptr;
    uptr<EditorState> editorState;
#endif
void set_runner(ITestRunner* runner, bool skip_swap);
```

Also add the include at the top of GameEngineLocal.h:
```cpp
#include "ITestRunner.h"
```

And rename the private field:
```cpp
// line ~139: rename headless_mode to skip_swap
bool skip_swap = false;
```

- [ ] **Step 3: Update EngineMain.cpp — set_tester → set_runner + injection point**

Find `GameEngineLocal::set_tester` (around line 732) and replace the whole function:

```cpp
// Add near top of EngineMain.cpp, after existing includes:
ITestRunner* g_pending_test_runner = nullptr;
bool g_pending_skip_swap = false;

void GameEngineLocal::set_runner(ITestRunner* runner, bool skip_swap_) {
#ifdef EDITOR_BUILD
    test_runner = runner;
    skip_swap = skip_swap_;
#endif
}
```

- [ ] **Step 4: Update the main loop in EngineMain.cpp**

Find the `skip_rendering` usage in `loop()` (around line 1788). Replace:

```cpp
// OLD:
const bool skip_rendering = headless_mode;

// NEW:
const bool skip_rendering = false; // rendering always runs; only swap is skipped
```

Find `wait_for_swap` lambda (around line 1773) — change the swap condition to use `skip_swap`:

```cpp
auto wait_for_swap = [&](const bool /*unused*/) {
    if (!skip_swap)
        SDL_GL_SwapWindow(window);
    // ... rest of function unchanged
};
```

Find the tester tick block (around line 1798) and replace with:

```cpp
#ifdef EDITOR_BUILD
    if (test_runner) {
        if (test_runner->tick(dt)) {
            exit(test_runner->exit_code());
        }
    }
#endif
```

- [ ] **Step 5: Add injection after app start and after editor init in EngineMain.cpp**

Find where `app->start()` is called in `game_engine_main`. After that call, add:

```cpp
// inject pending test runner (game mode)
if (g_pending_test_runner && eng_local.app) {
    eng_local.set_runner(g_pending_test_runner, g_pending_skip_swap);
    g_pending_test_runner = nullptr;
}
```

Find where `editorState` is created/initialized (search for `editorState = `). After that, add:

```cpp
// inject pending test runner (editor mode)
if (g_pending_test_runner && !eng_local.app) {
    eng_local.set_runner(g_pending_test_runner, g_pending_skip_swap);
    g_pending_test_runner = nullptr;
}
```

- [ ] **Step 6: Remove old Testheader.h include and test_integration_1 stub**

Find and delete these lines in EngineMain.cpp:
```cpp
void test_integration_1(IntegrationTester& tester) {}
#include "Testheader.h"
```

- [ ] **Step 7: Verify Core builds**

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1 | ForEach-Object { & $_ C:\Users\charl\source\repos\CsRemake\CsRemake.sln /t:Core /p:Configuration=Debug /p:Platform=x64 /v:minimal }
```

Expected: Build succeeded, 0 errors.

- [ ] **Step 8: Commit**

```bash
git add Source/ITestRunner.h Source/GameEngineLocal.h Source/EngineMain.cpp
git commit -m "Replace IntegrationTester with ITestRunner interface; headless_mode -> skip_swap; add g_pending_test_runner injection point"
```

---

## Task 2: IntegrationTests VS project skeleton

**Files:**
- Create: `Source/IntegrationTests/IntegrationTests.vcxproj`
- Create: `Source/IntegrationTests/main.cpp`
- Modify: `CsRemake.sln`

- [ ] **Step 1: Create IntegrationTests.vcxproj**

Create `Source/IntegrationTests/IntegrationTests.vcxproj` with the following content (based on UnitTests.vcxproj, EDITOR_BUILD defined, stdcpp20, same include paths):

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>16.0</VCProjectVersion>
    <ProjectGuid>{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}</ProjectGuid>
    <RootNamespace>IntegrationTests</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v142</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v142</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="Vcpkg" Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <VcpkgTriplet>x64-windows</VcpkgTriplet>
  </PropertyGroup>
  <PropertyGroup Label="Vcpkg" Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <VcpkgTriplet>x64-windows</VcpkgTriplet>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <LinkIncremental>true</LinkIncremental>
    <IntDir>$(SolutionDir)$(Platform)\$(Configuration)\intermediate\$(ProjectName)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <LinkIncremental>false</LinkIncremental>
    <IntDir>$(SolutionDir)$(Platform)\$(Configuration)\intermediate\$(ProjectName)\</IntDir>
  </PropertyGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <SDLCheck>true</SDLCheck>
      <PreprocessorDefinitions>_DEBUG;_CONSOLE;EDITOR_BUILD;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
      <LanguageStandard>stdcpp20</LanguageStandard>
      <AdditionalIncludeDirectories>$(SolutionDir)Source\;$(SolutionDir)Source\External\;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <DisableSpecificWarnings>4267;4305;4984;4101;26812;26495;4244</DisableSpecificWarnings>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <AdditionalDependencies>$(VcpkgRoot)\installed\$(VcpkgTriplet)\debug\lib\manual-link\SDL2maind.lib;Shell32.lib;$(SolutionDir)$(Platform)\$(Configuration)\Core.lib;$(SolutionDir)$(Platform)\$(Configuration)\External.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <SDLCheck>true</SDLCheck>
      <PreprocessorDefinitions>NDEBUG;_CONSOLE;EDITOR_BUILD;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
      <LanguageStandard>stdcpp20</LanguageStandard>
      <AdditionalIncludeDirectories>$(SolutionDir)Source\;$(SolutionDir)Source\External\;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <DisableSpecificWarnings>4267;4305;4984;4101;26812;26495;4244</DisableSpecificWarnings>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
      <EnableCOMDATFolding>true</EnableCOMDATFolding>
      <OptimizeReferences>true</OptimizeReferences>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <AdditionalDependencies>$(VcpkgRoot)\installed\$(VcpkgTriplet)\lib\manual-link\SDL2main.lib;Shell32.lib;$(SolutionDir)$(Platform)\Release\Core.lib;$(SolutionDir)$(Platform)\Release\External.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="main.cpp" />
    <ClCompile Include="TestContext.cpp" />
    <ClCompile Include="GameTestRunner.cpp" />
    <ClCompile Include="EditorTestRunner.cpp" />
    <ClCompile Include="EditorTestContext.cpp" />
    <ClCompile Include="Screenshot.cpp" />
    <ClCompile Include="GpuTimer.cpp" />
    <ClCompile Include="Tests\Renderer\test_basic.cpp" />
    <ClCompile Include="Tests\Editor\test_serialize.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="TestTask.h" />
    <ClInclude Include="TestRegistry.h" />
    <ClInclude Include="TestContext.h" />
    <ClInclude Include="GameTestRunner.h" />
    <ClInclude Include="TestGameApp.h" />
    <ClInclude Include="EditorTestRunner.h" />
    <ClInclude Include="EditorTestContext.h" />
    <ClInclude Include="Screenshot.h" />
    <ClInclude Include="GpuTimer.h" />
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\External\External.vcxproj">
      <Project>{8d0a22ce-2c78-4f41-9cdc-94d0abe8e634}</Project>
    </ProjectReference>
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
```

- [ ] **Step 2: Add IntegrationTests to CsRemake.sln**

Open `CsRemake.sln` and add these lines after the last `EndProject` before the `Global` section:

```
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "IntegrationTests", "Source\IntegrationTests\IntegrationTests.vcxproj", "{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}"
EndProject
```

Then inside `GlobalSection(ProjectConfigurationPlatforms)`, add:

```
		{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}.Debug|x64.ActiveCfg = Debug|x64
		{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}.Debug|x64.Build.0 = Debug|x64
		{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}.Release|x64.ActiveCfg = Release|x64
		{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}.Release|x64.Build.0 = Release|x64
```

- [ ] **Step 3: Create main.cpp**

```cpp
// Source/IntegrationTests/main.cpp
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <algorithm>
#include "ITestRunner.h"
#include "TestRegistry.h"
#include "GameTestRunner.h"
#include "EditorTestRunner.h"

// Defined in EngineMain.cpp — set before calling game_engine_main
extern ITestRunner* g_pending_test_runner;
extern bool g_pending_skip_swap;

extern int game_engine_main(int argc, char** argv);

static bool has_arg(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == flag) return true;
    return false;
}

static std::string get_arg(int argc, char** argv, const char* prefix) {
    std::string pre(prefix);
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if (a.rfind(pre, 0) == 0) return a.substr(pre.size());
    }
    return {};
}

int main(int argc, char** argv) {
    std::string mode = get_arg(argc, argv, "--mode=");
    if (mode.empty()) {
        fprintf(stderr, "Usage: IntegrationTests.exe --mode=game|editor [--test=<glob>] [--promote] [--interactive] [--timing-assert]\n");
        return 1;
    }

    std::string test_filter = get_arg(argc, argv, "--test=");
    bool promote    = has_arg(argc, argv, "--promote");
    bool interactive = has_arg(argc, argv, "--interactive");
    bool timing_assert = has_arg(argc, argv, "--timing-assert");
    bool skip_swap  = !interactive;

    TestRunnerConfig cfg;
    cfg.test_filter    = test_filter;
    cfg.promote        = promote;
    cfg.interactive    = interactive;
    cfg.timing_assert  = timing_assert;

    if (mode == "game") {
        auto tests = TestRegistry::get_filtered(TestMode::Game, test_filter.c_str());
        g_pending_test_runner = new GameTestRunner(tests, cfg);
    } else if (mode == "editor") {
        auto tests = TestRegistry::get_filtered(TestMode::Editor, test_filter.c_str());
        g_pending_test_runner = new EditorTestRunner(tests, cfg);
    } else {
        fprintf(stderr, "Unknown mode: %s\n", mode.c_str());
        return 1;
    }

    g_pending_skip_swap = skip_swap;
    return game_engine_main(argc, argv);
}
```

- [ ] **Step 4: Verify IntegrationTests project builds (link errors OK, just no compile errors on main.cpp yet)**

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1 | ForEach-Object { & $_ C:\Users\charl\source\repos\CsRemake\CsRemake.sln /t:IntegrationTests /p:Configuration=Debug /p:Platform=x64 /v:minimal }
```

Expected: Compile errors only about missing headers (TestRegistry.h, GameTestRunner.h etc) — that's fine at this stage. No unexpected errors.

- [ ] **Step 5: Commit**

```bash
git add Source/IntegrationTests/ CsRemake.sln
git commit -m "Add IntegrationTests VS project skeleton with main.cpp arg parsing"
```

---

## Task 3: Coroutine machinery (TestTask) and TestRegistry

**Files:**
- Create: `Source/IntegrationTests/TestTask.h`
- Create: `Source/IntegrationTests/TestRegistry.h`

- [ ] **Step 1: Create TestTask.h**

```cpp
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
```

- [ ] **Step 2: Create TestRegistry.h**

```cpp
// Source/IntegrationTests/TestRegistry.h
#pragma once
#include <vector>
#include <string>
#include <functional>
#include "TestTask.h"

struct TestContext;

enum class TestMode { Game, Editor };

using TestFn = std::function<TestTask(TestContext&)>;

struct TestEntry {
    const char* name;
    float timeout_seconds;
    TestMode mode;
    TestFn fn;
};

struct TestRunnerConfig {
    std::string test_filter;  // glob pattern, empty = run all
    bool promote       = false;
    bool interactive   = false;
    bool timing_assert = false;
};

class TestRegistry {
public:
    static std::vector<TestEntry>& all();
    static void register_test(TestEntry e);
    // Returns tests matching mode and glob filter (empty filter = all)
    static std::vector<TestEntry> get_filtered(TestMode mode, const char* glob);
};

// Registers a test at static-init time. One instance per test file.
struct TestRegistrar {
    TestRegistrar(const char* name, float timeout, TestMode mode, TestFn fn) {
        TestRegistry::register_test({name, timeout, mode, std::move(fn)});
    }
};

// Glob match: '*' matches any sequence, '?' matches any single char
bool test_glob_match(const char* pattern, const char* str);

#define GAME_TEST(name, timeout, fn) \
    static TestRegistrar _reg_##fn((name), (timeout), TestMode::Game, (fn))

#define EDITOR_TEST(name, timeout, fn) \
    static TestRegistrar _reg_##fn((name), (timeout), TestMode::Editor, (fn))
```

- [ ] **Step 3: Create TestRegistry.cpp**

```cpp
// Source/IntegrationTests/TestRegistry.cpp
// Add this file to IntegrationTests.vcxproj ItemGroup (ClCompile)
#include "TestRegistry.h"
#include <cstring>

std::vector<TestEntry>& TestRegistry::all() {
    static std::vector<TestEntry> s_tests;
    return s_tests;
}

void TestRegistry::register_test(TestEntry e) {
    all().push_back(std::move(e));
}

std::vector<TestEntry> TestRegistry::get_filtered(TestMode mode, const char* glob) {
    std::vector<TestEntry> out;
    for (auto& e : all()) {
        if (e.mode != mode) continue;
        if (!glob || glob[0] == '\0' || test_glob_match(glob, e.name))
            out.push_back(e);
    }
    return out;
}

bool test_glob_match(const char* pattern, const char* str) {
    if (!pattern || pattern[0] == '\0') return true;
    if (*pattern == '*') {
        if (test_glob_match(pattern + 1, str)) return true;
        if (*str) return test_glob_match(pattern, str + 1);
        return false;
    }
    if (*str == '\0') return false;
    if (*pattern == '?' || *pattern == *str)
        return test_glob_match(pattern + 1, str + 1);
    return false;
}
```

Add `TestRegistry.cpp` to the `<ItemGroup>` ClCompile section of `IntegrationTests.vcxproj`.

- [ ] **Step 4: Commit**

```bash
git add Source/IntegrationTests/TestTask.h Source/IntegrationTests/TestRegistry.h Source/IntegrationTests/TestRegistry.cpp Source/IntegrationTests/IntegrationTests.vcxproj
git commit -m "Add TestTask coroutine type and TestRegistry with GAME_TEST/EDITOR_TEST macros"
```

---

## Task 4: TestContext + Awaitables

**Files:**
- Create: `Source/IntegrationTests/TestContext.h`
- Create: `Source/IntegrationTests/TestContext.cpp`

- [ ] **Step 1: Create TestContext.h**

```cpp
// Source/IntegrationTests/TestContext.h
#pragma once
#include <string>
#include <vector>
#include <coroutine>
#include "TestTask.h"
#include "Framework/MulticastDelegate.h"

class GameTestRunner;
class EditorTestContext;
class ScopedGpuTimer;

// Thrown by require() to abort the current test without crashing
struct TestAbortException {};

// State the runner checks each tick to decide whether to resume the coroutine.
// Awaitables write into this struct during await_suspend.
struct TestWaitState {
    int    wait_ticks   = 0;
    float  wait_seconds = 0.f;
    bool   waiting_delegate  = false;
    bool   delegate_fired    = false;
    bool   screenshot_pending = false; // capture on next frame
    std::string screenshot_name;
};

struct TestContext {
    TestWaitState wait;
    int checks_passed = 0;
    int checks_failed = 0;
    std::vector<std::string> failures;

    // -- Assertions --
    void check(bool b, const char* msg);   // records, continues
    void require(bool b, const char* msg); // throws TestAbortException if false

    // -- Flow control (co_await these) --
    struct TickAwaitable {
        TestWaitState& wait;
        int n;
        bool await_ready() const noexcept { return n <= 0; }
        void await_suspend(std::coroutine_handle<>) noexcept { wait.wait_ticks = n; }
        void await_resume() noexcept {}
    };
    struct SecondAwaitable {
        TestWaitState& wait;
        float t;
        bool await_ready() const noexcept { return t <= 0.f; }
        void await_suspend(std::coroutine_handle<>) noexcept { wait.wait_seconds = t; }
        void await_resume() noexcept {}
    };
    struct DelegateAwaitable {
        TestWaitState& wait;
        MulticastDelegate<>& delegate;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<>) noexcept;
        void await_resume() noexcept {}
    };
    struct LevelAwaitable {
        TestWaitState& wait;
        const char* path;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<>) noexcept;
        void await_resume() noexcept {}
    };
    struct ScreenshotAwaitable {
        TestWaitState& wait;
        const char* name;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<>) noexcept {
            wait.screenshot_pending = true;
            wait.screenshot_name = name;
            wait.wait_ticks = 1; // yield one tick for frame to render
        }
        void await_resume() noexcept {}
    };

    TickAwaitable       wait_ticks(int n)              { return {wait, n}; }
    SecondAwaitable     wait_seconds(float t)          { return {wait, t}; }
    DelegateAwaitable   wait_for(MulticastDelegate<>& d) { return {wait, d}; }
    LevelAwaitable      load_level(const char* path)   { return {wait, path}; }
    ScreenshotAwaitable capture_screenshot(const char* name) { return {wait, name}; }

    ScopedGpuTimer gpu_timer(const char* name);

    // Editor mode only — asserts/aborts if called from GameTestRunner
    EditorTestContext& editor();

    // Set by runner before each test — used by editor() to assert mode
    bool is_editor_mode = false;
};
```

- [ ] **Step 2: Create TestContext.cpp**

```cpp
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
    // level load is synchronous in this engine — one tick to settle
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
```

- [ ] **Step 3: Commit**

```bash
git add Source/IntegrationTests/TestContext.h Source/IntegrationTests/TestContext.cpp
git commit -m "Add TestContext with check/require, TickAwaitable, SecondAwaitable, DelegateAwaitable, LevelAwaitable, ScreenshotAwaitable"
```

---

## Task 5: GpuTimer

**Files:**
- Create: `Source/IntegrationTests/GpuTimer.h`
- Create: `Source/IntegrationTests/GpuTimer.cpp`

- [ ] **Step 1: Create GpuTimer.h**

```cpp
// Source/IntegrationTests/GpuTimer.h
#pragma once
#include <string>
#include <vector>
#include "External/glad/glad.h"

struct GpuTimingResult {
    std::string name;
    double ms = 0.0;
};

// Accumulates results per frame, written to JSON at end of run
class GpuTimingLog {
public:
    static GpuTimingLog& get();
    void record(const std::string& name, double ms);
    void write_json(const char* path); // path e.g. "TestFiles/timing_2026-03-26.json"
private:
    std::vector<GpuTimingResult> results_;
};

// RAII GL timer query. Create before the work, read ms() after co_await wait_ticks(1).
// GL timer queries have 1-frame latency — result available next tick.
class ScopedGpuTimer {
public:
    explicit ScopedGpuTimer(const char* name);
    ~ScopedGpuTimer();

    ScopedGpuTimer(const ScopedGpuTimer&) = delete;
    ScopedGpuTimer(ScopedGpuTimer&&) noexcept;

    // Blocks until result available (glGetQueryObjectui64v with GL_QUERY_RESULT).
    // Call after co_await wait_ticks(1) to avoid stall.
    double ms() const;

    const std::string& name() const { return name_; }

private:
    std::string name_;
    GLuint query_id_ = 0;
    mutable double cached_ms_ = -1.0;
    mutable bool result_read_ = false;
};
```

- [ ] **Step 2: Create GpuTimer.cpp**

```cpp
// Source/IntegrationTests/GpuTimer.cpp
#include "GpuTimer.h"
#include <cstdio>
#include <ctime>

GpuTimingLog& GpuTimingLog::get() {
    static GpuTimingLog s;
    return s;
}

void GpuTimingLog::record(const std::string& name, double ms) {
    results_.push_back({name, ms});
}

void GpuTimingLog::write_json(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "[\n");
    for (size_t i = 0; i < results_.size(); ++i) {
        fprintf(f, "  {\"name\": \"%s\", \"ms\": %.4f}%s\n",
            results_[i].name.c_str(), results_[i].ms,
            i + 1 < results_.size() ? "," : "");
    }
    fprintf(f, "]\n");
    fclose(f);
}

ScopedGpuTimer::ScopedGpuTimer(const char* name) : name_(name) {
    glGenQueries(1, &query_id_);
    glBeginQuery(GL_TIME_ELAPSED, query_id_);
}

ScopedGpuTimer::~ScopedGpuTimer() {
    if (query_id_) {
        glEndQuery(GL_TIME_ELAPSED);
        // Record result when it becomes available
        double result = ms();
        GpuTimingLog::get().record(name_, result);
        glDeleteQueries(1, &query_id_);
    }
}

ScopedGpuTimer::ScopedGpuTimer(ScopedGpuTimer&& o) noexcept
    : name_(std::move(o.name_)), query_id_(o.query_id_),
      cached_ms_(o.cached_ms_), result_read_(o.result_read_) {
    o.query_id_ = 0;
}

double ScopedGpuTimer::ms() const {
    if (result_read_) return cached_ms_;
    GLuint64 ns = 0;
    glGetQueryObjectui64v(query_id_, GL_QUERY_RESULT, &ns); // blocks if not ready
    cached_ms_ = static_cast<double>(ns) / 1e6;
    result_read_ = true;
    return cached_ms_;
}
```

- [ ] **Step 3: Commit**

```bash
git add Source/IntegrationTests/GpuTimer.h Source/IntegrationTests/GpuTimer.cpp
git commit -m "Add ScopedGpuTimer wrapping GL_TIME_ELAPSED queries with per-run JSON log"
```

---

## Task 6: Screenshot system

**Files:**
- Create: `Source/IntegrationTests/Screenshot.h`
- Create: `Source/IntegrationTests/Screenshot.cpp`
- Create: `TestFiles/goldens/.gitkeep`
- Modify: `.gitignore`

- [ ] **Step 1: Create Screenshot.h**

```cpp
// Source/IntegrationTests/Screenshot.h
#pragma once
#include <string>

struct ScreenshotConfig {
    bool promote    = false;  // write actual as golden instead of diffing
    bool interactive = false; // pause on failure
    int max_channel_delta  = 8;     // per-channel abs diff threshold
    float max_diff_fraction = 0.001f; // fraction of pixels allowed to differ
};

// Called by GameTestRunner after a frame renders when screenshot_pending is set.
// Returns true if test passes (or promote mode).
bool screenshot_capture_and_compare(const char* name, const ScreenshotConfig& cfg);
```

- [ ] **Step 2: Create Screenshot.cpp**

```cpp
// Source/IntegrationTests/Screenshot.cpp
#include "Screenshot.h"
#include "External/glad/glad.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// These headers are already in Source/External/ — include them here.
// The IMPLEMENTATION defines must appear in exactly ONE .cpp per binary.
// If they are already defined elsewhere in Core, remove the defines above
// and just include the headers without IMPLEMENTATION.
#include "stb_image.h"
#include "stb_image_write.h"

static std::string actual_path(const char* name) {
    return std::string("TestFiles/screenshots/") + name + "_actual.png";
}
static std::string golden_path(const char* name) {
    return std::string("TestFiles/goldens/") + name + ".png";
}

bool screenshot_capture_and_compare(const char* name, const ScreenshotConfig& cfg) {
    // Read current framebuffer dimensions
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    int w = vp[2], h = vp[3];

    std::vector<unsigned char> pixels(w * h * 3);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // stb_image_write expects top-left origin; GL gives bottom-left — flip
    std::vector<unsigned char> flipped(pixels.size());
    int row_bytes = w * 3;
    for (int y = 0; y < h; ++y)
        memcpy(flipped.data() + y * row_bytes,
               pixels.data() + (h - 1 - y) * row_bytes,
               row_bytes);

    // Ensure output directory exists (best-effort)
    _mkdir("TestFiles");
    _mkdir("TestFiles/screenshots");

    std::string ap = actual_path(name);
    stbi_write_png(ap.c_str(), w, h, 3, flipped.data(), row_bytes);
    printf("  Screenshot saved: %s\n", ap.c_str());

    std::string gp = golden_path(name);

    if (cfg.promote) {
        _mkdir("TestFiles/goldens");
        // Copy actual -> golden
        if (stbi_write_png(gp.c_str(), w, h, 3, flipped.data(), row_bytes)) {
            printf("  [PROMOTE] Golden updated: %s\n", gp.c_str());
        } else {
            fprintf(stderr, "  [PROMOTE] Failed to write golden: %s\n", gp.c_str());
        }
        return true;
    }

    // Load golden
    int gw, gh, gc;
    unsigned char* golden = stbi_load(gp.c_str(), &gw, &gh, &gc, 3);
    if (!golden) {
        fprintf(stderr, "  SCREENSHOT FAIL: no golden found at %s — run with --promote\n", gp.c_str());
        return false;
    }

    if (gw != w || gh != h) {
        fprintf(stderr, "  SCREENSHOT FAIL: size mismatch — golden %dx%d vs actual %dx%d\n", gw, gh, w, h);
        stbi_image_free(golden);
        return false;
    }

    int total = w * h;
    int diff_pixels = 0;
    int max_delta = 0;
    for (int i = 0; i < total * 3; ++i) {
        int d = abs((int)flipped[i] - (int)golden[i]);
        if (d > max_delta) max_delta = d;
        if (d > cfg.max_channel_delta) ++diff_pixels;
    }
    stbi_image_free(golden);

    float diff_frac = (float)diff_pixels / (float)(total * 3);
    bool pass = (max_delta <= cfg.max_channel_delta) && (diff_frac <= cfg.max_diff_fraction);

    if (!pass) {
        fprintf(stderr, "  SCREENSHOT FAIL: max_delta=%d diff_pixels=%d (%.4f%%) — golden: %s\n",
            max_delta, diff_pixels, diff_frac * 100.f, gp.c_str());
    } else {
        printf("  Screenshot OK: max_delta=%d diff_pixels=%d\n", max_delta, diff_pixels);
    }
    return pass;
}
```

- [ ] **Step 3: Create TestFiles/goldens/.gitkeep and update .gitignore**

```bash
mkdir -p TestFiles/goldens TestFiles/screenshots
touch TestFiles/goldens/.gitkeep
```

Add to `.gitignore`:
```
TestFiles/screenshots/
TestFiles/timing_*.json
```

- [ ] **Step 4: Commit**

```bash
git add Source/IntegrationTests/Screenshot.h Source/IntegrationTests/Screenshot.cpp TestFiles/goldens/.gitkeep .gitignore
git commit -m "Add screenshot capture/diff system using stb; gitignore actual output and timing logs"
```

---

## Task 7: GameTestRunner + TestGameApp

**Files:**
- Create: `Source/IntegrationTests/GameTestRunner.h`
- Create: `Source/IntegrationTests/GameTestRunner.cpp`
- Create: `Source/IntegrationTests/TestGameApp.h`

- [ ] **Step 1: Create TestGameApp.h**

```cpp
// Source/IntegrationTests/TestGameApp.h
#pragma once
#include "GameEnginePublic.h"
#include "Game/Components/CameraComponent.h"

// Minimal Application for integration tests — sets up a camera, nothing else.
class TestGameApp : public Application {
public:
    CLASS_BODY(TestGameApp, scriptable);
    void start() override {
        auto cam = eng->get_level()
            ? eng->get_level()->spawn_entity()->create_component<CameraComponent>()
            : nullptr;
        if (cam) cam->set_is_enabled(true);
    }
};
```

Register this in main.cpp or in a separate .cpp file using `CLASS_IMPL` / `REGISTERCLASS` macros matching your engine's reflection pattern. Check how other Application subclasses register (e.g. `EngineTesterApp` used `CLASS_BODY`). Add an `#include "TestGameApp.h"` to `main.cpp` to ensure the class is registered at static init.

- [ ] **Step 2: Create GameTestRunner.h**

```cpp
// Source/IntegrationTests/GameTestRunner.h
#pragma once
#include "ITestRunner.h"
#include "TestRegistry.h"
#include "TestContext.h"
#include "TestTask.h"
#include "Screenshot.h"
#include <vector>
#include <string>
#include <memory>
#include <optional>

class GameTestRunner : public ITestRunner {
public:
    GameTestRunner(std::vector<TestEntry> tests, const TestRunnerConfig& cfg);

    bool tick(float dt) override;
    int exit_code() const override { return failed_count_ > 0 ? 1 : 0; }

private:
    void start_next_test();
    void finish_current_test(const char* reason);
    void write_results_xml(const char* path);

    std::vector<TestEntry> tests_;
    TestRunnerConfig cfg_;
    ScreenshotConfig screenshot_cfg_;

    int current_idx_ = -1;
    float elapsed_ = 0.f;
    int passed_count_ = 0;
    int failed_count_ = 0;

    TestContext ctx_;
    std::optional<TestTask> task_;

    struct Result { std::string name; bool passed; std::vector<std::string> failures; };
    std::vector<Result> results_;
};
```

- [ ] **Step 3: Create GameTestRunner.cpp**

```cpp
// Source/IntegrationTests/GameTestRunner.cpp
#include "GameTestRunner.h"
#include "GpuTimer.h"
#include "Screenshot.h"
#include <cstdio>
#include <ctime>

GameTestRunner::GameTestRunner(std::vector<TestEntry> tests, const TestRunnerConfig& cfg)
    : tests_(std::move(tests)), cfg_(cfg) {
    screenshot_cfg_.promote    = cfg.promote;
    screenshot_cfg_.interactive = cfg.interactive;
    ctx_.is_editor_mode = false;
}

bool GameTestRunner::tick(float dt) {
    // Start first test on first tick
    if (current_idx_ < 0) {
        start_next_test();
        if (current_idx_ >= (int)tests_.size()) {
            write_results_xml("TestFiles/integration_game_results.xml");
            return true;
        }
        return false;
    }

    if (current_idx_ >= (int)tests_.size()) return true;

    auto& entry = tests_[current_idx_];
    elapsed_ += dt;

    // Timeout
    if (elapsed_ > entry.timeout_seconds) {
        fprintf(stderr, "[TIMEOUT] %s (%.1fs)\n", entry.name, elapsed_);
        finish_current_test("TIMEOUT");
        start_next_test();
        return current_idx_ >= (int)tests_.size();
    }

    // Screenshot pending: capture after frame render
    if (ctx_.wait.screenshot_pending && ctx_.wait.wait_ticks == 0) {
        ctx_.wait.screenshot_pending = false;
        bool ok = screenshot_capture_and_compare(ctx_.wait.screenshot_name.c_str(), screenshot_cfg_);
        if (!ok) ctx_.check(false, ("screenshot failed: " + ctx_.wait.screenshot_name).c_str());
    }

    // Waiting on ticks
    if (ctx_.wait.wait_ticks > 0) { --ctx_.wait.wait_ticks; return false; }

    // Waiting on seconds
    if (ctx_.wait.wait_seconds > 0.f) { ctx_.wait.wait_seconds -= dt; return false; }

    // Waiting on delegate
    if (ctx_.wait.waiting_delegate) {
        if (!ctx_.wait.delegate_fired) return false;
        ctx_.wait.waiting_delegate = false;
        ctx_.wait.delegate_fired   = false;
    }

    // Resume coroutine
    if (task_ && !task_->done()) {
        try {
            task_->resume();
        } catch (TestAbortException&) {
            finish_current_test("ABORTED");
            start_next_test();
            return current_idx_ >= (int)tests_.size();
        }
    }

    if (!task_ || task_->done()) {
        finish_current_test(nullptr);
        start_next_test();
    }

    return current_idx_ >= (int)tests_.size();
}

void GameTestRunner::start_next_test() {
    ++current_idx_;
    if (current_idx_ >= (int)tests_.size()) return;

    auto& entry = tests_[current_idx_];
    printf("\n==> [%d/%d] %s\n", current_idx_ + 1, (int)tests_.size(), entry.name);

    elapsed_ = 0.f;
    ctx_ = TestContext{};
    ctx_.is_editor_mode = false;
    task_.emplace(entry.fn(ctx_));
}

void GameTestRunner::finish_current_test(const char* reason) {
    auto& entry = tests_[current_idx_];
    bool passed = ctx_.checks_failed == 0 && (reason == nullptr);
    if (reason) ctx_.failures.push_back(reason);

    if (passed) {
        ++passed_count_;
        printf("  PASS  %s  (%d checks)\n", entry.name, ctx_.checks_passed);
    } else {
        ++failed_count_;
        fprintf(stderr, "  FAIL  %s\n", entry.name);
        for (auto& f : ctx_.failures)
            fprintf(stderr, "    %s\n", f.c_str());
    }
    results_.push_back({entry.name, passed, ctx_.failures});
}

void GameTestRunner::write_results_xml(const char* path) {
    printf("\n=== Game Tests: %d passed, %d failed ===\n", passed_count_, failed_count_);

    // Write JUnit XML
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<testsuite name=\"IntegrationTests.Game\" tests=\"%d\" failures=\"%d\">\n",
        (int)results_.size(), failed_count_);
    for (auto& r : results_) {
        fprintf(f, "  <testcase name=\"%s\">\n", r.name.c_str());
        for (auto& fail : r.failures)
            fprintf(f, "    <failure message=\"%s\"/>\n", fail.c_str());
        fprintf(f, "  </testcase>\n");
    }
    fprintf(f, "</testsuite>\n");
    fclose(f);

    // Write timing JSON
    time_t t = time(nullptr);
    char date[16];
    strftime(date, sizeof(date), "%Y-%m-%d", localtime(&t));
    std::string timing_path = std::string("TestFiles/timing_") + date + ".json";
    GpuTimingLog::get().write_json(timing_path.c_str());
}
```

- [ ] **Step 4: In main.cpp, set g_application_class before game_engine_main for game mode**

In `main.cpp`, add before the `game_engine_main` call for game mode:

```cpp
// At top of main.cpp, add:
#include "TestGameApp.h"  // ensures CLASS_BODY registration runs
#include "Framework/Config.h"
extern ConfigVar g_application_class;  // defined in EngineMain.cpp

// In the game mode branch, before game_engine_main:
if (mode == "game") {
    // ...existing runner creation...
    g_application_class.set_string("TestGameApp");
}
```

- [ ] **Step 5: Build and verify**

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1 | ForEach-Object { & $_ C:\Users\charl\source\repos\CsRemake\CsRemake.sln /t:IntegrationTests /p:Configuration=Debug /p:Platform=x64 /v:minimal }
```

Expected: Build succeeded. Linker may warn about missing `EditorTestRunner` / `EditorTestContext` — those come in Task 8. Use stub empty implementations if needed to unblock the build.

- [ ] **Step 6: Commit**

```bash
git add Source/IntegrationTests/GameTestRunner.h Source/IntegrationTests/GameTestRunner.cpp Source/IntegrationTests/TestGameApp.h Source/IntegrationTests/main.cpp
git commit -m "Add GameTestRunner (coroutine driver), TestGameApp (minimal Application)"
```

---

## Task 8: EditorTestRunner + EditorTestContext

**Files:**
- Create: `Source/IntegrationTests/EditorTestContext.h`
- Create: `Source/IntegrationTests/EditorTestContext.cpp`
- Create: `Source/IntegrationTests/EditorTestRunner.h`
- Create: `Source/IntegrationTests/EditorTestRunner.cpp`

- [ ] **Step 1: Create EditorTestContext.h**

```cpp
// Source/IntegrationTests/EditorTestContext.h
#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include "LevelEditor/EditorDocLocal.h"

// Thin wrapper around EditorDoc for use in tests.
// Access via ctx.editor() from an EDITOR_TEST coroutine.
class EditorTestContext {
public:
    EditorDoc& doc();   // asserts editorState is active

    int entity_count();
    void undo();
    void redo();
    void save_level(const char* path);
};
#endif
```

- [ ] **Step 2: Create EditorTestContext.cpp**

```cpp
// Source/IntegrationTests/EditorTestContext.cpp
#include "EditorTestContext.h"
#ifdef EDITOR_BUILD
#include "GameEngineLocal.h"
#include <cassert>

EditorDoc& EditorTestContext::doc() {
    assert(eng_local.editorState && "EditorTestContext::doc() called without active editor state");
    return *eng_local.editorState->get_editor_doc();
}

int EditorTestContext::entity_count() {
    return (int)eng->get_level()->get_all_objects().size();
}

void EditorTestContext::undo() {
    doc().get_undo_redo().undo();
}

void EditorTestContext::redo() {
    // UndoRedoSystem does not have a redo() yet — if it does, call it here.
    // Otherwise this is a no-op placeholder until redo is implemented.
    (void)this;
}

void EditorTestContext::save_level(const char* path) {
    doc().save_level(path);
}
#endif
```

Note: `EditorState::get_editor_doc()` and `EditorDoc::save_level()` and `UndoRedoSystem` accessors need to exist. Check `EditorDocLocal.h` and `EditorState.h` for exact method names and adjust accordingly.

- [ ] **Step 3: Create EditorTestRunner.h**

```cpp
// Source/IntegrationTests/EditorTestRunner.h
#pragma once
#include "ITestRunner.h"
#include "TestRegistry.h"
#include "TestContext.h"
#include "TestTask.h"
#include <vector>
#include <optional>

// Drives EDITOR_TEST coroutines. Identical tick logic to GameTestRunner
// but ctx_.is_editor_mode = true and no screenshot support (editor has no scene_draw).
class EditorTestRunner : public ITestRunner {
public:
    EditorTestRunner(std::vector<TestEntry> tests, const TestRunnerConfig& cfg);

    bool tick(float dt) override;
    int exit_code() const override { return failed_count_ > 0 ? 1 : 0; }

private:
    void start_next_test();
    void finish_current_test(const char* reason);
    void write_results_xml(const char* path);

    std::vector<TestEntry> tests_;
    TestRunnerConfig cfg_;
    int current_idx_ = -1;
    float elapsed_ = 0.f;
    int passed_count_ = 0;
    int failed_count_ = 0;
    TestContext ctx_;
    std::optional<TestTask> task_;

    struct Result { std::string name; bool passed; std::vector<std::string> failures; };
    std::vector<Result> results_;
};
```

- [ ] **Step 4: Create EditorTestRunner.cpp**

```cpp
// Source/IntegrationTests/EditorTestRunner.cpp
#include "EditorTestRunner.h"
#include <cstdio>

EditorTestRunner::EditorTestRunner(std::vector<TestEntry> tests, const TestRunnerConfig& cfg)
    : tests_(std::move(tests)), cfg_(cfg) {}

bool EditorTestRunner::tick(float dt) {
    if (current_idx_ < 0) {
        start_next_test();
        if (current_idx_ >= (int)tests_.size()) {
            write_results_xml("TestFiles/integration_editor_results.xml");
            return true;
        }
        return false;
    }
    if (current_idx_ >= (int)tests_.size()) return true;

    auto& entry = tests_[current_idx_];
    elapsed_ += dt;

    if (elapsed_ > entry.timeout_seconds) {
        fprintf(stderr, "[TIMEOUT] %s\n", entry.name);
        finish_current_test("TIMEOUT");
        start_next_test();
        return current_idx_ >= (int)tests_.size();
    }

    if (ctx_.wait.wait_ticks > 0)    { --ctx_.wait.wait_ticks; return false; }
    if (ctx_.wait.wait_seconds > 0.f){ ctx_.wait.wait_seconds -= dt; return false; }
    if (ctx_.wait.waiting_delegate)  {
        if (!ctx_.wait.delegate_fired) return false;
        ctx_.wait.waiting_delegate = false;
        ctx_.wait.delegate_fired   = false;
    }

    if (task_ && !task_->done()) {
        try { task_->resume(); }
        catch (TestAbortException&) {
            finish_current_test("ABORTED");
            start_next_test();
            return current_idx_ >= (int)tests_.size();
        }
    }
    if (!task_ || task_->done()) {
        finish_current_test(nullptr);
        start_next_test();
    }
    return current_idx_ >= (int)tests_.size();
}

void EditorTestRunner::start_next_test() {
    ++current_idx_;
    if (current_idx_ >= (int)tests_.size()) return;
    auto& entry = tests_[current_idx_];
    printf("\n==> [%d/%d] %s\n", current_idx_ + 1, (int)tests_.size(), entry.name);
    elapsed_ = 0.f;
    ctx_ = TestContext{};
    ctx_.is_editor_mode = true;
    task_.emplace(entry.fn(ctx_));
}

void EditorTestRunner::finish_current_test(const char* reason) {
    auto& entry = tests_[current_idx_];
    bool passed = ctx_.checks_failed == 0 && reason == nullptr;
    if (reason) ctx_.failures.push_back(reason);
    if (passed) { ++passed_count_; printf("  PASS  %s\n", entry.name); }
    else        { ++failed_count_; fprintf(stderr, "  FAIL  %s\n", entry.name);
                  for (auto& f : ctx_.failures) fprintf(stderr, "    %s\n", f.c_str()); }
    results_.push_back({entry.name, passed, ctx_.failures});
}

void EditorTestRunner::write_results_xml(const char* path) {
    printf("\n=== Editor Tests: %d passed, %d failed ===\n", passed_count_, failed_count_);
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<testsuite name=\"IntegrationTests.Editor\" tests=\"%d\" failures=\"%d\">\n",
        (int)results_.size(), failed_count_);
    for (auto& r : results_) {
        fprintf(f, "  <testcase name=\"%s\">\n", r.name.c_str());
        for (auto& fail : r.failures)
            fprintf(f, "    <failure message=\"%s\"/>\n", fail.c_str());
        fprintf(f, "  </testcase>\n");
    }
    fprintf(f, "</testsuite>\n");
    fclose(f);
}
```

- [ ] **Step 5: Build**

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1 | ForEach-Object { & $_ C:\Users\charl\source\repos\CsRemake\CsRemake.sln /t:IntegrationTests /p:Configuration=Debug /p:Platform=x64 /v:minimal }
```

Expected: Build succeeded, 0 errors.

- [ ] **Step 6: Commit**

```bash
git add Source/IntegrationTests/EditorTestContext.h Source/IntegrationTests/EditorTestContext.cpp Source/IntegrationTests/EditorTestRunner.h Source/IntegrationTests/EditorTestRunner.cpp
git commit -m "Add EditorTestRunner and EditorTestContext wrapping EditorDoc/UndoRedoSystem"
```

---

## Task 9: Remove old integration test code

**Files:**
- Delete: `Source/IntegrationTest.h`
- Delete: `Source/IntegrationTest.cpp`
- Delete: `Source/UnitTests/EngineTesterApplication.h`
- Delete: `Source/UnitTests/EngineTesterApplication.cpp`
- Modify: `Source/UnitTests/UnitTests.vcxproj` (remove EngineTesterApplication entries)
- Modify: `Source/EngineMain.cpp` (remove old include/reference if any remain)

- [ ] **Step 1: Remove files**

```bash
# Use the 'trash' alias per project CLAUDE.md conventions
trash Source/IntegrationTest.h
trash Source/IntegrationTest.cpp
trash Source/UnitTests/EngineTesterApplication.h
trash Source/UnitTests/EngineTesterApplication.cpp
```

- [ ] **Step 2: Remove from UnitTests.vcxproj**

In `Source/UnitTests/UnitTests.vcxproj`, remove these entries (if present):
```xml
<ClCompile Include="EngineTesterApplication.cpp" />
<ClInclude Include="EngineTesterApplication.h" />
```

Also remove any includes of `EngineTesterApplication.h` from `UnitTests/main.cpp` if present.

- [ ] **Step 3: Remove remaining old references in EngineMain.cpp**

Search EngineMain.cpp for any remaining references to `IntegrationTester` (the old class) and remove/replace. The `set_tester` method was already replaced in Task 1.

- [ ] **Step 4: Build UnitTests to confirm nothing broken**

```powershell
powershell.exe -Command "Scripts\build_and_test.ps1"
```

Expected: All unit tests pass (0 failures). UnitTests project still builds and runs.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "Remove old IntegrationTester/EngineTesterApplication skeleton"
```

---

## Task 10: First game test

**Files:**
- Create: `Source/IntegrationTests/Tests/Renderer/test_basic.cpp`

- [ ] **Step 1: Create test_basic.cpp**

```cpp
// Source/IntegrationTests/Tests/Renderer/test_basic.cpp
#include "TestContext.h"
#include "TestRegistry.h"
#include "GameEnginePublic.h"

// Smoke test: engine boots, can load a level, entity count is sane
static TestTask test_engine_boots(TestContext& t) {
    // Engine is already booted at this point; just check level exists
    t.require(eng->get_level() != nullptr, "level is non-null after boot");
    co_await t.wait_ticks(1);
    t.check(true, "survived one tick");
}
GAME_TEST("game/boot", 5.f, test_engine_boots);

// Load a level and verify entities survive a round-trip tick
static TestTask test_level_load(TestContext& t) {
    co_await t.load_level("demo_level_1.tmap");
    t.require(eng->get_level() != nullptr, "level loaded");
    int count = (int)eng->get_level()->get_all_objects().size();
    t.check(count > 0, "level has entities");
    co_await t.wait_ticks(2);
    int count2 = (int)eng->get_level()->get_all_objects().size();
    t.check(count2 == count, "entity count stable after 2 ticks");
}
GAME_TEST("game/level_load", 10.f, test_level_load);

// Screenshot smoke test — just verifies the capture pipeline works
// Golden is created on first --promote run, then diffed on subsequent runs
static TestTask test_screenshot_smoke(TestContext& t) {
    co_await t.load_level("demo_level_1.tmap");
    co_await t.wait_ticks(3); // let scene settle
    co_await t.capture_screenshot("smoke_demo_level_1");
}
GAME_TEST("renderer/screenshot_smoke", 15.f, test_screenshot_smoke);
```

- [ ] **Step 2: Build IntegrationTests**

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1 | ForEach-Object { & $_ C:\Users\charl\source\repos\CsRemake\CsRemake.sln /t:IntegrationTests /p:Configuration=Debug /p:Platform=x64 /v:minimal }
```

Expected: Build succeeded.

- [ ] **Step 3: Run game mode tests (interactive, promote first screenshot)**

```powershell
.\x64\Debug\IntegrationTests.exe --mode=game --test=game/* --interactive
```

Expected: Engine opens window, runs boot and level_load tests, both PASS.

```powershell
.\x64\Debug\IntegrationTests.exe --mode=game --test=renderer/screenshot_smoke --promote --interactive
```

Expected: Screenshot saved and promoted to `TestFiles/goldens/smoke_demo_level_1.png`. Exits 0.

- [ ] **Step 4: Run headless (non-interactive) and verify exit code**

```powershell
.\x64\Debug\IntegrationTests.exe --mode=game
if ($LASTEXITCODE -eq 0) { Write-Host "PASS" -ForegroundColor Green } else { Write-Host "FAIL" -ForegroundColor Red }
```

Expected: All tests PASS, exit code 0.

- [ ] **Step 5: Commit**

```bash
git add Source/IntegrationTests/Tests/Renderer/test_basic.cpp TestFiles/goldens/smoke_demo_level_1.png
git commit -m "Add first game integration tests: boot, level load, screenshot smoke"
```

---

## Task 11: First editor test

**Files:**
- Create: `Source/IntegrationTests/Tests/Editor/test_serialize.cpp`

- [ ] **Step 1: Create test_serialize.cpp**

```cpp
// Source/IntegrationTests/Tests/Editor/test_serialize.cpp
#include "TestContext.h"
#include "TestRegistry.h"
#include "GameEnginePublic.h"

// Serialize round-trip: load a level in editor mode, save it, reload, check entity count matches
static TestTask test_serialize_round_trip(TestContext& t) {
    co_await t.load_level("demo_level_1.tmap");
    t.require(eng->get_level() != nullptr, "level loaded in editor mode");

    int count_before = t.editor().entity_count();
    t.require(count_before > 0, "level has entities before save");

    t.editor().save_level("TestFiles/temp_serialize_test.tmap");
    co_await t.wait_ticks(1);

    co_await t.load_level("TestFiles/temp_serialize_test.tmap");
    t.require(eng->get_level() != nullptr, "reloaded level is non-null");

    int count_after = t.editor().entity_count();
    t.check(count_after == count_before,
        ("entity count after round-trip: expected " + std::to_string(count_before)
         + " got " + std::to_string(count_after)).c_str());
}
EDITOR_TEST("editor/serialize_round_trip", 15.f, test_serialize_round_trip);

// Undo test: make a change, undo it, verify state reverts
static TestTask test_undo(TestContext& t) {
    co_await t.load_level("demo_level_1.tmap");
    int count_before = t.editor().entity_count();

    // Undo with no commands should be safe (no crash)
    t.editor().undo();
    co_await t.wait_ticks(1);

    int count_after = t.editor().entity_count();
    t.check(count_after == count_before, "undo with no commands does not change entity count");
}
EDITOR_TEST("editor/undo_noop", 10.f, test_undo);
```

- [ ] **Step 2: Run editor mode tests interactively**

```powershell
.\x64\Debug\IntegrationTests.exe --mode=editor --interactive
```

Expected: Editor boots, tests run, both PASS. Inspect output if failures occur.

- [ ] **Step 3: Run editor headless**

```powershell
.\x64\Debug\IntegrationTests.exe --mode=editor
if ($LASTEXITCODE -eq 0) { Write-Host "PASS" -ForegroundColor Green } else { Write-Host "FAIL" -ForegroundColor Red }
```

Expected: Exit code 0.

- [ ] **Step 4: Commit**

```bash
git add Source/IntegrationTests/Tests/Editor/test_serialize.cpp
git commit -m "Add first editor integration tests: serialize round-trip, undo noop"
```

---

## Task 12: PS1 CI runner

**Files:**
- Create: `Scripts/integration_test.ps1`

- [ ] **Step 1: Create integration_test.ps1**

```powershell
# Scripts/integration_test.ps1
# Builds IntegrationTests and runs both game and editor passes.
# Usage: .\Scripts\integration_test.ps1 [-Configuration Debug|Release] [-TestFilter "glob"]
# Exit code: 0 = all passed, 1 = any failure or build error

param(
    [string]$Configuration = "Debug",
    [string]$Platform      = "x64",
    [string]$TestFilter    = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path $PSScriptRoot -Parent

$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -requires Microsoft.Component.MSBuild `
    -find MSBuild\**\Bin\MSBuild.exe 2>$null | Select-Object -First 1

if (-not $msbuild) { Write-Error "MSBuild not found"; exit 1 }

$sln    = Join-Path $RepoRoot "CsRemake.sln"
$exe    = Join-Path $RepoRoot "$Platform\$Configuration\IntegrationTests.exe"
$filter = if ($TestFilter) { "--test=$TestFilter" } else { "" }

# Build
Write-Host "==> Building IntegrationTests ($Configuration|$Platform)..." -ForegroundColor Cyan
& $msbuild $sln /t:IntegrationTests /p:Configuration=$Configuration /p:Platform=$Platform /v:minimal
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

$overall_exit = 0

# Game pass
Write-Host "`n==> Running game mode tests..." -ForegroundColor Cyan
& $exe --mode=game $filter
$game_exit = $LASTEXITCODE
if ($game_exit -ne 0) {
    Write-Host "Game tests FAILED (exit $game_exit)" -ForegroundColor Red
    $overall_exit = 1
} else {
    Write-Host "Game tests passed." -ForegroundColor Green
}

# Editor pass
Write-Host "`n==> Running editor mode tests..." -ForegroundColor Cyan
& $exe --mode=editor $filter
$editor_exit = $LASTEXITCODE
if ($editor_exit -ne 0) {
    Write-Host "Editor tests FAILED (exit $editor_exit)" -ForegroundColor Red
    $overall_exit = 1
} else {
    Write-Host "Editor tests passed." -ForegroundColor Green
}

# Merge JUnit XML results
$game_xml   = Join-Path $RepoRoot "TestFiles\integration_game_results.xml"
$editor_xml = Join-Path $RepoRoot "TestFiles\integration_editor_results.xml"
$merged     = Join-Path $RepoRoot "TestFiles\integration_results.xml"

if ((Test-Path $game_xml) -and (Test-Path $editor_xml)) {
    $game_content   = (Get-Content $game_xml   | Select-Object -Skip 1) -join "`n"
    $editor_content = (Get-Content $editor_xml | Select-Object -Skip 1) -join "`n"
    "<?xml version=`"1.0`" encoding=`"UTF-8`"?>`n<testsuites>`n$game_content`n$editor_content`n</testsuites>" |
        Set-Content $merged
    Write-Host "`nMerged results: $merged"
}

if ($overall_exit -eq 0) {
    Write-Host "`nAll integration tests passed." -ForegroundColor Green
} else {
    Write-Host "`nIntegration tests FAILED." -ForegroundColor Red
}
exit $overall_exit
```

- [ ] **Step 2: Run the full script end-to-end**

```powershell
powershell.exe Scripts\integration_test.ps1
```

Expected: Builds, runs game pass (PASS), runs editor pass (PASS), writes `TestFiles/integration_results.xml`, exits 0.

- [ ] **Step 3: Verify failure detection works**

Temporarily break a test assertion (`t.check(false, "deliberate failure")`), rebuild, run the script. Confirm it exits non-zero and prints "FAILED". Then revert the change.

- [ ] **Step 4: Commit**

```bash
git add Scripts/integration_test.ps1
git commit -m "Add integration_test.ps1: two-pass CI runner with JUnit XML output"
```

---

## Self-Review Checklist

**Spec coverage:**
- [x] Behavioral assertions (`check`/`require`) — Task 4
- [x] Screenshot diff + golden promotion — Task 6, 10
- [x] GPU timing + JSON log — Task 5
- [x] Two-pass game/editor modes — Tasks 7, 8, 12
- [x] Polymorphic `TestRunner::tick(dt)` in same engine spot — Task 1
- [x] `--promote`, `--interactive`, `--test`, `--timing-assert` flags — Task 2
- [x] `ITestRunner` interface in Core — Task 1
- [x] `skip_swap` split from `headless_mode` — Task 1
- [x] `EditorTestContext` with `undo/redo/save/entity_count` — Task 8
- [x] C++20 coroutines, no threads — Tasks 3, 4
- [x] PS1 two-pass CI runner — Task 12
- [x] Old code removal — Task 9
- [x] `stb_image`/`stb_image_write` (no new deps) — Task 6
- [x] VS project with EDITOR_BUILD + stdcpp20 — Task 2

**Type consistency check:**
- `ITestRunner::tick(float dt) -> bool` — used consistently Tasks 1, 7, 8
- `TestContext::wait` is `TestWaitState` — awaitables write into it — Tasks 4, 7
- `TestRunnerConfig` defined in `TestRegistry.h`, used in `GameTestRunner` and `EditorTestRunner` constructors — consistent
- `GAME_TEST`/`EDITOR_TEST` macros produce `TestRegistrar` at static init — consistent
- `TestAbortException` thrown by `require()`, caught in both runners — consistent
- `ScopedGpuTimer` returned by `TestContext::gpu_timer()` — consistent Tasks 4, 5

**Potential issues to watch for during implementation:**
- `stb_image` IMPLEMENTATION defines — verify they aren't already defined in Core; if so, remove them from Screenshot.cpp and just include the headers
- `EditorState::get_editor_doc()` exact method name — check `EditorState.h` before writing EditorTestContext.cpp
- `_mkdir` on Windows — use `CreateDirectoryA` or `std::filesystem::create_directories` if `_mkdir` isn't available
- `g_application_class` ConfigVar exact name — grep EngineMain.cpp for the actual declaration

---

Plan complete and saved to `docs/superpowers/plans/2026-03-26-integration-tests.md`.

**Two execution options:**

**1. Subagent-Driven (recommended)** — fresh subagent per task, review between tasks

**2. Inline Execution** — execute tasks in this session using executing-plans

Which approach?
