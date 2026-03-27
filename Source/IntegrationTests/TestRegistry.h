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

// Registers a test at static-init time
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
