// Source/IntegrationTests/TestRunner.h
#pragma once
#include "ITestRunner.h"
#include "TestRegistry.h"
#include "TestContext.h"
#include "TestTask.h"
#include "Screenshot.h"
#include "Scripting/LuaTestRunner.h"
#include <vector>
#include <string>
#include <optional>

// Single mode-agnostic test runner. Drives C++ tests first, then the Lua
// phase if Data/scripts/tests/*.lua registered any tests via add_test().
// Both phases append into the same `results_` vector and a single XML file
// is written at the end (TestFiles/integration_<mode>_results.xml).
class TestRunner : public ITestRunner, public LuaTestRunner::Sink
{
public:
	TestRunner(TestMode mode, std::vector<TestEntry> tests, const TestRunnerConfig& cfg);
	~TestRunner();

	// Filter patterns (the same list passed on the command line) — propagated
	// to Lua so its add_test() set is filtered the same way.
	void set_lua_patterns(std::vector<std::string> patterns) { lua_patterns_ = std::move(patterns); }

	bool tick(float dt) override;
	int exit_code() const override { return failed_count_ > 0 ? 1 : 0; }

	// LuaTestRunner::Sink
	void report(const std::string& name, bool passed, const std::string& message) override;
	void set_done() override;

private:
	enum class Phase { Cpp, Lua, Done };

	void start_next_test();
	void finish_current_test(const char* reason);
	void write_results_xml(const char* path);
	void start_lua_phase();
	bool tick_lua(float dt);

	std::vector<TestEntry> tests_;
	TestRunnerConfig cfg_;
	ScreenshotConfig screenshot_cfg_;
	TestMode mode_;
	std::vector<std::string> lua_patterns_;

	int current_idx_ = -1;
	float elapsed_ = 0.f;
	int passed_count_ = 0;
	int failed_count_ = 0;

	TestContext ctx_;
	std::optional<TestTask> task_;

	struct Result
	{
		std::string name;
		bool passed;
		std::vector<std::string> failures;
	};
	std::vector<Result> results_;

	Phase phase_ = Phase::Cpp;
	int lua_co_ref_ = -1; // LUA_NOREF when no coroutine
	float lua_wait_ = 0.f;
	bool lua_done_ = false;
};
