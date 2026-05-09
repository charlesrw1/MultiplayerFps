// Source/IntegrationTests/TestRunner.h
#pragma once
#include "ITestRunner.h"
#include "TestRegistry.h"
#include "TestContext.h"
#include "TestTask.h"
#include "Screenshot.h"
#include <vector>
#include <string>
#include <optional>

// Single mode-agnostic test runner. The mode parameter is used only to set
// `ctx.is_editor_mode` and to derive the output XML file name.
class TestRunner : public ITestRunner
{
public:
	TestRunner(TestMode mode, std::vector<TestEntry> tests, const TestRunnerConfig& cfg);

	bool tick(float dt) override;
	int exit_code() const override { return failed_count_ > 0 ? 1 : 0; }

private:
	void start_next_test();
	void finish_current_test(const char* reason);
	void write_results_xml(const char* path);

	std::vector<TestEntry> tests_;
	TestRunnerConfig cfg_;
	ScreenshotConfig screenshot_cfg_;
	TestMode mode_;

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
};
