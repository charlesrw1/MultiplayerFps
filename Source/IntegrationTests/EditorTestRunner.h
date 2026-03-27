// Source/IntegrationTests/EditorTestRunner.h
#pragma once
#include "ITestRunner.h"
#include "TestRegistry.h"
#include <vector>

// EditorTestRunner — stub, full implementation in Task 8.
class EditorTestRunner : public ITestRunner
{
public:
	EditorTestRunner(std::vector<TestEntry> tests, const TestRunnerConfig& cfg);

	bool tick(float dt) override;
	int exit_code() const override { return 0; }

private:
	std::vector<TestEntry> tests_;
	TestRunnerConfig cfg_;
};
